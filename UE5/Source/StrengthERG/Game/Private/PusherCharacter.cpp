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

void APusherCharacter::UpdatePositionAlongPath(float DeltaTime)
{
    if (!Boulder) return;

    FVector BoulderPos = Boulder->GetActorLocation();

    // Full 3D hill-forward direction (path start ? end), same as Unity's GetHillForward()
    FVector HillFwd = Boulder->GetPathDirection();
    if (HillFwd.IsNearlyZero()) return;

    // Slope-perpendicular up vector: Cross(right, hillFwd)
    // Equivalent to Unity's slopeUp = Vector3.Cross(hillRight, hillFwd)
    FVector HillRight = FVector::CrossProduct(HillFwd, FVector::UpVector).GetSafeNormal();
    FVector SlopeUp   = FVector::CrossProduct(HillRight, HillFwd).GetSafeNormal();

    // Scale slope offset by boulder progress if ramping is enabled
    float Progress        = Boulder->GetProgress();
    float EffectiveOffset = bRampOffsetAlongPath
                          ? SlopeHeightOffset * Progress
                          : SlopeHeightOffset;

    // Rest position: behind boulder along hill, lifted perpendicular to slope
    FVector RestPos = BoulderPos
                    - HillFwd * StandOffsetBehind
                    + SlopeUp * EffectiveOffset;

    // Script-driven lunge: sine pulse toward boulder on each push
    if (bIsLunging)
    {
        LungeT += DeltaTime * LungeSpeed;
        if (LungeT >= 1.f)
        {
            LungeT     = 0.f;
            bIsLunging = false;
        }
    }
    float T = bIsLunging ? FMath::Sin(LungeT * PI) : 0.f;

    // Lunge forward along the FLAT (XY) push direction so Aoi doesn't float
    // upward on a steep slope — same intent as Unity's hillFwd lunge but
    // clamped to the ground plane.
    FVector LungeDir = FVector(HillFwd.X, HillFwd.Y, 0.f).GetSafeNormal();
    FVector Target   = RestPos + LungeDir * (T * LungeDistance);

    // Smooth follow (Lerp), equivalent to Unity's Vector3.Lerp with followSmoothSpeed
    FVector Current = GetActorLocation();
    FVector NewPos  = FMath::Lerp(Current, Target,
                                  FMath::Clamp(DeltaTime * FollowSmoothSpeed, 0.f, 1.f));

    // On the initial snap (DeltaTime == 0) teleport directly to RestPos
    if (DeltaTime <= 0.f) NewPos = RestPos;

    SetActorLocation(NewPos, false, nullptr, ETeleportType::TeleportPhysics);

    // Face directly toward the boulder (look-at), not just along HillFwd.
    // This works regardless of MetaHuman mesh orientation offsets.
    FVector ToBoulder = (BoulderPos - NewPos);
    ToBoulder.Z = 0.f;   // keep rotation on the horizontal plane — no tilt up
    if (!ToBoulder.IsNearlyZero())
    {
        FRotator LookAt = ToBoulder.GetSafeNormal().Rotation();
        FRotator Lean   = FRotator(0.f, 0.f, 0.f); // reserved for future lean
        FRotator TargetRot = LookAt + Lean;
        SetActorRotation(DeltaTime > 0.f
            ? FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 10.f)
            : TargetRot);
    }

    // Log once for debugging
    static bool bLogged = false;
    if (!bLogged)
    {
        bLogged = true;
        UE_LOG(LogTemp, Log, TEXT("[Pusher] First position: X=%.1f Y=%.1f Z=%.1f | RestPos: X=%.1f Y=%.1f Z=%.1f | SlopeOffset=%.1f"),
               NewPos.X, NewPos.Y, NewPos.Z,
               RestPos.X, RestPos.Y, RestPos.Z, EffectiveOffset);
    }
}

