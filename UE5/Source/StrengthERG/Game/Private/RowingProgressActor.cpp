#include "RowingProgressActor.h"
#include "RowingProgressActor.h"
#include "Components/StaticMeshComponent.h"
#include "Math/UnrealMathUtility.h"

ARowingProgressActor::ARowingProgressActor()
{
    PrimaryActorTick.bCanEverTick = false;

    ProgressMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressMesh"));
    SetRootComponent(ProgressMesh);
    ProgressMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARowingProgressActor::BeginPlay()
{
    Super::BeginPlay();

    ProgressMesh->SetRelativeScale3D(FVector(MeshScale));

    if (bUpdateActorLocationFromProgress)
    {
        UpdateLocationFromProgress();
    }
}

void ARowingProgressActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    ProgressMesh->SetRelativeScale3D(FVector(MeshScale));
}

void ARowingProgressActor::AddProgress(float Amount)
{
    CurrentProgress += Amount;
    if (bClampAtTarget)
    {
        CurrentProgress = FMath::Clamp(CurrentProgress, 0.f, TargetProgress);
    }
    else
    {
        CurrentProgress = FMath::Max(CurrentProgress, 0.f);
    }
}

void ARowingProgressActor::ApplyDrift(float DeltaSeconds, float DriftRate)
{
    CurrentProgress = FMath::Max(0.f, CurrentProgress - (DriftRate * DeltaSeconds));
}

void ARowingProgressActor::ResetProgress()
{
    CurrentProgress = 0.f;
}

float ARowingProgressActor::GetProgressNormalized() const
{
    return TargetProgress > 0.f ? FMath::Clamp(CurrentProgress / TargetProgress, 0.f, 1.f) : 0.f;
}

bool ARowingProgressActor::IsComplete() const
{
    return TargetProgress > 0.f && CurrentProgress >= TargetProgress;
}

void ARowingProgressActor::UpdateLocationFromProgress()
{
    const float Alpha = GetProgressNormalized();
    const FVector NewLocation = FMath::Lerp(StartLocation, EndLocation, Alpha);
    SetActorLocation(NewLocation);
}
