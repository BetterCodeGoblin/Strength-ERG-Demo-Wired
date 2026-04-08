#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ErgGameTypes.h"
#include "HudManager.generated.h"

class ABoulderGameMode;
class ABoulderActor;
class UQTEComponent;

/**
 * Manages all on-screen UI for the Boulder game.
 * Draws text and debug visuals to HUD canvas.
 * Listens to GameMode events and updates display accordingly.
 */
UCLASS(BlueprintType, Blueprintable)
class STRENGTHERGDEMO_API AHudManager : public AHUD
{
    GENERATED_BODY()

public:
    AHudManager();

    virtual void BeginPlay() override;
    virtual void DrawHUD() override;

    // ── Color Palette ────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorPerfect = FLinearColor(0.20f, 1.00f, 0.35f, 1.0f);     // Green

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorMiss = FLinearColor(1.00f, 0.45f, 0.10f, 1.0f);        // Orange

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorWindowOpen = FLinearColor(0.10f, 0.95f, 0.25f, 0.90f); // Green

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorWindowClosed = FLinearColor(0.90f, 0.20f, 0.15f, 0.55f); // Red

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorTimerWarning = FLinearColor(1.00f, 0.20f, 0.10f, 1.0f); // Red

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorZoneExplosive = FLinearColor(1.00f, 0.20f, 0.10f, 1.0f); // Red

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorZonePower = FLinearColor(1.00f, 0.60f, 0.10f, 1.0f);     // Orange

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorZoneModerate = FLinearColor(0.95f, 0.90f, 0.10f, 1.0f);  // Yellow

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Colors")
    FLinearColor ColorZoneLow = FLinearColor(0.70f, 0.70f, 0.70f, 1.0f);       // Grey

private:
    // References
    TObjectPtr<ABoulderGameMode> GameMode;
    TObjectPtr<ABoulderActor> Boulder;
    TObjectPtr<UQTEComponent> QTE;

    // Game state
    EBoulderGameState CurrentState = EBoulderGameState::Idle;
    float TimeRemaining = 0.f;
    int32 TotalReps = 0;
    int32 Combo = 0;
    int32 PerfectCount = 0;
    float CurrentPower = 0.f;
    EPowerZone CurrentZone = EPowerZone::Low;
    float BoulderProgress = 0.f;
    float AveragePower = 0.f;
    float PeakPower = 0.f;

    bool bIsQTEWindowOpen = false;
    float QTECycleProgress = 0.f;

    // Countdown display
    float CountdownTimer = 0.f;

    // Push feedback popup
    struct FPushFeedback
    {
        FString Text;
        FLinearColor Color;
        float Timer = 0.f;
        const float Duration = 1.1f;
        FVector2D StartPos;
    };
    TOptional<FPushFeedback> PushFeedback;

    // Event handlers
    UFUNCTION()
    void OnGameStateChanged(EBoulderGameState NewState);

    UFUNCTION()
    void OnRepProcessed(const FRepData& Rep);

    UFUNCTION()
    void OnGameEnded(const FGameStats& Stats);

    UFUNCTION()
    void OnCountdownTick(float SecondsRemaining);

    UFUNCTION()
    void OnQTEWindowOpened();

    UFUNCTION()
    void OnQTEWindowClosed();

    // Drawing helpers
    void DrawStartScreen();
    void DrawCountdownScreen();
    void DrawGameplayHUD();
    void DrawWinScreen();
    void DrawLoseScreen();

    void DrawQTEBar();
    void DrawProgressBar();
    void DrawLiveStats();
    void DrawPushFeedback(float DeltaTime);

    FLinearColor GetZoneColor(EPowerZone Zone) const;
    FString GetZoneName(EPowerZone Zone) const;
    FString FormatTime(float Seconds) const;

    // Screen-space helpers
    void DrawCenteredText(const FString& Text, float Y, float Scale, FLinearColor Color);
    void DrawText(const FString& Text, float X, float Y, float Scale, FLinearColor Color);

    // End game stats
    struct FEndGameStats
    {
        int32 TotalReps;
        int32 PerfectCount;
        float TimeUsedSec;
        float AveragePower;
        float PeakPower;
        float FinalProgress;
        bool bWon;
    };
    TOptional<FEndGameStats> EndStats;
};
