#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ErgTypes.h"
#include "RowingGameMode.generated.h"

class ARowingProgressActor;
class UErgManagerComponent;

UENUM(BlueprintType)
enum class ERowingGameState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Countdown   UMETA(DisplayName = "Countdown"),
    Playing     UMETA(DisplayName = "Playing"),
    Won         UMETA(DisplayName = "Won"),
    Lost        UMETA(DisplayName = "Lost")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRowingWon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRowingLost);

/**
 * Overnight rowing rescue mode built on the existing PM5 ErgBridge path.
 *
 * Uses generic ERG telemetry already provided by UErgManagerComponent:
 * - RepTimeSec is currently populated from the bridge's first CSV field
 * - PullDistance is currently populated from the bridge's power field
 * - LatestData.ElapsedSeconds is currently populated from the bridge's pace field
 *
 * For wired rowing bridge data, this effectively gives us:
 * - SPM via RepTimeSec field
 * - Power via PullDistance field
 * - Pace (/500) via ElapsedSeconds field
 */
UCLASS()
class STRENGTHERG_API ARowingGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARowingGameMode();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene")
    ARowingProgressActor* ProgressActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")
    float CountdownDuration = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")
    float TimeLimitSeconds = 180.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")
    bool bAutoStartOnFirstStroke = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing")
    float MetersPerWatt = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing")
    float MinSPMForProgress = 14.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing")
    float DriftDelaySec = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing")
    float DriftMetersPerSecond = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cadence Bonus")
    float IdealLowSPM = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cadence Bonus")
    float IdealHighSPM = 28.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cadence Bonus")
    float IdealLowPaceSeconds = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cadence Bonus")
    float IdealHighPaceSeconds = 180.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cadence Bonus")
    float CadenceBonusMultiplier = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
    bool bSimulateInput = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation", meta = (EditCondition = "bSimulateInput"))
    float SimSPM = 22.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation", meta = (EditCondition = "bSimulateInput"))
    float SimPaceSeconds = 160.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation", meta = (EditCondition = "bSimulateInput"))
    float SimPowerWatts = 150.f;

    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnRowingWon OnRowingWon;

    UPROPERTY(BlueprintAssignable, Category = "Game Events")
    FOnRowingLost OnRowingLost;

    UFUNCTION(BlueprintCallable, Category = "Game")
    void StartGame();

    UFUNCTION(BlueprintCallable, Category = "Game")
    void RestartGame();

    UFUNCTION(BlueprintCallable, Category = "Game")
    ERowingGameState GetState() const { return State; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetTimeRemaining() const { return TimeRemaining; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    int32 GetStrokeCount() const { return StrokeCount; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetCurrentSPM() const { return CurrentSPM; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetCurrentPaceSeconds() const { return CurrentPaceSeconds; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetCurrentPowerWatts() const { return CurrentPowerWatts; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetPeakPowerWatts() const { return PeakPowerWatts; }

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetProgressMeters() const;

    UFUNCTION(BlueprintCallable, Category = "Game")
    float GetProgressNormalized() const;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    ERowingGameState State = ERowingGameState::Idle;
    float TimeRemaining = 0.f;
    float CountdownTimer = 0.f;
    float TimeSinceLastStroke = 999.f;
    int32 StrokeCount = 0;
    float CurrentSPM = 0.f;
    float CurrentPaceSeconds = 0.f;
    float CurrentPowerWatts = 0.f;
    float PeakPowerWatts = 0.f;

    UErgManagerComponent* ErgComp = nullptr;

    void ChangeState(ERowingGameState Next);
    void EndGame(bool bWon);
    float EvaluateCadenceBonus(float SPM, float PaceSeconds) const;
    void ApplyStroke(float SPM, float PaceSeconds, float PowerWatts);

    UFUNCTION()
    void HandleNewRep(int32 RepNumber, float RepTimeSec, int32 PullDistance);

    UFUNCTION()
    void HandleErgDataUpdated(FErgData Data);
};
