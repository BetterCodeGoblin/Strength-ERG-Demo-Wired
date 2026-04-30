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
    CanvasSlot->SetAnchors(FAnchors(Alignment.X, 0.f));
    CanvasSlot->SetPosition(Position);
    CanvasSlot->SetAlignment(FVector2D(Alignment.X, 0.f));
    CanvasSlot->SetAutoSize(true);

    return TB;
}

void URaceHUD::BuildLayout()
{
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;

    // ?? Backing panel for lane lines ??????????????????????????????????????
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrushColor(PanelTint);
    UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
    PanelSlot->SetPosition(FVector2D(30.f, 108.f));
    PanelSlot->SetSize(FVector2D(460.f, 108.f));

    // ?? Title — top center ????????????????????????????????????????????????
    TitleText = MakeText(Canvas, FVector2D(0.f, 36.f), FVector2D(0.5f, 0.f),
                         TEXT("First to the Flag"), 32, /*bBold=*/true);
    TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

    // ?? Lane status lines — top left inside the panel ?????????????????????
    CyclingText  = MakeText(Canvas, FVector2D(50.f, 118.f), FVector2D(0.f, 0.f),
                            TEXT("Bike  --  offline"), 20);
    RowingText   = MakeText(Canvas, FVector2D(50.f, 152.f), FVector2D(0.f, 0.f),
                            TEXT("Row   --  offline"), 20);
    StrengthText = MakeText(Canvas, FVector2D(50.f, 186.f), FVector2D(0.f, 0.f),
                            TEXT("Push  --  offline"), 20);

    // ?? Winner text — top center, hidden until race ends ??????????????????
    WinnerText = MakeText(Canvas, FVector2D(0.f, 88.f), FVector2D(0.5f, 0.f),
                          TEXT(""), 52, /*bBold=*/true);
    WinnerText->SetColorAndOpacity(FSlateColor(ColWinner));
    WinnerText->SetVisibility(ESlateVisibility::Collapsed);
}

// ?? NativeConstruct ???????????????????????????????????????????????????????????

void URaceHUD::NativeConstruct()
{
    Super::NativeConstruct();

    // Only build the layout if the asset doesn't already have a root widget
    // (i.e. the WBP canvas is empty — which it will be for a blank parent-class asset).
    if (!WidgetTree->RootWidget)
        BuildLayout();

    SetLaneStatus(EDeviceChannel::Cycling,  false, 0.f, TEXT("waiting"));
    SetLaneStatus(EDeviceChannel::Rowing,   false, 0.f, TEXT("waiting"));
    SetLaneStatus(EDeviceChannel::Strength, false, 0.f, TEXT("waiting"));
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
