// ErgManagerComponent.cpp
// UE5 port of Unity's ErgManager + ErgBridgeClient + Concept2UsbReader.
//
// Threading model:
//   FErgReaderThread runs on a background FRunnableThread.
//   It connects to ErgBridge via TCP, reads lines at ~10 Hz,
//   and deposits parsed data into thread-safe structures.
//   TickComponent() on the game thread drains the queue and
//   fires delegates — identical to Unity's Update() pattern.

#include "ErgManagerComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Networking.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UErgManagerComponent — channel helpers (defined before thread uses them)
// ─────────────────────────────────────────────────────────────────────────────

FString UErgManagerComponent::ChannelTag() const
{
    switch (DeviceChannel)
    {
        case EDeviceChannel::Rowing:   return TEXT("[Rowing]");
        case EDeviceChannel::Cycling:  return TEXT("[Cycling]");
        case EDeviceChannel::Strength: // fall through
        default:                       return TEXT("[Strength]");
    }
}

FString UErgManagerComponent::GetDebugLine() const
{
    const FErgData& D = LatestData;

    // Disconnected: show status text loudly rather than a table of zeros.
    // This makes it impossible to mistake a dead channel for a live one.
    if (!D.bIsConnected)
    {
        FString Reason = D.StatusText.IsEmpty() ? TEXT("DISCONNECTED") : D.StatusText;
        return FString::Printf(TEXT("%s *** %s ***"), *ChannelTag(), *Reason);
    }

    switch (DeviceChannel)
    {
        case EDeviceChannel::Rowing:
            return FString::Printf(
                TEXT("[Rowing]   Connected:%s | StrokeRate:%.1f spm | Pace:%.0fs/500m | Power:%.0fW | Elapsed:%.0fs"),
                D.bIsConnected ? TEXT("Y") : TEXT("N"),
                D.StrokeRate,
                D.PaceSecPer500m,
                D.PowerWatts,
                D.ElapsedSeconds);

        case EDeviceChannel::Cycling:
            return FString::Printf(
                TEXT("[Cycling]  Connected:%s | Cadence:%.0f rpm | Power:%.0fW | Elapsed:%.0fs"),
                D.bIsConnected ? TEXT("Y") : TEXT("N"),
                D.StrokeRate,
                D.PowerWatts,
                D.ElapsedSeconds);

        case EDeviceChannel::Strength: // fall through
        default:
            return FString::Printf(
                TEXT("[Strength] Connected:%s | Reps:%d | RepTime:%.2fs | PullDist:%d | Elapsed:%.0fs"),
                D.bIsConnected ? TEXT("Y") : TEXT("N"),
                D.RepCount,
                D.RepTimeSec,
                D.PullDistance,
                D.ElapsedSeconds);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  FErgReaderThread
// ─────────────────────────────────────────────────────────────────────────────

bool FErgReaderThread::Init()
{
    return OwnerComp != nullptr;
}

uint32 FErgReaderThread::Run()
{
    ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SS) return 1;

    // Match Unity ErgBridgeClient: 5s startup delay so ErgBridge has time to bind its port
    FPlatformProcess::Sleep(5.0f);

    while (bRunning && OwnerComp->bReading)
    {
        // ── Create socket ──────────────────────────────────────────────────
        FSocket* Sock = SS->CreateSocket(NAME_Stream, TEXT("ErgBridgeClient"), false);
        if (!Sock)
        {
            FPlatformProcess::Sleep(3.f);
            continue;
        }

        TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
        bool bIsValid = false;
        Addr->SetIp(TEXT("127.0.0.1"), bIsValid);
        Addr->SetPort(OwnerComp->BridgePortInternal);

        if (!bIsValid || !Sock->Connect(*Addr))
        {
            UE_LOG(LogTemp, Warning, TEXT("%s Connection failed, retrying in 3s..."), *OwnerComp->ChannelTag());
            SS->DestroySocket(Sock);
            FPlatformProcess::Sleep(3.f);
            continue;
        }

        UE_LOG(LogTemp, Log, TEXT("%s Connected on port %d"), *OwnerComp->ChannelTag(), OwnerComp->BridgePortInternal);

        {
            FErgData D;
            D.bIsConnected = false;
            D.StatusText   = TEXT("Bridge connected");
            OwnerComp->ThreadSafe_UpdateData(D);
        }

        // Thread-local storage of the socket so Stop() can close it
        OwnerComp->TcpSocket = Sock;

        // ── Read loop ──────────────────────────────────────────────────────
        TArray<uint8> Buffer;
        Buffer.SetNumUninitialized(1024);
        FString LineBuffer;

        while (bRunning && OwnerComp->bReading)
        {
            // PM5HidDiag streams CSV lines continuously — no request byte needed.
            // Wait up to 1 second for data to arrive, then read whatever is in
            // the buffer.  This avoids false disconnects on a non-blocking socket
            // when the bridge hasn't sent a line yet.
            const bool bDataReady = Sock->Wait(
                ESocketWaitConditions::WaitForRead,
                FTimespan::FromSeconds(1.0));

            if (!bRunning || !OwnerComp->bReading)
                break;

            if (!bDataReady)
                continue;  // 1-second timeout with no data — loop and wait again

            int32 BytesRead = 0;
            if (!Sock->Recv(Buffer.GetData(), Buffer.Num(), BytesRead) || BytesRead == 0)
                break;  // genuine disconnect

            // Append received bytes to line buffer, split on newline
            FString Chunk = FString(BytesRead, UTF8_TO_TCHAR(
                reinterpret_cast<const char*>(Buffer.GetData())));
            LineBuffer += Chunk;

            int32 NewlineIdx;
            while (LineBuffer.FindChar(TEXT('\n'), NewlineIdx))
            {
                FString Line = LineBuffer.Left(NewlineIdx).TrimStartAndEnd();
                LineBuffer   = LineBuffer.Mid(NewlineIdx + 1);

                if (Line.IsEmpty()) continue;

                if (OwnerComp->bBleMode)
                    ParseJsonLine(Line);
                else
                    ParseCsvLine(Line);
            }
        }

        // ── Cleanup ────────────────────────────────────────────────────────
        OwnerComp->TcpSocket = nullptr;
        SS->DestroySocket(Sock);

        FErgData D;
        D.bIsConnected = false;
        D.StatusText   = TEXT("Disconnected");
        OwnerComp->ThreadSafe_UpdateData(D);

        if (bRunning && OwnerComp->bReading)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s Disconnected, reconnecting in 3s..."), *OwnerComp->ChannelTag());
            FPlatformProcess::Sleep(3.f);
        }
    }

    return 0;
}

