#include "RaceHUD.h"
#include "RaceHUD.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

// -- Colour palette --------------------------------------------------------
static const FLinearColor ColConnected    = FLinearColor(0.2f,  1.0f,  0.4f,  1.f);
static const FLinearColor ColDisconnected = FLinearColor(0.55f, 0.55f, 0.55f, 1.f);
static const FLinearColor ColWinner       = FLinearColor(1.0f,  0.85f, 0.0f,  1.f);
static const FLinearColor ColCountdown    = FLinearColor(1.0f,  0.95f, 0.95f, 1.f);

// -- NativeConstruct -------------------------------------------------------

void URaceHUD::NativeConstruct()
{
    // Guard: IsTemplate() is true for CDOs and archetype objects.
    // During Hot Reload the engine re-creates CDOs while iterating the UObject
    // hash table; any further UObject creation here would crash (FindOrAdd
    // during iteration). Real PIE instances have IsTemplate() == false.
    if (IsTemplate()) return;

    Super::NativeConstruct();

    // Build the widget tree if it hasn't been built yet.
    // BuildLayout creates a root CanvasPanel and six TextBlock children with
    // intentional centered screen positions. It replaces whatever the Blueprint
    // designer placed in WBP_RaceHUD, so no editor widget-placement is needed.
    if (!TitleText)
        BuildLayout();

    ShowWaiting(false, false, false);
}

// -- Layout builder --------------------------------------------------------

void URaceHUD::BuildLayout()
{
    if (!WidgetTree) return;

    // Create a full-screen root canvas panel.
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;

    // -- Title: top-center -------------------------------------------------
    TitleText = MakeText(Canvas,
        /*Offset=*/FVector2D(0.f, 36.f), /*Anchor=*/FVector2D(0.5f, 0.0f),
        TEXT("First to the Flag"), 32, /*bBold=*/true);
    TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

    // -- Lane status lines: centered, below title --------------------------
    CyclingText  = MakeText(Canvas, FVector2D(0.f, 108.f), FVector2D(0.5f, 0.f), TEXT(""), 20);
    RowingText   = MakeText(Canvas, FVector2D(0.f, 142.f), FVector2D(0.5f, 0.f), TEXT(""), 20);
    StrengthText = MakeText(Canvas, FVector2D(0.f, 176.f), FVector2D(0.5f, 0.f), TEXT(""), 20);

    // -- Winner: screen center, large gold, hidden until race ends ---------
    WinnerText = MakeText(Canvas, FVector2D(0.f, -80.f), FVector2D(0.5f, 0.5f), TEXT(""), 56, true);
    WinnerText->SetColorAndOpacity(FSlateColor(ColWinner));
    WinnerText->SetVisibility(ESlateVisibility::Collapsed);

    // -- Countdown: screen center, very large, hidden until countdown ------
    CountdownText = MakeText(Canvas, FVector2D(0.f, 0.f), FVector2D(0.5f, 0.5f), TEXT(""), 96, true);
    CountdownText->SetColorAndOpacity(FSlateColor(ColCountdown));
    CountdownText->SetVisibility(ESlateVisibility::Collapsed);

    UE_LOG(LogTemp, Log, TEXT("[RaceHUD] Widget tree built by C++ BuildLayout."));
}

UTextBlock* URaceHUD::MakeText(UCanvasPanel* Canvas, FVector2D Offset, FVector2D Anchor,
                                const FString& Default, int32 Size, bool bBold)
{
    UTextBlock* TB = WidgetTree->ConstructWidget<UTextBlock>();

    FSlateFontInfo Font = TB->GetFont();
    Font.Size = Size;
    if (bBold) Font.TypefaceFontName = FName("Bold");
    TB->SetFont(Font);
    TB->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    if (!Default.IsEmpty())
        TB->SetText(FText::FromString(Default));

    // Use PanelSlot to avoid shadowing the UWidget::Slot member.
    UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(TB);
    // Point anchor (Min==Max). Offset = pixel offset from that anchor point.
    // Alignment mirrors Anchor so the widget self-centres on the anchor.
    PanelSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
    PanelSlot->SetPosition(Offset);
    PanelSlot->SetAlignment(Anchor);
    PanelSlot->SetAutoSize(true);

    return TB;
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
