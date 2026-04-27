#include "RowingProgressActor.h"
#include "Math/UnrealMathUtility.h"

ARowingProgressActor::ARowingProgressActor()
{
    PrimaryActorTick.bCanEverTick = false;
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