void FErgReaderThread::ParseCsvLine(const FString& Line)
{
    // Supported strength bridge format:
    //   repCount,repTimeSec,pullDistance,connected
    // Legacy fallback format:
    //   rate,pace,power,connected
    TArray<FString> Parts;
    Line.ParseIntoArray(Parts, TEXT(","), true);
    if (Parts.Num() < 4) return;

    const bool bConnected = (Parts[3].TrimStartAndEnd() == TEXT("1"));

    FErgData D;
    D.bIsConnected = bConnected;
    D.StatusText   = bConnected ? TEXT("Connected") : TEXT("ERG not connected");

    const int32 FirstInt = FCString::Atoi(*Parts[0]);
    const bool bLooksLikeStrengthRepFrame = FirstInt > 0 && Parts[0].Find(TEXT(".")) == INDEX_NONE;

    if (bLooksLikeStrengthRepFrame)
    {
        D.RepCount     = FirstInt;
        D.RepTimeSec   = FCString::Atof(*Parts[1]);
        D.PullDistance = FCString::Atoi(*Parts[2]);
        D.bIsActive    = D.RepCount > 0;

        // CSV telemetry is a complete frame — use snapshot path so that a
        // resting device reporting RepTimeSec=0 actually shows 0, not the
        // last active value.
        OwnerComp->ThreadSafe_ReplaceSnapshot(D);
        OwnerComp->ThreadSafe_EnqueueRep(D.RepCount, D.RepTimeSec, D.PullDistance);
        return;
    }

    // Rowing/legacy telemetry fallback (also a complete frame)
    D.RepTimeSec     = FCString::Atof(*Parts[0]);
    D.ElapsedSeconds = FCString::Atof(*Parts[1]);
    D.PullDistance   = (int32)FCString::Atof(*Parts[2]);

    OwnerComp->ThreadSafe_ReplaceSnapshot(D);
}

