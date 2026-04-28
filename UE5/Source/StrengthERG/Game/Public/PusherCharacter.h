/**
 * PusherCharacter.h
 *
 * A MetaHuman character that stands behind the boulder and plays a
 * Mixamo pushing animation driven by game-state events from ABoulderGameMode.
 *
 * Setup summary (editor):
 *   1. Create BP_PusherCharacter (child of APusherCharacter).
 *   2. Assign the MetaHuman Skeletal Mesh to the inherited Mesh component.
 *   3. Set AnimClass to the Animation Blueprint you create (see below).
 *   4. Import the Mixamo FBX as a Skeletal Mesh Animation, retarget it to the
 *      MetaHuman skeleton, then create an Animation Montage from it.
 *   5. Assign that montage to PushMontage on BP_PusherCharacter.
 *   6. Place BP_PusherCharacter in the level near the boulder.
 *   7. Assign BP_PusherCharacter to ABoulderGameMode::Pusher (or let it auto-find).
 *
 * Animation Blueprint recommended setup:
 *   - Use a single State Machine with two states: Idle and Pushing.
 *   - Drive the transition with a bool variable "bIsPushing" exposed from
 *     this actor (GetIsPushing()).
 *   - Alternatively, use a Montage slot in the output pose so PushMontage
 *     plays on top of an idle pose without a full Anim BP overhaul.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PusherCharacter.generated.h"

class UAnimMontage;
class ABoulderActor;

UCLASS(BlueprintType, Blueprintable)
class STRENGTHERG_API APusherCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    APusherCharacter();

    // ?? Boulder Reference ?????????????????????????????????????????????????????

    /**
     * The boulder this character is pushing.
     * If left null in the editor, BeginPlay will auto-find ABoulderActor.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boulder")
    ABoulderActor* Boulder = nullptr;

    // ?? Positioning ???????????????????????????????????????????????????????????

    /**
     * Offset from the boulder's world position in the boulder's local push
     * direction (negative = behind the boulder, toward path start).
     * Tune this so the character's hands contact the boulder surface.
     */
    // Positioning

    /** How far behind the boulder centre the character stands (Unreal units). Tune in BP. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning")
    float StandOffsetBehind = 200.f;

    /** When true, position is updated every frame to follow the boulder. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning")
    bool bFollowBoulder = true;

    // ?? Push Lunge ?????????????????????????????????????????????????????????????????

    /** How far forward the character root lunges toward the boulder on each push (Unreal units). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lunge")
    float LungeDistance = 30.f;

    /** Lunge speed multiplier (higher = snappier). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lunge")
    float LungeSpeed = 18.f;

    /** Forward lean angle in degrees at peak lunge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lunge")
    float LeanAngleDegrees = 22.f;

    // ?? Animation ?????????????????????????????????????????????????????????????

    /**
     * Mixamo push animation retargeted to MetaHuman skeleton and converted to
     * an Animation Montage. Assign this in the Blueprint subclass details panel.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* PushMontage = nullptr;

    /**
     * Explicit reference to the Body skeletal mesh component on the MetaHuman.
     * When set, montage playback targets this mesh directly instead of relying on
     * the automatic search (which fails if only the Face mesh has an AnimInstance).
     * In BP_Aoi: set this to the "Body" SkeletalMeshComponent and assign its
     * AnimBlueprint so it has a live AnimInstance before the first push.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    USkeletalMeshComponent* AnimBodyMesh = nullptr;

    /**
     * Playback rate of the push montage. Increase to match faster rep cadence.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    float MontagePlayRate = 1.0f;

    /**
     * Section name inside PushMontage to loop while actively pushing.
     * Leave empty to play the montage from the beginning each time.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    FName PushLoopSectionName = NAME_None;

    // ?? State Query ???????????????????????????????????????????????????????????

    /** Returns true while the push montage is playing. */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    bool GetIsPushing() const { return bIsPushing; }

    // ?? API (called by ABoulderGameMode) ??????????????????????????????????????

    /**
     * Start (or continue) the push animation.
     * Called by ABoulderGameMode on every valid rep and while the boulder is
     * actively being pushed.
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void PlayPushAnimation();

    /**
     * Stop the push animation and return to idle.
     * Called by ABoulderGameMode when the game enters Idle, Rollback, Won, or Lost.
     */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    void StopPushAnimation();

    // ?? AActor ????????????????????????????????????????????????????????????????

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    bool bIsPushing  = false;
    bool bIsLunging  = false;
    float LungeT     = 0.f;

    /** Move the character to its offset position behind the boulder. */
    void UpdatePositionAlongPath(float DeltaTime);
};
