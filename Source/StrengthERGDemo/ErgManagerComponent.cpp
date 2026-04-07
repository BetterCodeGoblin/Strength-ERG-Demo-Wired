#include "ErgManagerComponent.h"
#include "Misc/Paths.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Common/TcpSocketBuilder.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Background receive thread
// ─────────────────────────────────────────────────────────────────────────────

class FErgReceiveRunnable : public FRunnable
{
public:
    FErgReceiveRunnable(FSocket* InSocket, FErgStagingBuffer& InBuffer)
        : Socket(InSocket), Buffer(InBuffer), bShouldRun(true)
    {}

    virtual bool Init() override { return true; }

    virtual uint32 Run() override
    {
        TArray<uint8> RecvBuffer;
        RecvBuffer.SetNumUninitialized(512);

        const uint8 RequestByte = 0x01;

        while (bShouldRun && Socket)
        {
            // Send poll request
            int32 Sent = 0;
            if (!Socket->Send(&RequestByte, 1, Sent) || Sent != 1)
                break;

            // Read response with a short timeout
            int32 BytesRead = 0;
            FMemory::Memzero(RecvBuffer.GetData(), RecvBuffer.Num());
            if (!Socket->Recv(RecvBuffer.GetData(), RecvBuffer.Num() - 1, BytesRead) || BytesRead == 0)
                break;

            // Parse CSV: "rate,pace,power,connected[,repNum,repTime,pullDist]"
            FString Raw = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(RecvBuffer.GetData())));
            Raw.TrimStartAndEndInline();

            TArray<FString> Parts;
            Raw.ParseIntoArray(Parts, TEXT(","), true);

            if (Parts.Num() >= 4)
            {
                FScopeLock Lock(&Buffer.Lock);

                Buffer.Frame.StrokeRate   = FCString::Atof(*Parts[0]);
                Buffer.Frame.PaceSeconds  = FCString::Atof(*Parts[1]);
                Buffer.Frame.PowerWatts   = FCString::Atof(*Parts[2]);
                Buffer.Frame.bConnected   = Parts[3].TrimStartAndEnd() == TEXT("1");
                Buffer.bHasNewFrame       = true;

                // Optional rep fields
                if (Parts.Num() >= 7)
                {
                    int32 RepNum = FCString::Atoi(*Parts[4]);
                    if (RepNum != Buffer.LastRepNumber)
                    {
                        Buffer.LastRepNumber    = RepNum;
                        Buffer.LastRepTimeSec   = FCString::Atof(*Parts[5]);
                        Buffer.LastPullDistance = FCString::Atoi(*Parts[6]);
                        Buffer.bHasNewRep       = true;
                    }
                }
            }

            FPlatformProcess::Sleep(0.1f); // 10 Hz
        }

        return 0;
    }

    virtual void Stop() override  { bShouldRun = false; }
    virtual void Exit() override  {}

private:
    FSocket*          Socket;
    FErgStagingBuffer& Buffer;
    volatile bool     bShouldRun;
};

// ─────────────────────────────────────────────────────────────────────────────
//  UErgManagerComponent
// ─────────────────────────────────────────────────────────────────────────────

UErgManagerComponent::UErgManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UErgManagerComponent::BeginPlay()
{
    Super::BeginPlay();

    if (bSimulateInput)
    {
        bIsConnected = true;
        UE_LOG(LogTemp, Log, TEXT("[ErgManager] Simulation mode active."));
        return;
    }

    // Delay initial connect
    ReconnectTimer = InitialConnectDelaySec;
}

void UErgManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopReceiveThread();
    DisconnectSocket();
    Super::EndPlay(EndPlayReason);
}

void UErgManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bSimulateInput)
    {
        // Update live frame with sim values
        LiveFrame.StrokeRate  = SimStrokeRate;
        LiveFrame.PowerWatts  = SimPowerWatts;
        LiveFrame.PaceSeconds = SimPaceSeconds;
        LiveFrame.bConnected  = true;
        return;
    }

    // Handle reconnect timer
    if (!bIsConnected && !ReceiveRunnable.IsValid())
    {
        ReconnectTimer -= DeltaTime;
        if (ReconnectTimer <= 0.f)
        {
            AttemptConnect();
            ReconnectTimer = ReconnectIntervalSec;
        }
    }

    // Drain staging buffer on game thread
    DrainStagingBuffer();
}

