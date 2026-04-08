#include "HudManager.h"
#include "BoulderGameMode.h"
#include "BoulderActor.h"
#include "QTEComponent.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"

AHudManager::AHudManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AHudManager::BeginPlay()
{
    Super::BeginPlay();

    // Find game mode
    GameMode = GetWorld()->GetAuthGameMode<ABoulderGameMode>();
    if (GameMode)
    {
        Boulder = GameMode->BoulderActorRef;
        QTE = GameMode->QTE;

        // Subscribe to events
        GameMode->OnGameStateChanged.AddDynamic(this, &AHudManager::OnGameStateChanged);
        GameMode->OnRepProcessed.AddDynamic(this, &AHudManager::OnRepProcessed);
        GameMode->OnGameEnded.AddDynamic(this, &AHudManager::OnGameEnded);
        GameMode->OnCountdownTick.AddDynamic(this, &AHudManager::OnCountdownTick);

        if (QTE)
        {
            QTE->OnWindowOpened.AddDynamic(this, &AHudManager::OnQTEWindowOpened);
            QTE->OnWindowClosed.AddDynamic(this, &AHudManager::OnQTEWindowClosed);
        }

        UE_LOG(LogTemp, Log, TEXT("[HudManager] Initialized and subscribed to GameMode events"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[HudManager] Could not find GameMode"));
    }
}

void AHudManager::DrawHUD()
{
    Super::DrawHUD();

    if (!GameMode) return;

    // Update state from game mode
    TimeRemaining = GameMode->TimeRemaining;
    TotalReps = GameMode->TotalReps;
    Combo = GameMode->Combo;
    PerfectCount = GameMode->PerfectCount;
    CurrentPower = GameMode->CurrentPower;
    CurrentZone = GameMode->CurrentZone;
    AveragePower = GameMode->AveragePower;
    PeakPower = GameMode->PeakPower;

    if (Boulder)
    {
        BoulderProgress = Boulder->Progress;
    }

    if (QTE)
    {
        bIsQTEWindowOpen = QTE->bIsWindowOpen;
        QTECycleProgress = QTE->CycleProgress;
    }

    // Draw based on game state
    switch (GameMode->CurrentBoulderGameState)
    {
        case EBoulderGameState::Idle:
            DrawStartScreen();
            break;

        case EBoulderGameState::Countdown:
            DrawCountdownScreen();
            break;

        case EBoulderGameState::Playing:
            DrawGameplayHUD();
            break;

        case EBoulderGameState::Won:
            DrawWinScreen();
            break;

        case EBoulderGameState::Lost:
            DrawLoseScreen();
            break;
    }

    // Update and draw push feedback popup
    if (PushFeedback.IsSet())
    {
        DrawPushFeedback(GetWorld()->DeltaTimeSeconds);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Event Handlers
// ─────────────────────────────────────────────────────────────────────────────

void AHudManager::OnGameStateChanged(EBoulderGameState NewState)
{
    CurrentState = NewState;
}

void AHudManager::OnRepProcessed(const FRepData& Rep)
{
    // Create push feedback popup
    FString ZoneLabel = GetZoneName(Rep.PowerZone);
    FString Rating = Rep.PushRating == EPushRating::Perfect ? TEXT("PERFECT") : TEXT("GOOD TRY");
    FString Text = FString::Printf(TEXT("%s %s"), *Rating, *ZoneLabel);

    if (Combo > 2)
    {
        Text = FString::Printf(TEXT("x%d  %s %s"), Combo, *Rating, *ZoneLabel);
    }

    FLinearColor Color = GetZoneColor(Rep.PowerZone);
    if (Rep.PushRating == EPushRating::Perfect)
    {
        Color = FMath::Lerp(Color, ColorPerfect, 0.5f);
    }

    FPushFeedback Feedback;
    Feedback.Text = Text;
    Feedback.Color = Color;
    Feedback.Timer = 0.f;
    Feedback.StartPos = FVector2D(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.7f);

    PushFeedback = Feedback;
}

void AHudManager::OnGameEnded(const FGameStats& Stats)
{
    FEndGameStats EndGameStats;
    EndGameStats.TotalReps = Stats.TotalReps;
    EndGameStats.PerfectCount = Stats.PerfectCount;
    EndGameStats.TimeUsedSec = Stats.TimeUsedSec;
    EndGameStats.AveragePower = Stats.AveragePower;
    EndGameStats.PeakPower = Stats.PeakPower;
    EndGameStats.FinalProgress = Stats.FinalProgress;
    EndGameStats.bWon = (GameMode->CurrentBoulderGameState == EBoulderGameState::Won);

    EndStats = EndGameStats;
}

void AHudManager::OnCountdownTick(float SecondsRemaining)
{
    CountdownTimer = SecondsRemaining;
}

void AHudManager::OnQTEWindowOpened()
{
    // Could trigger sound effect here
}

void AHudManager::OnQTEWindowClosed()
{
    // Could trigger sound effect here
}

// ─────────────────────────────────────────────────────────────────────────────
//  Draw Screen Panels
// ─────────────────────────────────────────────────────────────────────────────

void AHudManager::DrawStartScreen()
{
    if (!Canvas) return;

    // Semi-transparent background
    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

    // Title
    DrawCenteredText(TEXT("PUSH THE BOULDER"), Canvas->ClipY * 0.3f, 3.0f, FLinearColor::White);

    // Instructions
    DrawCenteredText(TEXT("Row to Start"), Canvas->ClipY * 0.5f, 1.5f, FLinearColor::Yellow);

    // Or show button hint
    DrawCenteredText(TEXT("Press SPACE to simulate push (test mode)"), Canvas->ClipY * 0.85f, 1.0f, FLinearColor::Gray);
}

void AHudManager::DrawCountdownScreen()
{
    if (!Canvas) return;

    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

    int32 SecondsLeft = FMath::CeilToInt(FMath::Max(CountdownTimer, 0.f));
    FString Text = SecondsLeft > 0 ? FString::FromInt(SecondsLeft) : TEXT("GO!");
    FLinearColor Color = SecondsLeft > 0 ? FLinearColor::Yellow : ColorPerfect;

    DrawCenteredText(Text, Canvas->ClipY * 0.5f, 5.0f, Color);
}

void AHudManager::DrawGameplayHUD()
{
    if (!Canvas) return;

    // Draw main HUD elements
    DrawQTEBar();
    DrawProgressBar();
    DrawLiveStats();
}

void AHudManager::DrawWinScreen()
{
    if (!Canvas || !EndStats.IsSet()) return;

    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

    float Y = Canvas->ClipY * 0.2f;
    DrawCenteredText(TEXT("BOULDER REACHED THE TOP!"), Y, 2.5f, ColorPerfect);

    Y += 100.f;
    FString Stats = FString::Printf(
        TEXT("Reps: %d | Perfects: %d | Time: %s\nAvg Power: %.1f | Peak: %.1f"),
        EndStats->TotalReps,
        EndStats->PerfectCount,
        *FormatTime(EndStats->TimeUsedSec),
        EndStats->AveragePower,
        EndStats->PeakPower
    );
    DrawCenteredText(Stats, Y, 1.5f, FLinearColor::White);

    DrawCenteredText(TEXT("Press SPACE or click to continue"), Canvas->ClipY * 0.85f, 1.0f, FLinearColor::Yellow);
}

void AHudManager::DrawLoseScreen()
{
    if (!Canvas || !EndStats.IsSet()) return;

    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

    float Y = Canvas->ClipY * 0.2f;
    DrawCenteredText(TEXT("The boulder rolled back..."), Y, 2.5f, ColorTimerWarning);

    Y += 100.f;
    FString Stats = FString::Printf(
        TEXT("%.0f%% up the hill | Reps: %d\nAvg Power: %.1f | Peak: %.1f"),
        EndStats->FinalProgress * 100.f,
        EndStats->TotalReps,
        EndStats->AveragePower,
        EndStats->PeakPower
    );
    DrawCenteredText(Stats, Y, 1.5f, FLinearColor::White);

    DrawCenteredText(TEXT("Press SPACE or click to continue"), Canvas->ClipY * 0.85f, 1.0f, FLinearColor::Yellow);
}

// ─────────────────────────────────────────────────────────────────────────────
//  HUD Elements
// ─────────────────────────────────────────────────────────────────────────────

void AHudManager::DrawQTEBar()
{
    if (!Canvas || !QTE) return;

    float X = Canvas->ClipX * 0.1f;
    float Y = Canvas->ClipY * 0.1f;
    float Width = Canvas->ClipX * 0.8f;
    float Height = 40.f;

    // Background
    DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), X, Y, Width, Height);

    // Cycle progress bar
    FLinearColor BarColor = bIsQTEWindowOpen ? ColorWindowOpen : ColorWindowClosed;
    DrawRect(BarColor, X, Y, Width * QTECycleProgress, Height);

    // Border
    DrawLine(X, Y, X + Width, Y, BarColor, 2.f);
    DrawLine(X, Y + Height, X + Width, Y + Height, BarColor, 2.f);

    // Status text
    FString StatusText = bIsQTEWindowOpen ? TEXT("PUSH NOW!") : TEXT("WAIT...");
    FLinearColor StatusColor = bIsQTEWindowOpen ? ColorPerfect : FLinearColor::Gray;
    DrawText(StatusText, X + 20.f, Y + 10.f, 1.5f, StatusColor);
}

void AHudManager::DrawProgressBar()
{
    if (!Canvas) return;

    float X = Canvas->ClipX * 0.05f;
    float Y = Canvas->ClipY * 0.2f;
    float Width = Canvas->ClipX * 0.3f;
    float Height = 30.f;

    // Background
    DrawRect(FLinearColor(0.2f, 0.2f, 0.2f, 0.8f), X, Y, Width, Height);

    // Progress fill
    FLinearColor ProgressColor = FMath::Lerp(FLinearColor::Red, ColorPerfect, BoulderProgress);
    DrawRect(ProgressColor, X, Y, Width * BoulderProgress, Height);

    // Border
    DrawLine(X, Y, X + Width, Y, FLinearColor::White, 2.f);
    DrawLine(X, Y + Height, X + Width, Y + Height, FLinearColor::White, 2.f);

    // Label
    FString ProgressText = FString::Printf(TEXT("Progress: %.0f%%"), BoulderProgress * 100.f);
    DrawText(ProgressText, X + 10.f, Y + 5.f, 1.2f, FLinearColor::White);
}

void AHudManager::DrawLiveStats()
{
    if (!Canvas) return;

    float X = Canvas->ClipX * 0.65f;
    float Y = Canvas->ClipY * 0.15f;
    float LineHeight = 25.f;

    // Timer
    FLinearColor TimerColor = TimeRemaining < 15.f ? ColorTimerWarning : FLinearColor::White;
    FString TimerText = FString::Printf(TEXT("Time: %s"), *FormatTime(TimeRemaining));
    DrawText(TimerText, X, Y, 1.2f, TimerColor);

    Y += LineHeight;

    // Reps
    FString RepsText = FString::Printf(TEXT("Reps: %d"), TotalReps);
    DrawText(RepsText, X, Y, 1.2f, FLinearColor::White);

    Y += LineHeight;

    // Combo (only show if > 1)
    if (Combo > 1)
    {
        FLinearColor ComboColor = FMath::Lerp(FLinearColor::White, ColorPerfect, FMath::Min((float)Combo / 6.f, 1.f));
        FString ComboText = FString::Printf(TEXT("COMBO x%d"), Combo);
        DrawText(ComboText, X, Y, 1.5f, ComboColor);
    }

    Y += LineHeight * 1.5f;

    // Power Zone
    if (TotalReps > 0)
    {
        FString ZoneText = FString::Printf(TEXT("%s  %.1f W"), *GetZoneName(CurrentZone), CurrentPower);
        FLinearColor ZoneColor = GetZoneColor(CurrentZone);
        DrawText(ZoneText, X, Y, 1.2f, ZoneColor);
    }
}

void AHudManager::DrawPushFeedback(float DeltaTime)
{
    if (!Canvas || !PushFeedback.IsSet()) return;

    FPushFeedback& Feedback = PushFeedback.GetValue();
    Feedback.Timer += DeltaTime;

    if (Feedback.Timer >= Feedback.Duration)
    {
        PushFeedback.Reset();
        return;
    }

    // Fade out over last 0.5 seconds
    float Alpha = Feedback.Timer < (Feedback.Duration * 0.45f) ? 1.0f : FMath::Clamp((Feedback.Duration - Feedback.Timer) / 0.5f, 0.0f, 1.0f);

    // Float upward
    float YOffset = (1.f - Feedback.Timer / Feedback.Duration) * 80.f;
    FVector2D Pos = Feedback.StartPos + FVector2D(0.f, YOffset);

    FLinearColor DrawColor = Feedback.Color;
    DrawColor.A *= Alpha;

    DrawCenteredText(Feedback.Text, Pos.Y, 1.8f, DrawColor);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper Functions
// ─────────────────────────────────────────────────────────────────────────────

FLinearColor AHudManager::GetZoneColor(EPowerZone Zone) const
{
    switch (Zone)
    {
        case EPowerZone::Explosive: return ColorZoneExplosive;
        case EPowerZone::Power:     return ColorZonePower;
        case EPowerZone::Moderate:  return ColorZoneModerate;
        default:                    return ColorZoneLow;
    }
}

FString AHudManager::GetZoneName(EPowerZone Zone) const
{
    switch (Zone)
    {
        case EPowerZone::Explosive: return TEXT("EXPLOSIVE");
        case EPowerZone::Power:     return TEXT("POWER");
        case EPowerZone::Moderate:  return TEXT("MODERATE");
        default:                    return TEXT("LOW");
    }
}

FString AHudManager::FormatTime(float Seconds) const
{
    int32 Minutes = FMath::FloorToInt(Seconds / 60.f);
    float SecondsRemainder = FMath::Fmod(Seconds, 60.f);
    return FString::Printf(TEXT("%02d:%05.1f"), Minutes, SecondsRemainder);
}

void AHudManager::DrawCenteredText(const FString& Text, float Y, float Scale, FLinearColor Color)
{
    if (!Canvas) return;

    float TextWidth = 0.f, TextHeight = 0.f;
    GetTextSize(Text, TextWidth, TextHeight, GEngine->GetSmallFont(), Scale);

    float X = (Canvas->ClipX - TextWidth) * 0.5f;
    DrawText(Text, X, Y, Scale, Color);
}

void AHudManager::DrawText(const FString& Text, float X, float Y, float Scale, FLinearColor Color)
{
    if (!Canvas) return;

    Canvas->SetDrawColor(FColor(Color.R * 255, Color.G * 255, Color.B * 255, Color.A * 255));
    Canvas->DrawText(GEngine->GetSmallFont(), Text, X, Y, Scale, Scale);
}