void FErgReaderThread::ParseJsonLine(const FString& Line)
{
    // Lightweight JSON parse — mirrors Unity's JsonField() method.
    // Fields: type, repCount, driveTimeSec, pullDistance, heartRate, elapsedSec, connected, statusText

    auto GetField = [&](const FString& Key) -> FString
    {
        FString Search = FString::Printf(TEXT("\"%s\":"), *Key);
        int32 Idx = Line.Find(Search);
        if (Idx == INDEX_NONE) return TEXT("");

        int32 Start = Idx + Search.Len();
        while (Start < Line.Len() && Line[Start] == TEXT(' ')) ++Start;
        if (Start >= Line.Len()) return TEXT("");

        if (Line[Start] == TEXT('"'))
        {
            int32 End = Line.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start + 1);
            return End != INDEX_NONE ? Line.Mid(Start + 1, End - Start - 1) : TEXT("");
        }
        else
        {
            int32 End = Start;
            while (End < Line.Len() && Line[End] != TEXT(',') && Line[End] != TEXT('}')) ++End;
            return Line.Mid(Start, End - Start).TrimStartAndEnd();
        }
    };

    FString Type = GetField(TEXT("type"));

    if (Type == TEXT("rep"))
    {
        int32 RepCount   = FCString::Atoi(*GetField(TEXT("repCount")));
        float DriveTime  = FCString::Atof(*GetField(TEXT("driveTimeSec")));
        int32 PullDist   = FCString::Atoi(*GetField(TEXT("pullDistance")));
        int32 HR         = FCString::Atoi(*GetField(TEXT("heartRate")));
        float Elapsed    = FCString::Atof(*GetField(TEXT("elapsedSec")));

        if (RepCount <= 0 || DriveTime <= 0.f || PullDist <= 0) return;

        FErgData D;
        D.RepCount       = RepCount;
        D.RepTimeSec     = DriveTime;
        D.PullDistance   = PullDist;
        D.HeartRate      = HR;
        D.ElapsedSeconds = Elapsed;
        D.bIsConnected   = true;
        D.bIsActive      = true;
        D.StatusText     = FString::Printf(TEXT("BLE Active (Rep #%d)"), RepCount);

        OwnerComp->ThreadSafe_UpdateData(D);
        OwnerComp->ThreadSafe_EnqueueRep(RepCount, DriveTime, PullDist);
    }
    else if (Type == TEXT("status"))
    {
        bool  bConnected = GetField(TEXT("connected")).ToBool();
        int32 HR         = FCString::Atoi(*GetField(TEXT("heartRate")));
        float Elapsed    = FCString::Atof(*GetField(TEXT("elapsedSec")));
        FString StatusTxt = GetField(TEXT("statusText"));

        // Streaming telemetry fields — populated by the bridge every poll cycle.
        // All are assigned directly into the struct with NO zero-guards so that
        // a legitimately idle device (0 cadence, 0 watts) reaches SharedData.
        // ThreadSafe_ReplaceSnapshot() will write them unconditionally.
        float StrokeRateVal = FCString::Atof(*GetField(TEXT("strokeRate")));
        float PowerWattsVal = FCString::Atof(*GetField(TEXT("powerWatts")));
        float PaceSecVal    = FCString::Atof(*GetField(TEXT("paceSec500m")));
        int32 RepCnt        = FCString::Atoi(*GetField(TEXT("repCount")));
        float DriveTime     = FCString::Atof(*GetField(TEXT("driveTimeSec")));
        int32 PullDst       = FCString::Atoi(*GetField(TEXT("pullDistance")));

        FErgData D;
        D.bIsConnected   = bConnected;
        D.bIsActive      = RepCnt > 0;
        D.HeartRate      = HR;
        D.ElapsedSeconds = Elapsed;
        D.StatusText     = StatusTxt.IsEmpty() ? TEXT("BLE status") : StatusTxt;
        D.StrokeRate     = StrokeRateVal;
        D.PowerWatts     = PowerWattsVal;
        D.PaceSecPer500m = PaceSecVal;
        D.RepCount       = RepCnt;
        D.RepTimeSec     = DriveTime;
        D.PullDistance   = PullDst;

        // Status frame is a complete snapshot — every field is authoritative.
        OwnerComp->ThreadSafe_ReplaceSnapshot(D);
    }
}

void FErgReaderThread::Stop()
{
    bRunning = false;
    if (OwnerComp && OwnerComp->TcpSocket)
        OwnerComp->TcpSocket->Close();
}

