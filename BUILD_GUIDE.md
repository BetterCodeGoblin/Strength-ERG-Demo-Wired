# StrengthERG UE5 Integration - Build & Setup Guide

## Project Structure

```
StrengthERG-UE5/
├── StrengthERG.sln              # Master solution (C# bridges + UE5 C++)
├── StrengthERGDemo.sln          # UE5-only solution
├── ERGBridge/                   # C# bridge for Concept2 USB (HID)
│   ├── ErgBridge.csproj
│   ├── ErgBridge.exe (compiled)
│   ├── ErgBridge.dll
│   └── PM3*.dll (Concept2 USB drivers)
├── ERGBridgeBLE/                # C# bridge for Concept2 Bluetooth LE
│   ├── ErgBridgeBLE.csproj
│   ├── Program.cs
│   ├── ErgBridgeBLE.exe (compiled)
│   └── ErgBridgeBLE.dll
├── Source/StrengthERGDemo/      # UE5 C++ game module
│   ├── Public/ (headers)
│   └── Private/ (implementation)
├── Intermediate/ProjectFiles/   # VS project files
├── Binaries/                    # Compiled game binaries
└── Config/                      # UE5 configuration files
```

## Building the Project

### 1. Build C# Bridges (Optional - Pre-compiled DLLs included)

```bash
# From PowerShell in the project root
cd ERGBridge
dotnet build -c Release
cd ..

cd ERGBridgeBLE
dotnet build -c Release
cd ..
```

This creates:
- `ERGBridge/bin/Release/net8.0-windows/win-x64/ErgBridge.exe`
- `ERGBridgeBLE/bin/Release/net8.0-windows/win-x64/ErgBridgeBLE.exe`

### 2. Build UE5 C++ Project

**Option A: Using Visual Studio**
1. Open `StrengthERG.sln` in Visual Studio 2022
2. Select **StrengthERGDemo** project
3. Build Configuration: **Development Editor|Win64**
4. Build → Build Solution (Ctrl+Shift+B)

**Option B: Using Unreal Editor**
1. Open `StrengthERGDemo.uproject` with UE5.6
2. When prompted, click "Yes" to rebuild C++ code
3. Visual Studio will compile the module

**Option C: Using Command Line**
```bash
cd C:\Users\jsypherd.ASURITE\StrengthERG-UE5
"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -Project="StrengthERGDemo.uproject" ^
  -SkipBuild ^
  -EditorTest
```

## Runtime Architecture

### Data Flow:
```
Concept2 PM5 (USB/BLE)
    ↓
[C# Bridge: ErgBridge.exe / ErgBridgeBLE.exe]
    ↓
TCP/IP Stream (JSON or binary)
    ↓
[UE5 C++: ErgManagerComponent]
    ↓
Game Logic (BoulderGameMode, QTEComponent, etc.)
```

### Port Configuration:
- **ErgBridge**: Port 6789 (USB HID protocol)
- **ErgBridgeBLE**: Port 6790 (Bluetooth LE protocol)
- **UE5 default in this branch**: 6790, because the active target workflow for this project is BLE-first

## Recommended Runtime Path (StrengthERG Project)

For this project, use the same practical methodology as the Unity version:
1. Start `ERGBridgeBLE.exe`
2. Confirm it detects the StrengthERG/PM5 and starts its TCP server on `127.0.0.1:6790`
3. Launch Unreal
4. Let `UErgManagerComponent` connect to `127.0.0.1:6790`

If Unreal still tries to use `6789`, you are either running stale binaries or an out-of-date class default. Rebuild the UE project and reopen the editor.

## Current Known Limitation

The UE repo is now aligned to the BLE-first port and startup flow, but the exact payload contract between `ERGBridgeBLE.exe` and `UErgManagerComponent` should still be verified against live runtime output if reps are not appearing in gameplay.

## Integration Points

### C++ ↔ C# Bridge Communication

The `ErgManagerComponent` (in [Source/StrengthERGDemo/Public/ErgManagerComponent.h](Source/StrengthERGDemo/Public/ErgManagerComponent.h)) handles:
1. **Launching** the appropriate bridge (USB or BLE)
2. **Connecting** via TCP to receive data
3. **Parsing** rep/stroke events
4. **Notifying** gameplay systems (BoulderActor, QTEComponent)

### Key Classes:

| C++ Class | Purpose |
|-----------|---------|
| `AErgManagerComponent` | TCP client for bridge communication |
| `ABoulderActor` | Game boulder - responds to force data |
| `ABoulderGameMode` | Manages game state and round progression |
| `UQTEComponent` | Quick-Time Event system |
| `FErgGameTypes.h` | Shared data structures (rep events, etc.) |

## Development Workflow

### 1. Modify C# Bridge
```bash
# Edit Program.cs in ERGBridge or ERGBridgeBLE
dotnet build -c Release
# New .exe appears in bin/Release/.../
```

### 2. Modify UE5 C++ Code
```bash
# Edit .cpp/.h files in Source/StrengthERGDemo/
# Use Visual Studio: Ctrl+Shift+B to compile
# Or Unreal Editor: Tools → Compile
```

### 3. Test Game
```bash
# In Unreal Editor
# Click Play or Package → Build
```

## Troubleshooting

### Bridge won't connect
- Ensure Concept2 device is powered on
- Check PM5 device pairing (BLE only)
- Verify TCP port (6789/6790) isn't blocked by firewall

### C++ Build Errors
- Regenerate Visual Studio files: Delete `Intermediate/ProjectFiles/`, rebuild
- Ensure UE5.6 is selected in `StrengthERGDemo.uproject`
- Check Windows SDK version: Visual Studio may need C++ dev kit

### Rebuild Everything
```bash
# Full clean rebuild
rm -r Intermediate, Binaries, obj, bin
dotnet clean
# In Visual Studio: Build → Clean Solution
# In Visual Studio: Build → Rebuild Solution
```

## Next Steps

1. ✅ Open `StrengthERG.sln` in Visual Studio 2022
2. ✅ Build the solution (C# bridges + UE5 C++)
3. → Implement ERG event handlers in gameplay code
4. → Test with real Concept2 device
5. → Package for distribution

---

**Questions?** Check the README.md in the project root for more details on game mechanics.
