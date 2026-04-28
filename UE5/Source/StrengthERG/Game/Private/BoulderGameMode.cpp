// BoulderGameMode.cpp
// UE5 port of Unity's BoulderGameManager + QTEController.
//
// Notes on merging QTEController into GameMode:
//   Unity's QTEController was a MonoBehaviour on a separate GameObject but had
//   no world presence — it was pure logic. In UE5 it cleanly merges into the
//   GameMode as private methods. Blueprint subclasses can still expose it.

#include "BoulderGameMode.h"
#include "BoulderActor.h"
#include "PusherCharacter.h"
#include "ErgManagerComponent.h"
#include "BoulderHUD.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ShapeComponent.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraActor.h"

ABoulderGameMode::ABoulderGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ABoulderGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Auto-find boulder if not explicitly set
    if (!Boulder)
    {
        for (TActorIterator<ABoulderActor> It(GetWorld()); It; ++It)
        {
            Boulder = *It;
            break;
        }
    }

    // Auto-find pusher character if not explicitly assigned
    if (!Pusher)
    {
        for (TActorIterator<APusherCharacter> It(GetWorld()); It; ++It)
        {
            Pusher = *It;
            UE_LOG(LogTemp, Log, TEXT("[BoulderGame] Auto-found pusher: %s"), *Pusher->GetName());
            break;
        }
    }
    if (!Pusher)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BoulderGame] No APusherCharacter found in level — place BP_PusherCharacter."));
    }

    // Auto-find AoiCharacter (BP_Aoi or any ACharacter) if not explicitly assigned.
    // This handles the case where BP_Aoi does not inherit from APusherCharacter.
    if (!AoiCharacter)
    {
        // First try to find by name containing "Aoi"
        for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
        {
            if (It->GetName().Contains(TEXT("Aoi")) ||
                It->GetClass()->GetName().Contains(TEXT("Aoi")))
            {
                AoiCharacter = *It;
                UE_LOG(LogTemp, Log, TEXT("[BoulderGame] Auto-found Aoi character: %s"), *AoiCharacter->GetName());
                break;
            }
        }
    }
    // Force Aoi visible — only show rendering components (SkeletalMesh, StaticMesh, Groom).
    // Deliberately skip UShapeComponent subclasses (box/capsule/sphere) to avoid
    // debug cage meshes appearing in-game.
    if (AoiCharacter)
    {
        AoiCharacter->SetActorHiddenInGame(false);
        AoiCharacter->SetActorEnableCollision(true);

        TArray<UPrimitiveComponent*> Primitives;
        AoiCharacter->GetComponents<UPrimitiveComponent>(Primitives);
        int32 ShownCount = 0;
        for (UPrimitiveComponent* Prim : Primitives)
        {
            if (!Prim) continue;
            // Skip collision shapes — they show as visible cages if forced on
            if (Prim->IsA<UShapeComponent>()) continue;
            Prim->SetHiddenInGame(false, true);
            Prim->SetVisibility(true, true);
            ++ShownCount;
        }

        FVector AoiLoc = AoiCharacter->GetActorLocation();
        UE_LOG(LogTemp, Log, TEXT("[BoulderGame] Aoi visibility enforced on %d mesh components. Location: X=%.1f Y=%.1f Z=%.1f"),
               ShownCount, AoiLoc.X, AoiLoc.Y, AoiLoc.Z);
    }

    // Find ErgManagerComponent — check the GameMode itself first (set in BP),
    // then fall back to any actor in the level.
    ErgComp = FindComponentByClass<UErgManagerComponent>();
    if (!ErgComp)
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            ErgComp = It->FindComponentByClass<UErgManagerComponent>();
            if (ErgComp) break;
        }
    }

    if (ErgComp)
    {
        ErgComp->OnNewRep.AddDynamic(this, &ABoulderGameMode::HandleNewRep);
        UE_LOG(LogTemp, Log, TEXT("[BoulderGame] Subscribed to ERG OnNewRep."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BoulderGame] UErgManagerComponent not found — add it to BP_BoulderGameMode."));
    }

    if (bSimulateInput)
    {
        UE_LOG(LogTemp, Log, TEXT("[BoulderGame][SIM] Simulate input mode is ACTIVE. Press Spacebar to start/rep."));
    }

    // Auto-find a CameraActor in the level if not explicitly assigned
    if (!GameCamera)
    {
        for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
        {
            GameCamera = *It;
            break;
        }
    }

    if (GameCamera)
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC)
        {
            PC->SetViewTargetWithBlend(GameCamera);
            UE_LOG(LogTemp, Log, TEXT("[BoulderGame] Game camera activated: %s"), *GameCamera->GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BoulderGame] No CameraActor found in level — using default view."));
    }

    ChangeState(EBoulderGameState::Idle);

    // Create and add HUD widget
    if (HUDWidgetClass)
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC)
        {
            UUserWidget* Widget = CreateWidget<UUserWidget>(PC, HUDWidgetClass);
            HUDInstance = Cast<UBoulderHUD>(Widget);
            if (HUDInstance)
            {
                HUDInstance->AddToViewport();
                HUDInstance->ShowStartScreen();
            }
            else if (Widget)
            {
                // Widget was created but is not a UBoulderHUD — wrong parent class
                UE_LOG(LogTemp, Warning, TEXT("[BoulderGame] WBP_BoulderHUD parent class is not UBoulderHUD — reparent it in the editor."));
                Widget->AddToViewport(); // still show it so the screen isn't blank
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[BoulderGame] HUDWidgetClass not set — assign WBP_BoulderHUD in the GameMode defaults."));
    }
}

void ABoulderGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateQTE(DeltaTime);

    // ── Simulation input ──────────────────────────────────────────────────────
    // Polled BEFORE the state switch so Spacebar works from Idle (starts game)
    // and from Playing (fires reps). Hardware PM5 path is unaffected.
    if (bSimulateInput)
    {
        APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
        if (PC && PC->WasInputKeyJustPressed(EKeys::SpaceBar))
        {
            UE_LOG(LogTemp, Log, TEXT("[BoulderGame][SIM] Spacebar detected | State=%d"), (int32)State);

            if (State == EBoulderGameState::Idle ||
                State == EBoulderGameState::Won  ||
                State == EBoulderGameState::Lost)
            {
                UE_LOG(LogTemp, Log, TEXT("[BoulderGame][SIM] Calling StartGame() from sim path"));
                StartGame();
            }
            else if (State == EBoulderGameState::Playing)
            {
                UE_LOG(LogTemp, Log, TEXT("[BoulderGame][SIM] Injecting simulated rep #%d | pull=%d | time=%.2f"),
                       SimRepNumber + 1, SimPullDistance, SimRepTimeSec);
                HandleNewRep(++SimRepNumber, SimRepTimeSec, SimPullDistance);
            }
            // Spacebar during Countdown is intentionally ignored — let it tick down.
        }
    }

    switch (State)
    {
    case EBoulderGameState::Countdown:
        CountdownTimer -= DeltaTime;
        if (HUDInstance) HUDInstance->UpdateCountdown(CountdownTimer);
        if (CountdownTimer <= 0.f)
        {
            TimeRemaining = TimeLimitSeconds;
            ChangeState(EBoulderGameState::Playing);
        }
        break;

    case EBoulderGameState::Playing:
        TimeRemaining     -= DeltaTime;
        TimeSinceLastRep  += DeltaTime;

        // Rollback when idle too long
        if (TimeSinceLastRep > RollbackDelaySec && Boulder)
        {
            Boulder->ApplyRollback(DeltaTime);
            if (Pusher) Pusher->StopPushAnimation();
        }

        // Win/Lose checks
        if (Boulder && Boulder->IsAtTop())
            EndGame(true);
        else if (TimeRemaining <= 0.f)
            EndGame(false);
        break;

    default:
        break;
    }

    // ── Camera Follow ─────────────────────────────────────────────────────────
    if (bCameraFollowBoulder && GameCamera && Boulder)
    {
        FVector BoulderPos = Boulder->GetActorLocation();
        FVector HillFwd    = Boulder->GetPathDirection();

        // Position: behind and above the boulder
        FVector DesiredPos = BoulderPos
                           - HillFwd * CameraFollowDistance
                           + FVector::UpVector * CameraFollowHeight;

        // Look toward a point slightly above the boulder
        FVector LookTarget = BoulderPos + FVector::UpVector * CameraLookAtHeightOffset;
        FRotator DesiredRot = (LookTarget - DesiredPos).GetSafeNormal().Rotation();

        FVector  CurrentPos = GameCamera->GetActorLocation();
        FRotator CurrentRot = GameCamera->GetActorRotation();

        GameCamera->SetActorLocation(
            FMath::VInterpTo(CurrentPos, DesiredPos, DeltaTime, CameraFollowSpeed));
        GameCamera->SetActorRotation(
            FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, CameraFollowSpeed));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public Controls
