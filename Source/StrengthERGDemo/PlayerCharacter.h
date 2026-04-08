#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ErgGameTypes.h"
#include "PlayerCharacter.generated.h"

class ABoulderGameMode;
class ABoulderActor;

/// <summary>
/// Player character that syncs animations to ERG rep timing.
///
/// Listens to BoulderGameMode.OnRepProcessed events and:
/// - Calculates animation playback speed from rep time
/// - Triggers push animation montage at correct speed
/// - Positions character relative to boulder
/// - Rotates character to face up the slope
/// </summary>
UCLASS(BlueprintType, Blueprintable)
class STRENGTHERGDEMO_API APlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    APlayerCharacter();

    // ── Scene References ─────────────────────────────────────────────────────

    /** Reference to the boulder actor for positioning relative to it */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "References")
    TObjectPtr<ABoulderActor> BoulderRef;

    /** Reference to the game mode for event subscription */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "References")
    TObjectPtr<ABoulderGameMode> GameModeRef;

    // ── Animation Settings ────────────────────────────────────────────────────

    /** Push animation montage to play on each rep */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    TObjectPtr<class UAnimMontage> PushMontage;

    /** Fraction through the montage where peak occurs (0-1), for QTE window timing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0", ClampMax = "1"))
    float AnimationPeakFraction = 0.40f;

    // ── Positioning Settings ──────────────────────────────────────────────────

    /** Distance behind boulder (negative Z from boulder) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning")
    float DistanceBehindBoulder = 200.f;

    /** Height offset above ground */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning")
    float HeightAboveGround = 0.f;

    /** Smooth damping factor for following boulder (0-1, lower = smoother) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning", meta = (ClampMin = "0", ClampMax = "1"))
    float FollowSmoothness = 0.1f;

    // ── Setup & Input ─────────────────────────────────────────────────────────

    /** Call this in Blueprint BeginPlay after assigning BoulderRef and GameModeRef */
    UFUNCTION(BlueprintCallable, Category = "Character")
    void InitializeCharacter();

    /**
     * Called by animation montage notify when push animation peaks.
     * This should trigger the QTE window opening.
     * Wire from Montage Notify "AnimationPeak" to this function.
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void OnAnimationPeakNotify();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // Event handler for new reps
    UFUNCTION()
    void HandleNewRepProcessed(const FRepData& RepData);

    // Smooth position tracking
    FVector TargetPosition = FVector::ZeroVector;
    FVector CurrentVelocity = FVector::ZeroVector;

    // Animation montage playback tracking
    float LastAnimationPlayRate = 1.0f;

    // Position calculation helper
    FVector ComputeTargetPosition() const;
};
