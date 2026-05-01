#include "RaceHUD.h"
#include "Components/TextBlock.h"

// -- Colour palette --------------------------------------------------------
static const FLinearColor ColConnected    = FLinearColor(0.2f,  1.0f,  0.4f,  1.f);
static const FLinearColor ColDisconnected = FLinearColor(0.55f, 0.55f, 0.55f, 1.f);
static const FLinearColor ColWinner       = FLinearColor(1.0f,  0.85f, 0.0f,  1.f);
static const FLinearColor ColCountdown    = FLinearColor(1.0f,  0.95f, 0.95f, 1.f);

// -- NativeConstruct -------------------------------------------------------

void URaceHUD::NativeConstruct()
{
    Super::NativeConstruct();
    // Blueprint owns the widget hierarchy (WBP_RaceHUD).
    // C++ only updates the bound BindWidgetOptional references from here on.
    ShowWaiting(false, false, false);
}

// -- Public API ------------------------------------------------------------

void URaceHUD::SetLaneStatus(EDeviceChannel Channel, bool bConnected, float NormalisedSpeed, const FString& Detail)
{
    switch (Channel)
    {
    case EDeviceChannel::Cycling:
        UpdateLaneText(CyclingText,  TEXT("Bike"), bConnected, NormalisedSpeed, Detail); break;
    case EDeviceChannel::Rowing:
        UpdateLaneText(RowingText,   TEXT("Row"),  bConnected, NormalisedSpeed, Detail); break;
    case EDeviceChannel::Strength:
        UpdateLaneText(StrengthText, TEXT("Push"), bConnected, NormalisedSpeed, Detail); break;
    default: break;
    }
}

void URaceHUD::ShowWinner(EDeviceChannel WinnerChannel)
{
    if (!WinnerText) return;

    const FString Name = StaticEnum<EDeviceChannel>()
        ->GetDisplayNameTextByValue((int64)WinnerChannel).ToString();
    WinnerText->SetText(FText::FromString(FString::Printf(TEXT("%s Wins!"), *Name)));
    WinnerText->SetColorAndOpacity(FSlateColor(ColWinner));
    WinnerText->SetVisibility(ESlateVisibility::Visible);
}

void URaceHUD::ShowWaiting(bool bCyclingReady, bool bRowingReady, bool bStrengthReady)
{
    auto ReadyStr = [](const FString& Label, bool bReady) -> FString
    {
        return FString::Printf(TEXT("%s  %s"), *Label, bReady ? TEXT("Ready") : TEXT("Waiting..."));
    };

    auto SetReady = [](UTextBlock* TB, const FString& Text, bool bReady)
    {
        if (!TB) return;
        TB->SetText(FText::FromString(Text));
        TB->SetColorAndOpacity(FSlateColor(bReady ? ColConnected : ColDisconnected));
    };

    SetReady(CyclingText,  ReadyStr(TEXT("Bike"), bCyclingReady),  bCyclingReady);
    SetReady(RowingText,   ReadyStr(TEXT("Row"),  bRowingReady),   bRowingReady);
    SetReady(StrengthText, ReadyStr(TEXT("Push"), bStrengthReady), bStrengthReady);

    if (CountdownText)
    {
        const bool bAllReady = bCyclingReady && bRowingReady && bStrengthReady;
        CountdownText->SetText(FText::FromString(bAllReady ? TEXT("") : TEXT("Waiting for all devices...")));
        CountdownText->SetColorAndOpacity(FSlateColor(ColDisconnected));
        CountdownText->SetVisibility(ESlateVisibility::Visible);
    }
}

void URaceHUD::ShowCountdown(int32 Value)
{
    if (!CountdownText) return;
    const FString Str = (Value <= 0) ? TEXT("GO!") : FString::Printf(TEXT("%d"), Value);
    CountdownText->SetText(FText::FromString(Str));
    CountdownText->SetColorAndOpacity(FSlateColor(Value <= 0 ? ColConnected : ColCountdown));
    CountdownText->SetVisibility(ESlateVisibility::Visible);
}

void URaceHUD::ShowRaceLive()
{
    if (CountdownText)
        CountdownText->SetVisibility(ESlateVisibility::Collapsed);
}

// -- Private ---------------------------------------------------------------

void URaceHUD::UpdateLaneText(UTextBlock* Target, const FString& Label, bool bConnected,
                               float NormalisedSpeed, const FString& Detail)
{
    if (!Target) return;

    const FString Line = bConnected
        ? FString::Printf(TEXT("%s  %.0f%%  %s"), *Label, NormalisedSpeed * 100.f, *Detail)
        : FString::Printf(TEXT("%s  --  offline"), *Label);

    Target->SetText(FText::FromString(Line));
    Target->SetColorAndOpacity(FSlateColor(bConnected ? ColConnected : ColDisconnected));
}
