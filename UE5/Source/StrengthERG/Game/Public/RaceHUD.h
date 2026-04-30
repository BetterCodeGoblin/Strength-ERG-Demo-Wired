#pragma once
#pragma once
/**
 * RaceHUD.h
 *
 * C++ owns the full widget layout: BuildLayout() constructs a root CanvasPanel
 * and six TextBlock children at their intended centered positions.
 * WBP_RaceHUD only needs URaceHUD set as its parent class -- no widget
 * placement required in the Blueprint designer.
 *
 * The five public functions drive all runtime data:
 *   ShowWaiting / ShowCountdown / ShowRaceLive / ShowWinner / SetLaneStatus
 */

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ErgTypes.h"
#include "RaceHUD.generated.h"

class UTextBlock;
class UCanvasPanel;

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
    // Widget pointers -- owned by BuildLayout(), tracked by GC via UPROPERTY.
    // NOT BindWidget: C++ constructs these directly so no editor placement needed.
    UPROPERTY()
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY()
    TObjectPtr<UTextBlock> CyclingText;

    UPROPERTY()
    TObjectPtr<UTextBlock> RowingText;

    UPROPERTY()
    TObjectPtr<UTextBlock> StrengthText;

    UPROPERTY()
    TObjectPtr<UTextBlock> WinnerText;

    UPROPERTY()
    TObjectPtr<UTextBlock> CountdownText;

    // Builds the full widget tree. Idempotent -- only runs if TitleText is null.
    void BuildLayout();
    UTextBlock* MakeText(UCanvasPanel* Canvas, FVector2D Offset, FVector2D Anchor,
                         const FString& Default, int32 Size, bool bBold = false);

    void UpdateLaneText(UTextBlock* Target, const FString& Label, bool bConnected,
                        float NormalisedSpeed, const FString& Detail);
};
