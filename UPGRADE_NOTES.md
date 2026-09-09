# WaveMaker — Unreal Engine 5.8.2 upgrade

This is the active project, upgraded from UE 5.1.1 to UE 5.8.2. The pre-upgrade version is preserved in Git history at commit `0b07fef`.

Open `WaveMaker.uproject` with UE 5.8.2. Generated C++ solution: `WaveMaker.sln`. In Visual Studio 2022, select `Development Editor` and `Win64`, then build the `WaveMaker` project under Games with Ctrl+Shift+B. Set Games > WaveMaker as the startup project before pressing F5 to launch the editor. `Automation_WaveMaker.sln` contains engine automation tooling.

## Build commands

Run from this directory in PowerShell:

```powershell
.\Build-UE58.ps1 -Action Generate
.\Build-UE58.ps1 -Action Editor
.\Build-UE58.ps1 -Action Package
```

The script defaults to `C:\Program Files\Epic Games\UE_5.8`; pass `-EngineRoot` for another installation. It copies the project-local compiler configuration into Unreal's Saved directory, overriding the machine's obsolete MSVC 14.34 pin without changing the global configuration. The verified toolchain is VS 2022 MSVC 14.44.35222 with Windows SDK 10.0.26100.0. The engine's bundled .NET 10 runtime runs the build tools.

The packaged Windows application is written to `Builds\UE5.8.2\Windows`. Keep the entire packaged directory together; its executable depends on the adjacent content and DLLs.

## Changes

- Engine association 5.8, BuildSettingsVersion.V7, and Unreal5_8 include order.
- Removed UE 5.1 installation-specific includes and migrated texture access to GetPlatformData().
- Updated the custom shader plugin's dependencies, loading phase, GPU profiling API, RDG parameter capture, and readback byte count.
- Replaced editor-only DesktopPlatform dialogs with Windows runtime file dialogs for Shipping builds.
- Loaded netCDF through the primary game module and staged its five required DLLs. Removed absolute installation paths and process-wide PATH mutations.
- Corrected unsafe netCDF size_t writes and validated metadata/data reads and failure cleanup.
- Initialized graph projection values and checked export bounds.
- Updated two obsolete renderer configuration values and resaved all 51 project assets using UE 5.8.2.

## Validation

- Visual Studio project generation: passed.
- Editor C++ build: passed.
- Asset conversion: 51/51 packages resaved, zero errors and zero warnings.
- Windows Shipping build, cook, stage, and archive: passed (AutomationTool exit code 0).
- Packaged application startup and native Open Data File dialog: inspected successfully.
- Functional testing: the user checked the application and confirmed everything works. Scripted end-to-end LST/netCDF/export checks were not completed; the synthetic fixtures remain available for future regression testing.

Logs are in `Saved\UpgradeLogs`. Synthetic netCDF source/data and expected sample values are in `Saved\UpgradeValidation`.

The solution was regenerated after moving this project to Grapher UE 5.8.2. If the engine selector does not list 5.8, click its browse button and select the engine root `C:\Program Files\Epic Games\UE_5.8`. The installed patch version is 5.8.2.
