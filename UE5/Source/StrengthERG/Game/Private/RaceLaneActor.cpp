// RaceLaneActor.cpp – Slice 2 Lane Actors

#include "RaceLaneActor.h"
#include "Components/StaticMeshComponent.h"

ARaceLaneActor::ARaceLaneActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
}

void ARaceLaneActor::BeginPlay()
{
    Super::BeginPlay();
}

void ARaceLaneActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bRaceStopped) return;

    // Decay impulse timer
    float EffectiveSpeed = bUseDebugConstantSpeed ? DebugConstantSpeed : NormalisedSpeed;
    if (ImpulseRemaining > 0.f)
    {
        ImpulseRemaining -= DeltaSeconds;
        // Blend: take the higher of continuous speed and impulse
        EffectiveSpeed = FMath::Max(EffectiveSpeed, ImpulseStrength);
    }

    const float SpeedCmS = FMath::Clamp(EffectiveSpeed, 0.f, 1.f) * MaxSpeedCmS;
    if (SpeedCmS <= 0.f) return;

    // Move along world +X so mesh rotation doesn't affect travel direction
    const FVector Delta = FVector::ForwardVector * SpeedCmS * DeltaSeconds;
    SetActorLocation(GetActorLocation() + Delta);
}

void ARaceLaneActor::SetNormalisedSpeed(float Speed)
{
    NormalisedSpeed = FMath::Clamp(Speed, 0.f, 1.f);
}

void ARaceLaneActor::ApplySpeedImpulse(float Strength, float DurationSeconds)
{
    // Always take the strongest pending impulse
    if (Strength >= ImpulseStrength || ImpulseRemaining <= 0.f)
    {
        ImpulseStrength  = FMath::Clamp(Strength, 0.f, 1.f);
        ImpulseRemaining = FMath::Max(DurationSeconds, 0.05f);
    }
}
