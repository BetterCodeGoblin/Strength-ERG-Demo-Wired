#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ErgGameTypes.h"
#include "BoulderActor.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ABoulderActor
//
//  Moves along a linear path between two target points (PathStart / PathEnd).
//  Progress 0 = foot of hill, 1 = summit.
//
//  Called by ABoulderGameMode each rep:
//    Push(RepPower, CombinedMultiplier)   — advances progress
//    ApplyRollback(DeltaTime)             — regresses progress when idle
//    ResetPosition()                      — snap back to start
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(BlueprintType, Blueprintable)
class STRENGTHERGDEMO_API ABoulderActor : public AActor
{
    GENERATED_BODY()

public:
    ABoulderActor();

    // ── Path ─────────────────────────────────────────────────────────────────

    /** Empty actor at the foot of the hill */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Path")
    TObjectPtr<AActor> PathStart;

    /** Empty actor at the summit */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Path")
    TObjectPtr<AActor> PathEnd;

    // ── Push Tuning ───────────────────────────────────────────────────────────

    /**
     * Converts raw rep power (PullDistance / RepTimeSec) to progress gain.
     * Default: 0.00028 → an average push (80 / 0.45s ≈ 178) → ~5% progress → ~20 reps to win.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Push", meta=(ClampMin="0.0001", ClampMax="0.01"))
    float PowerScale = 0.00028f;

    /** Maximum progress gain clamped from a single rep (0–1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Push", meta=(ClampMin="0.01", ClampMax="0.5"))
    float MaxPushPerRep = 0.10f;

    // ── Rollback ─────────────────────────────────────────────────────────────

    /** Progress units per second lost while idle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Rollback", meta=(ClampMin="0"))
    float RollbackSpeed = 0.022f;

    // ── Visuals ───────────────────────────────────────────────────────────────

    /** Position lerp smoothing time (seconds, SmoothDamp) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Visuals")
    float VisualSmoothTime = 0.25f;

    /** Perpendicular height offset to avoid clipping the mesh into the slope */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Visuals")
    float HeightOffset = 0.f;

    /** If true, height offset ramps from 0 at the bottom to full at the top */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Visuals")
    bool bRampOffsetAlongPath = true;

    /** Degrees of rotation added per unit of progress (visual rolling) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Visuals")
    float RollDegreesPerProgress = 720.f;

    /** Wobble angle injected on each push (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder|Visuals")
    float PushWobbleDegrees = 6.f;

    // ── Public State ──────────────────────────────────────────────────────────

    /** Logical progress 0–1 (0 = bottom, 1 = top) */
    UPROPERTY(BlueprintReadOnly, Category = "Boulder|State")
    float Progress = 0.f;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Boulder")
    bool IsAtTop() const { return Progress >= 0.999f; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Boulder")
    bool IsAtBottom() const { return Progress <= 0.001f; }

    // ── Public API ────────────────────────────────────────────────────────────

    /** Apply an ERG rep push: advances logical progress. */
    UFUNCTION(BlueprintCallable, Category = "Boulder")
    void Push(float RepPower, float CombinedMultiplier);

    /** Roll backward. Call every frame during idle period. */
    UFUNCTION(BlueprintCallable, Category = "Boulder")
    void ApplyRollback(float DeltaTime);

    /** Snap everything back to the bottom. */
    UFUNCTION(BlueprintCallable, Category = "Boulder")
    void ResetPosition();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // Visual smoothing state
    float VisualProgress = 0.f;
    float VisualVelocity = 0.f;     // SmoothDamp velocity

    // Rotation state
    FQuat RollQuat = FQuat::Identity;
    float WobbleAngle = 0.f;

    // Mesh
    UPROPERTY(VisibleAnywhere, Category = "Boulder")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    FVector ComputePathPosition(float T) const;
    void SnapVisualToLogical();
};
