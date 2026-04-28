/**
 * BoulderHUD.cpp
 *
 * UE5 equivalent of Unity's BoulderHUD.cs.
 *
 * Setup guide (do once in editor):
 *   1. Create a Widget Blueprint "WBP_BoulderHUD" that inherits from UBoulderHUD.
 *   2. Add five named panels:  StartPanel, CountdownPanel, GameplayPanel,
 *      WinPanel, LosePanel  (CanvasPanel or Overlay widgets).
 *   3. Inside each panel add the widgets listed in BoulderHUD.h using those
 *      exact variable names (BindWidget enforces this at compile time).
 *   4. In ABoulderGameMode set HUDWidgetClass = WBP_BoulderHUD.
 *   5. ABoulderGameMode::BeginPlay creates the widget and calls ShowStartScreen.
 */

#include "BoulderHUD.h"
#include "BoulderGameMode.h"
#include "ErgManagerComponent.h"
#include "BoulderActor.h"
#include "GameFramework/GameState.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

// ?????????????????????????????????????????????????????????????????????????????
//  NativeConstruct  (? Unity Start)
// ?????????????????????????????????????????????????????????????????????????????

void UBoulderHUD::NativeConstruct()
{
    Super::NativeConstruct();

    // Resolve game mode reference
    GameMode = Cast<ABoulderGameMode>(UGameplayStatics::GetGameMode(this));

    // Wire buttons
    if (Startbutton)
        Startbutton->OnClicked.AddDynamic(this, &UBoulderHUD::OnStartClicked);
    if (WinRestartButton)
        WinRestartButton->OnClicked.AddDynamic(this, &UBoulderHUD::OnWinRestartClicked);
    if (LoseRestartButton)
        LoseRestartButton->OnClicked.AddDynamic(this, &UBoulderHUD::OnLoseRestartClicked);

    if (StartTitleText)
        StartTitleText->SetText(FText::FromString(TEXT("PUSH\nTHE BOULDER")));

    // Set button label — prefer named BindWidget, then existing child, then
    // create one programmatically so no WBP changes are needed.
    if (Startbutton)
    {
        UTextBlock* BtnLabel = StartButtonText
            ? StartButtonText.Get()
            : Cast<UTextBlock>(Startbutton->GetChildAt(0));

        if (!BtnLabel)
        {
            BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            if (BtnLabel)
                Startbutton->SetContent(BtnLabel);
        }

        if (BtnLabel)
            BtnLabel->SetText(FText::FromString(TEXT("Push to Start")));
    }

    SetupLayout();
    HideAllPanels();
}

// ?????????????????????????????????????????????????????????????????????????????
//  SetupLayout  — anchors and positions every widget at runtime so the WBP
//  designer does not need manual positioning.  Based on a 1920x1080 reference.
//  All coordinates use UMG convention: Y=0 is TOP, Y=1 is BOTTOM.
// ?????????????????????????????????????????????????????????????????????????????

