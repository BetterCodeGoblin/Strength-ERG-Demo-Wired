#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "RowingProgressActor.generated.h"

/**
 * World actor for the overnight rowing rescue slice.
 *
 * Represents progress toward a target distance/altitude and can
 * physically move between StartLocation and EndLocation as progress
 * advances. The visible ProgressMesh gives designers an immediate
 * in-editor placeholder (replace with a boat/marker BP as needed).
 */
UCLASS()
class STRENGTHERG_API ARowingProgressActor : public AActor
{
    GENERATED_BODY()

public:
    ARowingProgressActor();

    // ----------------------------------------------------------------
    // Presentation
    // ----------------------------------------------------------------

    /** Visible placeholder mesh. Assign any static mesh in the Details panel. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rowing|Presentation")
    TObjectPtr<UStaticMeshComponent> ProgressMesh;

    /** World-space start point of the progress journey. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing|Presentation")
    FVector StartLocation = FVector::ZeroVector;

    /** World-space end point of the progress journey. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing|Presentation")
    FVector EndLocation = FVector(1000.f, 0.f, 0.f);

    /**
     * When true, calling UpdateLocationFromProgress() (or Tick if
     * bUpdateEachTick is set) moves the actor along the
     * StartLocation -> EndLocation line automatically.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing|Presentation")
    bool bUpdateActorLocationFromProgress = true;

    /** Uniform scale applied to the ProgressMesh component. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing|Presentation", meta = (ClampMin = "0.01"))
    float MeshScale = 1.f;

    // ----------------------------------------------------------------
    // Game rules
    // ----------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing")
    float TargetProgress = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing")
    float CurrentProgress = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing")
    bool bClampAtTarget = true;

    // ----------------------------------------------------------------
    // API
    // ----------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Rowing")
    void AddProgress(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Rowing")
    void ApplyDrift(float DeltaSeconds, float DriftRate);

    UFUNCTION(BlueprintCallable, Category = "Rowing")
    void ResetProgress();

    UFUNCTION(BlueprintCallable, Category = "Rowing")
    float GetProgressNormalized() const;

    UFUNCTION(BlueprintCallable, Category = "Rowing")
    bool IsComplete() const;

    /**
     * Moves the actor to the world position that corresponds to the
     * current normalised progress along StartLocation -> EndLocation.
     * Call this from Blueprint (e.g. on a timer or after AddProgress)
     * or let it run automatically each tick by enabling bUpdateEachTick.
     */
    UFUNCTION(BlueprintCallable, Category = "Rowing")
    void UpdateLocationFromProgress();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
};
