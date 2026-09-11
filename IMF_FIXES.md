# IMF changes and desktop handoff

## Current state

The IMF C++ fixes and Blueprint wiring are saved in this repository for Unreal Engine 5.8.2. The latest Editor build succeeded. All six `WaveMaker.IMF` automation suites passed, including numeric expressions, import/calendar mapping, gaps and clock wrapping, averaging, time validation, and readable axis ticks. Changed Blueprints compiled without warnings.

The latest visual import/export checks are pending user verification. The packaged executable has not been rebuilt since these IMF changes.

## Changes

- Import preview supports configurable data and time columns, labels, units, coordinate frames, and per-column missing-value tokens. Shared starter profiles are in `Config/IMFImportProfiles`.
- Equation parsing handles numeric syntax, operator precedence, domain errors, and invalid samples. Intentional undefined/infinite periods remain gaps.
- Native curve registration now drives Blueprint toggles, legends, hover, painting, and refresh. Axis inputs are validated; averaging offers arithmetic and circular modes.
- Offscreen PNG export includes curves and markers without application controls. Resolution scaling and asynchronous image writing were corrected.
- Latest presentation fixes use rounded, responsive time ticks, seconds when zoomed in, and day offsets at rollover. Export has a bold centered title, darker dashed markers, dotted guide lines, and no metadata footer. The screen title is also bold and centered.

## Continue on the desktop

Install UE 5.8.2 and the Visual Studio C++ toolchain described in `UPGRADE_NOTES.md`, pull `main`, and run from the project directory:

```powershell
.\Build-UE58.ps1 -Action Generate
.\Build-UE58.ps1 -Action Editor
```

Use `-EngineRoot` if Unreal is installed elsewhere. Open `WaveMaker.uproject`, run Play, and verify the time labels and a newly exported PNG, including several vertical markers, grid visibility, title, and footer removal. Check that invalid-data gaps remain intact.

Once validated, create an updated standalone executable with `Build-UE58.ps1 -Action Package`.

Generated solutions, build outputs, caches, and `Saved` remain Git-ignored. Locally saved import-profile overrides in `Saved/IMFImportProfiles` are not shared. The two `.lst` samples are tracked; the supplied `image_20160113_2.txt` resides outside this repository and must be transferred separately if needed on the desktop.
