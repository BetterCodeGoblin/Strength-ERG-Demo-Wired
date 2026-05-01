#pragma once
#pragma once

/**
 * RaceLaneActor.h
 *
 * Slice 2 – Lane Actors
 *
 * One actor per PM5 lane (Bike / Row / Strength).
 * Moves forward along its local +X axis each tick, driven by MovementSpeedCmS.
 *
 * Telemetry hookup (Slice 3) will call SetMovementSpeed() from DeviceLabActor
 * or a Blueprint binding. No telemetry coupling in this file.
 *
 * Editor placement:
 *   - Drop three of these into the race map, spaced 400 cm apart on Y.
 *   - Set LaneChannel to Cycling / Rowing / Strength respectively.
 *   - Assign a visible Static Mesh in the Details panel (placeholder cube is fine).
 *   - Point them toward the finish flag (rotate or rely on SpawnRotation).
 */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ErgTypes.h"
#include "RaceLaneActor.generated.h"

UCLASS(Blueprintable, meta = (DisplayName = "Race Lane Actor"))
class STRENGTHERG_API ARaceLaneActor : public AActor
{
    GENERATED_BODY()

public:
    ARaceLaneActor();

    // ?? Identity ??????????????????????????????????????????????????????????

    /** Which PM5 channel drives this lane. Set in the Details panel. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Lane")
    EDeviceChannel LaneChannel = EDeviceChannel::Cycling;

    // ?? Visible mesh ??????????????????????????????????????????????????????

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Race|Lane")
    UStaticMeshComponent* Mesh;

    // ?? Movement tuning ???????????????????????????????????????????????????

    /**
     * How fast the actor should move forward when receiving a normalised speed
     * of 1.0 (i.e. full effort). Exposed so designers can balance lanes
     * without recompiling.
     *
     * Default 600 cm/s ? 21.6 km/h, roughly typical cycling cadence feel.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Movement",
              meta = (ClampMin = "10", ClampMax = "5000", UIMin = "10", UIMax = "5000"))
    float MaxSpeedCmS = 600.f;

    /**
     * Current normalised movement input [0..1].
     * Set every frame by the telemetry hookup (Slice 3).
     * 0 = stationary, 1 = MaxSpeedCmS.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Movement")
    float NormalisedSpeed = 0.f;

    // ?? Debug / editor helpers ????????????????????????????????????????????

    /** When true, DebugConstantSpeed is used instead of telemetry-fed NormalisedSpeed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Debug")
    bool bUseDebugConstantSpeed = false;

    /** Normalised speed [0..1] used when bUseDebugConstantSpeed is true. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Debug",
              meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1",
                      EditCondition = "bUseDebugConstantSpeed"))
    float DebugConstantSpeed = 0.5f;

    // ?? Race state ????????????????????????????????????????????????????????

    /** Set to true by the game mode when any lane wins. Stops movement. */
    UPROPERTY(BlueprintReadWrite, Category = "Race")
    bool bRaceStopped = false;

    // ?? API for Slice 3 telemetry hookup ??????????????????????????????????

    /**
     * Set the normalised forward speed for this lane.
     * Value is clamped to [0, 1].  Call this every frame or on data update.
     *
     * Bike   ? map cadence (RPM / MaxCadence) or (PowerWatts / MaxWatts)
     * Row    ? map stroke-burst velocity or pace
     * Strength ? map rep impulse as a short speed burst
     */
    UFUNCTION(BlueprintCallable, Category = "Race|Movement")
    void SetNormalisedSpeed(float Speed);

    /** Convenience: apply a brief speed impulse that decays over ImpulseDuration. */
    UFUNCTION(BlueprintCallable, Category = "Race|Movement")
    void ApplySpeedImpulse(float Strength, float DurationSeconds = 0.4f);

    // ?? AActor interface ??????????????????????????????????????????????????

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    // Impulse state
    float ImpulseRemaining  = 0.f;  // seconds left on current impulse
    float ImpulseStrength   = 0.f;  // normalised strength of impulse [0..1]
};
