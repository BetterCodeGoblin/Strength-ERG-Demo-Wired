# StrengthERG + Rowing Rescue Plan for Apr 28 Demo

## Reality

The fastest honest path for tomorrow is not a full Imagine Unreal rebuild.
It is a rescue build that reuses the already-working PM5 transport and the most complete gameplay shells.

## Recommended Demo Split

### Strength
- Primary path: existing `Strength-ERG-Demo-Wired` Unity scene
- Secondary path: `UE5/` branch `ue5-rebuild` if Windows UE editor wiring is already close
- Reason: Unity scene already has HUD, boulder loop, camera, and PM5 plumbing

### Rowing
- Primary path: add a minimal rowing rescue scene/loop inside `Strength-ERG-Demo-Wired`
- Uses the same existing `ErgBridge/ErgBridge.exe` transport through `ErgManager`
- New script added: `Assets/Scripts/Game/RowingRescueManager.cs`
- Reason: old Imagine rowing logic existed, but there was no scene to run it

## What RowingRescueManager Does
- Starts a session on first detected stroke, or via UI button
- Reads live `StrokeRate`, `PaceSeconds`, and `PowerWatts` from `ErgManager`
- Converts watts into forward progress
- Requires minimum SPM to progress
- Applies a light cadence bonus for staying in a good SPM + pace band
- Drifts backward if the rower stops too long
- Ends on target distance or time limit

## Fast Wiring Instructions in Unity 2022.3.28f1

1. Open `/data/projects/unity/Strength-ERG-Demo-Wired`
2. Duplicate `Assets/Scenes/SampleScene.unity` into a new rowing rescue scene
3. In the duplicated scene:
   - disable or remove boulder-specific gameplay objects if they interfere
   - keep or duplicate the existing Canvas if useful
4. Create an empty GameObject named `RowingManager`
5. Add components:
   - `ErgManager`
   - `RowingRescueManager`
6. On `ErgManager`:
   - `simulateInput = false` for real PM5
   - `bridgeExePath = ErgBridge/ErgBridge.exe`
7. Add TMP text labels and wire them into `RowingRescueManager`:
   - `statusText`
   - `timerText`
   - `progressText`
   - `telemetryText`
   - `resultText`
8. Optional UI buttons:
   - Start button -> `RowingRescueManager.StartSession()`
   - Reset button -> `RowingRescueManager.ResetSession()`
9. Save scene and add to Build Settings

## Suggested Rowing Demo Tuning
- `sessionLengthSeconds = 180`
- `targetDistanceMeters = 250`
- `minSPMForProgress = 14`
- `metersPerWatt = 0.05`
- `driftDelaySeconds = 2.0`
- `driftMetersPerSecond = 1.2`
- `idealLowSPM = 20`
- `idealHighSPM = 28`
- `idealLowPaceSeconds = 120`
- `idealHighPaceSeconds = 180`

## Tonight's Validation Order

1. Validate PM5 cable connection with the existing Unity strength scene
2. Confirm `ErgBridge.exe` launches and telemetry changes live
3. Wire rowing scene with `RowingRescueManager`
4. Test rowing with simulation first
5. Test rowing on real hardware
6. Only use UE5 tomorrow if it is already editor-wired and stable

## Brutal Truth

If there is only time to make one engine stable, choose the engine with the existing runnable scene: Unity.
If both demos must work by 11 AM, shipping mixed-engine internal prototypes is better than pretending the Unreal reboot is demo-ready.