void UErgManagerComponent::AttemptConnect()
{
    ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSub)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ErgManager] No socket subsystem."));
        return;
    }

    // Resolve endpoint
    FIPv4Address Addr;
    FIPv4Address::Parse(BridgeHost, Addr);
    FIPv4Endpoint Endpoint(Addr, (uint16)BridgePort);

    Socket = SocketSub->CreateSocket(NAME_Stream, TEXT("ErgBridgeSocket"), false);
    if (!Socket)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ErgManager] Failed to create socket."));
        return;
    }

    Socket->SetNonBlocking(false);
    Socket->SetNoDelay(true);

    TSharedRef<FInternetAddr> InternetAddr = SocketSub->CreateInternetAddr();
    InternetAddr->SetIp(Addr.Value);
    InternetAddr->SetPort(BridgePort);

    if (!Socket->Connect(*InternetAddr))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ErgManager] Could not connect to ErgBridge at %s:%d"), *BridgeHost, BridgePort);
        SocketSub->DestroySocket(Socket);
        Socket = nullptr;
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[ErgManager] Connected to ErgBridge at %s:%d"), *BridgeHost, BridgePort);
    bIsConnected = true;
    OnErgConnectionChanged.Broadcast(true);

    StartReceiveThread();
}

void UErgManagerComponent::DisconnectSocket()
{
    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        Socket = nullptr;
    }

    if (bIsConnected)
    {
        bIsConnected = false;
        OnErgConnectionChanged.Broadcast(false);
    }
}

void UErgManagerComponent::StartReceiveThread()
{
    if (!Socket) return;

    ReceiveRunnable = MakeShared<FErgReceiveRunnable>(Socket, Staging);
    ReceiveThread = TSharedPtr<FRunnableThread>(
        FRunnableThread::Create(ReceiveRunnable.Get(), TEXT("ErgBridgeReceive"), 0, TPri_Normal)
    );
}

void UErgManagerComponent::StopReceiveThread()
{
    if (ReceiveRunnable.IsValid())
    {
        ReceiveRunnable->Stop();
    }
    if (ReceiveThread.IsValid())
    {
        ReceiveThread->WaitForCompletion();
        ReceiveThread.Reset();
    }
    ReceiveRunnable.Reset();
}

void UErgManagerComponent::DrainStagingBuffer()
{
    bool bGotFrame = false;
    bool bGotRep   = false;
    FErgFrameData FrameCopy;
    int32 RepNum = 0;
    float RepTime = 0.f;
    int32 PullDist = 0;

    {
        FScopeLock Lock(&Staging.Lock);

        if (Staging.bHasNewFrame)
        {
            FrameCopy          = Staging.Frame;
            bGotFrame          = true;
            Staging.bHasNewFrame = false;
        }

        if (Staging.bHasNewRep)
        {
            RepNum             = Staging.LastRepNumber;
            RepTime            = Staging.LastRepTimeSec;
            PullDist           = Staging.LastPullDistance;
            bGotRep            = true;
            Staging.bHasNewRep = false;
        }
    }

    if (bGotFrame)
    {
        LiveFrame = FrameCopy;
        if (FrameCopy.bConnected != bIsConnected)
        {
            bIsConnected = FrameCopy.bConnected;
            OnErgConnectionChanged.Broadcast(bIsConnected);
        }
        OnErgFrameReceived.Broadcast(LiveFrame);
    }

    if (bGotRep && LastRepNumber != RepNum)
    {
        LastRepNumber = RepNum;
        FRepData Rep = BuildRepData(RepNum, RepTime, PullDist);
        OnNewRep.Broadcast(Rep);
    }
}

void UErgManagerComponent::FireSimulatedRep(float RepTimeSec, int32 PullDistance)
{
    ++SimRepCounter;
    FRepData Rep = BuildRepData(SimRepCounter, RepTimeSec, PullDistance);
    OnNewRep.Broadcast(Rep);
}

FRepData UErgManagerComponent::BuildRepData(int32 RepNum, float RepTime, int32 PullDist) const
{
    FRepData Rep;
    Rep.RepNumber    = RepNum;
    Rep.RepTimeSec   = RepTime;
    Rep.PullDistance = PullDist;
    Rep.RepPower     = (RepTime > KINDA_SMALL_NUMBER) ? (float)PullDist / RepTime : 0.f;

    // Power zone classification
    if      (Rep.RepPower >= 70.f) Rep.PowerZone = EPowerZone::Explosive;
    else if (Rep.RepPower >= 45.f) Rep.PowerZone = EPowerZone::Power;
    else if (Rep.RepPower >= 25.f) Rep.PowerZone = EPowerZone::Moderate;
    else                           Rep.PowerZone = EPowerZone::Low;

    return Rep;
}
