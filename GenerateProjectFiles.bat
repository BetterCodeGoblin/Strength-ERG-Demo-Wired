@echo off
setlocal enabledelayedexpansion

REM Generate Visual Studio project files for UE5 project
REM This script must be run from the project directory

set UE_PATH=C:\Program Files\Epic Games\UE_5.6
set PROJECT_FILE=%~dp0StrengthERGDemo.uproject

echo Generating Visual Studio project files...
echo UE Engine Path: !UE_PATH!
echo Project File: !PROJECT_FILE!

REM Call the engine's GenerateProjectFiles script
call "!UE_PATH!\Engine\Build\BatchFiles\GenerateProjectFiles.bat" "!PROJECT_FILE!" -Game

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Project files generated successfully!
    echo Solution file should be at: %~dp0StrengthERGDemo.sln
) else (
    echo.
    echo Error generating project files. Trying alternative method...
    REM Alternative: use RunUAT
    call "!UE_PATH!\Engine\Build\BatchFiles\RunUAT.bat" GenerateProjectFiles -Project="!PROJECT_FILE!" -Game
)

pause
