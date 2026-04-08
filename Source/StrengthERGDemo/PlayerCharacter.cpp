#include "PlayerCharacter.h"
#include "BoulderGameMode.h"
#include "BoulderActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"

APlayerCharacter::APlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Disable physics - character position is driven by script
    GetCharacterMovement()->MovementMode = MOVE_None;
    GetCharacterMovement()->GravityScale = 0.f;
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    InitializeCharacter();
}

void APlayerCharacter::InitializeCharacter()
{
    if (!GameModeRef)
    {
        GameModeRef = GetWorld()->GetAuthGameMode<ABoulderGameMode>();
    }

    if (!BoulderRef && GameModeRef)
    {
        BoulderRef = GameModeRef->BoulderActorRef;
    }

    if (GameModeRef)
    {
        GameModeRef->OnRepProcessed.AddDynamic(this, &APlayerCharacter::HandleNewRepProcessed);
        UE_LOG(LogTemp, Log, TEXT("[PlayerCharacter] Subscribed to GameMode.OnRepProcessed"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerCharacter] GameMode not found - character animations will not sync"));
    }

    // Snap to initial position
    if (BoulderRef)
    {
        TargetPosition = ComputeTargetPosition();
        SetActorLocation(TargetPosition);
    }
}

void APlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update position to follow boulder with smooth damping
    if (BoulderRef)
    {
        TargetPosition = ComputeTargetPosition();

        // Simple exponential smoothing
        FVector Delta = TargetPosition - GetActorLocation();
        CurrentVelocity = FMath::Lerp(CurrentVelocity, Delta / FMath::Max(DeltaTime, 0.016f), FollowSmoothness);
        SetActorLocation(GetActorLocation() + CurrentVelocity * DeltaTime);

        // Rotate to face direction of slope
        if (BoulderRef->PathStart && BoulderRef->PathEnd)
        {
            FVector HillDirection = (BoulderRef->PathEnd->GetActorLocation() - BoulderRef->PathStart->GetActorLocation()).GetSafeNormal();
            FRotator TargetRot = HillDirection.Rotation() + FRotator(0.f, 180.f, 0.f); // Face away from hill
            SetActorRotation(FMath::Lerp(GetActorRotation().Quaternion(), TargetRot.Quaternion(), 0.1f).Rotator());
        }
    }
}

void APlayerCharacter::HandleNewRepProcessed(const FRepData& RepData)
{
    if (!PushMontage || !GetMesh() || !GetMesh()->GetAnimInstance())
    {
        return;
    }

    // Calculate playback speed: 1.0 / RepTimeSec
    // Default rep time is ~0.45s, so default playback speed is ~2.2x (to play in 0.45s)
    float PlayRate = FMath::Max(RepData.RepTimeSec, 0.1f) > 0.f ? 1.0f / FMath::Max(RepData.RepTimeSec, 0.1f) : 1.0f;
    LastAnimationPlayRate = PlayRate;

    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance)
    {
        AnimInstance->Montage_Play(PushMontage, PlayRate);
        UE_LOG(LogTemp, Log, TEXT("[PlayerCharacter] Playing push montage at speed %.2f (rep time %.2fs)"), PlayRate, RepData.RepTimeSec);
    }
}

void APlayerCharacter::OnAnimationPeakNotify()
{
    if (GameModeRef)
    {
        GameModeRef->NotifyAnimationPeak();
    }
}

FVector APlayerCharacter::ComputeTargetPosition() const
{
    if (!BoulderRef)
    {
        return GetActorLocation();
    }

    FVector BoulderPos = BoulderRef->GetActorLocation();

    // Position behind boulder toward camera
    // If hill goes forward (X), position character back (negative X)
    if (BoulderRef->PathStart && BoulderRef->PathEnd)
    {
        FVector HillDirection = (BoulderRef->PathEnd->GetActorLocation() - BoulderRef->PathStart->GetActorLocation()).GetSafeNormal();
        FVector BehindOffset = -HillDirection * DistanceBehindBoulder;
        FVector FinalPos = BoulderPos + BehindOffset;
        FinalPos.Z += HeightAboveGround;
        return FinalPos;
    }

    return BoulderPos;
}
