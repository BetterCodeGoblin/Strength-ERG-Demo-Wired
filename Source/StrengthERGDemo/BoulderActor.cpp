#include "BoulderActor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

ABoulderActor::ABoulderActor()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoulderMesh"));
    RootComponent = MeshComponent;

    // Load engine sphere mesh as default — replace in Blueprint with a boulder asset
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(SphereMesh.Object);
    }

    MeshComponent->SetSimulatePhysics(false);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
}

void ABoulderActor::BeginPlay()
{
    Super::BeginPlay();
    SnapVisualToLogical();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void ABoulderActor::Push(float RepPower, float CombinedMultiplier)
{
    float Gain = FMath::Clamp(RepPower * PowerScale * CombinedMultiplier, 0.f, MaxPushPerRep);
    Progress   = FMath::Clamp01(Progress + Gain);
    WobbleAngle = PushWobbleDegrees;
}

void ABoulderActor::ApplyRollback(float DeltaTime)
{
    Progress = FMath::Clamp01(Progress - RollbackSpeed * DeltaTime);
}

void ABoulderActor::ResetPosition()
{
    Progress      = 0.f;
    VisualProgress = 0.f;
    VisualVelocity = 0.f;
    RollQuat      = FQuat::Identity;
    WobbleAngle   = 0.f;
    SnapVisualToLogical();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tick — visual smoothing + rolling rotation
// ─────────────────────────────────────────────────────────────────────────────

void ABoulderActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const float PrevVisual = VisualProgress;

    // Smooth visual progress toward logical with a critically-damped spring
    // UE doesn't ship SmoothDamp but a simple exponential approach works:
    VisualVelocity  = FMath::Lerp(VisualVelocity, (Progress - VisualProgress) / FMath::Max(VisualSmoothTime, KINDA_SMALL_NUMBER), DeltaTime * 10.f);
    VisualProgress  = FMath::Clamp01(VisualProgress + VisualVelocity * DeltaTime);

    // Update world position
    if (PathStart && PathEnd)
    {
        SetActorLocation(ComputePathPosition(VisualProgress));
    }

    // Roll rotation proportional to visual delta
    const float Delta = VisualProgress - PrevVisual;
    if (PathStart && PathEnd && FMath::Abs(Delta) > SMALL_NUMBER)
    {
        const FVector HillFwd  = (PathEnd->GetActorLocation() - PathStart->GetActorLocation()).GetSafeNormal();
        const FVector RollAxis = FVector::CrossProduct(FVector::UpVector, HillFwd).GetSafeNormal();
        const float   Degrees  = Delta * RollDegreesPerProgress;
        RollQuat = FQuat(RollAxis, FMath::DegreesToRadians(Degrees)) * RollQuat;
        RollQuat.Normalize();
    }

    // Apply roll + wobble
    FQuat WobbleQ = FQuat::Identity;
    if (WobbleAngle > 0.05f)
    {
        const float WobbleRad = FMath::DegreesToRadians(WobbleAngle * FMath::Sin(GetWorld()->GetTimeSeconds() * 14.f));
        WobbleQ = FQuat(FVector::ForwardVector, WobbleRad);

        // Decay wobble
        WobbleAngle = FMath::Lerp(WobbleAngle, 0.f, DeltaTime * 8.f);
        if (WobbleAngle < 0.05f) WobbleAngle = 0.f;
    }

    SetActorRotation((RollQuat * WobbleQ).Rotator());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

FVector ABoulderActor::ComputePathPosition(float T) const
{
    if (!PathStart || !PathEnd)
        return GetActorLocation();

    const FVector Start   = PathStart->GetActorLocation();
    const FVector End     = PathEnd->GetActorLocation();
    const FVector BasePos = FMath::Lerp(Start, End, T);

    const float EffectiveOffset = bRampOffsetAlongPath ? HeightOffset * T : HeightOffset;
    if (FMath::IsNearlyZero(EffectiveOffset))
        return BasePos;

    const FVector HillFwd   = (End - Start).GetSafeNormal();
    const FVector HillRight = FVector::CrossProduct(HillFwd, FVector::UpVector).GetSafeNormal();
    const FVector SlopeUp   = FVector::CrossProduct(HillRight, HillFwd).GetSafeNormal();

    return BasePos + SlopeUp * EffectiveOffset;
}

void ABoulderActor::SnapVisualToLogical()
{
    VisualProgress = Progress;
    VisualVelocity = 0.f;
    if (PathStart && PathEnd)
        SetActorLocation(ComputePathPosition(VisualProgress));
}
