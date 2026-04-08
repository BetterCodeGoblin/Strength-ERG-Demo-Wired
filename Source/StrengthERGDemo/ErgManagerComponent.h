#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ErgGameTypes.h"
#include "Networking.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "ErgManagerComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────────────────────

/** Fired on the game thread when ErgBridge delivers a new frame of live data. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnErgFrameReceived, const FErgFrameData&, FrameData);

/** Fired on the game thread when a discrete rep is detected. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNewRep, const FRepData&, RepData);

/** Fired when connection state changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnErgConnectionChanged, bool, bConnected);

// ─────────────────────────────────────────────────────────────────────────────
//  Thread-safe staging buffer for data from receive thread → game thread
// ─────────────────────────────────────────────────────────────────────────────

struct FErgStagingBuffer
{
    FErgFrameData Frame;
    bool          bHasNewFrame    = false;
    bool          bHasNewRep      = false;
    int32         LastRepNumber   = 0;
    float         LastRepTimeSec  = 0.f;
    int32         LastPullDistance = 0;
    FCriticalSection Lock;
};

// ─────────────────────────────────────────────────────────────────────────────
//  UErgManagerComponent
//
//  Owns the TCP socket to the bridge process (USB or BLE) and fires events.
//  Attach to the GameMode or a persistent actor.
//
//  Protocol:
//    Send: 0x01 (single byte request)
//    Recv: bridge frame payload over TCP (expected CSV from bridge runtime)
//          Fields 5-7 are optional and present only when a new rep is ready.
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class STRENGTHERGDEMO_API UErgManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UErgManagerComponent();

    // ── Bridge Settings ───────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERG|Bridge")
    FString BridgeHost = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERG|Bridge")
    int32 BridgePort = 6790;

    /** Seconds to wait after BeginPlay before first connect attempt */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERG|Bridge")
    float InitialConnectDelaySec = 2.f;

    /** Seconds between reconnect attempts when disconnected */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERG|Bridge")
    float ReconnectIntervalSec = 3.f;

    // ── Simulation Settings ───────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERG|Simulation")
    bool bSimulateInput = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERG|Simulation")
    float SimStrokeRate = 22.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERG|Simulation")
    float SimPowerWatts = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERG|Simulation")
    float SimPaceSeconds = 160.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERG|Simulation")
    int32 SimPullDistance = 80;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ERG|Simulation")
    float SimRepTimeSec = 0.45f;

    // ── Read-Only State ───────────────────────────────────────────────────────

    UPROPERTY(BlueprintReadOnly, Category = "ERG|State")
    FErgFrameData LiveFrame;

    UPROPERTY(BlueprintReadOnly, Category = "ERG|State")
    bool bIsConnected = false;

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "ERG|Events")
    FOnErgFrameReceived OnErgFrameReceived;

    UPROPERTY(BlueprintAssignable, Category = "ERG|Events")
    FOnNewRep OnNewRep;

    UPROPERTY(BlueprintAssignable, Category = "ERG|Events")
    FOnErgConnectionChanged OnErgConnectionChanged;

    // ── Public API ────────────────────────────────────────────────────────────

    /** Manually trigger a simulated rep (useful for keyboard testing in PIE) */
    UFUNCTION(BlueprintCallable, Category = "ERG")
    void FireSimulatedRep(float RepTimeSec, int32 PullDistance);

    /** Returns true if TCP socket is connected to ErgBridge */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ERG")
    bool IsConnectedToErg() const { return bIsConnected; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    // Socket management
    FSocket*       Socket = nullptr;
    bool           bConnecting = false;
    float          ReconnectTimer = 0.f;

    // Background thread
    TSharedPtr<FRunnableThread> ReceiveThread;
    TSharedPtr<class FErgReceiveRunnable> ReceiveRunnable;

    // Thread-safe staging
    FErgStagingBuffer Staging;

    // Last rep number seen — used to detect new reps
    int32 LastRepNumber = 0;
    int32 SimRepCounter = 0;

    void AttemptConnect();
    void DisconnectSocket();
    void StartReceiveThread();
    void StopReceiveThread();
    void DrainStagingBuffer();
    FRepData BuildRepData(int32 RepNum, float RepTime, int32 PullDist) const;
};
