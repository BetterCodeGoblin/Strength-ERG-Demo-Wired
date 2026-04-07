#include "QTEComponent.h"
#include "Engine/World.h"

UQTEComponent::UQTEComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UQTEComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // ── Forced window countdown ───────────────────────────────────────────────
    bool bForcedOpen = false;
    if (ForcedWindowTimer > 0.f)
    {
        ForcedWindowTimer -= DeltaTime;
        bForcedOpen = (ForcedWindowTimer > 0.f);
    }

    // ── Cycle-based window ────────────────────────────────────────────────────
    const float WorldTime     = GetWorld()->GetTimeSeconds();
    const float CycleTime     = FMath::Fmod(WorldTime, CycleDuration);
    CycleProgress             = CycleTime / CycleDuration;

    const float WindowStart   = WindowStartFraction * CycleDuration;
    const float WindowEnd     = WindowStart + WindowDuration;
    const bool  bCycleOpen    = CycleTime >= WindowStart && CycleTime < WindowEnd;

    if (bCycleOpen && !FMath::IsNearlyZero(CycleDuration))
        WindowProgress = (CycleTime - WindowStart) / WindowDuration;
    else
        WindowProgress = 0.f;

    // ── Combine both sources ──────────────────────────────────────────────────
    const bool bShouldBeOpen = bForcedOpen || bCycleOpen;
    UpdateWindowState(bShouldBeOpen);
}

void UQTEComponent::EvaluatePush(EPushRating& OutRating, float& OutMultiplier) const
{
    if (bIsWindowOpen)
    {
        OutRating     = EPushRating::Perfect;
        OutMultiplier = PerfectMultiplier;
    }
    else
    {
        OutRating     = EPushRating::Miss;
        OutMultiplier = MissMultiplier;
    }
}

void UQTEComponent::ForceOpenWindow(float Duration)
{
    ForcedWindowTimer = FMath::Max(ForcedWindowTimer, Duration);
}

void UQTEComponent::UpdateWindowState(bool bOpen)
{
    if (bOpen && !bWasPreviouslyOpen)
    {
        OnWindowOpened.Broadcast();
    }
    else if (!bOpen && bWasPreviouslyOpen)
    {
        OnWindowClosed.Broadcast();
        WindowProgress = 0.f;
    }

    bIsWindowOpen        = bOpen;
    bWasPreviouslyOpen   = bOpen;
}
