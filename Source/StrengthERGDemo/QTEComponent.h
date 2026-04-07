#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ErgGameTypes.h"
#include "QTEComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWindowOpened);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnQTEWindowClosed);

// ─────────────────────────────────────────────────────────────────────────────
//  UQTEComponent
//
//  Manages the "PUSH NOW" timing window rhythm.
//  Two modes:
//    - Cycle-based: pulses on a fixed timer
//    - Animation-driven: ForceOpenWindow() opens it on demand
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class STRENGTHERGDEMO_API UQTEComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UQTEComponent();

    // ── Cycle Timing ──────────────────────────────────────────────────────────

    /** Total length of one QTE cycle (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    float CycleDuration = 3.0f;

    /** How long the green window stays open per cycle (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    float WindowDuration = 1.2f;

    /** Fraction into cycle at which window opens (0–1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE", meta=(ClampMin="0", ClampMax="0.9"))
    float WindowStartFraction = 0.10f;

    // ── Multipliers ───────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    float PerfectMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    float MissMultiplier = 0.55f;

    // ── Read-Only State ───────────────────────────────────────────────────────

    UPROPERTY(BlueprintReadOnly, Category = "QTE")
    bool bIsWindowOpen = false;

    /** 0–1 through the current cycle */
    UPROPERTY(BlueprintReadOnly, Category = "QTE")
    float CycleProgress = 0.f;

    /** 0–1 through the open window; 0 when closed */
    UPROPERTY(BlueprintReadOnly, Category = "QTE")
    float WindowProgress = 0.f;

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "QTE|Events")
    FOnWindowOpened OnWindowOpened;

    UPROPERTY(BlueprintAssignable, Category = "QTE|Events")
    FOnQTEWindowClosed OnWindowClosed;

    // ── Public API ────────────────────────────────────────────────────────────

    /**
     * Evaluate a push. Returns rating + timing multiplier.
     * Call when a rep is received during gameplay.
     */
    UFUNCTION(BlueprintCallable, Category = "QTE")
    void EvaluatePush(EPushRating& OutRating, float& OutMultiplier) const;

    /**
     * Open the QTE window for a fixed duration (animation-driven path).
     * Used by the game mode when the character's push animation peaks.
     */
    UFUNCTION(BlueprintCallable, Category = "QTE")
    void ForceOpenWindow(float Duration);

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    bool  bWasPreviouslyOpen = false;
    float ForcedWindowTimer  = 0.f;

    void UpdateWindowState(bool bOpen);
};
