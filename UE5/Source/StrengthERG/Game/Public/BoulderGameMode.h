#pragma once

/**
 * BoulderGameMode.h
 *
 * UE5 equivalent of Unity's BoulderGameManager.
 *
 * Conversion notes:
 *   - AGameModeBase hosts the game-state machine (Idle/Countdown/Playing/Won/Lost).
 *   - QTE logic is inlined from QTEController — pure math, no actor needed.
 *   - ERG data arrives via delegate from UErgManagerComponent on the GameState actor.
 *   - UI is driven via a BoulderHUD widget class (see BoulderHUD.h).
 *   - PlayerPusherController is ported as APusherCharacter (separate file).
 *   - UnityEvents → DECLARE_DYNAMIC_MULTICAST_DELEGATE.
 */

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ErgTypes.h"
#include "BoulderGameMode.generated.h"

class ABoulderActor;
class UErgManagerComponent;
class ACameraActor;

// ── Delegates (replaces Unity's UnityEvent onGameWon / onGameLost) ────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameWon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameLost);

UCLASS()
class STRENGTHERG_API ABoulderGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ABoulderGameMode();

    // ── Scene References (set in editor or via BeginPlay find) ────────────────

    /** The boulder actor in the level. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
    ABoulderActor* Boulder = nullptr;

    /** Optional camera actor to activate on PIE start. Assign in editor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
    ACameraActor* GameCamera = nullptr;

    // ── Game Rules ────────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")
    float TimeLimitSeconds = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")
    float RollbackDelaySec = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")
    float AnimationWindowDuration = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")
    float CountdownDuration = 3.f;

    // ── Power Zones ───────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Zones")
    float ExplosiveThreshold = 70.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Zones")
    float PowerThreshold = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Power Zones")
    float ModerateThreshold = 25.f;

    // ── QTE Settings ──────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    float QTECycleDuration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    float QTEWindowDuration = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    float QTEWindowStartFraction = 0.10f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    float QTEPerfectMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    float QTEMissMultiplier = 0.55f;

    // ── Simulation ────────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
    bool bSimulateInput = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation",
              meta = (EditCondition = "bSimulateInput"))
    int32 SimPullDistance = 80;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation",
              meta = (EditCondition = "bSimulateInput"))
    float SimRepTimeSec = 0.45f;

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnGameWon OnGameWon;

    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnGameLost OnGameLost;

    // ── Public Read-Only State ────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Game")
    EBoulderGameState GetState() const { return State; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetTimeRemaining() const { return TimeRemaining; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    int32 GetTotalReps() const { return TotalReps; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    int32 GetCombo() const { return Combo; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    int32 GetPerfectCount() const { return PerfectCount; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetCurrentPower() const { return CurrentPower; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetPeakPower() const { return PeakPower; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    EErgPowerZone GetCurrentZone() const { return CurrentZone; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetAveragePower() const
    {
        return TotalReps > 0 ? TotalPowerAccum / (float)TotalReps : 0.f;
    }

    // QTE state for HUD
    UFUNCTION(BlueprintCallable, Category = "QTE")
    bool IsQTEWindowOpen() const { return bQTEWindowOpen; }

    UFUNCTION(BlueprintCallable, Category = "QTE")
    float GetQTECycleProgress() const { return QTECycleProgress; }

    // ── Public Controls ───────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Game")
    void StartGame();

    UFUNCTION(BlueprintCallable, Category = "Game")
    void RestartGame();

    /** Called by the pusher character animation at peak effort moment. */
    UFUNCTION(BlueprintCallable, Category = "Game")
    void OnAnimationPeak();

    // ── AGameModeBase ─────────────────────────────────────────────────────────

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // ── State Machine ─────────────────────────────────────────────────────────

    EBoulderGameState State = EBoulderGameState::Idle;
    float TimeRemaining   = 0.f;
    int32 TotalReps       = 0;
    int32 Combo           = 0;
    int32 PerfectCount    = 0;
    float CurrentPower    = 0.f;
    float PeakPower       = 0.f;
    float TotalPowerAccum = 0.f;
    EErgPowerZone CurrentZone = EErgPowerZone::Low;

    float TimeSinceLastRep = 0.f;
    float CountdownTimer   = 0.f;
    int32 SimRepNumber     = 0;

    void ChangeState(EBoulderGameState Next);
    void EndGame(bool bWon);

    // ── Rep Handler ───────────────────────────────────────────────────────────

    UFUNCTION()
    void HandleNewRep(int32 RepNumber, float RepTimeSec, int32 PullDistance);

    // ── QTE (inlined from QTEController) ─────────────────────────────────────

    bool  bQTEWindowOpen     = false;
    float QTECycleProgress   = 0.f;
    float ForcedWindowTimer  = 0.f;
    bool  bPrevWindowOpen    = false;

    void  UpdateQTE(float DeltaTime);
    TTuple<EQTERating, float> EvaluatePush(float RepTimeSec);

    // ── ERG component ref ─────────────────────────────────────────────────────

    UErgManagerComponent* ErgComp = nullptr;
};
