#include "RaceDeviceLabActor.h"

#include "RaceGameMode.h"
#include "RaceLaneActor.h"
#include "Kismet/GameplayStatics.h"

// bIsConnected goes true the moment the bridge TCP socket connects, before any
// live telemetry has arrived. Require at least one non-zero telemetry value so
// we know the PM5 is actively streaming, not just socket-handshaked.
static bool IsDeviceTrulyReady(EDeviceChannel Channel, const FErgData& Data)
{
    if (!Data.bIsConnected) return false;
    switch (Channel)
    {
    case EDeviceChannel::Cycling:
    case EDeviceChannel::Rowing:
        return Data.PowerWatts > 0.f || Data.StrokeRate > 0.f;
    case EDeviceChannel::Strength:
        return Data.PullDistance > 0 || Data.RepCount > 0;
    default:
        return false;
    }
}

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
        break;

    case EDeviceChannel::Rowing:
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
        const bool bTrulyReady = IsDeviceTrulyReady(Channel, Data);
        UE_LOG(LogTemp, Log, TEXT("[RaceDevice] Channel=%d bIsConnected=%d bTrulyReady=%d"),
            (int32)Channel, (int32)Data.bIsConnected, (int32)bTrulyReady);
        RaceGameMode->NotifyDeviceConnected(Channel, bTrulyReady);
        RaceGameMode->UpdateLaneHud(Channel, bTrulyReady, NormalisedSpeed, Detail);
    }
}
