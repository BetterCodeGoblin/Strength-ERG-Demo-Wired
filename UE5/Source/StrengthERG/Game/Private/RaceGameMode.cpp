// RaceGameMode.cpp – Slice 1 Race Map Foundation

#include "RaceGameMode.h"
#include "Engine/Engine.h"

ARaceGameMode::ARaceGameMode()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ARaceGameMode::NotifyLaneFinished(EDeviceChannel Channel)
{
    // Only the first caller wins.
    if (bRaceFinished) return;

    bRaceFinished  = true;
    WinnerChannel  = Channel;

    const FString WinnerName = StaticEnum<EDeviceChannel>()
        ->GetDisplayNameTextByValue((int64)Channel).ToString();

    UE_LOG(LogTemp, Warning, TEXT("[Race] Winner: %s"), *WinnerName);

    // On-screen message visible even without a HUD widget.
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            99, 10.f, FColor::Yellow,
            FString::Printf(TEXT("?? WINNER: %s!"), *WinnerName));
    }

    // Fire Blueprint event so BP_RaceGameMode can show a proper banner.
    OnRaceWon(Channel);
}