void UBoulderHUD::SetupLayout()
{
    // Helper: set anchors + offsets + alignment on any widget's CanvasPanelSlot.
    // anchorMin/Max fill the panel fraction; offsets are edge margins in Slate units.
    auto Pin = [](UWidget* W,
                  float AnchorX0, float AnchorY0,
                  float AnchorX1, float AnchorY1,
                  float Left = 0.f, float Top = 0.f,
                  float Right = 0.f, float Bottom = 0.f,
                  FVector2D Alignment = FVector2D(0.f, 0.f))
    {
        if (!W) return;
        if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
        {
            S->SetAnchors(FAnchors(AnchorX0, AnchorY0, AnchorX1, AnchorY1));
            S->SetOffsets(FMargin(Left, Top, Right, Bottom));
            S->SetAlignment(Alignment);
        }
    };

    // ?? Panels — each fills the full canvas ??????????????????????????????????
    Pin(StartPanel,     0.f, 0.f, 1.f, 1.f);
    Pin(CountdownPanel, 0.f, 0.f, 1.f, 1.f);
    Pin(GameplayPanel,  0.f, 0.f, 1.f, 1.f);
    Pin(WinPanel,       0.f, 0.f, 1.f, 1.f);
    Pin(LosePanel,      0.f, 0.f, 1.f, 1.f);

    // ?? Start Screen ?????????????????????????????????????????????????????????
    // Title block: centre of screen, upper half
    Pin(StartTitleText, 0.2f, 0.15f, 0.8f, 0.55f,
        0.f, 0.f, 0.f, 0.f, FVector2D(0.5f, 0.5f));
    if (StartTitleText) StartTitleText->SetJustification(ETextJustify::Center);

    // Start button: dead centre horizontally, just below the title.
    // Use SetPosition + SetSize for point anchors — SetOffsets semantics differ.
    if (Startbutton)
    {
        if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Startbutton->Slot))
        {
            S->SetAnchors(FAnchors(0.5f, 0.5f));
            S->SetAlignment(FVector2D(0.5f, 0.5f));
            S->SetPosition(FVector2D(0.f, 120.f));  // 120 Slate units below screen centre
            S->SetSize(FVector2D(320.f, 60.f));
        }
    }

    // ?? Countdown ?????????????????????????????????????????????????????????????
    Pin(CountdownText, 0.3f, 0.3f, 0.7f, 0.7f,
        0.f, 0.f, 0.f, 0.f, FVector2D(0.5f, 0.5f));
    if (CountdownText) CountdownText->SetJustification(ETextJustify::Center);

    // ?? Gameplay — top bar ????????????????????????????????????????????????????
    // Timer: top-right
    Pin(TimerText, 0.72f, 0.02f, 0.98f, 0.08f,
        0.f, 0.f, 0.f, 0.f, FVector2D(1.f, 0.f));
    if (TimerText) TimerText->SetJustification(ETextJustify::Right);

    // Rep count: top-left
    Pin(RepCountText, 0.02f, 0.02f, 0.28f, 0.08f);

    // Boulder progress bar: full-width strip just below the top stats
    Pin(ProgressBar, 0.02f, 0.09f, 0.88f, 0.135f);

    // Progress percentage label: right of the progress bar
    Pin(ProgressLabel, 0.89f, 0.09f, 0.98f, 0.135f);
    if (ProgressLabel) ProgressLabel->SetJustification(ETextJustify::Left);

    // ?? Gameplay — combo (centre, fades in/out in code) ??????????????????????
    Pin(ComboText, 0.25f, 0.18f, 0.75f, 0.27f,
        0.f, 0.f, 0.f, 0.f, FVector2D(0.5f, 0.5f));
    if (ComboText) ComboText->SetJustification(ETextJustify::Center);

    // ?? Gameplay — QTE bar area (lower third) ?????????????????????????????????
    // "PUSH NOW!" / "WAIT..." status label above the bar
    Pin(QteStatusText, 0.3f, 0.80f, 0.7f, 0.86f,
        0.f, 0.f, 0.f, 0.f, FVector2D(0.5f, 0.5f));
    if (QteStatusText) QteStatusText->SetJustification(ETextJustify::Center);

    // Cycle track: main horizontal bar
    Pin(QteCycleTrack, 0.05f, 0.87f, 0.95f, 0.92f);

    // Window flash indicator: small square left of the bar
    Pin(QteWindowFlash, 0.005f, 0.865f, 0.045f, 0.925f);

    // ?? Gameplay — bottom status strip ???????????????????????????????????????
    // Power zone label: bottom-left
    Pin(PowerZoneText, 0.02f, 0.93f, 0.45f, 0.98f);

    // ERG connection status: bottom-right
    Pin(ErgStatusText, 0.55f, 0.93f, 0.98f, 0.98f,
        0.f, 0.f, 0.f, 0.f, FVector2D(1.f, 0.f));
    if (ErgStatusText) ErgStatusText->SetJustification(ETextJustify::Right);

    // ?? Feedback popup: screen centre, animates upward in code ???????????????
    Pin(FeedbackText, 0.2f, 0.42f, 0.8f, 0.52f,
        0.f, 0.f, 0.f, 0.f, FVector2D(0.5f, 0.5f));
    if (FeedbackText) FeedbackText->SetJustification(ETextJustify::Center);

    // Win / Lose screens — body text and buttons are re-pinned at runtime
    // by PinEndScreen() when those screens are shown, so no static pin needed.
}

// ?????????????????????????????????????????????????????????????????????????????
//  NativeTick  (? Unity Update)
// ?????????????????????????????????????????????????????????????????????????????

void UBoulderHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (GameMode && GameMode->GetState() == EBoulderGameState::Playing)
        UpdateGameplayUI(InDeltaTime);

    TickFeedbackPopup(InDeltaTime);
}

// ?????????????????????????????????????????????????????????????????????????????
//  Panel Switchers
// ?????????????????????????????????????????????????????????????????????????????

