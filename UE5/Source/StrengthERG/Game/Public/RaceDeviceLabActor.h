#pragma once
/**
 * RaceDeviceLabActor.h
 *
 * Race-specific wrapper around DeviceLabActor. Converts live PM5 telemetry into:
 * - lane movement inputs
 * - race HUD status lines
 */

#include "CoreMinimal.h"
#include "DeviceLabActor.h"
#include "RaceDeviceLabActor.generated.h"

class ARaceLaneActor;
class ARaceGameMode;

UCLASS(Blueprintable, meta = (DisplayName = "Race Device Lab Actor"))
class STRENGTHERG_API ARaceDeviceLabActor : public ADeviceLabActor
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Lanes")
    TObjectPtr<ARaceLaneActor> CyclingLane;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Lanes")
    TObjectPtr<ARaceLaneActor> RowingLane;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Lanes")
    TObjectPtr<ARaceLaneActor> StrengthLane;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Balance", meta = (ClampMin = "1.0"))
    float CyclingReferencePower = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Balance", meta = (ClampMin = "1.0"))
    float RowingReferencePower = 220.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Balance", meta = (ClampMin = "1.0"))
    float StrengthImpulseScale = 0.0125f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Balance", meta = (ClampMin = "0.05"))
    float StrengthImpulseDuration = 0.45f;

private:
    UPROPERTY()
    TObjectPtr<ARaceGameMode> RaceGameMode;

    UFUNCTION()
    void OnRaceDeviceUpdated(EDeviceChannel Channel, FErgData Data);
};
