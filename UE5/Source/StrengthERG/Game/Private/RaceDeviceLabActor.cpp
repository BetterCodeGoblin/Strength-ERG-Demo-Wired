#include "RaceDeviceLabActor.h"

#include "RaceGameMode.h"
#include "RaceLaneActor.h"
#include "Kismet/GameplayStatics.h"

// ?? Idle deadzones ????????????????????????????????????????????????????????????
// Concept2 PM5 ergometers report low baseline noise on connected-but-idle
// devices: typically ~10-30 W of "power" with 0 stroke rate / cadence.
// These thresholds filter that noise before it drives lane movement or
// triggers race-ready detection.
//
//   MinActiveStrokeRate  – SPM (rowing) or RPM (cycling) below which the
//                          device is considered idle regardless of power.
//                          5.0 filters lingering 1 rpm / 1 spm tail-noise after real effort.
//
//   MinActivePowerWatts  – fallback threshold for power-only frames (rare).
//                          30 W is well above observed idle noise (~21 W)
//                          and below any meaningful effort.
//
// Both conditions are OR'd: either meaningful cadence OR meaningful power
// must be present for the channel to count as active.
static constexpr float MinActiveStrokeRate = 5.0f;   // spm / rpm
static constexpr float MinActivePowerWatts = 30.f;   // watts

// Returns true when telemetry represents real physical effort on a rowing
// or cycling machine — not idle baseline noise.
static bool IsActiveEffort(const FErgData& Data)
{
    return Data.StrokeRate >= MinActiveStrokeRate
        || Data.PowerWatts >= MinActivePowerWatts;
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
        if (!Data.bIsConnected)
        {
            if (TargetLane) TargetLane->SetNormalisedSpeed(0.f);
            break;
        }
        if (IsActiveEffort(Data))
        {
            NormalisedSpeed = FMath::Clamp(Data.PowerWatts / FMath::Max(1.f, CyclingReferencePower), 0.f, 1.f);
            Detail = FString::Printf(TEXT("%0.0fW %0.0frpm"), Data.PowerWatts, Data.StrokeRate);
        }
        else
        {
            NormalisedSpeed = 0.f;
            Detail = FString::Printf(TEXT("idle (%0.0fW %0.0frpm — below threshold)"), Data.PowerWatts, Data.StrokeRate);
        }
        if (TargetLane) TargetLane->SetNormalisedSpeed(NormalisedSpeed);
        break;

    case EDeviceChannel::Rowing:
        TargetLane = RowingLane;
        if (!Data.bIsConnected)
        {
            if (TargetLane) TargetLane->SetNormalisedSpeed(0.f);
            break;
        }
        if (IsActiveEffort(Data))
        {
            NormalisedSpeed = FMath::Clamp(Data.PowerWatts / FMath::Max(1.f, RowingReferencePower), 0.f, 1.f);
            Detail = FString::Printf(TEXT("%0.0fW %0.0fspm"), Data.PowerWatts, Data.StrokeRate);
        }
        else
        {
            NormalisedSpeed = 0.f;
            Detail = FString::Printf(TEXT("idle (%0.0fW %0.0fspm — below threshold)"), Data.PowerWatts, Data.StrokeRate);
        }
        if (TargetLane) TargetLane->SetNormalisedSpeed(NormalisedSpeed);
        break;

    case EDeviceChannel::Strength:
        TargetLane = StrengthLane;
        NormalisedSpeed = FMath::Clamp(Data.PullDistance * StrengthImpulseScale, 0.f, 1.f);
        Detail = FString::Printf(TEXT("rep %d dist %d"), Data.RepCount, Data.PullDistance);
        if (TargetLane)
        {
            TargetLane->SetNormalisedSpeed(0.f);
            if (Data.bIsConnected && Data.RepCount > 0 && Data.PullDistance > 0)
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
        // For cycling/rowing, "ready" requires real physical effort, not just a
        // connected-and-idle device reporting baseline noise.  Strength readiness
        // uses bIsConnected directly because it fires only on discrete rep events.
        bool bReadyForRace = Data.bIsConnected;
        if (Channel == EDeviceChannel::Cycling || Channel == EDeviceChannel::Rowing)
            bReadyForRace = Data.bIsConnected && IsActiveEffort(Data);

        RaceGameMode->NotifyDeviceConnected(Channel, bReadyForRace);
        RaceGameMode->UpdateLaneHud(Channel, Data.bIsConnected, NormalisedSpeed, Detail);
    }
}
