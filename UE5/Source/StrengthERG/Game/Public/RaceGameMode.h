#pragma once
/**
 * RaceGameMode.h
 *
 * Slice 1 � Race Map Foundation
 *
 * Minimal game mode for the three-lane PM5 race prototype.
 * Tracks whether the race is running and which lane won.
 *
 * Wire-up:
 *   - Set this as the Game Mode in your RaceMap World Settings.
 *   - Blueprint-subclass it (BP_RaceGameMode) to implement OnRaceWon for HUD.
 *   - ARaceFinishActor calls NotifyLaneFinished() on overlap.
 */

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ErgTypes.h"
#include "RaceGameMode.generated.h"

class URaceHUD;
class UUserWidget;

UCLASS(Blueprintable, meta = (DisplayName = "Race Game Mode"))
class STRENGTHERG_API ARaceGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARaceGameMode();

    virtual void BeginPlay() override;

    // ?? State ?????????????????????????????????????????????????????????????

    /** True once the finish trigger has been hit by any lane. */
    UPROPERTY(BlueprintReadOnly, Category = "Race")
    bool bRaceFinished = false;

    /** Channel that crossed the finish first (valid only when bRaceFinished). */
    UPROPERTY(BlueprintReadOnly, Category = "Race")
    EDeviceChannel WinnerChannel = EDeviceChannel::Strength;

    // ?? Called by ARaceFinishActor ?????????????????????????????????????????

    /**
     * Called the first time any lane actor overlaps the finish trigger.
     * Safe to call multiple times � only the first call counts.
     */
    UFUNCTION(BlueprintCallable, Category = "Race")
    void NotifyLaneFinished(EDeviceChannel Channel);

    // ?? Blueprint event � override in BP_RaceGameMode for HUD ?????????????

    /**
     * Fired once when a winner is determined.
     * Override in Blueprint to show the winner banner / freeze the race.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Race")
    void OnRaceWon(EDeviceChannel Winner);

    UFUNCTION(BlueprintCallable, Category = "Race|HUD")
    void UpdateLaneHud(EDeviceChannel Channel, bool bConnected, float NormalisedSpeed, const FString& Detail);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|HUD")
    TSubclassOf<UUserWidget> RaceHUDClass;

    UPROPERTY(BlueprintReadOnly, Category = "Race|HUD")
    TObjectPtr<URaceHUD> RaceHUD;
};
