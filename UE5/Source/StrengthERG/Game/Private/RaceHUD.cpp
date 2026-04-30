#include "RaceHUD.h"

#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Widget.h"

// ?? Colour palette ????????????????????????????????????????????????????????????
static const FLinearColor ColConnected    = FLinearColor(0.2f,  1.0f,  0.4f,  1.f); // green
static const FLinearColor ColDisconnected = FLinearColor(0.55f, 0.55f, 0.55f, 1.f); // grey
static const FLinearColor ColWinner       = FLinearColor(1.0f,  0.85f, 0.0f,  1.f); // gold
static const FLinearColor ColTitle        = FLinearColor::White;
static const FLinearColor PanelTint       = FLinearColor(0.0f,  0.0f,  0.0f,  0.55f);

void URaceHUD::NativeConstruct()
{
    Super::NativeConstruct();

    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TEXT("First to the Flag")));
        TitleText->SetColorAndOpacity(FSlateColor(ColTitle));
    }

    if (WinnerText)
    {
        WinnerText->SetText(FText::GetEmpty());
        WinnerText->SetVisibility(ESlateVisibility::Collapsed);
        WinnerText->SetColorAndOpacity(FSlateColor(ColWinner));
    }

    if (LanePanel)
    {
        LanePanel->SetBrushColor(PanelTint);
    }

    SetLaneStatus(EDeviceChannel::Cycling,  false, 0.f, TEXT("waiting"));
    SetLaneStatus(EDeviceChannel::Rowing,   false, 0.f, TEXT("waiting"));
    SetLaneStatus(EDeviceChannel::Strength, false, 0.f, TEXT("waiting"));
}

void URaceHUD::SetLaneStatus(EDeviceChannel Channel, bool bConnected, float NormalisedSpeed, const FString& Detail)
{
    switch (Channel)
    {
    case EDeviceChannel::Cycling:
        UpdateLaneText(CyclingText,  TEXT("Bike"), bConnected, NormalisedSpeed, Detail);
        break;
    case EDeviceChannel::Rowing:
        UpdateLaneText(RowingText,   TEXT("Row"),  bConnected, NormalisedSpeed, Detail);
        break;
    case EDeviceChannel::Strength:
        UpdateLaneText(StrengthText, TEXT("Push"), bConnected, NormalisedSpeed, Detail);
        break;
    default:
        break;
    }
}

void URaceHUD::ShowWinner(EDeviceChannel WinnerChannel)
{
    if (!WinnerText) return;

    const FString WinnerName = StaticEnum<EDeviceChannel>()
        ->GetDisplayNameTextByValue((int64)WinnerChannel).ToString();
    WinnerText->SetText(FText::FromString(
        FString::Printf(TEXT("%s Wins!"), *WinnerName)));
    WinnerText->SetColorAndOpacity(FSlateColor(ColWinner));
    WinnerText->SetVisibility(ESlateVisibility::Visible);
}

void URaceHUD::UpdateLaneText(UTextBlock* Target, const FString& Label, bool bConnected, float NormalisedSpeed, const FString& Detail)
{
    if (!Target) return;

    const FString Line = bConnected
        ? FString::Printf(TEXT("%s  %.0f%%  %s"), *Label, NormalisedSpeed * 100.f, *Detail)
        : FString::Printf(TEXT("%s  --  offline"), *Label);

    Target->SetText(FText::FromString(Line));
    Target->SetColorAndOpacity(FSlateColor(bConnected ? ColConnected : ColDisconnected));
}