void FErgReaderThread::Exit()
{
    bRunning = false;
}

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
        LatestData.bIsConnected = true;
        LatestData.StatusText   = TEXT("Simulated");
        UE_LOG(LogTemp, Log, TEXT("[ErgManager] Simulation mode."));
        return;
    }

    BridgePortInternal = bUseBleWireless ? 6790 : BridgePort;
    bBleMode           = bUseBleWireless || bUseJsonFormat;
    bReading           = true;

    LaunchBridgeProcess();

    ReaderRunnable = new FErgReaderThread(this);
    FString ThreadName = FString::Printf(TEXT("ErgReaderThread_%s"), *ChannelTag());
    ReaderThread   = FRunnableThread::Create(ReaderRunnable, *ThreadName,
                                             0, TPri_BelowNormal);
}

void UErgManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bSimulateInput)
    {
        LatestData.RepTimeSec  = SimulatedStrokeRate;
        LatestData.PullDistance = (int32)SimulatedPower;
        LatestData.ElapsedSeconds = SimulatedPaceSec;
        return;
    }

    // Copy shared data to game-thread cache
    {
        FScopeLock Lock(&DataLock);
        LatestData = SharedData;
    }

    // Fire new-rep events
    TArray<FPendingRep> Reps;
    {
        FScopeLock Lock(&RepLock);
        Reps = MoveTemp(PendingReps);
    }

    for (const FPendingRep& Rep : Reps)
    {
        // Only fire if rep count advanced (handles thread races)
        if (Rep.RepNumber > LastRepCount)
        {
            LastRepCount = Rep.RepNumber;
            OnNewRep.Broadcast(Rep.RepNumber, Rep.RepTimeSec, Rep.PullDistance);
        }
    }

    // CSV strength bridge may update RepCount without queue timing if a frame lands before first enqueue.
    if (LatestData.RepCount > 0)
    {
        if (!bCsvRepCountSeen)
        {
            bCsvRepCountSeen = true;
            LastRepCount = FMath::Max(LastRepCount, LatestData.RepCount - 1);
        }

        if (LatestData.RepCount > LastRepCount)
        {
            LastRepCount = LatestData.RepCount;
            OnNewRep.Broadcast(LatestData.RepCount, LatestData.RepTimeSec, LatestData.PullDistance);
        }
    }

    OnErgDataUpdated.Broadcast(LatestData);
}

void UErgManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bReading = false;

    if (ReaderRunnable) ReaderRunnable->Stop();
    if (ReaderThread)
    {
        ReaderThread->WaitForCompletion();
        delete ReaderThread;
        ReaderThread = nullptr;
    }
    delete ReaderRunnable;
    ReaderRunnable = nullptr;

    KillBridgeProcess();
    Super::EndPlay(EndPlayReason);
}

void UErgManagerComponent::ThreadSafe_ReplaceSnapshot(const FErgData& Snapshot)
{
    FScopeLock Lock(&DataLock);

    // These fields are authoritative in every frame, connected or not.
    SharedData.bIsConnected = Snapshot.bIsConnected;
    SharedData.bIsActive    = Snapshot.bIsActive;
    if (!Snapshot.StatusText.IsEmpty())
        SharedData.StatusText = Snapshot.StatusText;

    // Disconnected: clear all live streaming fields immediately.
    // Rep history (RepCount/RepTimeSec/PullDistance) is preserved because it
    // represents completed work, not a live stream, and clearing it on a brief
    // BLE blip would corrupt the strength rep history mid-session.
    if (!Snapshot.bIsConnected)
    {
        SharedData.StrokeRate     = 0.f;
        SharedData.PowerWatts     = 0.f;
        SharedData.PaceSecPer500m = 0.f;
        SharedData.ElapsedSeconds = 0.f;
        SharedData.HeartRate      = 0;
        return;
    }

    // Connected snapshot: assign ALL fields directly.
    // Zero IS a valid measurement here — a resting rower has 0 cadence and 0 watts,
    // and the HUD must show that truth, not the last active value.
    SharedData.StrokeRate     = Snapshot.StrokeRate;
    SharedData.PowerWatts     = Snapshot.PowerWatts;
    SharedData.PaceSecPer500m = Snapshot.PaceSecPer500m;
    SharedData.ElapsedSeconds = Snapshot.ElapsedSeconds;
    SharedData.HeartRate      = Snapshot.HeartRate;
    SharedData.RepCount       = Snapshot.RepCount;
    SharedData.RepTimeSec     = Snapshot.RepTimeSec;
    SharedData.PullDistance   = Snapshot.PullDistance;
}

