# UE5 Rowing Rescue Wiring

## Goal
Get a real PM5-backed rowing loop running in Unreal as fast as possible using the existing `UErgManagerComponent` bridge stack.

## New Classes Added
- `ARowingGameMode`
- `ARowingProgressActor`

## Core Assumption
The wired ErgBridge CSV already reaches `UErgManagerComponent` in this form:
- field 1 -> rate/SPM
- field 2 -> pace seconds per 500m
- field 3 -> power watts
- field 4 -> connected

Current C++ mapping in the rescue mode uses:
- `FErgData.RepTimeSec` as SPM
- `FErgData.ElapsedSeconds` as pace seconds
- `FErgData.PullDistance` as power watts

This is a temporary naming mismatch, but it lets us ship tomorrow without refactoring the whole telemetry type system tonight.

## Editor Steps

1. Open `StrengthERG.uproject` in Unreal on the Windows machine.
2. Let it compile.
3. Create a new test map, for example `L_RowingRescue`.
4. In World Settings, set GameMode Override to `ARowingGameMode` or a Blueprint subclass.
5. Place one actor in the level based on `ARowingProgressActor`.
6. Place or create an actor that contains `UErgManagerComponent`.
   - easiest path: create a plain Blueprint Actor named `BP_ErgManagerHost`
   - add `ErgManagerComponent`
7. On the ERG manager component:
   - `BridgeExePath = ErgBridge/ErgBridge.exe`
   - `BridgePort = 6789`
   - `bUseBleWireless = false` unless testing BLE
   - optional first pass: `bSimulateInput = true`
8. On the Rowing game mode or BP subclass, tune:
   - `TimeLimitSeconds = 180`
   - `MetersPerWatt = 0.05`
   - `MinSPMForProgress = 14`
   - `DriftDelaySec = 2.0`
   - `DriftMetersPerSecond = 1.2`
   - `IdealLowSPM = 20`
   - `IdealHighSPM = 28`
   - `IdealLowPaceSeconds = 120`
   - `IdealHighPaceSeconds = 180`
9. For instant testing with no PM5 yet:
   - enable `bSimulateInput = true` on `ARowingGameMode`
   - press Spacebar during Play
10. For real hardware test:
   - disable rowing sim mode
   - keep real ERG manager active
   - launch play, row on the machine, watch output log for `[RowingGame] Stroke #...`

## What Still Needs Blueprint/UI Work
- start/countdown screens
- progress bar widget
- current SPM/pace/power display
- win/lose panels
- camera/presentation polish

## Brutal Priority
If time is short, do not chase pretty UI.
Get this sequence working first:
1. project compiles
2. map loads
3. ERG bridge launches
4. real rowing strokes move progress
5. strength map still works
