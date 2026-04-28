#pragma once

/**
 * BoulderPathMarker.h
 *
 * Lightweight level-placed actor that marks either the Start (progress = 0)
 * or End (progress = 1) of the boulder's path.
 *
 * Equivalent to Unity's empty GameObjects used as pathStart / pathEnd.
 *
 * In the editor the actor shows:
 *   - A billboard sprite so it's easy to select
 *   - An arrow component whose direction is updated every tick to always point
 *     from this marker toward its paired marker (set via PairedMarker).
 *     On the Start marker this arrow shows the live push direction.
 */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoulderPathMarker.generated.h"

class UBillboardComponent;
class UArrowComponent;

UCLASS(HideCategories = (Rendering, Replication, Collision, HLOD, Physics,
                          Networking, Input, Actor, LevelInstance, Cooking))
class STRENGTHERG_API ABoulderPathMarker : public AActor
{
    GENERATED_BODY()

public:
    ABoulderPathMarker();

    /**
     * Optional: set this to the other marker so the arrow always points toward it.
     * On the Start marker, point it at the End marker to visualise push direction.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Path")
    ABoulderPathMarker* PairedMarker = nullptr;

    virtual void Tick(float DeltaTime) override;

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    UBillboardComponent* Billboard = nullptr;
#endif

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UArrowComponent* Arrow = nullptr;
};
