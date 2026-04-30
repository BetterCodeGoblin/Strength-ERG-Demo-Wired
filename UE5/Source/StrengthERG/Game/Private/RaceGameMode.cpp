// RaceGameMode.cpp - Race flow: waiting -> countdown -> racing -> finished

#include "RaceGameMode.h"

#include "Engine/Engine.h"
#include "RaceHUD.h"
#include "RaceLaneActor.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

ARaceGameMode::ARaceGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = nullptr;
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

    // Start in waiting state - freeze all lanes
    RaceState = ERaceState::Waiting;
    TArray<AActor*> Lanes;
    UGameplayStatics::GetAllActorsOfClass(this, ARaceLaneActor::StaticClass(), Lanes);
    for (AActor* A : Lanes)
        if (ARaceLaneActor* L = Cast<ARaceLaneActor>(A))
            L->bRaceStarted = false;

    PushReadyStateToHUD();
}

void ARaceGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (RaceState != ERaceState::Countdown) return;

    CountdownRemaining -= DeltaSeconds;

    if (RaceHUD)
    {
        const int32 DisplayCount = FMath::CeilToInt(CountdownRemaining);
        RaceHUD->ShowCountdown(FMath::Max(DisplayCount, 0));
    }

    if (CountdownRemaining <= 0.f)
    {
        BeginRace();
    }
}

void ARaceGameMode::NotifyDeviceConnected(EDeviceChannel Channel, bool bConnected)
{
    switch (Channel)
    {
    case EDeviceChannel::Cycling:  bCyclingReady  = bConnected; break;
    case EDeviceChannel::Rowing:   bRowingReady   = bConnected; break;
    case EDeviceChannel::Strength: bStrengthReady = bConnected; break;
    default: break;
    }

    PushReadyStateToHUD();

    if (RaceState == ERaceState::Waiting && AllDevicesReady())
    {
        StartCountdown();
    }
    else if (RaceState == ERaceState::Countdown && !AllDevicesReady())
    {
        // Device dropped during countdown - cancel and return to waiting
        RaceState = ERaceState::Waiting;
        CountdownRemaining = 0.f;
        UE_LOG(LogTemp, Warning, TEXT("[Race] Device disconnected during countdown - returning to waiting."));
        PushReadyStateToHUD();
    }
}

void ARaceGameMode::NotifyLaneFinished(EDeviceChannel Channel)
{
    if (RaceState != ERaceState::Racing) return;
    if (bRaceFinished) return;

    bRaceFinished  = true;
    RaceState      = ERaceState::Finished;
    WinnerChannel  = Channel;

    const FString WinnerName = StaticEnum<EDeviceChannel>()
        ->GetDisplayNameTextByValue((int64)Channel).ToString();

    UE_LOG(LogTemp, Warning, TEXT("[Race] Winner: %s"), *WinnerName);

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(99, 10.f, FColor::Yellow,
            FString::Printf(TEXT("WINNER: %s!"), *WinnerName));

    if (RaceHUD)
        RaceHUD->ShowWinner(Channel);

    OnRaceWon(Channel);
}

void ARaceGameMode::UpdateLaneHud(EDeviceChannel Channel, bool bConnected, float NormalisedSpeed, const FString& Detail)
{
    if (RaceHUD && RaceState == ERaceState::Racing)
        RaceHUD->SetLaneStatus(Channel, bConnected, NormalisedSpeed, Detail);
}

// -- Private helpers -------------------------------------------------------

void ARaceGameMode::StartCountdown()
{
    RaceState          = ERaceState::Countdown;
    CountdownRemaining = CountdownDuration;
    UE_LOG(LogTemp, Warning, TEXT("[Race] All devices ready - starting %g-second countdown."), CountdownDuration);

    if (RaceHUD)
        RaceHUD->ShowCountdown(FMath::CeilToInt(CountdownDuration));
}

void ARaceGameMode::BeginRace()
{
    RaceState    = ERaceState::Racing;
    bRaceStarted = true;

    UE_LOG(LogTemp, Warning, TEXT("[Race] Race started!"));

    TArray<AActor*> Lanes;
    UGameplayStatics::GetAllActorsOfClass(this, ARaceLaneActor::StaticClass(), Lanes);
    for (AActor* A : Lanes)
        if (ARaceLaneActor* L = Cast<ARaceLaneActor>(A))
            L->bRaceStarted = true;

    if (RaceHUD)
        RaceHUD->ShowRaceLive();
}

void ARaceGameMode::PushReadyStateToHUD()
{
    if (RaceHUD && RaceState == ERaceState::Waiting)
        RaceHUD->ShowWaiting(bCyclingReady, bRowingReady, bStrengthReady);
}
