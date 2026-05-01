#pragma once
/**
 * RaceHUD.h
 *
 * Small race HUD for the three-lane capture-the-flag prototype.
 * Shows connection/movement values and winner text without requiring a UMG pass yet.
 */

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ErgTypes.h"
#include "RaceHUD.generated.h"

class UTextBlock;
class UWidget;

UCLASS(Blueprintable, BlueprintType)
class STRENGTHERG_API URaceHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> RootPanel;

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

    UFUNCTION(BlueprintCallable, Category = "RaceHUD")
    void SetLaneStatus(EDeviceChannel Channel, bool bConnected, float NormalisedSpeed, const FString& Detail);

    UFUNCTION(BlueprintCallable, Category = "RaceHUD")
    void ShowWinner(EDeviceChannel WinnerChannel);

protected:
    virtual void NativeConstruct() override;

private:
    void UpdateLaneText(UTextBlock* Target, const FString& Label, bool bConnected, float NormalisedSpeed, const FString& Detail);
};
