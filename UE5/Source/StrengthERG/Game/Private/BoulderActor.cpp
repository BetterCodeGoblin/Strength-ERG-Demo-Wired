// BoulderActor.cpp
// UE5 port of Unity's BoulderController.cs
//
// Key differences:
//   1. Movement is applied to the Actor root, not a Transform.
//   2. Rotation is accumulated as an FQuat, same as Unity.
//   3. SmoothDamp is implemented manually (UE lacks a built-in equivalent).
//   4. DrawDebugLine replaces OnDrawGizmosSelected.
//   5. Rolling axis: Cross(FVector::UpVector, HillForward) — same math.

#include "BoulderActor.h"
#include "BoulderPathMarker.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Math/UnrealMathUtility.h"

ABoulderActor::ABoulderActor()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoulderMesh"));
    RootComponent = MeshComp;
}

void ABoulderActor::BeginPlay()
{
    Super::BeginPlay();

    // Resolve path from placed markers, falling back to raw FVectors if none assigned.
    if (PathStartMarker)
    {
        PathStart = PathStartMarker->GetActorLocation();
        UE_LOG(LogTemp, Log, TEXT("[Boulder] PathStart resolved from marker '%s': %s"),
               *PathStartMarker->GetName(), *PathStart.ToString());
    }
    else
    {
        PathStart = FallbackPathStart;
        UE_LOG(LogTemp, Warning, TEXT("[Boulder] No PathStartMarker assigned — using FallbackPathStart."));
    }

    if (PathEndMarker)
    {
        PathEnd = PathEndMarker->GetActorLocation();
        UE_LOG(LogTemp, Log, TEXT("[Boulder] PathEnd resolved from marker '%s': %s"),
               *PathEndMarker->GetName(), *PathEnd.ToString());
    }
    else
    {
        PathEnd = FallbackPathEnd;
        UE_LOG(LogTemp, Warning, TEXT("[Boulder] No PathEndMarker assigned — using FallbackPathEnd."));
    }

    ResetPosition();
}

void ABoulderActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ── Smooth position (replicates Unity SmoothDamp) ─────────────────────────
    float PrevVisual = VisualProgress;
    VisualProgress = SmoothDamp(VisualProgress, Progress, VelocityRef, VisualSmoothTime, DeltaTime);

    // ── World position along path + height offset ─────────────────────────────
    FVector NewPos = ComputePathPosition(VisualProgress);
    SetActorLocation(NewPos, false, nullptr, ETeleportType::TeleportPhysics);

    // ── Rolling rotation proportional to visual delta ─────────────────────────
    float Delta = VisualProgress - PrevVisual;
    if (FMath::Abs(Delta) > 1e-5f)
    {
        FVector HillFwd  = (PathEnd - PathStart).GetSafeNormal();
        FVector RollAxis = FVector::CrossProduct(FVector::UpVector, HillFwd).GetSafeNormal();
        float   Degrees  = Delta * RollDegreesPerProgress;

        FQuat DeltaRot = FQuat(RollAxis, FMath::DegreesToRadians(Degrees));
        RollRotation   = DeltaRot * RollRotation;
    }

    // ── Wobble ────────────────────────────────────────────────────────────────
    FQuat WobbleQ = FQuat::Identity;
    if (Wobble > 0.05f)
    {
        float WobbleX = Wobble * FMath::Sin(GetWorld()->GetTimeSeconds() * 14.f);
        float WobbleZ = Wobble * 0.4f;
        WobbleQ = FQuat(FRotator(WobbleX, 0.f, WobbleZ));
    }

    SetActorRotation(RollRotation * WobbleQ);

    // Decay wobble
    if (Wobble > 0.05f)
        Wobble = FMath::Lerp(Wobble, 0.f, DeltaTime * 8.f);
    else
        Wobble = 0.f;

    // ── Debug path visualization ──────────────────────────────────────────────
#if ENABLE_DRAW_DEBUG
    DrawDebugLine(GetWorld(), PathStart, PathEnd, FColor::Yellow, false, -1.f, 0, 5.f);
    DrawDebugSphere(GetWorld(), PathStart, 15.f, 8, FColor::Yellow, false, -1.f);
    DrawDebugSphere(GetWorld(), PathEnd,   15.f, 8, FColor::Yellow, false, -1.f);
#endif
}

// ── Public API ────────────────────────────────────────────────────────────────

void ABoulderActor::Push(float RepPower, float CombinedMultiplier)
{
    float Gain  = FMath::Clamp(RepPower * PowerScale * CombinedMultiplier, 0.f, MaxPushPerRep);
    Progress    = FMath::Clamp(Progress + Gain, 0.f, 1.f);
    Wobble      = PushWobbleDegrees;

    UE_LOG(LogTemp, Log, TEXT("[Boulder] Push | power=%.1f mult=%.2f gain=%.4f | Progress=%.4f VisualProgress=%.4f"),
           RepPower, CombinedMultiplier, Gain, Progress, VisualProgress);
}

void ABoulderActor::ApplyRollback(float DeltaTime)
{
    Progress = FMath::Clamp(Progress - RollbackSpeed * DeltaTime, 0.f, 1.f);
}

void ABoulderActor::ResetPosition()
{
    Progress       = 0.f;
    VisualProgress = 0.f;
    VelocityRef    = 0.f;
    RollRotation   = FQuat::Identity;
    Wobble         = 0.f;
    SetActorLocation(ComputePathPosition(0.f), false, nullptr, ETeleportType::TeleportPhysics);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

FVector ABoulderActor::ComputePathPosition(float T) const
{
    FVector BasePos = FMath::Lerp(PathStart, PathEnd, T);
    float EffectiveOffset = bRampOffsetAlongPath ? HeightOffset * T : HeightOffset;

    if (FMath::IsNearlyZero(EffectiveOffset))
        return BasePos;

    FVector HillFwd   = (PathEnd - PathStart).GetSafeNormal();
    FVector HillRight = FVector::CrossProduct(HillFwd, FVector::UpVector).GetSafeNormal();
    FVector SlopeUp   = FVector::CrossProduct(HillRight, HillFwd).GetSafeNormal();

    return BasePos + SlopeUp * EffectiveOffset;
}

float ABoulderActor::SmoothDamp(float Current, float Target, float& Velocity,
                                float SmoothTime, float DeltaTime)
{
    // Spring-damper approximation (matches Unity SmoothDamp exactly)
    SmoothTime = FMath::Max(0.0001f, SmoothTime);
    float Omega   = 2.f / SmoothTime;
    float X       = Omega * DeltaTime;
    float Exp     = 1.f / (1.f + X + 0.48f * X * X + 0.235f * X * X * X);
    float Change  = Current - Target;
    float OrigTarget = Target;

    float MaxChange = 1e8f;  // unbounded by default
    Change = FMath::Clamp(Change, -MaxChange, MaxChange);
    Target = Current - Change;

    float Temp   = (Velocity + Omega * Change) * DeltaTime;
    Velocity     = (Velocity - Omega * Temp) * Exp;
    float Output = Target + (Change + Temp) * Exp;

    // Prevent overshoot
    if ((OrigTarget - Current > 0.f) == (Output > OrigTarget))
    {
        Output   = OrigTarget;
        Velocity = (Output - OrigTarget) / DeltaTime;
    }

    return Output;
}

#if WITH_EDITOR
void ABoulderActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    // Snap preview position when path is edited in editor
    SetActorLocation(ComputePathPosition(Progress), false, nullptr, ETeleportType::TeleportPhysics);
}
#endif
