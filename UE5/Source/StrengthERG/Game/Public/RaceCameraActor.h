#pragma once
/**
 * RaceCameraActor.h
 *
 * Simple placed camera for the three-lane race. Point it at the lane actors and
 * set Auto Activate For Player in the editor.
 */

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "RaceCameraActor.generated.h"

UCLASS(Blueprintable, meta = (DisplayName = "Race Camera Actor"))
class STRENGTHERG_API ARaceCameraActor : public ACameraActor
{
    GENERATED_BODY()

public:
    ARaceCameraActor();
};
