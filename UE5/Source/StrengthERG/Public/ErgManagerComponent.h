#pragma once

/**
 * ErgManagerComponent.h
 *
 * UE5 equivalent of Unity's ErgManager + Concept2UsbReader.
 *
 * Architecture change from Unity:
 *   Unity used a MonoBehaviour singleton with DontDestroyOnLoad.
 *   In UE5, this is an ActorComponent you attach to the GameMode or
 *   a persistent "ERGManager" Actor in the persistent level.
 *   The component handles:
 *     1. Launching ErgBridge.exe as a child process
 *     2. Connecting to it via TCP on 127.0.0.1:6789
 *     3. Reading CSV lines (rate,pace,power,connected) at ~10 Hz
 *     4. Exposing live data + a delegate for rep detection
 *
 * Simulation mode: set bSimulateInput = true in the Inspector.
 * BLE mode (port 6790, JSON lines): toggled by bUseBleWireless.
 *
 * NOTE: The x86 DLL path (Concept2Native.cs / PM3CsafeCP.dll) is NOT
 * ported here. In UE5 on Windows you can call the DLL via FPlatformProcess
 * or LoadLibraryW, but the preferred path remains the ErgBridge/ErgBridgeBLE
 * process approach because:
 *   a) The DLL is x86-only; UE5 editors ship as x64.
 *   b) The bridge decouples the game process from USB driver ownership.
 * See Concept2NativeWrapper.h / .cpp if you need direct DLL access.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/CircularQueue.h"
#include "ErgTypes.h"
#include "ErgManagerComponent.generated.h"

// ── Delegates ──────────────────────────────────────────────────────────────

/** Fired on the game thread when a new rep is detected. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNewRep,
    int32,  RepNumber,
    float,  RepTimeSec,
    int32,  PullDistance);

/** Fired on the game thread whenever the ERG data snapshot is refreshed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnErgDataUpdated, FErgData, Data);

// ── Pending rep POD (inter-thread transfer) ────────────────────────────────

struct FPendingRep
{
    int32 RepNumber;
    float RepTimeSec;
    int32 PullDistance;
};

// ── Background read thread ─────────────────────────────────────────────────

class FErgReaderThread : public FRunnable
{
public:
    FErgReaderThread(class UErgManagerComponent* Owner) : OwnerComp(Owner) {}

    virtual bool   Init()   override;
    virtual uint32 Run()    override;
    virtual void   Stop()   override;
    virtual void   Exit()   override;

private:
    class UErgManagerComponent* OwnerComp;
    bool bRunning = true;
};

// ── Component ──────────────────────────────────────────────────────────────

UCLASS(ClassGroup = (StrengthERG), meta = (BlueprintSpawnableComponent))
class STRENGTHERG_API UErgManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UErgManagerComponent();

    // ── Inspector / Config ──────────────────────────────────────────────────

    /** Path to ErgBridge.exe, relative to project root (or absolute). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
    FString BridgeExePath = TEXT("ErgBridge/ErgBridge.exe");

    /** Port ErgBridge listens on (wired USB: 6789, BLE: 6790). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
    int32 BridgePort = 6789;

    /** Use BLE bridge (JSON lines on port 6790) instead of wired CSV. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
    bool bUseBleWireless = false;

    /** Skip hardware; emit simulated data in Update. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
    bool bSimulateInput = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation",
              meta = (EditCondition = "bSimulateInput"))
    float SimulatedStrokeRate = 22.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation",
              meta = (EditCondition = "bSimulateInput"))
    float SimulatedPower = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation",
              meta = (EditCondition = "bSimulateInput"))
    float SimulatedPaceSec = 160.f;

    // ── Delegates ───────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "ERG|Events")
    FOnNewRep OnNewRep;

    UPROPERTY(BlueprintAssignable, Category = "ERG|Events")
    FOnErgDataUpdated OnErgDataUpdated;

    // ── Public Read-Only Data (game-thread safe) ────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "ERG")
    FErgData GetCurrentData() const { return LatestData; }

    // ── UActorComponent interface ────────────────────────────────────────────

    virtual void BeginPlay()  override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ── Called by reader thread (thread-safe writes) ─────────────────────────

    void ThreadSafe_UpdateData(const FErgData& NewData);
    void ThreadSafe_EnqueueRep(int32 Num, float Time, int32 Dist);

private:
    // ── Process management ──────────────────────────────────────────────────

    void LaunchBridgeProcess();
    void KillBridgeProcess();

    // ── Thread plumbing ─────────────────────────────────────────────────────

    FErgReaderThread*   ReaderRunnable = nullptr;
    FRunnableThread*    ReaderThread   = nullptr;

    // ── Thread-safe shared state ────────────────────────────────────────────

    mutable FCriticalSection DataLock;
    FErgData                 SharedData;          // written by thread, read by Tick

    TArray<FPendingRep>      PendingReps;          // flushed on game thread
    FCriticalSection         RepLock;

    // ── Game-thread cache ───────────────────────────────────────────────────

    FErgData LatestData;
    int32    LastRepCount = -1;

    // ── Bridge process ──────────────────────────────────────────────────────

    FProcHandle  BridgeProcessHandle;

public:
    // Exposed to reader thread — use lock before accessing!
    FSocket*    TcpSocket   = nullptr;
    TAtomic<bool> bReading  = false;
    int32        BridgePortInternal = 6789;
    bool         bBleMode   = false;
};
