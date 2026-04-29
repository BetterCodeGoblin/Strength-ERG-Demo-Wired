#pragma once

/**
 * DeviceLabActor.h
 *
 * Debug-first Actor for the "Device Lab" map.
 *
 * Drop one of these into any level and it will own three ErgManagerComponents,
 * one per device channel (Strength, Rowing, Cycling). Each component connects
 * to its own bridge process on its own port — fully independent streams.
 *
 * Default port assignments:
 *   Strength : 6789  (PM5HidDiag.exe — wired USB HID to PM5)
 *   Rowing   : 6791  (RowingSimBridge.exe — PM5 RowErg or sim)
 *   Cycling  : 6792  (CyclingSimBridge.exe — BikeErg or sim)
 *
 * Blueprint usage:
 *   GetStrengthData() / GetRowingData() / GetCyclingData()  — per-channel snapshot
 *   GetAllDebugText()                                        — three-line string for on-screen HUD
 *
 * For the Device Lab map, bind a UMG TextBlock to GetAllDebugText() on Tick,
 * or subscribe to OnAnyDeviceUpdated for event-driven refresh.
 */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ErgTypes.h"
#include "ErgManagerComponent.h"
#include "DeviceLabActor.generated.h"

/** Fired on the game thread whenever any channel updates its data snapshot. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAnyDeviceUpdated,
    EDeviceChannel, Channel,
    FErgData,       Data);

UCLASS(Blueprintable, meta = (DisplayName = "Device Lab Actor"))
class STRENGTHERG_API ADeviceLabActor : public AActor
{
    GENERATED_BODY()

public:
    ADeviceLabActor();

    // ?? Sub-components (configure ports/paths in the Details panel) ?????????

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeviceLab|Components")
    UErgManagerComponent* StrengthDevice;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeviceLab|Components")
    UErgManagerComponent* RowingDevice;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeviceLab|Components")
    UErgManagerComponent* CyclingDevice;

    // ?? Aggregate delegate ??????????????????????????????????????????????????

    UPROPERTY(BlueprintAssignable, Category = "DeviceLab|Events")
    FOnAnyDeviceUpdated OnAnyDeviceUpdated;

    // ?? Blueprint-callable accessors ????????????????????????????????????????

    UFUNCTION(BlueprintCallable, Category = "DeviceLab")
    FErgData GetStrengthData() const;

    UFUNCTION(BlueprintCallable, Category = "DeviceLab")
    FErgData GetRowingData() const;

    UFUNCTION(BlueprintCallable, Category = "DeviceLab")
    FErgData GetCyclingData() const;

    /**
     * Returns a three-line debug string, one line per channel.
     * Bind this to a UMG TextBlock for the simplest possible debug HUD.
     *
     * Example output:
     *   [Strength] Connected:Y | Reps:4 | RepTime:0.82s | PullDist:1140 | Elapsed:38s
     *   [Rowing]   Connected:Y | StrokeRate:24.0 spm | Pace:132s/500m | Power:210W | Elapsed:120s
     *   [Cycling]  Connected:N | Cadence:0 rpm | Power:0W | Speed:0.0 kph | Elapsed:0s
     */
    UFUNCTION(BlueprintCallable, Category = "DeviceLab")
    FString GetAllDebugText() const;

    // ?? AActor interface ?????????????????????????????????????????????????????

    virtual void BeginPlay()  override;
    virtual void Tick(float DeltaSeconds) override;

private:
    /** Forward per-channel delegate fires to the aggregate delegate. */
    UFUNCTION()
    void OnStrengthUpdated(FErgData Data);

    UFUNCTION()
    void OnRowingUpdated(FErgData Data);

    UFUNCTION()
    void OnCyclingUpdated(FErgData Data);
};