void UErgManagerComponent::ThreadSafe_UpdateData(const FErgData& NewData)
{
    FScopeLock Lock(&DataLock);

    // These fields are authoritative in every frame, connected or not.
    SharedData.bIsConnected = NewData.bIsConnected;
    SharedData.bIsActive    = NewData.bIsActive;
    if (!NewData.StatusText.IsEmpty())
        SharedData.StatusText = NewData.StatusText;

    // Disconnected: same clear as ReplaceSnapshot.
    if (!NewData.bIsConnected)
    {
        SharedData.StrokeRate     = 0.f;
        SharedData.PowerWatts     = 0.f;
        SharedData.PaceSecPer500m = 0.f;
        SharedData.ElapsedSeconds = 0.f;
        SharedData.HeartRate      = 0;
        return;
    }

    // Connected sparse event (rep event or connection housekeeping):
    // Only update rep-specific fields and supplemental context (HR, elapsed).
    // StrokeRate / PowerWatts / PaceSecPer500m are intentionally NOT touched here —
    // a rep event carries zeros for those fields because they are absent from the
    // rep JSON, and writing those zeros would wipe the last good snapshot value
    // for the fraction of a second before the next status frame arrives.
    if (NewData.RepCount       > 0) SharedData.RepCount       = NewData.RepCount;
    if (NewData.RepTimeSec     > 0) SharedData.RepTimeSec     = NewData.RepTimeSec;
    if (NewData.PullDistance   > 0) SharedData.PullDistance   = NewData.PullDistance;
    if (NewData.ElapsedSeconds > 0) SharedData.ElapsedSeconds = NewData.ElapsedSeconds;
    if (NewData.HeartRate      > 0) SharedData.HeartRate      = NewData.HeartRate;
    // StrokeRate, PowerWatts, PaceSecPer500m: owned by ThreadSafe_ReplaceSnapshot only.
}

void UErgManagerComponent::ThreadSafe_EnqueueRep(int32 Num, float Time, int32 Dist)
{
    FScopeLock Lock(&RepLock);
    FPendingRep R;
    R.RepNumber   = Num;
    R.RepTimeSec  = Time;
    R.PullDistance = Dist;
    PendingReps.Add(R);
}

void UErgManagerComponent::LaunchBridgeProcess()
{
    // Empty path = another process (e.g. PM5BleBridge) owns this port. Skip silently.
    if (BridgeExePath.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("%s No bridge exe configured — expecting external process on port %d"),
               *ChannelTag(), BridgePortInternal);
        return;
    }

    FString FullPath;

    if (FPaths::IsRelative(BridgeExePath))
    {
        // Get absolute project directory
        FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

        // Combine with bridge exe path using concatenation
        // NOTE: do not use the / operator here — ProjectDir already ends with /
        // and the / operator will strip the leading / from ../ causing path corruption
        FullPath = ProjectDir + BridgeExePath;

        // Normalize slashes
        FPaths::NormalizeFilename(FullPath);

        // Collapse ../ and ./ markers
        FPaths::CollapseRelativeDirectories(FullPath);
    }
    else
    {
        // Already absolute, just normalize
        FullPath = BridgeExePath;
        FPaths::NormalizeFilename(FullPath);
    }

    if (!FPaths::FileExists(FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[ErgManager] ErgBridge not found at: %s"), *FullPath);
        return;
    }

    BridgeProcessHandle = FPlatformProcess::CreateProc(
        *FullPath,          // exe
        TEXT(""),           // args
        true,               // bLaunchDetached
        false,              // bLaunchHidden — visible so we can see crash output
        false,              // bLaunchReallyHidden
        nullptr,            // out PID
        0,                  // priority
        nullptr,            // opt dir
        nullptr,            // pipe write
        nullptr             // pipe read
    );

    if (BridgeProcessHandle.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("[ErgManager] ErgBridge launched."));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[ErgManager] Failed to launch ErgBridge."));
    }
}

void UErgManagerComponent::KillBridgeProcess()
{
    if (BridgeProcessHandle.IsValid())
    {
        FPlatformProcess::TerminateProc(BridgeProcessHandle, true);
        FPlatformProcess::CloseProc(BridgeProcessHandle);
        UE_LOG(LogTemp, Log, TEXT("[ErgManager] ErgBridge killed."));
    }
}