// ─────────────────────────────────────────────────────────────────────────────

void ABoulderGameMode::StartGame()
{
    if (State == EBoulderGameState::Playing || State == EBoulderGameState::Countdown)
        return;

    UE_LOG(LogTemp, Log, TEXT("[BoulderGame] StartGame() called | bSimulateInput=%d"), (int32)bSimulateInput);

    if (Boulder) Boulder->ResetPosition();

    TotalReps       = 0;
    Combo           = 0;
    PerfectCount    = 0;
    CurrentPower    = 0.f;
    PeakPower       = 0.f;
    TotalPowerAccum = 0.f;
    TimeSinceLastRep = 0.f;
    SimRepNumber    = 0;

    CountdownTimer = CountdownDuration;
    ChangeState(EBoulderGameState::Countdown);
}

void ABoulderGameMode::RestartGame()
{
    StartGame();
}

void ABoulderGameMode::OnAnimationPeak()
{
    if (State != EBoulderGameState::Playing) return;
    ForcedWindowTimer = AnimationWindowDuration;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Rep Handler
// ─────────────────────────────────────────────────────────────────────────────

void ABoulderGameMode::HandleNewRep(int32 RepNumber, float RepTimeSec, int32 PullDistance)
{
    UE_LOG(LogTemp, Log, TEXT("[BoulderGame] Rep #%d | state=%d | pull=%d"),
           RepNumber, (int32)State, PullDistance);

    // Auto-start on first rep from idle screen
    if (State == EBoulderGameState::Idle)
    {
        StartGame();
        return;
    }

    if (State != EBoulderGameState::Playing) return;

    TimeSinceLastRep = 0.f;
    TotalReps++;

    // 1. Rep power (same formula as Unity)
    float RepPower   = (float)PullDistance / FMath::Max(RepTimeSec, 0.05f);
    CurrentPower     = RepPower;
    TotalPowerAccum += RepPower;
    if (RepPower > PeakPower) PeakPower = RepPower;

    // 2. Power zone
    CurrentZone = RepPower >= ExplosiveThreshold ? EErgPowerZone::Explosive
                : RepPower >= PowerThreshold     ? EErgPowerZone::Power
                : RepPower >= ModerateThreshold  ? EErgPowerZone::Moderate
                :                                  EErgPowerZone::Low;

    // 3. QTE evaluation
    auto [Rating, TimingMult] = EvaluatePush(RepTimeSec);

    // 4. Combo
    if (Rating == EQTERating::Perfect) { Combo++; PerfectCount++; }
    else                                Combo = 0;
    float ComboBonus = 1.f + FMath::Min(FMath::Max(Combo - 1, 0) * 0.10f, 0.50f);

    // 5. Combined multiplier
    float Combined = TimingMult * ComboBonus;

    // 6. Push boulder
    if (Boulder) Boulder->Push(RepPower, Combined);

    // 7. Drive pusher animation
    if (Pusher) Pusher->PlayPushAnimation();

    // 8. HUD feedback
    if (HUDInstance)
        HUDInstance->ShowPushFeedback(Rating, CurrentZone, RepPower, Combo);
}

// ─────────────────────────────────────────────────────────────────────────────
//  QTE (inlined from QTEController.cs)
// ─────────────────────────────────────────────────────────────────────────────

void ABoulderGameMode::UpdateQTE(float DeltaTime)
{
    float WorldTime  = GetWorld()->GetTimeSeconds();
    float CycleTime  = FMath::Fmod(WorldTime, QTECycleDuration);
    QTECycleProgress = CycleTime / QTECycleDuration;

    float WOpen  = QTEWindowStartFraction * QTECycleDuration;
    float WClose = WOpen + QTEWindowDuration;
    bool  bCycleOpen = (CycleTime >= WOpen && CycleTime < WClose);

    if (ForcedWindowTimer > 0.f)
        ForcedWindowTimer -= DeltaTime;

    bQTEWindowOpen = bCycleOpen || ForcedWindowTimer > 0.f;
}

TTuple<EQTERating, float> ABoulderGameMode::EvaluatePush(float RepTimeSec)
{
    float WorldTime = GetWorld()->GetTimeSeconds();
    float EvalTime  = WorldTime - RepTimeSec;
    float CycleTime = FMath::Fmod(EvalTime, QTECycleDuration);

    float WStart = QTEWindowStartFraction * QTECycleDuration;
    float WEnd   = WStart + QTEWindowDuration;
    bool  bInCycle  = CycleTime >= WStart && CycleTime < WEnd;
    bool  bInForced = (ForcedWindowTimer + RepTimeSec) > 0.f;

    if (bInCycle || bInForced)
        return MakeTuple(EQTERating::Perfect, QTEPerfectMultiplier);
    else
        return MakeTuple(EQTERating::Miss,    QTEMissMultiplier);
}

// ─────────────────────────────────────────────────────────────────────────────
//  State Machine
// ─────────────────────────────────────────────────────────────────────────────

void ABoulderGameMode::ChangeState(EBoulderGameState Next)
{
    State = Next;
    UE_LOG(LogTemp, Log, TEXT("[BoulderGame] State -> %d"), (int32)State);
    UpdateHUDForState(Next);
}

void ABoulderGameMode::EndGame(bool bWon)
{
    if (State != EBoulderGameState::Playing) return;

    if (Pusher) Pusher->StopPushAnimation();

    float AvgPower = TotalReps > 0 ? TotalPowerAccum / (float)TotalReps : 0.f;
    float Progress = Boulder ? Boulder->GetProgress() : 0.f;

    if (bWon)
    {
        ChangeState(EBoulderGameState::Won);
        if (HUDInstance)
            HUDInstance->ShowWinScreen(TotalReps, PerfectCount,
                TimeLimitSeconds - TimeRemaining, AvgPower, PeakPower);
        OnGameWon.Broadcast();
    }
    else
    {
        ChangeState(EBoulderGameState::Lost);
        if (HUDInstance)
            HUDInstance->ShowLoseScreen(Progress, TotalReps, AvgPower, PeakPower);
        OnGameLost.Broadcast();
    }
}

void ABoulderGameMode::UpdateHUDForState(EBoulderGameState NewState)
{
    if (!HUDInstance) return;

    switch (NewState)
    {
    case EBoulderGameState::Idle:
        HUDInstance->ShowStartScreen();
        break;
    case EBoulderGameState::Countdown:
        HUDInstance->ShowCountdown();
        break;
    case EBoulderGameState::Playing:
        HUDInstance->ShowGameplay();
        break;
    // Won / Lost screens are set in EndGame() with stats — nothing extra here.
    default:
        break;
    }
}
