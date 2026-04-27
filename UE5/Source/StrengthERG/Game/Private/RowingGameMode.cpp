#include "RowingGameMode.h"
#include "RowingProgressActor.h"
#include "ErgManagerComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Math/UnrealMathUtility.h"

ARowingGameMode::ARowingGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ARowingGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (!ProgressActor)
    {
        for (TActorIterator<ARowingProgressActor> It(GetWorld()); It; ++It)
        {
            ProgressActor = *It;
            break;
        }
    }

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        ErgComp = It->FindComponentByClass<UErgManagerComponent>();
        if (ErgComp) break;
    }

    if (ErgComp)
    {
        ErgComp->OnNewRep.AddDynamic(this, &ARowingGameMode::HandleNewRep);
        ErgComp->OnErgDataUpdated.AddDynamic(this, &ARowingGameMode::HandleErgDataUpdated);
        UE_LOG(LogTemp, Log, TEXT("[RowingGame] Subscribed to ERG events."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[RowingGame] UErgManagerComponent not found. Enable simulation or place an ERG manager actor in the level."));
    }

    ChangeState(ERowingGameState::Idle);
}

void ARowingGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    TimeSinceLastStroke += DeltaTime;

    switch (State)
    {
    case ERowingGameState::Countdown:
        CountdownTimer -= DeltaTime;
        if (CountdownTimer <= 0.f)
        {
            TimeRemaining = TimeLimitSeconds;
            ChangeState(ERowingGameState::Playing);
        }
        break;

    case ERowingGameState::Playing:
        TimeRemaining -= DeltaTime;

        if (ProgressActor && TimeSinceLastStroke > DriftDelaySec)
        {
            ProgressActor->ApplyDrift(DeltaTime, DriftMetersPerSecond);
        }

        if (bSimulateInput)
        {
            APlayerController* PC = GetWorld()->GetFirstPlayerController();
            if (PC && PC->WasInputKeyJustPressed(EKeys::SpaceBar))
            {
                ApplyStroke(SimSPM, SimPaceSeconds, SimPowerWatts);
            }
        }

        if (ProgressActor && ProgressActor->IsComplete())
        {
            EndGame(true);
        }
        else if (TimeRemaining <= 0.f)
        {
            EndGame(false);
        }
        break;

    default:
        break;
    }
}

void ARowingGameMode::StartGame()
{
    if (State == ERowingGameState::Playing || State == ERowingGameState::Countdown)
        return;

    StrokeCount = 0;
    CurrentSPM = 0.f;
    CurrentPaceSeconds = 0.f;
    CurrentPowerWatts = 0.f;
    PeakPowerWatts = 0.f;
    TimeSinceLastStroke = 999.f;

    if (ProgressActor)
    {
        ProgressActor->ResetProgress();
    }

    CountdownTimer = CountdownDuration;
    ChangeState(ERowingGameState::Countdown);
}

void ARowingGameMode::RestartGame()
{
    StartGame();
}

float ARowingGameMode::GetProgressMeters() const
{
    return ProgressActor ? ProgressActor->CurrentProgress : 0.f;
}

float ARowingGameMode::GetProgressNormalized() const
{
    return ProgressActor ? ProgressActor->GetProgressNormalized() : 0.f;
}

void ARowingGameMode::ChangeState(ERowingGameState Next)
{
    State = Next;
    UE_LOG(LogTemp, Log, TEXT("[RowingGame] State -> %d"), (int32)State);
}

void ARowingGameMode::EndGame(bool bWon)
{
    if (State != ERowingGameState::Playing) return;

    if (bWon)
    {
        ChangeState(ERowingGameState::Won);
        OnRowingWon.Broadcast();
    }
    else
    {
        ChangeState(ERowingGameState::Lost);
        OnRowingLost.Broadcast();
    }
}

float ARowingGameMode::EvaluateCadenceBonus(float SPM, float PaceSeconds) const
{
    const bool bSPMGood = SPM >= IdealLowSPM && SPM <= IdealHighSPM;
    const bool bPaceGood = PaceSeconds >= IdealLowPaceSeconds && PaceSeconds <= IdealHighPaceSeconds;

    if (bSPMGood && bPaceGood) return CadenceBonusMultiplier;
    if (bSPMGood || bPaceGood) return FMath::Lerp(1.f, CadenceBonusMultiplier, 0.5f);
    return 1.f;
}

void ARowingGameMode::ApplyStroke(float SPM, float PaceSeconds, float PowerWatts)
{
    if (State == ERowingGameState::Idle && bAutoStartOnFirstStroke)
    {
        StartGame();
        return;
    }

    if (State != ERowingGameState::Playing) return;

    TimeSinceLastStroke = 0.f;
    StrokeCount++;

    CurrentSPM = SPM;
    CurrentPaceSeconds = PaceSeconds;
    CurrentPowerWatts = PowerWatts;
    PeakPowerWatts = FMath::Max(PeakPowerWatts, PowerWatts);

    if (SPM < MinSPMForProgress || !ProgressActor)
        return;

    const float BaseProgress = PowerWatts * MetersPerWatt;
    const float Bonus = EvaluateCadenceBonus(SPM, PaceSeconds);
    ProgressActor->AddProgress(BaseProgress * Bonus);

    UE_LOG(LogTemp, Log, TEXT("[RowingGame] Stroke #%d | SPM=%.1f Pace=%.1f Power=%.1f Progress=%.1f"),
           StrokeCount, SPM, PaceSeconds, PowerWatts, ProgressActor->CurrentProgress);
}

void ARowingGameMode::HandleNewRep(int32 RepNumber, float RepTimeSec, int32 PullDistance)
{
    const float DerivedSPM = RepTimeSec;
    const float DerivedPower = (float)PullDistance;
    const float DerivedPace = CurrentPaceSeconds;

    if (State == ERowingGameState::Idle && bAutoStartOnFirstStroke)
    {
        StartGame();
        return;
    }

    ApplyStroke(DerivedSPM, DerivedPace, DerivedPower);
}

void ARowingGameMode::HandleErgDataUpdated(FErgData Data)
{
    CurrentSPM = Data.RepTimeSec;
    CurrentPaceSeconds = Data.ElapsedSeconds;
    CurrentPowerWatts = (float)Data.PullDistance;
}
