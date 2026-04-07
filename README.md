# Strength ERG Demo — UE5 C++ Port

A UE5 5.3+ C++ port of the Unity boulder-push game driven by a Concept2 PM5 rowing ergometer.

## Architecture

```
ABoulderGameMode
  ├── UErgManagerComponent  — TCP socket to ErgBridge (localhost:6789)
  ├── UQTEComponent         — timing window / rhythm system
  └── drives → ABoulderActor (moves up/down path based on rep power)
```

## C++ Classes

| Class | File | Purpose |
|-------|------|---------|
| `UErgManagerComponent` | ErgManagerComponent.h/cpp | TCP client to ErgBridge; fires `OnNewRep` / `OnErgFrameReceived` events |
| `ABoulderActor` | BoulderActor.h/cpp | Moves along PathStart→PathEnd with smooth visual lerp & rolling rotation |
| `ABoulderGameMode` | BoulderGameMode.h/cpp | State machine: Idle → Countdown → Playing → Won/Lost |
| `UQTEComponent` | QTEComponent.h/cpp | "PUSH NOW" timing window (cycle-based + animation-driven) |
| `FErgGameTypes` | ErgGameTypes.h | Shared enums + structs (all USTRUCT/UENUM, Blueprint-accessible) |

## Data Structures (ErgGameTypes.h)

- `FErgFrameData` — live telemetry frame (rate, pace, power, connected)
- `FRepData` — single rep data (pull distance, rep time, computed power, power zone, QTE rating)
- `FGameStats` — end-of-run summary
- `EBoulderGameState` — Idle / Countdown / Playing / Won / Lost
- `EPowerZone` — Low / Moderate / Power / Explosive
- `EPushRating` — Perfect / Miss

## Setup in UE5 Editor

1. Clone repo and open `StrengthERGDemo.uproject` in UE5.3+
2. Generate project files (right-click `.uproject` → Generate Visual Studio project files)
3. Build from VS / Rider / `UnrealBuildTool`
4. Create a Blueprint subclass of `ABoulderGameMode` (e.g. `BP_BoulderGameMode`)
5. In the level, place:
   - A `ABoulderActor` (or Blueprint subclass) — assign PathStart and PathEnd actors
   - Set `BoulderActorRef` on the game mode
6. Set the game mode on the level/world settings
7. For keyboard testing: enable `bSimulateInput = true` and press **Space** to fire reps

## ErgBridge Protocol

ErgBridge runs on `localhost:6789`. The component sends `0x01` and receives:

```
"strokeRate,paceSeconds,powerWatts,connected[,repNum,repTimeSec,pullDistance]\n"
```

Optonal rep fields (5–7) are appended when a new rep is detected.

## Power Zones

| Zone | Threshold (PullDist / RepTimeSec) |
|------|----------------------------------|
| Explosive | ≥ 70 |
| Power | ≥ 45 |
| Moderate | ≥ 25 |
| Low | < 25 |

## Branch

This is the `ue5-rebuild-v2` branch — a full UE5 C++ rebuild.
The original Unity implementation lives on `main`.
