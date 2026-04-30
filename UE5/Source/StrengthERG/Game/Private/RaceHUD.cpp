#include "RaceHUD.h"

#include "RaceHUD.h"

#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

// ?? Colour palette ????????????????????????????????????????????????????????????
static const FLinearColor ColConnected    = FLinearColor(0.2f,  1.0f,  0.4f,  1.f);
static const FLinearColor ColDisconnected = FLinearColor(0.55f, 0.55f, 0.55f, 1.f);
static const FLinearColor ColWinner       = FLinearColor(1.0f,  0.85f, 0.0f,  1.f);
static const FLinearColor ColCountdown    = FLinearColor(1.0f,  0.95f, 0.95f, 1.f);
static const FLinearColor PanelTint       = FLinearColor(0.0f,  0.0f,  0.0f,  0.55f);

// ?? Helpers ???????????????????????????????????????????????????????????????????

UTextBlock* URaceHUD::MakeText(UCanvasPanel* Canvas, FVector2D Position, FVector2D Alignment,
                                const FString& DefaultStr, int32 FontSize, bool bBold)
{
    UTextBlock* TB = WidgetTree->ConstructWidget<UTextBlock>();
    TB->SetText(FText::FromString(DefaultStr));

    FSlateFontInfo Font = TB->GetFont();
    Font.Size = FontSize;
    if (bBold)
        Font.TypefaceFontName = FName("Bold");
    TB->SetFont(Font);
    TB->SetColorAndOpacity(FSlateColor(FLinearColor::White));

    UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(TB);
    CanvasSlot->SetAnchors(FAnchors(Alignment.X, Alignment.Y));
    CanvasSlot->SetPosition(Position);
    CanvasSlot->SetAlignment(Alignment);
    CanvasSlot->SetAutoSize(true);

    return TB;
}

void URaceHUD::BuildLayout()
{
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;

    // -- Title: top-center ------------------------------------------------
    TitleText = MakeText(Canvas, FVector2D(0.f, 36.f), FVector2D(0.5f, 0.f),
                         TEXT("First to the Flag"), 32, /*bBold=*/true);

    // -- Backing panel: horizontally centered below the title -------------
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrushColor(PanelTint);
    UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(0.5f, 0.f));
    PanelSlot->SetPosition(FVector2D(-230.f, 96.f));
    PanelSlot->SetSize(FVector2D(460.f, 116.f));
    PanelSlot->SetAlignment(FVector2D(0.f, 0.f));

    // -- Lane status lines: centered top ----------------------------------
    CyclingText  = MakeText(Canvas, FVector2D(0.f, 106.f), FVector2D(0.5f, 0.f),
                            TEXT("Bike  --  waiting"), 20);
    RowingText   = MakeText(Canvas, FVector2D(0.f, 140.f), FVector2D(0.5f, 0.f),
                            TEXT("Row   --  waiting"), 20);
    StrengthText = MakeText(Canvas, FVector2D(0.f, 174.f), FVector2D(0.5f, 0.f),
                            TEXT("Push  --  waiting"), 20);

    // -- Countdown text: screen center, large, hidden until needed --------
    CountdownText = MakeText(Canvas, FVector2D(0.f, 0.f), FVector2D(0.5f, 0.5f),
                             TEXT(""), 96, /*bBold=*/true);
    CountdownText->SetColorAndOpacity(FSlateColor(ColCountdown));
    CountdownText->SetVisibility(ESlateVisibility::Collapsed);

    // -- Winner text: center screen, prominent, hidden until race ends ----
    WinnerText = MakeText(Canvas, FVector2D(0.f, -80.f), FVector2D(0.5f, 0.5f),
                          TEXT(""), 56, /*bBold=*/true);
    WinnerText->SetColorAndOpacity(FSlateColor(ColWinner));
    WinnerText->SetVisibility(ESlateVisibility::Collapsed);
}

// ?? NativeConstruct ???????????????????????????????????????????????????????????

void URaceHUD::NativeConstruct()
{
    Super::NativeConstruct();

    // Only build the layout if the asset doesn't already have a root widget
    // (i.e. the WBP canvas is empty — which it will be for a blank parent-class asset).
    // Always build from C++ so the Blueprint designer canvas never overrides
    // the intended layout. Any default root widget added by the asset is
    // replaced here - C++ is the single source of truth for placement.
    BuildLayout();

    ShowWaiting(false, false, false);
}

// ?? Public API ????????????????????????????????????????????????????????????????

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
