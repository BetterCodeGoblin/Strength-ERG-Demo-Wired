// RaceGameMode.cpp - Slice 1/2 race map foundation + small HUD support

#include "RaceGameMode.h"

#include "Engine/Engine.h"
#include "RaceHUD.h"
#include "Blueprint/UserWidget.h"

ARaceGameMode::ARaceGameMode()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ARaceGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (RaceHUDClass)
    {
        if (UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), RaceHUDClass))
        {
            Widget->AddToViewport();
            RaceHUD = Cast<URaceHUD>(Widget);
        }
    }
}

void ARaceGameMode::NotifyLaneFinished(EDeviceChannel Channel)
{
    if (bRaceFinished) return;

    bRaceFinished  = true;
    WinnerChannel  = Channel;

    const FString WinnerName = StaticEnum<EDeviceChannel>()
        ->GetDisplayNameTextByValue((int64)Channel).ToString();

    UE_LOG(LogTemp, Warning, TEXT("[Race] Winner: %s"), *WinnerName);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            99, 10.f, FColor::Yellow,
            FString::Printf(TEXT("WINNER: %s!"), *WinnerName));
    }

    if (RaceHUD)
    {
        RaceHUD->ShowWinner(Channel);
    }

    OnRaceWon(Channel);
}

void ARaceGameMode::UpdateLaneHud(EDeviceChannel Channel, bool bConnected, float NormalisedSpeed, const FString& Detail)
{
    if (RaceHUD)
    {
        RaceHUD->SetLaneStatus(Channel, bConnected, NormalisedSpeed, Detail);
    }
}
