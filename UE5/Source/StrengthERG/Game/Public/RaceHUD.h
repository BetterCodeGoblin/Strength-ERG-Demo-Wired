#pragma once
/**
 * RaceHUD.h
 *
 * Blueprint owns the widget hierarchy (WBP_RaceHUD in Content/HUD/).
 * C++ only binds to existing named widgets and updates their text/visibility/color.
 *
 * Required named widgets in WBP_RaceHUD:
 *   TitleText, CyclingText, RowingText, StrengthText, WinnerText, CountdownText
 *   (all UTextBlock, BindWidgetOptional so missing ones degrade gracefully)
 *
 * Public API:
 *   ShowWaiting / ShowCountdown / ShowRaceLive / ShowWinner / SetLaneStatus
 */

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ErgTypes.h"
#include "RaceHUD.generated.h"

class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class STRENGTHERG_API URaceHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "RaceHUD")
    void SetLaneStatus(EDeviceChannel Channel, bool bConnected, float NormalisedSpeed, const FString& Detail);

    UFUNCTION(BlueprintCallable, Category = "RaceHUD")
    void ShowWinner(EDeviceChannel WinnerChannel);

    UFUNCTION(BlueprintCallable, Category = "RaceHUD")
    void ShowWaiting(bool bCyclingReady, bool bRowingReady, bool bStrengthReady);

    UFUNCTION(BlueprintCallable, Category = "RaceHUD")
    void ShowCountdown(int32 Value);

    UFUNCTION(BlueprintCallable, Category = "RaceHUD")
    void ShowRaceLive();

protected:
    virtual void NativeConstruct() override;

private:
    // Bound to named widgets placed in WBP_RaceHUD by the Blueprint designer.
    // BindWidgetOptional: missing widgets log a warning instead of crashing.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CyclingText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> RowingText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> StrengthText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> WinnerText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CountdownText;

    void UpdateLaneText(UTextBlock* Target, const FString& Label, bool bConnected,
                        float NormalisedSpeed, const FString& Detail);
};

