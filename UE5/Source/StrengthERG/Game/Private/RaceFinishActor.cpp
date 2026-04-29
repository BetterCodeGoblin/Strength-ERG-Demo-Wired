// RaceFinishActor.cpp – Slice 1 Race Map Foundation

#include "RaceFinishActor.h"
#include "RaceLaneActor.h"
#include "RaceGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "Kismet/GameplayStatics.h"

ARaceFinishActor::ARaceFinishActor()
{
    PrimaryActorTick.bCanEverTick = false;

    FinishBox = CreateDefaultSubobject<UBoxComponent>(TEXT("FinishBox"));
    FinishBox->SetBoxExtent(FVector(100.f, 750.f, 100.f)); // spans three 500-cm-spaced lanes
    FinishBox->SetCollisionProfileName(TEXT("Trigger"));
    SetRootComponent(FinishBox);

    Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
    Billboard->SetupAttachment(FinishBox);
}

void ARaceFinishActor::BeginPlay()
{
    Super::BeginPlay();
    FinishBox->OnComponentBeginOverlap.AddDynamic(this, &ARaceFinishActor::OnFinishOverlap);
}

void ARaceFinishActor::OnFinishOverlap(UPrimitiveComponent* /*OverlappedComp*/,
                                        AActor*              OtherActor,
                                        UPrimitiveComponent* /*OtherComp*/,
                                        int32                /*OtherBodyIndex*/,
                                        bool                 /*bFromSweep*/,
                                        const FHitResult&    /*SweepResult*/)
{
    ARaceLaneActor* Lane = Cast<ARaceLaneActor>(OtherActor);
    if (!Lane) return;

    ARaceGameMode* GM = Cast<ARaceGameMode>(
        UGameplayStatics::GetGameMode(this));
    if (!GM) return;

    GM->NotifyLaneFinished(Lane->LaneChannel);

    // Freeze all lane actors in the world so the race clearly stops.
    TArray<AActor*> AllLanes;
    UGameplayStatics::GetAllActorsOfClass(this, ARaceLaneActor::StaticClass(), AllLanes);
    for (AActor* A : AllLanes)
    {
        if (ARaceLaneActor* L = Cast<ARaceLaneActor>(A))
            L->bRaceStopped = true;
    }
}
