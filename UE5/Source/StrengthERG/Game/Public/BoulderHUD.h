#pragma once

/**
 * BoulderHUD.h
 *
 * UE5 equivalent of Unity's BoulderHUD.cs.
 *
 * Architecture notes:
 *   - This is a UUserWidget subclass.  Create a WBP_BoulderHUD Blueprint
 *     child class in the editor and bind every widget property to the
 *     matching BindWidget variable below.
 *   - The widget is added to the viewport by ABoulderGameMode::BeginPlay.
 *   - NativeTick drives all per-frame updates (QTE bar, timer, combo glow).
 *   - ShowXxx / UpdateXxx functions are called directly by ABoulderGameMode
 *     when the game-state machine changes.
 *   - BindWidget meta tag makes the UMG editor enforce that a widget with
 *     exactly the same variable name exists in the Blueprint layout.
 */

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ErgTypes.h"
#include "BoulderHUD.generated.h"

// Forward declarations
class UTextBlock;
class UButton;
class UProgressBar;
class UImage;
class UWidget;
class UCanvasPanel;
class ABoulderGameMode;

UCLASS(Blueprintable, BlueprintType)
class STRENGTHERG_API UBoulderHUD : public UUserWidget
{
    GENERATED_BODY()

public:

    // ?? Panels ????????????????????????????????????????????????????????????????
    // Each panel is a named widget (e.g. a CanvasPanel or Overlay) in the WBP.

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> StartPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> CountdownPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> GameplayPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> WinPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> LosePanel;

    // ?? Start Screen ??????????????????????????????????????????????????????????

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StartTitleText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Startbutton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StartButtonText;

    // ?? Countdown ?????????????????????????????????????????????????????????????

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CountdownText;

    // ?? QTE Timing Bar ????????????????????????????????????????????????????????

    /** Horizontal progress bar; percent = QTE cycle progress (0-1). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> QteCycleTrack;

    /** Small image / panel that pulses green when the window is open. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> QteWindowFlash;

    /** "PUSH NOW!" / "WAIT..." label. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> QteStatusText;

    // ?? Boulder Progress ??????????????????????????????????????????????????????

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> ProgressBar;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ProgressLabel;

    // ?? In-Game Stats ?????????????????????????????????????????????????????????

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TimerText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> RepCountText;

    /** Combo multiplier label – hidden when combo <= 1. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ComboText;

    /** ERG connection status (e.g. "ERG  connected" / "SIM MODE"). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ErgStatusText;

    /** Current power-zone label (e.g. "EXPLOSIVE  82.3"). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PowerZoneText;

    // ?? Push Feedback Popup ???????????????????????????????????????????????????

    /** Floating "PERFECT!" / "PUSH!" label. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> FeedbackText;

    // ?? End Screens ???????????????????????????????????????????????????????????

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> WinBodyText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> WinRestartButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> LoseBodyText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> LoseRestartButton;

    // ?? Colors (editable in Blueprint defaults) ???????????????????????????????

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorPerfect      = FLinearColor(0.20f, 1.00f, 0.35f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorMiss         = FLinearColor(1.00f, 0.45f, 0.10f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorWindowOpen   = FLinearColor(0.10f, 0.95f, 0.25f, 0.90f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorWindowClosed = FLinearColor(0.90f, 0.20f, 0.15f, 0.55f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor TimerWarning      = FLinearColor(1.00f, 0.20f, 0.10f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ZoneExplosive     = FLinearColor(1.00f, 0.20f, 0.10f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ZonePower         = FLinearColor(1.00f, 0.60f, 0.10f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ZoneModerate      = FLinearColor(0.95f, 0.90f, 0.10f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ZoneLow           = FLinearColor(0.70f, 0.70f, 0.70f, 1.f);

    // ?? Feedback animation settings ???????????????????????????????????????????

    /** Duration (seconds) the feedback popup is visible before fading out. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Feedback")
    float FeedbackDuration = 1.1f;

    /** How far (in Slate units) the popup floats upward over its lifetime. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Feedback")
    float FeedbackFloatDistance = 80.f;

    // ?? Panel Switchers (called by ABoulderGameMode) ??????????????????????????

    UFUNCTION(BlueprintCallable, Category = "BoulderHUD")
    void ShowStartScreen();

    UFUNCTION(BlueprintCallable, Category = "BoulderHUD")
    void ShowCountdown();

    UFUNCTION(BlueprintCallable, Category = "BoulderHUD")
    void UpdateCountdown(float Remaining);

    UFUNCTION(BlueprintCallable, Category = "BoulderHUD")
    void ShowGameplay();

    UFUNCTION(BlueprintCallable, Category = "BoulderHUD")
    void ShowWinScreen(int32 Reps, int32 Perfects, float TimeUsed,
                       float AvgPower, float PeakPower);

    UFUNCTION(BlueprintCallable, Category = "BoulderHUD")
    void ShowLoseScreen(float Progress, int32 Reps,
                        float AvgPower, float PeakPower);

    // ?? Push Feedback (called by ABoulderGameMode on each rep) ????????????????

    UFUNCTION(BlueprintCallable, Category = "BoulderHUD")
    void ShowPushFeedback(EQTERating Rating, EErgPowerZone Zone,
                          float RepPower, int32 Combo);

protected:

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

    // Cached reference to the game mode, resolved once in NativeConstruct.
    UPROPERTY()
    TObjectPtr<ABoulderGameMode> GameMode;

    // ?? Feedback popup state ??????????????????????????????????????????????????

    float FeedbackTimer       = 0.f;
    float FeedbackStartY      = 0.f;   // render translation Y at pop start
    FLinearColor FeedbackColor = FLinearColor::White;

    // ?? Private helpers ???????????????????????????????????????????????????????

    void HideAllPanels();
    void SetupLayout();
    void UpdateGameplayUI(float DeltaTime);
    void TickFeedbackPopup(float DeltaTime);
    void ResetFeedbackPopup();

    FLinearColor ZoneColor(EErgPowerZone Zone) const;
    static FText FormatTime(float Seconds);

    // Button click handlers
    UFUNCTION()
    void OnStartClicked();

    UFUNCTION()
    void OnWinRestartClicked();

    UFUNCTION()
    void OnLoseRestartClicked();
};
