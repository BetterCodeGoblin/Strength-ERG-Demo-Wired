#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RowingProgressActor.generated.h"

/**
 * Minimal world actor for the overnight rowing rescue slice.
 *
 * Represents climb progress toward a target distance/altitude.
 * Designers can later replace this with a boat, marker, spline mover,
 * or full Imagine presentation layer without rewriting game rules.
 */
UCLASS()
class STRENGTHERG_API ARowingProgressActor : public AActor
{
    GENERATED_BODY()

public:
    ARowingProgressActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing")
    float TargetProgress = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing")
    float CurrentProgress = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rowing")
    bool bClampAtTarget = true;

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
};