void UBoulderHUD::ShowStartScreen()
{
    HideAllPanels();
    if (StartPanel) StartPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UBoulderHUD::ShowCountdown()
{
    HideAllPanels();
    if (CountdownPanel) CountdownPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (CountdownText)  CountdownText->SetText(FText::FromString(TEXT("3")));
}

void UBoulderHUD::UpdateCountdown(float Remaining)
{
    if (!CountdownText) return;
    int32 N = FMath::CeilToInt(FMath::Max(Remaining, 0.f));
    CountdownText->SetText(N > 0
        ? FText::FromString(FString::FromInt(N))
        : FText::FromString(TEXT("GO!")));
}

void UBoulderHUD::ShowGameplay()
{
    HideAllPanels();
    if (GameplayPanel) GameplayPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    ResetFeedbackPopup();
}

// Mirrors Unity's PinEndScreen(): anchors body text to the top half of its
// panel and pins the restart button to the bottom-centre.  Works only when
// both widgets sit directly inside a CanvasPanel (the expected WBP layout).
static void PinEndScreen(UTextBlock* BodyText, UButton* Button)
{
    if (BodyText)
    {
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(BodyText->Slot))
        {
            // anchorMin=(0,0), anchorMax=(1,0.65)  ?  top 65% of the panel
            // (UMG Y=0 is top, Y=1 is bottom — inverted vs Unity)
            Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.65f));
            Slot->SetOffsets(FMargin(20.f, 20.f, 20.f, 0.f));
            Slot->SetAlignment(FVector2D(0.f, 0.f));
        }
    }
    if (Button)
    {
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Button->Slot))
        {
            // Pinned to bottom-centre — matches Unity anchorMin/Max (0.5,0)
            Slot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
            Slot->SetAlignment(FVector2D(0.5f, 1.f));
            Slot->SetOffsets(FMargin(0.f, 0.f, 0.f, 20.f));
        }
    }
}

void UBoulderHUD::ShowWinScreen(int32 Reps, int32 Perfects, float TimeUsed,
                                 float AvgPower, float PeakPower)
{
    HideAllPanels();
    if (WinPanel) WinPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (WinBodyText)
    {
        FString Body = FString::Printf(
            TEXT("BOULDER REACHED THE TOP!\nReps: %d  |  Perfects: %d  |  Time: %s\nAvg Power: %.1f  |  Peak: %.1f"),
            Reps, Perfects, *FormatTime(TimeUsed).ToString(), AvgPower, PeakPower);
        WinBodyText->SetText(FText::FromString(Body));
        WinBodyText->SetJustification(ETextJustify::Center);
        WinBodyText->SetAutoWrapText(false);
    }
    PinEndScreen(WinBodyText, WinRestartButton);
}

void UBoulderHUD::ShowLoseScreen(float Progress, int32 Reps,
                                  float AvgPower, float PeakPower)
{
    HideAllPanels();
    if (LosePanel) LosePanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (LoseBodyText)
    {
        FString Body = FString::Printf(
            TEXT("The boulder rolled back...\n%.0f%% up the hill  |  Reps: %d\nAvg Power: %.1f  |  Peak: %.1f"),
            Progress * 100.f, Reps, AvgPower, PeakPower);
        LoseBodyText->SetText(FText::FromString(Body));
        LoseBodyText->SetJustification(ETextJustify::Center);
        LoseBodyText->SetAutoWrapText(false);
    }
    PinEndScreen(LoseBodyText, LoseRestartButton);
}

// ?????????????????????????????????????????????????????????????????????????????
//  Push Feedback Popup
// ?????????????????????????????????????????????????????????????????????????????

void UBoulderHUD::ShowPushFeedback(EQTERating Rating, EErgPowerZone Zone,
                                    float RepPower, int32 Combo)
{
    if (!FeedbackText) return;

    bool bPerfect = (Rating == EQTERating::Perfect);

    FString ZoneLabel;
    switch (Zone)
    {
        case EErgPowerZone::Explosive: ZoneLabel = TEXT("EXPLOSIVE"); break;
        case EErgPowerZone::Power:     ZoneLabel = TEXT("POWER");     break;
        case EErgPowerZone::Moderate:  ZoneLabel = TEXT("PUSH");      break;
        default:                       ZoneLabel = TEXT("weak...");   break;
    }

    FString Label = bPerfect
        ? FString::Printf(TEXT("PERFECT  %s!"), *ZoneLabel)
        : FString::Printf(TEXT("%s!"), *ZoneLabel);

    if (Combo > 2)
        Label = FString::Printf(TEXT("x%d  %s"), Combo, *Label);

    FeedbackText->SetText(FText::FromString(Label));

    // Color: blend zone color toward perfect green if it was perfect
    FLinearColor C = ZoneColor(Zone);
    if (bPerfect)
        C = FLinearColor::LerpUsingHSV(C, ColorPerfect, 0.5f);
    C.A = 1.f;
    FeedbackColor = C;
    FeedbackText->SetColorAndOpacity(FSlateColor(C));

    // Capture current render translation as the animation start so the float
    // begins from wherever the text actually sits, not always from Y=0.
    FeedbackStartY = FeedbackText->GetRenderTransform().Translation.Y;
    // Inline reset: restore translation and kick the timer without calling
    // ResetFeedbackPopup(), which would overwrite FeedbackStartY.
    FeedbackText->SetRenderTranslation(FVector2D(0.f, FeedbackStartY));
    FeedbackTimer = FeedbackDuration;
}

