#pragma once
/**
 * RaceFinishActor.h
 *
 * Slice 1 – Race Map Foundation
 *
 * A Box Trigger placed at the finish line.
 * When any ARaceLaneActor overlaps it, it calls
 * ARaceGameMode::NotifyLaneFinished() with that lane's channel.
 *
 * Editor placement:
 *   - Drop one instance across all three lane paths at the finish line.
 *   - Scale the box to span all three lanes (e.g. 200 x 1500 x 200 cm).
 *   - No per-lane instances needed – one actor handles all overlaps.
 */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceFinishActor.generated.h"

class UBoxComponent;
class UBillboardComponent;
class UStaticMeshComponent;
class UMaterialInterface;

UCLASS(Blueprintable, meta = (DisplayName = "Race Finish Actor"))
class STRENGTHERG_API ARaceFinishActor : public AActor
{
    GENERATED_BODY()

public:
    ARaceFinishActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Race|Finish")
    UBoxComponent* FinishBox;

    /** Optional visual indicator (visible in editor only). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Race|Finish")
    UBillboardComponent* Billboard;

    /** Optional visible mesh for the finish line (e.g. a banner or gate). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Race|Finish")
    UStaticMeshComponent* Mesh;

    /** Material applied to the finish line mesh. Swap in the Details panel. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Finish")
    UMaterialInterface* FinishMaterial;

    // ?? AActor interface ??????????????????????????????????????????????????

    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void OnFinishOverlap(UPrimitiveComponent* OverlappedComp,
                         AActor*              OtherActor,
                         UPrimitiveComponent* OtherComp,
                         int32                OtherBodyIndex,
                         bool                 bFromSweep,
                         const FHitResult&    SweepResult);
};
