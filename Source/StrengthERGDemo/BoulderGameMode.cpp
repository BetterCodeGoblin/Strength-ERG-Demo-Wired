#include "BoulderGameMode.h"
#include "BoulderActor.h"
#include "ErgManagerComponent.h"
#include "QTEComponent.h"
#include "PlayerCharacter.h"
#include "HudManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ABoulderGameMode::ABoulderGameMode()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create sub-components owned by the GameMode actor
    ErgManager = CreateDefaultSubobject<UErgManagerComponent>(TEXT("ErgManager"));
    QTE        = CreateDefaultSubobject<UQTEComponent>(TEXT("QTE"));

    // Let the engine spawn the HUD before BeginPlay; avoids UObject hash mutation at runtime.
    HUDClass = AHudManager::StaticClass();
}

void ABoulderGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Wire ERG events
    if (ErgManager)
    {
        ErgManager->bSimulateInput = bSimulateInput;
        ErgManager->OnNewRep.AddDynamic(this, &ABoulderGameMode::HandleNewRep);
        ErgManager->OnErgConnectionChanged.AddDynamic(this, &ABoulderGameMode::OnErgConnectionChanged);
    }

    // Spawn player character if not already in world
    APlayerCharacter* PlayerChar = GetWorld()->SpawnActor<APlayerCharacter>();
    if (PlayerChar)
    {
        PlayerChar->BoulderRef = BoulderActorRef;
        PlayerChar->GameModeRef = this;
        PlayerChar->PushMontage = PushAnimationMontage;
        PlayerChar->InitializeCharacter();
        UE_LOG(LogTemp, Log, TEXT("[GameMode] Spawned PlayerCharacter"));
    }

    ChangeState(EBoulderGameState::Idle);
}

void ABoulderGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    switch (CurrentBoulderGameState)
    {
        case EBoulderGameState::Countdown:
        {
            CountdownTimer -= DeltaTime;
            OnCountdownTick.Broadcast(FMath::Max(CountdownTimer, 0.f));

            if (CountdownTimer <= 0.f)
            {
                TimeRemaining = TimeLimitSeconds;
                if (QTE) QTE->SetComponentTickEnabled(true);
                ChangeState(EBoulderGameState::Playing);
            }
            break;
        }

        case EBoulderGameState::Playing:
        {
            TimeRemaining     -= DeltaTime;
            TimeSinceLastRep  += DeltaTime;

            // Rollback when idle too long
            if (TimeSinceLastRep > RollbackDelaySec && BoulderActorRef)
                BoulderActorRef->ApplyRollback(DeltaTime);

            // Win/Lose checks
            if (BoulderActorRef && BoulderActorRef->IsAtTop())
                EndGame(true);
            else if (TimeRemaining <= 0.f)
                EndGame(false);

            break;
        }

        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void ABoulderGameMode::StartGame()
{
    if (CurrentBoulderGameState == EBoulderGameState::Playing || CurrentBoulderGameState == EBoulderGameState::Countdown)
        return;

    if (BoulderActorRef)
        BoulderActorRef->ResetPosition();

    TotalReps       = 0;
    Combo           = 0;
    PerfectCount    = 0;
    CurrentPower    = 0.f;
    PeakPower       = 0.f;
    AveragePower    = 0.f;
    TotalPowerSum   = 0.f;
    TimeSinceLastRep = 0.f;
    SimRepCounter   = 0;

    CountdownTimer = CountdownDuration;
    ChangeState(EBoulderGameState::Countdown);
}

void ABoulderGameMode::NotifyAnimationPeak()
{
    if (CurrentBoulderGameState != EBoulderGameState::Playing) return;
    if (QTE) QTE->ForceOpenWindow(AnimationWindowDuration);
}

void ABoulderGameMode::FireSimulatedRep()
{
    if (ErgManager)
        ErgManager->FireSimulatedRep(SimRepTimeSec, SimPullDistance);
}

// ─────────────────────────────────────────────────────────────────────────────
//  ERG Connection Handler
// ─────────────────────────────────────────────────────────────────────────────

void ABoulderGameMode::OnErgConnectionChanged(bool bConnected)
{
    // Use LiveFrame.bConnected as the authoritative PM5 device signal.
    // The TCP socket connecting to the bridge does NOT mean the PM5 is ready.
    const bool bPM5Ready = ErgManager ? ErgManager->LiveFrame.bConnected : false;
    bErgDeviceReady = bPM5Ready;

    UE_LOG(LogTemp, Log, TEXT("[BoulderGame] ERG connection changed: bridge=%s, PM5=%s"),
        bConnected ? TEXT("connected") : TEXT("disconnected"),
        bPM5Ready  ? TEXT("ready")     : TEXT("not ready"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Rep Handler
// ─────────────────────────────────────────────────────────────────────────────

void ABoulderGameMode::HandleNewRep(const FRepData& Rep)
{
    UE_LOG(LogTemp, Log, TEXT("[BoulderGame] Rep #%d received — state=%d, pull=%d, power=%.1f"),
        Rep.RepNumber, (int32)CurrentBoulderGameState, Rep.PullDistance, Rep.RepPower);

    // Auto-start: first row on Idle screen kicks off the countdown,
    // but only when the PM5 device is confirmed connected.
    if (CurrentBoulderGameState == EBoulderGameState::Idle)
    {
        if (bErgDeviceReady)
        {
            StartGame();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[BoulderGame] Rep received but ERG device not ready yet — ignoring"));
        }
        return;
    }

    if (CurrentBoulderGameState != EBoulderGameState::Playing) return;

    TimeSinceLastRep = 0.f;
    TotalReps++;

    CurrentPower  = Rep.RepPower;
    TotalPowerSum += Rep.RepPower;
    AveragePower  = TotalPowerSum / TotalReps;
    if (Rep.RepPower > PeakPower) PeakPower = Rep.RepPower;

    CurrentZone = Rep.PowerZone;

    // QTE evaluation
    EPushRating Rating;
    float TimingMult;
    if (QTE)
        QTE->EvaluatePush(Rating, TimingMult);
    else
    {
        Rating     = EPushRating::Miss;
        TimingMult = 0.8f;
    }

    // Combo tracker
    if (Rating == EPushRating::Perfect)
    {
        Combo++;
        PerfectCount++;
    }
    else
    {
        Combo = 0;
    }

    const float ComboBonus = 1.f + FMath::Min(FMath::Max(Combo - 1, 0) * 0.10f, 0.50f);
    const float Combined   = TimingMult * ComboBonus;

    // Mutable copy to fill in rating (BuildRepData gave us zone, game mode adds rating)
    FRepData RepWithRating      = Rep;
    RepWithRating.PushRating    = Rating;

    // Push boulder
    if (BoulderActorRef)
        BoulderActorRef->Push(Rep.RepPower, Combined);

    // Notify listeners (HUD, effects, sound, etc.)
    OnRepProcessed.Broadcast(RepWithRating);
}

// ─────────────────────────────────────────────────────────────────────────────
//  State management
// ─────────────────────────────────────────────────────────────────────────────

void ABoulderGameMode::ChangeState(EBoulderGameState NewState)
{
    CurrentBoulderGameState = NewState;

    if (QTE)
    {
        const bool bQTEActive = (NewState == EBoulderGameState::Playing);
        QTE->SetComponentTickEnabled(bQTEActive);
    }

    OnGameStateChanged.Broadcast(NewState);

    UE_LOG(LogTemp, Log, TEXT("[BoulderGame] State → %d"), (int32)NewState);
}

void ABoulderGameMode::EndGame(bool bWon)
{
    if (CurrentBoulderGameState != EBoulderGameState::Playing) return;

    FGameStats Stats = BuildStats(bWon);
    ChangeState(bWon ? EBoulderGameState::Won : EBoulderGameState::Lost);
    OnGameEnded.Broadcast(Stats);

    UE_LOG(LogTemp, Log, TEXT("[BoulderGame] %s — reps=%d, avg_power=%.1f, peak=%.1f"),
        bWon ? TEXT("WIN") : TEXT("LOSS"), Stats.TotalReps, Stats.AveragePower, Stats.PeakPower);
}

FGameStats ABoulderGameMode::BuildStats(bool bWon) const
{
    FGameStats Stats;
    Stats.TotalReps     = TotalReps;
    Stats.PerfectCount  = PerfectCount;
    Stats.AveragePower  = AveragePower;
    Stats.PeakPower     = PeakPower;
    Stats.TimeUsedSec   = TimeLimitSeconds - TimeRemaining;
    Stats.FinalProgress = BoulderActorRef ? BoulderActorRef->Progress : 0.f;
    return Stats;
}
