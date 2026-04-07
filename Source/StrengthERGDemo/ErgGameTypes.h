#pragma once

#include "CoreMinimal.h"
#include "ErgGameTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Game State Enum
// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EBoulderGameState : uint8
{
    Idle       UMETA(DisplayName = "Idle"),
    Countdown  UMETA(DisplayName = "Countdown"),
    Playing    UMETA(DisplayName = "Playing"),
    Won        UMETA(DisplayName = "Won"),
    Lost       UMETA(DisplayName = "Lost")
};

// ─────────────────────────────────────────────────────────────────────────────
//  Power Zone Enum
// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EPowerZone : uint8
{
    Low        UMETA(DisplayName = "Low"),
    Moderate   UMETA(DisplayName = "Moderate"),
    Power      UMETA(DisplayName = "Power"),
    Explosive  UMETA(DisplayName = "Explosive")
};

// ─────────────────────────────────────────────────────────────────────────────
//  Push Rating Enum
// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EPushRating : uint8
{
    Perfect    UMETA(DisplayName = "Perfect"),
    Miss       UMETA(DisplayName = "Miss")
};

// ─────────────────────────────────────────────────────────────────────────────
//  ERG Frame Data — live data from one poll cycle of ErgBridge
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FErgFrameData
{
    GENERATED_BODY()

    /** Strokes per minute */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    float StrokeRate = 0.f;

    /** Pace in seconds per 500m */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    float PaceSeconds = 0.f;

    /** Output in Watts */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    float PowerWatts = 0.f;

    /** True when ErgBridge reports a connected PM5 */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    bool bConnected = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Rep Data — data for a single completed rowing rep
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FRepData
{
    GENERATED_BODY()

    /** Sequential rep number from PM5 */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    int32 RepNumber = 0;

    /** Drive phase duration in seconds (shorter = more explosive) */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    float RepTimeSec = 0.f;

    /** Cable pull distance in raw PM5 units */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    int32 PullDistance = 0;

    /** Computed power: PullDistance / RepTimeSec */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    float RepPower = 0.f;

    /** Power zone classification */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    EPowerZone PowerZone = EPowerZone::Low;

    /** QTE timing rating */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    EPushRating PushRating = EPushRating::Miss;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Game Stats — end-of-run summary
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FGameStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TotalReps = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 PerfectCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float AveragePower = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float PeakPower = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float TimeUsedSec = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float FinalProgress = 0.f;
};
