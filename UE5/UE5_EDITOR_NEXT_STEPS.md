# UE5 Editor Next Steps (Strength + Rowing)

## Purpose
This is the single execution runbook for the next 19 hours.
Use it instead of splitting attention across multiple plan docs.

## Current Reality (Verified from Code)
- UE5 C++ gameplay shells exist for both demos:
  - Strength: `ABoulderGameMode`, `ABoulderActor`
  - Rowing: `ARowingGameMode`, `ARowingProgressActor`
- PM5 bridge integration exists in UE5 via `UErgManagerComponent`.
- Wired CSV and BLE JSON paths are both implemented.
- UE5 content folders are mostly empty right now (`Content/Maps`, `Content/Blueprints`), so editor wiring is the main gap.

## Simplified Rules (Do Not Break These Tonight)
1. Use one Unreal project: `UE5/StrengthERG.uproject`.
2. Use one transport host per map: one actor with `UErgManagerComponent`.
3. Validate wired mode first, BLE second.
4. Keep UI minimal until telemetry is confirmed live.
5. No architecture refactors before demo lock.

## Phase 1: Build and Boot (Hour 0-2)
1. Open `UE5/StrengthERG.uproject` on Windows.
2. Let Unreal compile C++ modules.
3. If prompted to rebuild, choose Yes.
4. Confirm editor opens with no compile errors in Output Log.
5. Verify `ErgBridge/ErgBridge.exe` exists at repo root.
6. Verify `ERGBridgeBLE/ErgBridgeBLE.exe` exists. If missing, build it once with:
   - `dotnet build -c Release` inside `ERGBridgeBLE`.

Exit criteria:
- UE editor opens.
- C++ build is green.
- Both bridge executables exist.

## Phase 2: Create Shared Base Assets (Hour 2-4)
1. Create map `L_StrengthDemo` in `Content/Maps`.
2. Create map `L_RowingDemo` in `Content/Maps`.
3. Create blueprint actor `BP_ErgManagerHost` in `Content/Blueprints`.
4. Add component `ErgManagerComponent` to `BP_ErgManagerHost`.
5. Default `ErgManagerComponent` settings:
   - `BridgeExePath = ErgBridge/ErgBridge.exe`
   - `BridgePort = 6789`
   - `bUseBleWireless = false`
   - `bSimulateInput = false`
6. Place one `BP_ErgManagerHost` actor in each map.

Exit criteria:
- Both maps exist.
- Shared ERG manager host exists and is placed in both maps.

## Phase 3: Strength Demo Wiring (Hour 4-8)
1. In `L_StrengthDemo`, set World Settings -> GameMode Override to `ABoulderGameMode` (or BP subclass).
2. Place one `ABoulderActor` (or BP subclass) in the level.
3. In GameMode details, assign `Boulder` reference if not auto-found.
4. For first pass, enable GameMode `bSimulateInput = true` and test Spacebar.
5. Press Play:
   - Confirm state transitions Idle -> Countdown -> Playing.
   - Confirm boulder moves upward when simulated reps fire.
6. Disable simulate input, keep ERG manager real hardware mode.
7. Connect PM5 wired and press Play.
8. Verify Output Log shows strength rep activity and boulder response.

Minimal UI for demo:
- Add one widget with text bindings for:
  - game state
  - reps
  - current power
  - time remaining

Exit criteria:
- Strength loop responds to real PM5 input.
- Demo can run end-to-end with readable status.

## Phase 4: Rowing Demo Wiring (Hour 8-13)
1. In `L_RowingDemo`, set GameMode Override to `ARowingGameMode` (or BP subclass).
2. Place one `ARowingProgressActor` in the level.
3. In GameMode details, assign `ProgressActor` if not auto-found.
4. Start with GameMode `bSimulateInput = true` and test Spacebar.
5. Confirm progress actor advances and drift logic works.
6. Set rowing tune values:
   - `TimeLimitSeconds = 180`
   - `MetersPerWatt = 0.05`
   - `MinSPMForProgress = 14`
   - `DriftDelaySec = 2.0`
   - `DriftMetersPerSecond = 1.2`
   - `IdealLowSPM = 20`
   - `IdealHighSPM = 28`
   - `IdealLowPaceSeconds = 120`
   - `IdealHighPaceSeconds = 180`
7. Disable simulate input and test real PM5 wired path.
8. Verify Output Log prints rowing stroke lines with progress movement.

Minimal UI for demo:
- Add one widget with text/progress bindings for:
  - current SPM
  - pace
  - power
  - elapsed/remaining time
  - progress percent

Exit criteria:
- Rowing loop moves from real PM5 data.
- Win/lose condition is visible.

## Phase 5: BLE Backup Path (Hour 13-15)
Use only after wired is stable.

1. Duplicate each map's `BP_ErgManagerHost` into BLE variant or toggle in-place.
2. Set:
   - `bUseBleWireless = true`
   - `BridgePort = 6790` (or leave default and rely on BLE flag behavior)
3. Ensure no other app is connected to PM5 BLE.
4. Play and confirm bridge logs show PM5 discovery and connection.
5. Verify live telemetry reaches both game modes.

Exit criteria:
- BLE can be demonstrated as fallback transport.

## Phase 6: Demo Lock (Hour 15-19)
1. Keep exactly two launcher maps:
   - `L_StrengthDemo`
   - `L_RowingDemo`
2. Add a simple front menu level with two buttons, or use editor map list.
3. Freeze tuning values once both loops are stable.
4. Record a 30-second dry-run script for each demo.
5. Do a full restart test (cold boot) before handoff.

Exit criteria:
- Both demos launch reliably.
- Both respond to live PM5 data.
- Operator has a repeatable run sequence.

## Known Technical Debt (Accept for Demo)
- Rowing currently reuses generic `FErgData` fields with temporary semantic mapping.
- HUD polish is intentionally minimal.
- No deep subsystem refactor before deadline.

## Quick Troubleshooting
- No telemetry in UE:
  1. Confirm bridge exe exists and launches.
  2. Confirm only one `UErgManagerComponent` is active in map.
  3. Confirm PM5 workout is started (device awake and active).
  4. Check Output Log for connect/reconnect messages.
- Wired fails but BLE works:
  1. Check cable and PM5 USB mode.
  2. Verify no stale bridge process is holding port 6789.
- BLE fails:
  1. Ensure phone/other app is disconnected from PM5.
  2. Retry with PM5 awake and in active workout.

## Tomorrow Morning Demo Order
1. Wired strength demo.
2. Wired rowing demo.
3. BLE fallback only if requested.
