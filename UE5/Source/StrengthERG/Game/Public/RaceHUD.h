#pragma once
#pragma once
/**
 * RaceHUD.h
 *
 * Race HUD — layout built entirely in C++ so no manual UMG positioning is needed.
 * The WBP_RaceHUD asset just needs URaceHUD set as its parent class.
 */

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ErgTypes.h"
#include "RaceHUD.generated.h"

class UTextBlock;
class UBorder;
class UCanvasPanel;
class UVerticalBox;

UCLASS(Blueprintable, BlueprintType)
class STRENGTHERG_API URaceHUD : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "RaceHUD")
    void SetLaneStatus(EDeviceChannel Channel, bool bConnected, float NormalisedSpeed, const FString& Detail);

    UFUNCTION(BlueprintCallable, Category = "RaceHUD")
    void ShowWinner(EDeviceChannel WinnerChannel);

protected:
    virtual void NativeConstruct() override;

private:
    // Built at construct time
    TObjectPtr<UTextBlock> TitleText;
    TObjectPtr<UTextBlock> CyclingText;
    TObjectPtr<UTextBlock> RowingText;
    TObjectPtr<UTextBlock> StrengthText;
    TObjectPtr<UTextBlock> WinnerText;

    void BuildLayout();
    UTextBlock* MakeText(UCanvasPanel* Canvas, FVector2D Position, FVector2D Alignment,
                         const FString& DefaultStr, int32 FontSize, bool bBold = false);
    void UpdateLaneText(UTextBlock* Target, const FString& Label, bool bConnected,
                        float NormalisedSpeed, const FString& Detail);
};
