#pragma once
/**
 * RaceCoordinator.h
 *
 * Lightweight coordinator actor.  Drop ONE of these into the race level and
 * point its properties at the already-placed actors – no existing actors need
 * to be deleted or replaced.
 *
 * Wiring (Details panel):
 *   DeviceLab    ? the existing placed DeviceLabActor (telemetry source)
 *   CyclingLane  ? the existing placed RaceLaneActor for cycling
 *   RowingLane   ? the existing placed RaceLaneActor for rowing
 *   StrengthLane ? the existing placed RaceLaneActor for strength
 *
 * At BeginPlay the coordinator subscribes to DeviceLab->OnAnyDeviceUpdated
 * and converts incoming FErgData into speed/impulse calls on the lane actors
 * and HUD status updates via RaceGameMode.
 */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ErgTypes.h"
#include "RaceCoordinator.generated.h"

class ADeviceLabActor;
class ARaceLaneActor;
class ARaceGameMode;

UCLASS(Blueprintable, meta = (DisplayName = "Race Coordinator"))
class STRENGTHERG_API ARaceCoordinator : public AActor
{
    GENERATED_BODY()

public:
    ARaceCoordinator();

    // ?? Wiring ????????????????????????????????????????????????????????????

    /** The existing placed DeviceLabActor that owns the ErgManagerComponents. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Setup")
    TObjectPtr<ADeviceLabActor> DeviceLab;

    /** Existing placed RaceLaneActor that represents the cycling vehicle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Lanes")
    TObjectPtr<ARaceLaneActor> CyclingLane;

    /** Existing placed RaceLaneActor that represents the rowing vehicle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Lanes")
    TObjectPtr<ARaceLaneActor> RowingLane;

    /** Existing placed RaceLaneActor that represents the strength vehicle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Lanes")
    TObjectPtr<ARaceLaneActor> StrengthLane;

    // ?? Balance tuning ????????????????????????????????????????????????????

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Balance",
              meta = (ClampMin = "1.0"))
    float CyclingReferencePower = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Balance",
              meta = (ClampMin = "1.0"))
    float RowingReferencePower = 220.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Balance",
              meta = (ClampMin = "0.001"))
    float StrengthImpulseScale = 0.0125f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Balance",
              meta = (ClampMin = "0.05"))
    float StrengthImpulseDuration = 0.45f;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    TObjectPtr<ARaceGameMode> RaceGameMode;

    UFUNCTION()
    void OnDeviceUpdated(EDeviceChannel Channel, FErgData Data);
};
