#pragma once
#pragma once
/**
 * RaceHUD.h
 *
 * Binds to named TextBlock widgets already placed in WBP_RaceHUD.
 * The Blueprint designer owns layout/positioning - C++ drives data only.
 * Widget names must match the variable names below exactly (BindWidget).
 *
 * Required widgets in WBP_RaceHUD:
 *   TitleText, CyclingText, RowingText, StrengthText, WinnerText
 * Optional (add to Blueprint for countdown display):
 *   CountdownText
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

    // -- Bound to named widgets in WBP_RaceHUD ----------------------------

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CyclingText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> RowingText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> StrengthText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> WinnerText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CountdownText;

private:
    void UpdateLaneText(UTextBlock* Target, const FString& Label, bool bConnected,
                        float NormalisedSpeed, const FString& Detail);
};
