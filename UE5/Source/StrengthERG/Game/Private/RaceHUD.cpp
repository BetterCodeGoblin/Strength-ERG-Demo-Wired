#include "RaceHUD.h"
#include "RaceHUD.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"

// -- Colour palette --------------------------------------------------------
static const FLinearColor ColConnected    = FLinearColor(0.2f,  1.0f,  0.4f,  1.f);
static const FLinearColor ColDisconnected = FLinearColor(0.55f, 0.55f, 0.55f, 1.f);
static const FLinearColor ColWinner       = FLinearColor(1.0f,  0.85f, 0.0f,  1.f);
static const FLinearColor ColCountdown    = FLinearColor(1.0f,  0.95f, 0.95f, 1.f);

// -- Helpers ---------------------------------------------------------------

static void ApplySlot(UWidget* Widget, FVector2D AnchorXY, FVector2D Position, FVector2D Alignment)
{
    if (!Widget) return;
    UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
    if (!Slot) return;
    Slot->SetAnchors(FAnchors(AnchorXY.X, AnchorXY.Y));
    Slot->SetPosition(Position);
    Slot->SetAlignment(Alignment);
    Slot->SetAutoSize(true);
}

static void ApplyFont(UTextBlock* TB, int32 Size, bool bBold = false)
{
    if (!TB) return;
    FSlateFontInfo F = TB->GetFont();
    F.Size = Size;
    if (bBold) F.TypefaceFontName = FName("Bold");
    TB->SetFont(F);
}

// -- NativeConstruct -------------------------------------------------------

void URaceHUD::NativeConstruct()
{
    // Guard: IsTemplate() is true for CDOs and archetype objects created during
    // Hot Reload / Blueprint compilation. UUserWidget CDO construction calls
    // CreateDefaultSubobject<UWidgetTree> which already touches the UObject hash;
    // any further UObject or Slate operations here while the hash iterator is
    // active will trigger the "FindOrAdd during iteration" fatal error.
    if (IsTemplate()) return;

    Super::NativeConstruct();

    // BindWidget has already resolved all pointers from WBP_RaceHUD.
    // Now enforce the intended screen positions via canvas slots so the
    // Blueprint designer's default top-left placement is overridden.

    // Title: top-center
    ApplySlot(TitleText,    FVector2D(0.5f, 0.0f), FVector2D(  0.f,  36.f), FVector2D(0.5f, 0.0f));
    ApplyFont(TitleText,    32, true);
    if (TitleText) TitleText->SetText(FText::FromString(TEXT("First to the Flag")));

    // Lane status lines: centered, below title
    ApplySlot(CyclingText,  FVector2D(0.5f, 0.0f), FVector2D(  0.f, 108.f), FVector2D(0.5f, 0.0f));
    ApplyFont(CyclingText,  20);
    ApplySlot(RowingText,   FVector2D(0.5f, 0.0f), FVector2D(  0.f, 142.f), FVector2D(0.5f, 0.0f));
    ApplyFont(RowingText,   20);
    ApplySlot(StrengthText, FVector2D(0.5f, 0.0f), FVector2D(  0.f, 176.f), FVector2D(0.5f, 0.0f));
    ApplyFont(StrengthText, 20);

    // Winner: screen center, large gold, hidden until race ends
    ApplySlot(WinnerText,   FVector2D(0.5f, 0.5f), FVector2D(  0.f, -80.f), FVector2D(0.5f, 0.5f));
    ApplyFont(WinnerText,   56, true);
    if (WinnerText)
    {
        WinnerText->SetColorAndOpacity(FSlateColor(ColWinner));
        WinnerText->SetVisibility(ESlateVisibility::Collapsed);
    }

    // Countdown: screen center, very large, hidden until countdown starts
    ApplySlot(CountdownText, FVector2D(0.5f, 0.5f), FVector2D( 0.f,   0.f), FVector2D(0.5f, 0.5f));
    ApplyFont(CountdownText, 96, true);
    if (CountdownText)
    {
        CountdownText->SetColorAndOpacity(FSlateColor(ColCountdown));
        CountdownText->SetVisibility(ESlateVisibility::Collapsed);
    }

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