// ?????????????????????????????????????????????????????????????????????????????
//  Gameplay UI Update  (called every tick while Playing)
// ?????????????????????????????????????????????????????????????????????????????

void UBoulderHUD::UpdateGameplayUI(float DeltaTime)
{
    if (!GameMode) return;

    const float ElapsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    // ?? QTE Timing Bar ????????????????????????????????????????????????????????

    bool  bWindowOpen = GameMode->IsQTEWindowOpen();
    float CycleProg   = GameMode->GetQTECycleProgress();

    if (QteCycleTrack)
    {
        QteCycleTrack->SetPercent(CycleProg);
        QteCycleTrack->SetFillColorAndOpacity(bWindowOpen ? ColorWindowOpen : ColorWindowClosed);
    }

    if (QteWindowFlash)
    {
        if (bWindowOpen)
        {
            float Pulse = 1.f + 0.07f * FMath::Sin(ElapsedTime * 10.f);
            QteWindowFlash->SetRenderScale(FVector2D(Pulse, Pulse));
            QteWindowFlash->SetColorAndOpacity(ColorWindowOpen);
        }
        else
        {
            QteWindowFlash->SetRenderScale(FVector2D(1.f, 1.f));
            QteWindowFlash->SetColorAndOpacity(ColorWindowClosed);
        }
    }

    if (QteStatusText)
    {
        QteStatusText->SetText(FText::FromString(bWindowOpen
            ? TEXT("PUSH NOW!") : TEXT("WAIT...")));
        QteStatusText->SetColorAndOpacity(FSlateColor(bWindowOpen
            ? ColorPerfect : FLinearColor::Gray));
    }

    // ?? Boulder Progress ??????????????????????????????????????????????????????
    // BoulderActor::GetProgress() is exposed via the game mode's Boulder ref.
    // We grab it through the game mode to avoid a hard dependency here.
    float BoulderProg = 0.f;
    if (GameMode->Boulder)
        BoulderProg = GameMode->Boulder->GetProgress();

    if (ProgressBar)   ProgressBar->SetPercent(BoulderProg);
    if (ProgressLabel)  ProgressLabel->SetText(
        FText::FromString(FString::Printf(TEXT("%.0f%%"), BoulderProg * 100.f)));

    // ?? Timer ?????????????????????????????????????????????????????????????????

    if (TimerText)
    {
        float T = GameMode->GetTimeRemaining();
        TimerText->SetText(FormatTime(T));
        TimerText->SetColorAndOpacity(FSlateColor(T < 15.f
            ? TimerWarning : FLinearColor::White));
    }

    // ?? Reps / Combo ??????????????????????????????????????????????????????????

    if (RepCountText)
        RepCountText->SetText(
            FText::FromString(FString::Printf(TEXT("Reps: %d"), GameMode->GetTotalReps())));

    if (ComboText)
    {
        int32 C = GameMode->GetCombo();
        bool bShowCombo = C > 1;
        ComboText->SetVisibility(bShowCombo
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

        if (bShowCombo)
        {
            ComboText->SetText(
                FText::FromString(FString::Printf(TEXT("COMBO  x%d"), C)));
            float Glow   = FMath::Sin(ElapsedTime * 6.f) * 0.15f + 0.85f;
            FLinearColor CC = FLinearColor::LerpUsingHSV(
                FLinearColor::White, ColorPerfect,
                FMath::Min(C / 6.f, 1.f));
            CC.A = Glow;
            ComboText->SetColorAndOpacity(FSlateColor(CC));
        }
    }

    // ?? Power Zone ????????????????????????????????????????????????????????????

    if (PowerZoneText && GameMode->GetTotalReps() > 0)
    {
        FString ZoneName;
        switch (GameMode->GetCurrentZone())
        {
            case EErgPowerZone::Explosive: ZoneName = TEXT("EXPLOSIVE");  break;
            case EErgPowerZone::Power:     ZoneName = TEXT("POWER ZONE"); break;
            case EErgPowerZone::Moderate:  ZoneName = TEXT("MODERATE");   break;
            default:                       ZoneName = TEXT("LOW");        break;
        }
        PowerZoneText->SetText(FText::FromString(
            FString::Printf(TEXT("%s  %.1f"), *ZoneName, GameMode->GetCurrentPower())));
        PowerZoneText->SetColorAndOpacity(FSlateColor(ZoneColor(GameMode->GetCurrentZone())));
    }

    // ?? ERG Status ????????????????????????????????????????????????????????????

    if (ErgStatusText)
    {
        // Use the already-cached ErgComp on the GameMode — avoids a per-frame
        // component search and fixes the "SIM MODE" fallback when the component
        // lives on the GameMode Blueprint rather than the GameState.
        UErgManagerComponent* Erg = GameMode ? GameMode->ErgComp : nullptr;

        if (Erg)
        {
            const FErgData Data = Erg->GetCurrentData();
            FString Txt = Data.bIsConnected
                ? FString::Printf(TEXT("ERG  %s"),
                    Data.HeartRate > 0
                        ? *FString::Printf(TEXT("%d bpm"), Data.HeartRate)
                        : TEXT("connected"))
                : TEXT("ERG  disconnected");
            ErgStatusText->SetText(FText::FromString(Txt));
            ErgStatusText->SetColorAndOpacity(FSlateColor(
                Data.bIsConnected ? FLinearColor::Green : FLinearColor::Red));
        }
        else
        {
            ErgStatusText->SetText(FText::FromString(TEXT("SIM MODE")));
            ErgStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));
        }
    }
}

