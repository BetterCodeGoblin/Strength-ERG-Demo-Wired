#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ErgGameTypes.h"
#include "BoulderGameMode.generated.h"

class ABoulderActor;
class UErgManagerComponent;
class UQTEComponent;

// ─────────────────────────────────────────────────────────────────────────────
//  Delegates — usable from Blueprint event graphs
// ─────────────────────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameStateChanged, EBoulderGameState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepProcessed, const FRepData&, Rep);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameEnded, const FGameStats&, Stats);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCountdownTick, float, SecondsRemaining);

// ─────────────────────────────────────────────────────────────────────────────
//  ABoulderGameMode
//
//  Central game-state machine.
//  - Owns UErgManagerComponent + UQTEComponent
//  - Drives ABoulderActor
//  - Handles game flow: Idle → Countdown → Playing → Won/Lost
//
//  Wire everything in Blueprint by subclassing this and assigning BoulderActorRef.
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(BlueprintType, Blueprintable)
class STRENGTHERGDEMO_API ABoulderGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ABoulderGameMode();

    // ── Scene References ──────────────────────────────────────────────────────

    /** The boulder actor placed in the level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|References")
    TObjectPtr<ABoulderActor> BoulderActorRef;

    // ── Game Rules ────────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Rules")
    float TimeLimitSeconds = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Rules")
    float RollbackDelaySec = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Rules")
    float AnimationWindowDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Rules")
    float CountdownDuration = 3.f;

    // ── Power Zone Thresholds ─────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|PowerZones")
    float ExplosiveThreshold = 70.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|PowerZones")
    float PowerThreshold = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|PowerZones")
    float ModerateThreshold = 25.f;

    // ── Simulation ────────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Simulation")
    bool bSimulateInput = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Simulation")
    int32 SimPullDistance = 80;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Simulation")
    float SimRepTimeSec = 0.45f;

    // ── Read-Only State ───────────────────────────────────────────────────────

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|State")
    EBoulderGameState GameState = EBoulderGameState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|State")
    float TimeRemaining = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|State")
    int32 TotalReps = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|State")
    int32 Combo = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|State")
    int32 PerfectCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|State")
    float CurrentPower = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|State")
    float PeakPower = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|State")
    float AveragePower = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|State")
    EPowerZone CurrentZone = EPowerZone::Low;

    // ── Sub-components ────────────────────────────────────────────────────────

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|Components")
    TObjectPtr<UErgManagerComponent> ErgManager;

    UPROPERTY(BlueprintReadOnly, Category = "Boulder|Components")
    TObjectPtr<UQTEComponent> QTE;

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Boulder|Events")
    FOnGameStateChanged OnGameStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Boulder|Events")
    FOnRepProcessed OnRepProcessed;

    UPROPERTY(BlueprintAssignable, Category = "Boulder|Events")
    FOnGameEnded OnGameEnded;

    UPROPERTY(BlueprintAssignable, Category = "Boulder|Events")
    FOnCountdownTick OnCountdownTick;

    // ── Public API ────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Boulder")
    void StartGame();

    UFUNCTION(BlueprintCallable, Category = "Boulder")
    void RestartGame() { StartGame(); }

    /**
     * Call this from your player character's push animation Blueprint
     * at the peak of the push effort to open the QTE bonus window.
     */
    UFUNCTION(BlueprintCallable, Category = "Boulder")
    void NotifyAnimationPeak();

    /** Keyboard/controller simulated rep for testing */
    UFUNCTION(BlueprintCallable, Category = "Boulder")
    void FireSimulatedRep();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    float CountdownTimer    = 0.f;
    float TimeSinceLastRep  = 0.f;
    float TotalPowerSum     = 0.f;
    int32 SimRepCounter     = 0;

    // Input binding (keyboard sim)
    void SetupInputBindings();

    // ERG event handler
    UFUNCTION()
    void HandleNewRep(const FRepData& Rep);

    void ChangeState(EBoulderGameState NewState);
    void EndGame(bool bWon);
    FGameStats BuildStats(bool bWon) const;
};
