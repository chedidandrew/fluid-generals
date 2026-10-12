# Build the Cnc240 preview

Upstream: https://github.com/TheSuperHackers/GeneralsGameCode
Revision: ba7a17a684d6466a49f42652838f81ff90d9bd1b
Source modifications are already applied in EngineSource.zip; do not apply the
included comparison patch again. New Cnc240 headers are included in the archive.

Build tools used: Visual Studio 2022 Community, MSVC 14.44.35207, Windows SDK,
CMake 3.31.6, x86 Release. Enable Desktop development with C++.

From the extracted source directory:

```powershell
cmake -S GeneralsGameCode -B build -G "Visual Studio 17 2022" -A Win32 `
  -DRTS_BUILD_CORE_TOOLS=OFF -DRTS_BUILD_ZEROHOUR_TOOLS=OFF `
  -DRTS_BUILD_GENERALS_TOOLS=OFF -DRTS_BUILD_OUTPUT_SUFFIX=240 `
  -DRTS_DEBUG_LOGGING=OFF -DRTS_DEBUG_CRASHING=OFF `
  -DRTS_DEBUG_STACKTRACE=OFF -DRTS_DEBUG_PROFILE=OFF `
  -DRTS_MEMORYPOOL_DEBUG=OFF -DRTS_MEMORYPOOL_DEBUG_BOUNDINGWALL=OFF
cmake --build build --config Release --target g_generals z_generals --parallel 8
```

CMake fetches dependencies pinned by the source and needs Git/network access on
first configuration. Their fetched source trees, including the legacy zlib tree,
are also included in `ThirdParty` for inspection and reuse. The upstream single-call
LZHL and zlib population functions do not accept the usual source-directory overrides.
Do not run `cmake --install` against an original game directory. Use the supplied
installer to keep the replacement engines and profiles isolated.

Outputs: `build/Generals/Release/generalsv240.exe` and
`build/GeneralsMD/Release/generalszh240.exe`.

The source uses maintained upstream fixes and interpolation. Local changes enable
independent simulation timing by default, clamp rendering to 30–240, preserve game
speed and script timing commands, add `-renderfps`, and isolate profiles through
`CNC240_USER_DATA`. The resource scripts use Windows SDK headers instead of MFC.
Stock Generals X/Y particle fields are accepted even when their visual rotations
are disabled by upstream. Debug-only data assertion dialogs and allocation guard
instrumentation are disabled in the playable Release configuration.
An opt-in `CNC240_VERIFY_SECONDS=1..120` environment setting prints cumulative
render-loop/logic counts and successful Direct3D frame presentations, then quits
after that interval for runtime checks.
With that setting absent, normal gameplay has no verification time limit.
For a fixed-seed in-game check, additionally set `CNC240_VERIFY_SCENARIO` to
`USA`, `GLA`, `China`, or `skirmish`. The helper starts the first campaign mission
or a human/medium-AI Alpine Assault match, reports simulation CRC at frame 300,
then exits after the verification interval. These environment settings are for
testing; the normal launchers do not set them. Bounded campaign checks skip briefing
movies to enter the simulation directly. Normal gameplay retains its briefing movies.

Manual camera zoom permits up to 900 height units via Cnc240Config.h. The height
clamp and terrain-following constraint check apply that extension only while the
user controls the camera. Stock initial zoom and scripted limits are retained.
With bounded verification enabled, CNC240_VERIFY_ZOOM=1 exercises the manual
zoom clamp and reports its settled height in a skirmish check.

Extended manual zoom sizes the terrain patch from view-plane rays at the minimum
and maximum legal terrain heights. The same footprint centers the patch; dimensions
are rounded to 32-cell tiles with panning margin and clamped to actual map extents.
The enlarged allocation is retained while panning at extended zoom, and normal
zoom restores the stock size. Far clipping follows the updated terrain draw region.
LeftHUDInput captures left-button gestures; normal selected-unit move clicks are
deferred until release, and drags issue camera actions without unit move messages.
Explicit GUI targeting commands retain their original immediate-click behavior.
Wheel input and screen-edge scrolling have 3x multipliers in LookAtXlat.cpp.
Manual camera zoom settling performs three ordinary adjustment steps per update;
scripted camera adjustment retains the original rate. Frame-pacer scaling remains.
Bounded zoom verification also exercises terrain coverage with rotated/edge views,
minimap dragging through the window dispatcher, ordinary move clicks and the faster
wheel/edge controls. Normal launchers never enable these diagnostics.

This modern compiler build does not claim CRC compatibility with EA retail builds.
See the upstream wiki's replay testing documentation and the accompanying runtime
validation report. The unchanged engine and dependency license notices are retained
in the source archive. Engine derivatives retain GPL plus the additional EA terms.

The included ClientInstance source enables concurrent local games by default and assigns separate writable profiles. This does not require a debug multi-instance build option.
