# Strength ERG Demo — UE5 Conversion Plan

**Branch:** `ue5-rebuild`  
**Source:** Unity 6 C# project (Boulder QTE + Concept2 PM5 StrengthErg)  
**Target:** Unreal Engine 5.3 C++ project

---

## Audit Summary

### Unity Project Structure

```
Assets/
  Scripts/
    Concept2/
      Concept2Native.cs     — P/Invoke DLL wrapper (PM3CsafeCP.dll, x86)
      CSAFECommands.cs      — CSAFE protocol constants
      ErgBridgeClient.cs    — TCP client (CSV lines from ErgBridge)
      ERGManager.cs         — Singleton MonoBehaviour, launches ErgBridge
    Game/
      BoulderController.cs  — Path movement, rollback, rolling rotation
      BoulderGameManager.cs — Game-state machine + QTE eval + rep handling
      BoulderHUD.cs         — TMP/uGUI HUD with panels + feedback popup
      CameraController.cs   — Third-person follow camera
      Concept2UsbReader.cs  — HidSharp USB + BLE bridge reader
      Concept2debughud.cs   — Debug overlay
      PlayerPusherController.cs — Animation controller for push character
      QTEController.cs      — Timing window cycle + EvaluatePush()
  Plugins/Concept2/x86/     — PM3CsafeCP.dll, PM3DDICP.dll, PM3USBCP.dll
  Packages/HidSharp.2.6.4/  — USB HID library (NuGet)
  Animations/               — FBX character + animator controller
  Scenes/SampleScene.unity
```

### Key Concepts Being Ported

| Unity Component | UE5 Equivalent | Notes |
|---|---|---|
| `ErgManager` (MonoBehaviour singleton) | `UErgManagerComponent` | Attach to GameMode or a persistent Actor |
| `ErgBridgeClient` (TCP MonoBehaviour) | `FErgReaderThread` (FRunnable) | Background thread, drains to game thread in Tick |
| `Concept2UsbReader` (HidSharp + BLE) | Merged into `UErgManagerComponent` | BLE mode: port 6790 JSON; USB mode: ErgBridge CSV |
| `Concept2Native` (P/Invoke DLL) | `Concept2NativeWrapper` (LoadLibraryW) | Optional; ErgBridge preferred for x64 UE5 |
| `BoulderController` | `ABoulderActor` | ✅ Ported |
| `BoulderGameManager` | `ABoulderGameMode` | ✅ Ported |
| `QTEController` | Inlined into `ABoulderGameMode` | ✅ Ported |
| `BoulderHUD` | UMG Blueprint Widget | Implement as WBP_BoulderHUD — Blueprint only |
| `PlayerPusherController` | `APusherCharacter` | To-do: port coroutine anims to UE5 montages |
| `CameraController` | `APlayerCameraManager` or Spring Arm | To-do: port LateUpdate → ViewTarget |

---

## Files Completed (this branch)

```
UE5/
  StrengthERG.uproject
  Source/StrengthERG/
    StrengthERG.Build.cs
    Public/
      ErgTypes.h              — FErgData, EErgPowerZone, EQTERating, EBoulderGameState
      ErgManagerComponent.h   — UErgManagerComponent + FErgReaderThread
    Private/
      StrengthERGModule.cpp
      ErgManagerComponent.cpp — TCP bridge reader (CSV + JSON/BLE)
    Game/
      Public/
        BoulderActor.h        — Path movement + rollback + rolling rotation
        BoulderGameMode.h     — Game-state machine + QTE logic
      Private/
        BoulderActor.cpp      — ✅ Full port of BoulderController.cs
        BoulderGameMode.cpp   — ✅ Full port of BoulderGameManager.cs + QTEController.cs
```

---

## Remaining Work

### 1. `APusherCharacter` (Medium)
Port `PlayerPusherController.cs`.  
- Unity coroutines → UE5 Animation Montages (one montage = one push clip)  
- `animator.Play(...)` → `PlayAnimMontage(PushMontage, Speed)`  
- `OnPushPeak` event → Anim Notify in the montage at 40% through  
- Hill-relative positioning → attach to a Socket on the boulder mesh

### 2. `UBoulderCameraActor` (Easy)
Port `CameraController.cs`.  
- `LateUpdate` follow → `APlayerCameraManager::UpdateViewTarget` override  
- Or: Spring Arm component on the boulder actor pointed behind/above  
- `FindObjectOfType<BoulderController>()` → direct reference set in BP

### 3. `WBP_BoulderHUD` — UMG Widget (Largest task)
Port `BoulderHUD.cs` entirely in Blueprints.  
- 5 panels → 5 named WidgetSwitcher slots or overlay panels  
- QTE bar: UProgressBar or custom material  
- Data binding: bind to GameMode functions (GetTimeRemaining, GetState, etc.)  
- Push feedback popup: animated via UWidgetAnimation (fade + float up)  
- Heart rate / ERG status: bind to ErgManagerComponent OnErgDataUpdated delegate

### 4. `WBP_DebugHUD` (Low priority)
Port `Concept2debughud.cs` — display raw FErgData fields.

### 5. `Concept2NativeWrapper` (Optional)
For direct USB access without ErgBridge:  
- `FPlatformProcess::GetDllHandle("PM3CsafeCP.dll")`  
- `FPlatformProcess::GetDllExport(Handle, "tkcmdsetDDI_init")`  
- Blocked by: PM3CsafeCP.dll is x86-only; UE5 editor is x64  
- Workaround: compile ErgBridge as x86, let it own the USB; UE5 talks to it via TCP  
- Recommendation: **Keep the bridge approach.**

### 6. Content / Assets
- Import FBX animations from `Assets/Animations/` into UE5 skeleton  
- Configure AnimBP with Idle → Push transitions  
- Set up Physics Asset for boulder mesh (sphere collider)  
- Replace Unity terrain with Landscape (or a simple BSP ramp for prototype)

### 7. Platform / Build
- Add `ErgBridge/ErgBridge.exe` to `<ProjectRoot>/ErgBridge/` (same relative path as Unity)  
- Package Step: include ErgBridge in staged files via `DefaultGame.ini` `[Staging]` section  
- Linux note: ErgBridge is Windows-only (uses Win32 USB HID); use BLE bridge on Linux

---

## Architecture Decisions

### Why ErgManagerComponent instead of a subsystem?
Unity's singleton `DontDestroyOnLoad` pattern maps to a UGameInstanceSubsystem.
However, `UActorComponent` was chosen for:
- Easier Blueprint wiring in the editor  
- Explicit lifecycle tied to level (EndPlay kills the bridge process safely)  
- Can be promoted to UGameInstanceSubsystem later if cross-level persistence needed

### Why inlined QTE?
`QTEController.cs` had no world presence (no position, no visual). In UE5, a pure-logic
class is either a subsystem, a library, or methods on the GameMode. The GameMode was
the right home — same tick budget, direct access to game state, no cross-actor delegates needed.

### HUD via UMG Blueprint vs C++?
`BoulderHUD.cs` is entirely data binding + color math. UMG Blueprint widget classes
handle this idiomatically in UE5. All the data is exposed via `BlueprintCallable`
functions on `ABoulderGameMode` and `UErgManagerComponent`, making full Blueprint wiring possible.

---

## Blockers

None blocking. ErgBridge binary must be present at runtime for hardware connection.