// ?????????????????????????????????????????????????????????????????????????????
//  Feedback Popup Tick
// ?????????????????????????????????????????????????????????????????????????????

void UBoulderHUD::TickFeedbackPopup(float DeltaTime)
{
    if (FeedbackTimer <= 0.f || !FeedbackText) return;

    FeedbackTimer -= DeltaTime;

    // Float upward using render translation
    float T         = 1.f - (FeedbackTimer / FeedbackDuration);   // 0?1 over lifetime
    float OffsetY   = -(T * FeedbackFloatDistance);                // negative = upward in Slate
    FeedbackText->SetRenderTranslation(FVector2D(0.f, FeedbackStartY + OffsetY));

    // Fade out over last 45% of duration
    FLinearColor C  = FeedbackColor;
    C.A = FMath::Clamp(FeedbackTimer / (FeedbackDuration * 0.45f), 0.f, 1.f);
    FeedbackText->SetColorAndOpacity(FSlateColor(C));

    if (FeedbackTimer <= 0.f)
        ResetFeedbackPopup();
}

void UBoulderHUD::ResetFeedbackPopup()
{
    FeedbackTimer  = 0.f;
    FeedbackStartY = 0.f;

    if (FeedbackText)
    {
        FeedbackText->SetRenderTranslation(FVector2D::ZeroVector);
        FeedbackText->SetText(FText::GetEmpty());
        FLinearColor C = FeedbackColor;
        C.A = 0.f;
        FeedbackText->SetColorAndOpacity(FSlateColor(C));
    }
}

// ?????????????????????????????????????????????????????????????????????????????
//  Helpers
// ?????????????????????????????????????????????????????????????????????????????

void UBoulderHUD::HideAllPanels()
{
    auto Hide = [](UWidget* W) { if (W) W->SetVisibility(ESlateVisibility::Collapsed); };
    Hide(StartPanel);
    Hide(CountdownPanel);
    Hide(GameplayPanel);
    Hide(WinPanel);
    Hide(LosePanel);
}

FLinearColor UBoulderHUD::ZoneColor(EErgPowerZone Zone) const
{
    switch (Zone)
    {
        case EErgPowerZone::Explosive: return ZoneExplosive;
        case EErgPowerZone::Power:     return ZonePower;
        case EErgPowerZone::Moderate:  return ZoneModerate;
        default:                       return ZoneLow;
    }
}

FText UBoulderHUD::FormatTime(float Seconds)
{
    int32 M = FMath::FloorToInt(Seconds / 60.f);
    float S = FMath::Fmod(Seconds, 60.f);
    return FText::FromString(FString::Printf(TEXT("%02d:%04.1f"), M, S));
}

// ?????????????????????????????????????????????????????????????????????????????
//  Button Handlers
// ?????????????????????????????????????????????????????????????????????????????

void UBoulderHUD::OnStartClicked()
{
    if (GameMode) GameMode->StartGame();
}

void UBoulderHUD::OnWinRestartClicked()
{
    if (GameMode) GameMode->RestartGame();
}

void UBoulderHUD::OnLoseRestartClicked()
{
    if (GameMode) GameMode->RestartGame();
}
