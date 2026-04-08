# StrengthERG UE5 Port - Final Setup Guide

## Overview
All C++ systems are now compiled and integrated. This guide covers the final setup steps in the Unreal Editor to complete the 1:1 port. Most steps are configuration-based; no additional coding required.

**Estimated Time:** 30-45 minutes  
**Prerequisites:** 
- UE 5.6 Editor installed
- Project compiled successfully (`StrengthERGDemo` game target built)
- EditorPref.ini may need Live Coding disabled if it blocks editor launch

---

## Part 1: Open Project in Editor

1. **Close any running Unreal Editor instances** (Live Coding can block editor builds)
2. **Launch the Unreal Editor:**
   ```bash
   "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe" \
     "c:\Users\jsypherd.ASURITE\StrengthERG-UE5\StrengthERGDemo.uproject"
   ```
3. Once editor opens, you may see compilation warnings. **Let it finish compiling** (this is normal - it's the editor target).
4. Once stable, open the **Content Browser** (if not visible: Window → Content Browser)

---

## Part 2: Create & Configure Level

### Step 2.1: Open or Create BoulderPush Map
1. In Content Browser, navigate to **Content/Maps** folder (create if doesn't exist)
2. If `BoulderPush.umap` exists, **double-click to open it**
3. If not:
   - Right-click in Content Browser → **New Level**
   - Name it `BoulderPush`
   - Save it in **Content/Maps** folder

### Step 2.2: Configure World Settings
1. In the Level Editor toolbar: **Window → World Settings**
2. Under **Game Mode**, set:
   - **GameMode Class:** `BP_BoulderGameMode` (or `ABoulderGameMode` if BP doesn't exist)
   - **HUD Class:** Leave as default (HudManager spawns from GameMode)
   - **Player Controller Class:** `PlayerController` (default)
   - **Pawn Class:** `Pawn` or leave default

3. Close World Settings panel

---

## Part 3: Create Scene Geometry & Path Markers

### Step 3.1: Create a Simple Hill/Slope
1. In the main viewport, place a landscape or use basic BSP:
   - **Place Actor** (drag from Content Browser) → **Cube** or create a slope mesh
   - Scale to represent a hill (e.g., 400x100 units, tilted)
   - Material: Use a simple material (grey works fine)

**Alternative:** Use a pre-made ramp asset if available

### Step 3.2: Create Path Markers (Empty Actors)
These define the start and end points for boulder movement.

1. **Place PathStart Actor:**
   - Drag **Actor** from Content Browser into viewport
   - Position at the **BASE of the hill**
   - Position height on ground level (Z≈0)
   - Rename in Outliner: `PathStart_Actor`

2. **Place PathEnd Actor:**
   - Drag another **Actor** into viewport
   - Position at the **TOP of the hill**
   - Align Z height to where you want the boulder to stop
   - Rename: `PathEnd_Actor`

**Tip:** Use **Ctrl+Home** to focus on actors in the viewport

---

## Part 4: Place Boulder Actor

### Step 4.1: Create Boulder Actor
1. **Place an instance of ABoulderActor:**
   - Open C++ Classes in Content Browser → **StrengthERGDemo** → Search for **ABoulderActor**
   - Right-click → **Create Blueprint** → Name it `BP_Boulder`

2. **Configure Boulder Mesh:**
   - Open `BP_Boulder`
   - In Details panel, find **Mesh Component**
   - Set **Static Mesh** to `Engine/BasicShapes/Sphere` (or a custom boulder mesh)
   - Adjust scale (e.g., 2.0x)

3. **Save and place in level:**
   - Close BP_Boulder editor
   - Drag `BP_Boulder` into the level viewport
   - Position near **PathStart** position

### Step 4.2: Wire Path Markers to Boulder
1. **Select BP_Boulder in the level**
2. In Details panel (bottom right), find **Boulder** section:
   - **PathStart:** Select `PathStart_Actor` from dropdown
   - **PathEnd:** Select `PathEnd_Actor` from dropdown
   - **PowerScale:** Leave as `0.00028` (default)
   - **MaxPushPerRep:** Leave as `0.10` (default)
3. **Save the level** (Ctrl+S)

---

## Part 5: Setup GameMode & References

### Step 5.1: Configure GameMode Blueprint
1. In Content Browser, find or create **BP_BoulderGameMode:**
   - If exists: Right-click → **Edit Blueprint**
   - If not: Right-click → New Blueprint → Parent: `ABoulderGameMode` → Name: `BP_BoulderGameMode`

2. In the Blueprint editor, find the **Details** panel (right side) and set:
   - **Boulder Actor Ref:** Select your `BP_Boulder` instance from the level
   - **TimeLimitSeconds:** `90.0` (default)
   - **RollbackDelaySec:** `2.5` (default)
   - **Countdown Duration:** `3.0` (default)
   - **Power Zones:** Keep defaults (Explosive=70, Power=45, Moderate=25)

3. **Compile** and **Save**

### Step 5.2: Assign GameMode to World
1. In the opened level, go to **Window → World Settings**
2. Set **GameMode Class** to `BP_BoulderGameMode`
3. **Save the level**

---

## Part 6: Create Push Animation System

### Step 6.1: Create a Basic Animation Montage
For a quick 1:1 port, you can use a placeholder montage. For a full demo, you'd need an actual skeleton and animation.

1. **Create Montage (Placeholder Method):**
   - In Content Browser, create folder: **Content/Animations**
   - Right-click → **Create Blueprint** → Name: `AM_Push`
   - Change parent class to `AnimMontage`
   - Set **Montage Length** to `0.45` seconds (matches default rep time)
   - **Add Notify:** At 0.18 seconds (40% of 0.45), add a notify named `AnimationPeak`
   - Save

**Advanced Method (if you have a skeletal character):**
- Create a `Character` Blueprint with a skeletal mesh
- Create animation sequences for push motion
- Create montage from those sequences
- Add `AnimationPeak` notify at 40% mark

### Step 6.2: Assign Montage to GameMode
1. Open **BP_BoulderGameMode** Blueprint
2. In Details, find **Push Animation Montage** property
3. Select **AM_Push** (the montage you created)
4. **Compile** and **Save**

---

## Part 7: Create Player Character Blueprint

### Step 7.1: Create Character Blueprint
1. In Content Browser, create folder: **Content/Characters**
2. Right-click → **New Blueprint** → Parent: `APlayerCharacter` → Name: `BP_PlayerCharacter`
3. In the Blueprint editor:
   - **Add Component:** Skeletal Mesh Component
   - Assign a default skeleton (e.g., `SK_Mannequin_Female` from Engine)
   - Set **Anim Class** if using skeletal mesh
4. **Compile** and **Save**

The character will spawn automatically when the level starts (GameMode.BeginPlay).

---

## Part 8: Test Basic Game Flow

### Step 8.1: Play in Editor
1. Click **Play** button in the Level Editor toolbar
2. You should see:
   - Game starts in **Idle** state
   - HUD displays "PUSH THE BOULDER" text
   - No errors in Output Log

### Step 8.2: Test Auto-Start
1. Press **Space** (simulated rep) to trigger a push
2. Verify:
   - Countdown starts (3...2...1...GO!)
   - After countdown, game enters **Playing** state
   - Boulder remains visible and doesn't crash

### Step 8.3: Test Gameplay Loop
1. Repeatedly press **Space** to simulate reps
2. Verify:
   - Boulder progress increases with each rep
   - HUD updates: rep counter, timer counts down
   - Boulder reaches 100% → **Win screen** appears
   - Or timer hits 0 → **Lose screen** appears

---

## Part 9: Connect to Real ERG Bridge (Optional)

If you have ErgBridge or ErgBridgeBLE running:

1. **Start the bridge process:**
   ```bash
   # USB connection (Concept2 PM5 direct via bridge)
   .\ErgBridge.exe

   # Or BLE connection
   .\ErgBridgeBLE.exe
   ```

2. **In-game:**
   - Bridge connects on localhost:6790
   - After first ERG rep arrives, countdown auto-starts
   - HUD shows "ERG connected" status

3. **If bridge isn't running:**
   - Game uses **simulation mode** (SpaceBar triggers reps)
   - HUD shows "SIM MODE" status
   - Gameplay fully functional for testing

---

## Part 10: Verification Checklist

Run through this list to verify 1:1 parity with Unity:

### Game Flow
- [ ] Start screen displays "PUSH THE BOULDER"
- [ ] First rep (simulated or real) auto-starts countdown
- [ ] Countdown shows 3...2...1...GO!
- [ ] Game enters playing state
- [ ] Timer counts down (format MM:SS.0)
- [ ] Timer turns red warning <15 seconds remaining

### Boulder Physics
- [ ] Each rep advances boulder progress
- [ ] Progress formula: `Gain = Clamp(power * 0.00028 * multiplier, 0, 0.10)`
- [ ] Boulder rolls back when idle >2.5 seconds
- [ ] Boulder reaches 100% → displays win screen
- [ ] Boulder doesn't reach 100% before timer expires → displays lose screen

### QTE System
- [ ] QTE bar shows cycle progress (0-1)
- [ ] QTE bar is green when window open, red when closed
- [ ] "PUSH NOW!" text appears when window open
- [ ] Perfect bonus (1.5x) applied when pushing in window
- [ ] Miss bonus (0.55x) applied when pushing outside window

### UI Feedback
- [ ] Rep counter increments on each push
- [ ] Combo counter visible when combo > 1
- [ ] Power zone displays current rep power + zone classification
- [ ] Power zone colors match design: red (explosive), orange (power), yellow (moderate), grey (low)
- [ ] Push feedback popup shows ("PERFECT POWER!" / "GOOD TRY MODERATE!")
- [ ] Feedback popup floats upward and fades

### End Screens
- [ ] Win screen shows: total reps, perfects, time, avg/peak power
- [ ] Lose screen shows: % progress, reps, avg/peak power
- [ ] Restart button or Space key to restart game

### ERG Integration (if connected)
- [ ] ErgBridge.exe connects on localhost:6790
- [ ] Real rep data parsed correctly
- [ ] Power zones computed correctly
- [ ] HUD shows "ERG connected" + heart rate (if available)

---

## Part 11: Troubleshooting

| Issue | Solution |
|-------|----------|
| **Compile errors in editor** | Close editor, ensure game built successfully, delete Intermediate folder, rebuild |
| **Boulder doesn't appear** | Check PathStart/PathEnd are assigned and at different locations |
| **HUD text too small/large** | Adjust canvas draw functions in `HudManager.cpp` DrawCenteredText parameters |
| **No animation on push** | Assign `AM_Push` montage to GameMode.PushAnimationMontage |
| **Reps not detected** | Check ErgBridge.exe is running and reachable at 127.0.0.1:6790, or use Space key simulation |
| **Game crashes on start** | Check Output Log for specific errors; likely missing asset references |

---

## Part 12: Next Steps for Polish

**Optional enhancements (out of scope for 1:1 port):**
- Add sound effects (push impact, QTE window open/close, win/lose fanfare)
- Add particle effects (push impact, boulder wobble accentuation)
- Implement screen shake on perfect reps
- Add background music
- Implement custom camera controller (follows boulder with look-ahead)
- Replace placeholder montage with full skeletal animation

---

## Part 13: Commit Progress

Once everything is working:

```bash
git add -A
git commit -m "feat: complete UE5 port with C++ character, HUD, and game wiring

- Implement APlayerCharacter with animation syncing
- Implement AHudManager for all UI display (5 screens/panels)
- Wire GameMode to spawn character and HUD at BeginPlay
- Configure BoulderPush level with path markers and boulder
- Create push animation montage with peak notify
- Verify 1:1 parity with Unity original reference implementation"
```

---

## Success Criteria

The port is **complete and 1:1** when:
1. ✅ Game launches and enters Idle state
2. ✅ First rep auto-starts countdown
3. ✅ Gameplay functions with correct physics (power scale, rollback, QTE bonuses)
4. ✅ HUD displays all 5 panels with correct data (timer, reps, combo, power, progress)
5. ✅ Win/lose screens appear at correct conditions
6. ✅ All color schemes match design specification
7. ✅ Works with real ERG data OR simulation mode
8. ✅ No crashes or console errors in typical gameplay

---

**Your C++ implementation is complete. The rest is editor configuration!**
