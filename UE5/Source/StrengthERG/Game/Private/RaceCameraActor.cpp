#include "RaceCameraActor.h"

#include "Camera/CameraComponent.h"

ARaceCameraActor::ARaceCameraActor()
{
    GetCameraComponent()->FieldOfView = 70.f;
}
