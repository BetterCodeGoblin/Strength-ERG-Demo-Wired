#pragma once
/**
 * RaceCameraActor.h
 *
 * Mario-Party-style shared follow camera for the three-lane PM5 race.
 * Assign the three lane actors in the Details panel. Each tick the camera
 * smoothly tracks the group centre, maintains a designer-tunable offset,
 * and optionally widens its FOV when racers spread out.
 */

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "RaceCameraActor.generated.h"

class ARaceLaneActor;

UCLASS(Blueprintable, meta = (DisplayName = "Race Camera Actor"))
class STRENGTHERG_API ARaceCameraActor : public ACameraActor
{
    GENERATED_BODY()

public:
    ARaceCameraActor();

    // ?? Targets ???????????????????????????????????????????????????????????

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Camera")
    TObjectPtr<ARaceLaneActor> BikeTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Camera")
    TObjectPtr<ARaceLaneActor> RowingTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Camera")
    TObjectPtr<ARaceLaneActor> StrengthTarget;

    // ?? Follow tuning ?????????????????????????????????????????????????????

    /** World-space offset from the group centre to the camera. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Camera")
    FVector FollowOffset = FVector(-800.f, 0.f, 400.f);

    /** Smoothing speed (higher = snappier). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Camera",
              meta = (ClampMin = "0.5", ClampMax = "20"))
    float InterpSpeed = 5.f;

    /** Extra height added to the look-at point so the camera tilts down slightly. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Camera")
    float LookAtHeightOffset = 100.f;

    // ?? Spread / zoom ?????????????????????????????????????????????????????

    /** When true, FOV widens as racers spread apart on X. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Camera")
    bool bDynamicFOV = true;

    /** Base FOV used when racers are bunched together. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Camera",
              meta = (ClampMin = "40", ClampMax = "120"))
    float BaseFOV = 70.f;

    /** Maximum FOV allowed when racers spread to MaxSpreadDistance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Camera",
              meta = (ClampMin = "40", ClampMax = "150"))
    float MaxFOV = 90.f;

    /** Spread distance (cm) at which MaxFOV is reached. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Camera",
              meta = (ClampMin = "100", ClampMax = "10000"))
    float MaxSpreadDistance = 2000.f;

    // ?? AActor interface ??????????????????????????????????????????????????

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    /** Collect valid (non-null) target locations into OutPositions. */
    void GatherTargetPositions(TArray<FVector>& OutPositions) const;
};
