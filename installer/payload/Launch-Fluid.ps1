[CmdletBinding()]
param(
    [ValidateSet('Generals','ZeroHour')][string]$Game = 'ZeroHour',
    [ValidateRange(0,240)][int]$RenderFPS = 0,
    [ValidateRange(0,240)][int]$SimulationHz = 0,
    [string]$AssetRoot = '',
    [ValidateSet('Saved','Fullscreen','Borderless','Windowed')][string]$DisplayMode = 'Saved',
    [ValidateRange(0,7680)][int]$Width = 0,
    [ValidateRange(0,4320)][int]$Height = 0,
    [switch]$Fullscreen,
    [switch]$Diagnostics,
    [switch]$Inspect
)
$ErrorActionPreference = 'Stop'
$project = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not ('CncAdaptiveDisplay' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CncAdaptiveDisplay {
    [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr window);
    [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr window, IntPtr dc);
    [DllImport("gdi32.dll")] static extern int GetDeviceCaps(IntPtr dc, int index);
    public static int RefreshHz() {
        IntPtr dc = GetDC(IntPtr.Zero);
        if (dc == IntPtr.Zero) return 60;
        try { int hz = GetDeviceCaps(dc, 116); return hz > 1 ? hz : 60; }
        finally { ReleaseDC(IntPtr.Zero, dc); }
    }
}
'@
}
$monitorHz = [CncAdaptiveDisplay]::RefreshHz()
if ($RenderFPS -eq 0) { $RenderFPS = [Math]::Min(240, [Math]::Max(30, $monitorHz)) }
if ($RenderFPS -lt 30) { throw 'RenderFPS must be zero (automatic), or 30 through 240.' }
# Simulation is independent of display refresh/rendering. Explicit experimental choices
# are persisted separately from tick-indexed game profiles and take effect next launch.
$simulationSettings = Join-Path $PSScriptRoot "simulation-$Game.ini"
if ($SimulationHz -eq 0) {
    $SimulationHz = 120
    if (Test-Path -LiteralPath $simulationSettings) {
        $saved = Get-Content -LiteralPath $simulationSettings -Raw
        if ($saved -match '^\s*SimulationHz\s*=\s*(\d+)\s*$' -and [int]$Matches[1] -ge 30 -and [int]$Matches[1] -le 240) { $SimulationHz = [int]$Matches[1] }
    }
}
if ($SimulationHz -lt 30) { throw 'SimulationHz must be zero (saved/default 120), or an experimental rate from 30 through 240.' }
if (-not $AssetRoot -and (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'experimental-install-state.json'))) {
    $installed = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'experimental-install-state.json') -Raw | ConvertFrom-Json
    if ($installed.game -ne $Game) { throw 'Use the launcher for this game.' }
    $AssetRoot = $installed.asset_root
}
if (-not $AssetRoot) { throw 'Run INSTALL.cmd to select this computer''s installed game first.' }
$variant = if ($Game -eq 'ZeroHour') { 'GeneralsMD' } else { 'Generals' }
$exeName = if ($Game -eq 'ZeroHour') { 'generalszhsim.exe' } else { 'generalsvsim.exe' }
$engine = Join-Path $PSScriptRoot "bin\$Game\$exeName"
# Tick-indexed saves/replays from different simulation rates must not be mixed.
$profile = Join-Path $PSScriptRoot "profiles\$Game\sim-$SimulationHz"
if ($Fullscreen) { $DisplayMode = 'Fullscreen' }
if ($DisplayMode -eq 'Saved') {
    $DisplayMode = 'Fullscreen'
    $optionsPath = Join-Path $profile 'Options.ini'
    if (Test-Path -LiteralPath $optionsPath) {
        $text = Get-Content -LiteralPath $optionsPath -Raw
        if ($text -match '(?mi)^\s*DisplayMode\s*=\s*([012])\s*$') {
            $DisplayMode = @('Fullscreen','Borderless','Windowed')[[int]$Matches[1]]
        }
    }
}
if (($Width -eq 0) -ne ($Height -eq 0) -or ($Width -gt 0 -and ($Width -lt 640 -or $Height -lt 480))) {
    throw 'Set both Width and Height, with a minimum of 640 x 480.'
}
$selection = [pscustomobject]@{
    game = $Game; monitor_hz = $monitorHz; render_fps = $RenderFPS; simulation_hz = $SimulationHz
    engine = $engine; profile = $profile; display_mode = $DisplayMode; width = $Width; height = $Height
    default_simulation_hz = 120; simulation_tier = $(if ($SimulationHz -eq 120) {'Default'} elseif ($SimulationHz -eq 30) {'Original'} else {'Experimental'})
    status = 'Fluid v1.0.0'; diagnostics = [bool]$Diagnostics
}
if ($Inspect) { $selection | ConvertTo-Json; return }
if (-not (Test-Path -LiteralPath $engine)) { throw "Engine missing; extract the complete ZIP again: $engine" }
if (-not (Test-Path -LiteralPath $AssetRoot)) { throw "Game assets missing: $AssetRoot" }
if (-not (Test-Path -LiteralPath (Join-Path $AssetRoot 'install-state.json'))) { throw 'Run INSTALL.cmd first.' }
$assets = Get-Content -LiteralPath (Join-Path $AssetRoot 'install-state.json') -Raw | ConvertFrom-Json
if ($assets.game -ne $Game) { throw 'AssetRoot supplies the other game. Run INSTALL.cmd with the matching game.' }
$audioRepair = Join-Path $PSScriptRoot 'Ensure-OwnedMilesAudio.ps1'
if (-not (Test-Path -LiteralPath $audioRepair)) { $audioRepair = Join-Path $project 'tools/Ensure-OwnedMilesAudio.ps1' }
if (-not (Test-Path -LiteralPath $audioRepair)) { throw 'The package is missing Ensure-OwnedMilesAudio.ps1. Extract the complete updated package.' }
foreach ($dll in @('BINKW32.DLL','mss32.dll')) {
    $originalDll = Join-Path $AssetRoot "bin\$dll"
    $targetDll = Join-Path (Split-Path $engine -Parent) $dll
    if (-not (Test-Path -LiteralPath $targetDll)) { Copy-Item -LiteralPath $originalDll -Destination $targetDll }
    if ((Get-FileHash -LiteralPath $originalDll).Hash -ne (Get-FileHash -LiteralPath $targetDll).Hash) {
        throw "Experimental runtime library does not match this game's owned library: $dll"
    }
}
& $audioRepair -Game $Game -AssetRoot $AssetRoot -EnginePath $engine | Out-Null
New-Item -ItemType Directory -Path $profile -Force | Out-Null
if (-not (Test-Path -LiteralPath (Join-Path $profile 'Options.ini'))) {
    $originalOptions = Join-Path $AssetRoot 'Profile\Options.ini'
    if (Test-Path -LiteralPath $originalOptions) { Copy-Item -LiteralPath $originalOptions -Destination (Join-Path $profile 'Options.ini') }
}
& (Join-Path $PSScriptRoot 'Ensure-AdaptiveCameraInput.ps1') -Profile $profile
$names = @('FLUID_SHOW_DIAGNOSTICS','CNC240_VERIFY_GAMEPLAY','CNC240_VERIFY_DISPLAY_SWITCH','CNC240_VERIFY_SCENE','CNC240_VERIFY_MISSION','CNC240_VERIFY_BACKGROUND','CNC240_VERIFY_COMBAT','CNC240_VERIFY_AIRCRAFT','CNC240_SIM_HZ','CNC240_SIM_SETTINGS_FILE','CNC240_USER_DATA','CNC240_VERIFY_SECONDS','CNC240_VERIFY_SCENARIO','CNC240_VERIFY_ZOOM','CNC240_VERIFY_SIMULATION','CNC240_VERIFY_EFFECTS','CNC240_VERIFY_TRAIN','CNC240_VERIFY_SCREENSHOT','CNC240_VERIFY_CAMERA','CNC240_VERIFY_SETTINGS','CNC240_VERIFY_TEXT','CNC240_VERIFY_INPUT_ISOLATED')
$previous = @{}
$present = @{}
foreach ($name in $names) { $present[$name] = Test-Path -LiteralPath "Env:\$name"; $previous[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    foreach ($name in $names) { Remove-Item -LiteralPath "Env:\$name" -ErrorAction SilentlyContinue }
    $env:FLUID_SHOW_DIAGNOSTICS = $(if ($Diagnostics) {'1'} else {'0'})
    $env:CNC240_SIM_HZ = "$SimulationHz"
    $env:CNC240_SIM_SETTINGS_FILE = $simulationSettings
    $env:CNC240_USER_DATA = $profile
    $arguments = "-useCwd -nologo -renderfps $RenderFPS"
    $arguments += " -displaymode $DisplayMode"
    if ($Width -gt 0) { $arguments += " -xres $Width -yres $Height" }
    $selection | ConvertTo-Json
    Start-Process -FilePath $engine -WorkingDirectory $AssetRoot -ArgumentList $arguments
} finally {
    foreach ($name in $names) {
        if ($present[$name]) { [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process') }
        else { Remove-Item -LiteralPath "Env:\$name" -ErrorAction SilentlyContinue }
    }
}
