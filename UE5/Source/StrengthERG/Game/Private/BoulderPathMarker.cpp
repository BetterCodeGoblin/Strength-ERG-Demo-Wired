#include "BoulderPathMarker.h"
#include "Components/ArrowComponent.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITORONLY_DATA
#include "Components/BillboardComponent.h"
#endif

ABoulderPathMarker::ABoulderPathMarker()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // Always create a plain scene component as root so Arrow (runtime) has a
    // non-editor-only parent. The Billboard is attached to this root as a child.
    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;

#if WITH_EDITORONLY_DATA
    Billboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
    if (Billboard)
    {
        Billboard->bIsScreenSizeScaled = true;
        Billboard->SetupAttachment(RootComponent);
    }
#endif

    Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
    Arrow->SetupAttachment(RootComponent);   // attached to plain root, never to Billboard
    Arrow->ArrowSize      = 2.0f;
    Arrow->ArrowColor     = FColor(255, 165, 0);
    Arrow->bHiddenInGame  = true;
    Arrow->SetRelativeRotation(FRotator::ZeroRotator);
}

void ABoulderPathMarker::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Keep the arrow pointing at the paired marker every tick so designers
    // get live feedback when moving either marker in the viewport.
    if (PairedMarker && Arrow)
    {
        FVector ToTarget = (PairedMarker->GetActorLocation() - GetActorLocation());
        if (!ToTarget.IsNearlyZero())
        {
            Arrow->SetWorldRotation(ToTarget.ToOrientationRotator());
        }
    }
}
