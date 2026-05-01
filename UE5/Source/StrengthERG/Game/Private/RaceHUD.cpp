#include "RaceHUD.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"

void URaceHUD::NativeConstruct()
{
    Super::NativeConstruct();

    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TEXT("First to the Flag")));
    }

    if (WinnerText)
    {
        WinnerText->SetText(FText::GetEmpty());
        WinnerText->SetVisibility(ESlateVisibility::Collapsed);
    }

    SetLaneStatus(EDeviceChannel::Cycling, false, 0.f, TEXT("waiting"));
    SetLaneStatus(EDeviceChannel::Rowing, false, 0.f, TEXT("waiting"));
    SetLaneStatus(EDeviceChannel::Strength, false, 0.f, TEXT("waiting"));
}

void URaceHUD::SetLaneStatus(EDeviceChannel Channel, bool bConnected, float NormalisedSpeed, const FString& Detail)
{
    switch (Channel)
    {
    case EDeviceChannel::Cycling:
        UpdateLaneText(CyclingText, TEXT("Bike"), bConnected, NormalisedSpeed, Detail);
        break;
    case EDeviceChannel::Rowing:
        UpdateLaneText(RowingText, TEXT("Row"), bConnected, NormalisedSpeed, Detail);
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
    WinnerText->SetText(FText::FromString(FString::Printf(TEXT("Winner: %s"), *WinnerName)));
    WinnerText->SetVisibility(ESlateVisibility::Visible);
}

void URaceHUD::UpdateLaneText(UTextBlock* Target, const FString& Label, bool bConnected, float NormalisedSpeed, const FString& Detail)
{
    if (!Target) return;

    const FString Status = bConnected ? TEXT("connected") : TEXT("offline");
    Target->SetText(FText::FromString(FString::Printf(
        TEXT("%s: %s  speed %.2f  %s"),
        *Label,
        *Status,
        NormalisedSpeed,
        *Detail)));
}
