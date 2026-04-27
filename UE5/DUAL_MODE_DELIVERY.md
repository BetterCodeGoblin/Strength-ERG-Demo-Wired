# Unreal Dual-Mode Delivery Plan

## Target
By tomorrow at 11 AM, both demos live in the same Unreal project:
- Strength mode
- Rowing mode

Project:
- `/data/projects/unity/Strength-ERG-Demo-Wired/UE5`
- branch: `ue5-rebuild`

## Current Truth

### Strength in Unreal
Already present:
- `UErgManagerComponent`
- `ABoulderGameMode`
- `ABoulderActor`
- PM5 bridge support for wired and BLE

Still requires editor validation:
- level wiring
- HUD/widget hookup
- camera/presentation polish
- hardware smoke test on Windows machine

### Rowing in Unreal
Now added:
- `ARowingGameMode`
- `ARowingProgressActor`
- `ROWING_EDITOR_WIRING.md`

Current wired rowing telemetry interpretation:
- `RepTimeSec` -> SPM
- `ElapsedSeconds` -> pace seconds/500m
- `PullDistance` -> power watts

This is naming debt, not a blocker.

## Recommended Overnight Execution Order

1. Open UE5 project on the Windows machine
2. Compile C++ successfully
3. Verify existing strength map still runs
4. Create and wire rowing rescue map
5. Test rowing in simulation
6. Test rowing on real PM5 hardware
7. Do one pass of HUD/readability polish only after real data is moving

## Minimum Acceptable Demo State

### Strength
- real PM5 connection
- reps detected live
- boulder loop visibly responds
- end condition or progress feedback visible

### Rowing
- real PM5 connection
- strokes detected live
- progress actor advances from rower output
- time/progress/telemetry readable in-engine or log-backed debug HUD

## Non-Goals for Tonight
- perfect UI
- cleaned-up telemetry naming
- full Imagine art direction
- deep refactor into the rebooted Imagine Unreal repo

## Brutal Rule
If a choice appears between:
- cleaner architecture
- or a verified Unreal PM5 demo by morning

choose the verified demo.
