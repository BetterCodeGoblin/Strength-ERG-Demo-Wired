#include "RaceDeviceLabActor.h"

#include "RaceGameMode.h"
#include "RaceLaneActor.h"
#include "Kismet/GameplayStatics.h"

void ARaceDeviceLabActor::BeginPlay()
{
    Super::BeginPlay();

    RaceGameMode = Cast<ARaceGameMode>(UGameplayStatics::GetGameMode(this));
    OnAnyDeviceUpdated.AddDynamic(this, &ARaceDeviceLabActor::OnRaceDeviceUpdated);
}

void ARaceDeviceLabActor::OnRaceDeviceUpdated(EDeviceChannel Channel, FErgData Data)
{
    ARaceLaneActor* TargetLane = nullptr;
    float NormalisedSpeed = 0.f;
    FString Detail = TEXT("idle");

    switch (Channel)
    {
    case EDeviceChannel::Cycling:
        TargetLane = CyclingLane;
        NormalisedSpeed = FMath::Clamp(Data.PowerWatts / FMath::Max(1.f, CyclingReferencePower), 0.f, 1.f);
        Detail = FString::Printf(TEXT("%0.0fW %0.0frpm"), Data.PowerWatts, Data.StrokeRate);
        if (TargetLane) TargetLane->SetNormalisedSpeed(NormalisedSpeed);
        TargetLane = RowingLane;
        NormalisedSpeed = FMath::Clamp(Data.PowerWatts / FMath::Max(1.f, RowingReferencePower), 0.f, 1.f);
        Detail = FString::Printf(TEXT("%0.0fW %0.0fspm"), Data.PowerWatts, Data.StrokeRate);
        if (TargetLane) TargetLane->SetNormalisedSpeed(NormalisedSpeed);
        break;

    case EDeviceChannel::Strength:
        TargetLane = StrengthLane;
        NormalisedSpeed = FMath::Clamp(Data.PullDistance * StrengthImpulseScale, 0.f, 1.f);
        Detail = FString::Printf(TEXT("rep %d dist %d"), Data.RepCount, Data.PullDistance);
        if (TargetLane)
        {
            TargetLane->SetNormalisedSpeed(0.f);
            if (Data.RepCount > 0 && Data.PullDistance > 0)
            {
                TargetLane->ApplySpeedImpulse(NormalisedSpeed, StrengthImpulseDuration);
            }
        }
        break;

    default:
        break;
    }

    if (RaceGameMode)
    {
        RaceGameMode->NotifyDeviceConnected(Channel, Data.bIsConnected);
        RaceGameMode->UpdateLaneHud(Channel, Data.bIsConnected, NormalisedSpeed, Detail);
    }
}
