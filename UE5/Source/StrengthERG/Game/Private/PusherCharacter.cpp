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
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

// Returns the body skeletal mesh component that has a live AnimInstance.
// Prefers the Character's main mesh (GetMesh), then falls back to the first
// component whose name contains "body" (case-insensitive), and finally to any
// component that has a live AnimInstance — so the Face mesh is never picked first.
static USkeletalMeshComponent* FindAnimatedMesh(AActor* Actor)
{
    TArray<USkeletalMeshComponent*> Meshes;
    Actor->GetComponents<USkeletalMeshComponent>(Meshes);

    // Log all meshes once to help diagnose which one to use
    static bool bMeshesLogged = false;
    if (!bMeshesLogged)
    {
        bMeshesLogged = true;
        for (USkeletalMeshComponent* M : Meshes)
            UE_LOG(LogTemp, Log, TEXT("[Pusher] Mesh found: %s | HasAnimInst: %d"),
                   *M->GetName(), M->GetAnimInstance() != nullptr);
    }

    // 1. Prefer the ACharacter root mesh if it has an AnimInstance
    if (ACharacter* Char = Cast<ACharacter>(Actor))
    {
        USkeletalMeshComponent* Main = Char->GetMesh();
        if (Main && Main->GetAnimInstance())
            return Main;
    }

    // 2. Any mesh whose name contains "body" (case-insensitive)
    for (USkeletalMeshComponent* Mesh : Meshes)
        if (Mesh && Mesh->GetAnimInstance() &&
            Mesh->GetName().ToLower().Contains(TEXT("body")))
            return Mesh;

    // 3. Any mesh that is NOT the face and has an AnimInstance
    for (USkeletalMeshComponent* Mesh : Meshes)
        if (Mesh && Mesh->GetAnimInstance() &&
            !Mesh->GetName().ToLower().Contains(TEXT("face")))
            return Mesh;

    // 4. Any mesh with an AnimInstance — last resort
    for (USkeletalMeshComponent* Mesh : Meshes)
        if (Mesh && Mesh->GetAnimInstance())
            return Mesh;

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

    // Ensure the character is actually visible — MetaHuman Blueprints sometimes
    // have Actor Hidden In Game checked by default, or individual mesh components
    // may be hidden. Force everything visible here.
    SetActorHiddenInGame(false);
    TArray<USceneComponent*> Components;
    GetRootComponent()->GetChildrenComponents(true, Components);
    for (USceneComponent* Comp : Components)
    {
        Comp->SetHiddenInGame(false, false);
    }

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
    UpdatePositionAlongPath(0.f);
}

void APusherCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bFollowBoulder)
    {
        UpdatePositionAlongPath(DeltaTime);
    }
}

// ?? Animation ?????????????????????????????????????????????????????????????????

void APusherCharacter::PlayPushAnimation()
{
    bIsPushing = true;

    // Trigger lunge only if one isn’t already running (avoids snap/jitter mid-lunge)
    if (!bIsLunging)
    {
        LungeT     = 0.f;
        bIsLunging = true;
    }

    if (!PushMontage)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pusher] PushMontage is null — assign a montage in the Blueprint details."));
        return;
    }

    USkeletalMeshComponent* AnimMesh = AnimBodyMesh ? AnimBodyMesh : FindAnimatedMesh(this);
    if (!AnimMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pusher] No skeletal mesh with an AnimInstance found on %s. Assign AnimBodyMesh in the Blueprint."), *GetName());
        return;
    }
    if (!AnimMesh->GetAnimInstance())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Pusher] AnimBodyMesh '%s' has no AnimInstance — assign an AnimBlueprint to it in the Blueprint."), *AnimMesh->GetName());
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
        USkeletalMeshComponent* AnimMesh = AnimBodyMesh ? AnimBodyMesh : FindAnimatedMesh(this);
            if (AnimMesh && AnimMesh->GetAnimInstance())
            {
                AnimMesh->GetAnimInstance()->Montage_Stop(0.25f, PushMontage);
            }
    }

    UE_LOG(LogTemp, Log, TEXT("[Pusher] Push montage stopped."));
}

// ?? Positioning ???????????????????????????????????????????????????????????????

void APusherCharacter::UpdatePositionAlongPath(float DeltaTime)
{
    if (!Boulder) return;

    FVector BoulderPos = Boulder->GetActorLocation();

    FVector HillFwd = Boulder->GetPathDirection();
    if (HillFwd.IsNearlyZero()) return;

    // Step back from the boulder centre along the slope direction.
    FVector TargetXY = BoulderPos - HillFwd * StandOffsetBehind;

    // Line trace straight down to find the actual slope surface at this XY position.
    // This corrects for the boulder centre being above the ground by its own radius.
    FVector TraceStart = FVector(TargetXY.X, TargetXY.Y, TargetXY.Z + 1000.f);
    FVector TraceEnd   = FVector(TargetXY.X, TargetXY.Y, TargetXY.Z - 1000.f);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(Boulder);

    FVector NewPos = TargetXY; // fallback if trace misses
    if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
    {
        // Place capsule so feet land exactly on the surface
        const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        NewPos = Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight);
    }

    SetActorLocation(NewPos, false, nullptr, ETeleportType::TeleportPhysics);

    // Face toward the boulder. MetaHuman mesh root is +Y so subtract 90° yaw.
    FVector ToBoulder = BoulderPos - NewPos;
    ToBoulder.Z = 0.f;
    if (!ToBoulder.IsNearlyZero())
    {
        FRotator TargetRot = ToBoulder.GetSafeNormal().Rotation() + FRotator(0.f, -90.f, 0.f);
        SetActorRotation(DeltaTime > 0.f
            ? FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 10.f)
            : TargetRot);
    }
}

