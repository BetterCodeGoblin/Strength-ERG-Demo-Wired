#pragma once

#include "CoreMinimal.h"
#include "ErgTypes.generated.h"

/**
 * Live data snapshot from the Concept2 PM5 StrengthErg.
 * Equivalent to the public properties spread across Unity's Concept2UsbReader.
 */
USTRUCT(BlueprintType)
struct STRENGTHERG_API FErgData
{
    GENERATED_BODY()

    /** Total rep count this workout session. */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    int32 RepCount = 0;

    /** Drive time of the most recent rep (seconds). */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    float RepTimeSec = 0.f;

    /** Raw pull-distance units from the PM5 for the last rep. */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    int32 PullDistance = 0;

    /** Workout elapsed time in seconds. */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    float ElapsedSeconds = 0.f;

    /** Heart rate in BPM (0 if no HR belt detected). */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    int32 HeartRate = 0;

    /** True when connected to the PM5 or BLE bridge. */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    bool bIsConnected = false;

    /** True once reps are being recorded this session. */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    bool bIsActive = false;

    /** Human-readable status string for HUD display. */
    UPROPERTY(BlueprintReadOnly, Category = "ERG")
    FString StatusText = TEXT("Disconnected");
};

/**
 * Power zone classification for a rep.
 * Thresholds are configured on UBoulderGameSubsystem or ABoulderGameMode.
 */
UENUM(BlueprintType)
enum class EErgPowerZone : uint8
{
    Low         UMETA(DisplayName = "Low"),
    Moderate    UMETA(DisplayName = "Moderate"),
    Power       UMETA(DisplayName = "Power"),
    Explosive   UMETA(DisplayName = "Explosive")
};

/**
 * QTE push rating.
 */
UENUM(BlueprintType)
enum class EQTERating : uint8
{
    Perfect UMETA(DisplayName = "Perfect"),
    Miss    UMETA(DisplayName = "Miss")
};

/**
 * Game state — mirrors Unity's GameState enum.
 */
UENUM(BlueprintType)
enum class EBoulderGameState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Countdown   UMETA(DisplayName = "Countdown"),
    Playing     UMETA(DisplayName = "Playing"),
    Won         UMETA(DisplayName = "Won"),
    Lost        UMETA(DisplayName = "Lost")
};
