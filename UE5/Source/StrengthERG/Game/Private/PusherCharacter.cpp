// PusherCharacter.cpp
//
// Drives a MetaHuman character to follow the boulder and play a Mixamo
// push animation montage in sync with game-state events.

#include "PusherCharacter.h"
#include "BoulderActor.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

// Returns the first skeletal mesh component on this actor that has a live AnimInstance.
static USkeletalMeshComponent* FindAnimatedMesh(AActor* Actor)
{
    TArray<USkeletalMeshComponent*> Meshes;
    Actor->GetComponents<USkeletalMeshComponent>(Meshes);
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        if (Mesh && Mesh->GetAnimInstance())
        {
            return Mesh;
        }
    }
    return nullptr;
}

APusherCharacter::APusherCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // The character should not move under its own physics — the boulder path
    // drives all movement.
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->DisableMovement();
        MoveComp->SetComponentTickEnabled(false);
    }
}

void APusherCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Auto-find the boulder if not explicitly assigned in the editor.
    if (!Boulder)
    {
        for (TActorIterator<ABoulderActor> It(GetWorld()); It; ++It)
        {
            Boulder = *It;
            UE_LOG(LogTemp, Log, TEXT("[Pusher] Auto-found boulder: %s"), *Boulder->GetName());
            break;
        }
    }

    if (!Boulder)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pusher] No ABoulderActor found — character will not follow the path."));
    }

    // Snap to the correct starting position immediately.
    UpdatePositionAlongPath();
}

void APusherCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bFollowBoulder)
    {
        UpdatePositionAlongPath();
    }
}

// ?? Animation ?????????????????????????????????????????????????????????????????

void APusherCharacter::PlayPushAnimation()
{
    bIsPushing = true;

    if (!PushMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pusher] PushMontage is null — assign a montage in the Blueprint details."));
        return;
    }

    USkeletalMeshComponent* AnimMesh = FindAnimatedMesh(this);
    if (!AnimMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pusher] No skeletal mesh with an AnimInstance found on %s."), *GetName());
        return;
    }

    float Duration = AnimMesh->GetAnimInstance()->Montage_Play(PushMontage, MontagePlayRate);
    UE_LOG(LogTemp, Log, TEXT("[Pusher] Push montage started on %s (rate=%.2f, duration=%.2f)."), *AnimMesh->GetName(), MontagePlayRate, Duration);
}

void APusherCharacter::StopPushAnimation()
{
    if (!bIsPushing)
    {
        return;
    }

    bIsPushing = false;

    if (PushMontage)
    {
        USkeletalMeshComponent* AnimMesh = FindAnimatedMesh(this);
        if (AnimMesh)
        {
            AnimMesh->GetAnimInstance()->Montage_Stop(0.25f, PushMontage);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[Pusher] Push montage stopped."));
}

// ?? Positioning ???????????????????????????????????????????????????????????????

void APusherCharacter::UpdatePositionAlongPath()
{
    if (!Boulder)
    {
        return;
    }

    // Compute the push direction (path start ? path end) from the boulder's
    // current world location. We ask the boulder for its location directly
    // since PathStart/PathEnd are private — the character just needs to stand
    // behind the boulder in the opposite direction of travel.
    FVector BoulderPos    = Boulder->GetActorLocation();

    // We reconstruct the push direction from the boulder's forward vector.
    // ABoulderActor aligns its rolling axis to the hill, so the path direction
    // is recoverable from its world-space forward (X axis after rolling).
    // However, to keep this robust and independent of mesh orientation, we use
    // the GameMode's stored path vectors via a BlueprintCallable getter if
    // available. For now we derive it from the marker actors if accessible,
    // falling back to -Boulder forward.
    //
    // The simplest robust approach: use the boulder's velocity direction.
    // Since the boulder teleports via ETeleportPhysics we can't rely on velocity.
    // Instead we store the last known push direction between frames.
    //
    // Practical solution: expose PathDirection as a BlueprintCallable on
    // ABoulderActor (added separately) and call it here.
    FVector PushDir = Boulder->GetPathDirection(); // see BoulderActor addition below

    // Stand behind the boulder: offset in the -PushDir direction.
    FVector TargetPos = BoulderPos
                      - PushDir * BoulderOffsetBehind
                      + FVector(0.f, 0.f, VerticalOffset);

    SetActorLocation(TargetPos, false, nullptr, ETeleportType::TeleportPhysics);

    // Face the boulder (i.e. face in the PushDir direction).
    if (!PushDir.IsNearlyZero())
    {
        FRotator FaceRot = PushDir.Rotation();
        SetActorRotation(FaceRot);
    }
}
