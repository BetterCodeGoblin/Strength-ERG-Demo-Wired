#include "RaceCameraActor.h"

#include "Camera/CameraComponent.h"
#include "RaceLaneActor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ARaceCameraActor::ARaceCameraActor()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCameraComponent()->FieldOfView = 70.f;
}

void ARaceCameraActor::BeginPlay()
{
    Super::BeginPlay();

    // Automatically make this the view for Player 0 — no Blueprint wiring needed.
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->SetViewTargetWithBlend(this, 0.5f);
    }
}

void ARaceCameraActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    TArray<FVector> Positions;
    GatherTargetPositions(Positions);

    if (Positions.IsEmpty()) return;

    // ?? Group centre ??????????????????????????????????????????????????????
    FVector Centre = FVector::ZeroVector;
    for (const FVector& P : Positions)
        Centre += P;
    Centre /= Positions.Num();

    // ?? Desired camera position ???????????????????????????????????????????
    const FVector DesiredLocation = Centre + FollowOffset;
    const FVector SmoothedLocation = FMath::VInterpTo(
        GetActorLocation(), DesiredLocation, DeltaSeconds, InterpSpeed);

    SetActorLocation(SmoothedLocation);

    // ?? Look at group centre ??????????????????????????????????????????????
    const FVector LookAtPoint = Centre + FVector(0.f, 0.f, LookAtHeightOffset);
    const FRotator DesiredRotation = (LookAtPoint - SmoothedLocation).Rotation();
    const FRotator SmoothedRotation = FMath::RInterpTo(
        GetActorRotation(), DesiredRotation, DeltaSeconds, InterpSpeed);

    SetActorRotation(SmoothedRotation);

    // ?? Optional dynamic FOV based on racer spread on X ???????????????????
    if (bDynamicFOV && Positions.Num() > 1)
    {
        float MinX =  FLT_MAX;
        float MaxX = -FLT_MAX;
        for (const FVector& P : Positions)
        {
            MinX = FMath::Min(MinX, P.X);
            MaxX = FMath::Max(MaxX, P.X);
        }
        const float Spread = FMath::Max(0.f, MaxX - MinX);
        const float Alpha  = FMath::Clamp(Spread / MaxSpreadDistance, 0.f, 1.f);
        const float TargetFOV = FMath::Lerp(BaseFOV, MaxFOV, Alpha);
        GetCameraComponent()->FieldOfView = FMath::FInterpTo(
            GetCameraComponent()->FieldOfView, TargetFOV, DeltaSeconds, InterpSpeed);
    }
}

void ARaceCameraActor::GatherTargetPositions(TArray<FVector>& OutPositions) const
{
    for (ARaceLaneActor* Target : { BikeTarget.Get(), RowingTarget.Get(), StrengthTarget.Get() })
    {
        if (Target)
            OutPositions.Add(Target->GetActorLocation());
    }

    // Fallback: if no manual targets were assigned in the editor, auto-discover
    // any RaceLaneActors placed in the world. This keeps the camera functional
    // even when the editor UPROPERTY refs are null (e.g. after map reload).
    if (OutPositions.IsEmpty() && GetWorld())
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARaceLaneActor::StaticClass(), Found);
        if (!Found.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("[RaceCamera] Manual targets not set -- auto-discovered %d lane actor(s)."), Found.Num());
            for (AActor* A : Found)
                OutPositions.Add(A->GetActorLocation());
        }
    }
}
