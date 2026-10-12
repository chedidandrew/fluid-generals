# Restore the game's owned Miles providers/codecs; no proprietary files are bundled (0BSD).
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Generals','ZeroHour')][string]$Game,
    [Parameter(Mandatory)][string]$AssetRoot,
    [Parameter(Mandatory)][string]$EnginePath
)
$ErrorActionPreference = 'Stop'
$AssetRoot = [IO.Path]::GetFullPath($AssetRoot).TrimEnd('\')
$statePath = Join-Path $AssetRoot 'install-state.json'
$state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
if ($state.schema -ne 1 -or $state.game -ne $Game -or [IO.Path]::GetFullPath($state.root).TrimEnd('\') -ne $AssetRoot) { throw 'Audio repair requires the matching installed preview.' }
$originalRoot = [IO.Path]::GetFullPath($state.originalRoot).TrimEnd('\')
if ($state.portable_assets -ne 1 -and $AssetRoot -ne (Join-Path $originalRoot 'Cnc240-preview')) { throw 'Audio assets must match the recorded owned game.' }
$sourceDir = Join-Path $originalRoot 'MSS'
$EnginePath = [IO.Path]::GetFullPath($EnginePath)
$engineDir = Split-Path $EnginePath -Parent
$targetDir = Join-Path $engineDir 'MSS'
$validExe = if ($Game -eq 'Generals') { @('generalsvsim.exe','generalsv240.exe') } else { @('generalszhsim.exe','generalszh240.exe') }
if ((Split-Path $EnginePath -Leaf) -notin $validExe -or -not (Test-Path -LiteralPath $EnginePath)) { throw 'Audio repair requires the matching source-built game executable.' }
if ((Get-FileHash -LiteralPath (Join-Path $engineDir 'mss32.dll')).Hash -ne (Get-FileHash -LiteralPath (Join-Path $originalRoot 'mss32.dll')).Hash) { throw 'The engine audio library differs from the matching owned game.' }
$required = @('msssoft.m3d','mssmp3.asi','mssvoice.asi','mssdsp.flt')
$allowed = $required + @('mssa3d.m3d','mssds3d.m3d','mssdx7.m3d','msseax.m3d','mssrsx.m3d')
foreach ($dir in @($originalRoot,$AssetRoot,$sourceDir,$engineDir,$targetDir)) {
    if (Test-Path -LiteralPath $dir) {
        $entry = Get-Item -LiteralPath $dir -Force
        if (-not $entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Audio directory must be an ordinary folder: $dir" }
    }
}
$plan = foreach ($name in $allowed) {
    $source = Join-Path $sourceDir $name
    if (-not (Test-Path -LiteralPath $source)) {
        if ($name -in $required) { throw "Owned Miles audio component missing: $source. Verify the original game installation." }
        continue
    }
    $entry = Get-Item -LiteralPath $source -Force
    if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Redirected audio component rejected: $source" }
    $hash = (Get-FileHash -LiteralPath $source).Hash
    $target = Join-Path $targetDir $name
    $exists = Test-Path -LiteralPath $target
    if ($exists) {
        $entry = Get-Item -LiteralPath $target -Force
        if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Redirected audio component rejected: $target" }
        if ((Get-FileHash -LiteralPath $target).Hash -ne $hash) { throw "Existing audio component differs from the owned game: $target" }
    }
    [pscustomobject]@{name=$name;source=$source;target=$target;sha256=$hash;copy=(-not $exists)}
}
# Preflight the whole set before creating files. Existing matching files are retained.
New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
foreach ($item in $plan) {
    if ($item.copy) { Copy-Item -LiteralPath $item.source -Destination $item.target }
    if ((Get-FileHash -LiteralPath $item.target).Hash -ne $item.sha256) { throw "Audio copy verification failed: $($item.target)" }
}
$ownedFiles = @($state.files)
foreach ($item in $plan) {
    if ($item.target.StartsWith($AssetRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        $relative = $item.target.Substring($AssetRoot.Length + 1)
        if ($relative -notin $ownedFiles) { $ownedFiles += $relative }
    }
}
if (@(Compare-Object @($state.files) $ownedFiles).Count) {
    $state.files = $ownedFiles
    $state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $statePath -Encoding UTF8
}
[pscustomobject]@{game=$Game;asset_root=$AssetRoot;runtime_root=$engineDir;components=@($plan | Select-Object name,sha256);restored=@($plan | Where-Object copy | Select-Object -ExpandProperty name)}
