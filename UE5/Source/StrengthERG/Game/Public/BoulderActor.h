#pragma once

/**
 * BoulderActor.h
 *
 * UE5 equivalent of Unity's BoulderController.
 *
 * Conversion notes:
 *   - Unity's Transform.position is now the Actor's RootComponent world location.
 *   - Progress/rollback logic is unchanged; same float math.
 *   - SmoothDamp is replicated with UKismetMathLibrary::VSmoothDamp (or manual impl).
 *   - Rolling rotation accumulates the same way; UE4 quaternion API maps 1:1.
 *   - pathStart/pathEnd become FVector references (or child SceneComponents).
 *   - Wobble is kept as a procedural rotation on the mesh component, not the root.
 *   - OnDrawGizmosSelected → DrawDebugLine in the UE editor.
 */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoulderActor.generated.h"

class ABoulderPathMarker;

UCLASS()
class STRENGTHERG_API ABoulderActor : public AActor
{
    GENERATED_BODY()

public:
    ABoulderActor();

    // ── Path ──────────────────────────────────────────────────────────────────

    /** Actor placed in the level at the foot of the hill (progress = 0).
     *  Equivalent to Unity's pathStart GameObject. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Path")
    ABoulderPathMarker* PathStartMarker = nullptr;

    /** Actor placed in the level at the top of the hill (progress = 1).
     *  Equivalent to Unity's pathEnd GameObject. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Path")
    ABoulderPathMarker* PathEndMarker = nullptr;

    /** Fallback used only when no markers are placed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Path",
              meta = (EditCondition = "PathStartMarker == nullptr"))
    FVector FallbackPathStart = FVector::ZeroVector;

    /** Fallback used only when no markers are placed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Path",
              meta = (EditCondition = "PathEndMarker == nullptr"))
    FVector FallbackPathEnd = FVector(0.f, 0.f, 1000.f);

    // ── Push Tuning ───────────────────────────────────────────────────────────

    /** Converts raw rep power into progress gain. Same default as Unity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Tuning")
    float PowerScale = 0.00028f;

    /** Maximum progress a single rep can contribute. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Push Tuning")
    float MaxPushPerRep = 0.10f;

    // ── Rollback ──────────────────────────────────────────────────────────────

    /** Progress lost per second when idle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback")
    float RollbackSpeed = 0.022f;

    // ── Visuals ───────────────────────────────────────────────────────────────

    /** Height offset perpendicular to slope (prevents mesh clipping). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    float HeightOffset = 0.f;

    /** Scale height offset linearly from 0 at bottom to full at top. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    bool bRampOffsetAlongPath = false;

    /** Degrees of rotation per unit of progress (rolling appearance). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    float RollDegreesPerProgress = 720.f;

    /** Wobble impulse magnitude on each push. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    float PushWobbleDegrees = 6.f;

    /** How quickly VisualProgress catches up to logical Progress (seconds).
     *  Lower = snappier. 0.25 was the original hardcoded value. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float VisualSmoothTime = 0.08f;

    // ── State ─────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Boulder")
    float GetProgress() const { return Progress; }

    UFUNCTION(BlueprintCallable, Category = "Boulder")
    bool IsAtTop() const { return Progress >= 0.999f; }

    UFUNCTION(BlueprintCallable, Category = "Boulder")
    bool IsAtBottom() const { return Progress <= 0.001f; }

    // ── API ───────────────────────────────────────────────────────────────────

    /** Called by the game mode for every valid rep. */
    UFUNCTION(BlueprintCallable, Category = "Boulder")
    void Push(float RepPower, float CombinedMultiplier);

    /** Called every frame during idle rollback. */
    UFUNCTION(BlueprintCallable, Category = "Boulder")
    void ApplyRollback(float DeltaTime);

    /** Snap progress back to 0. */
    UFUNCTION(BlueprintCallable, Category = "Boulder")
    void ResetPosition();

    // ── AActor ────────────────────────────────────────────────────────────────

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    // Resolved world positions — set from markers in BeginPlay, or from fallback vectors.
    FVector PathStart = FVector::ZeroVector;
    FVector PathEnd   = FVector(0.f, 0.f, 1000.f);

    float Progress      = 0.f;
    float VisualProgress = 0.f;
    float VelocityRef   = 0.f;  // for SmoothDamp
    float Wobble        = 0.f;

    FQuat RollRotation = FQuat::Identity;

    FVector ComputePathPosition(float T) const;

    /** Smooth damp: returns value moved toward Target at SmoothTime speed. */
    static float SmoothDamp(float Current, float Target, float& Velocity,
                             float SmoothTime, float DeltaTime);

    /** Static mesh component for the boulder visual. */
    UPROPERTY(VisibleAnywhere, Category = "Components")
    class UStaticMeshComponent* MeshComp = nullptr;
};
