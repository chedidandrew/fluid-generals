# Setup helper: 0BSD. Engine and runtime licenses are in the package.
[CmdletBinding()]
param(
 [ValidateSet('','Both','Generals','ZeroHour')][string]$Game='',
 [string]$GeneralsPath='', [string]$ZeroHourPath='',
 [string]$InstallBase='', [switch]$NoShortcuts, [switch]$SkipPrerequisite
)
$ErrorActionPreference='Stop'
$package=$PSScriptRoot
$payload=Join-Path $package 'payload'
if(-not $InstallBase){$InstallBase=Join-Path $package 'Games'}
$InstallBase=[IO.Path]::GetFullPath($InstallBase)
function Assert-PlainFolder([string]$Path){
 if(Test-Path -LiteralPath $Path){$p=Get-Item -LiteralPath $Path -Force;if(-not $p.PSIsContainer -or ($p.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Select an ordinary folder: $Path"}}
}
# Verify every packaged file before installing either game or a prerequisite.
$manifest=Get-Content -LiteralPath (Join-Path $package 'transfer-manifest.json') -Raw | ConvertFrom-Json
foreach($entry in $manifest.PSObject.Properties){
 $path=[IO.Path]::GetFullPath((Join-Path $package $entry.Name))
 if(-not $path.StartsWith($package+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid manifest path.'}
 if(-not(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ne $entry.Value.sha256){throw "Package damaged or incomplete: $($entry.Name). Extract the complete ZIP again."}
}
function Find-Game([string]$Which){
 $required=if($Which -eq 'Generals'){'INI.big'}else{'INIZH.big'}
 $names=if($Which -eq 'Generals'){@('Command and Conquer Generals','Command & Conquer Generals','Command and Conquer Generals (TM)')}else{@('Command & Conquer Generals - Zero Hour','Command and Conquer Generals Zero Hour','Command and Conquer Generals Zero Hour (TM)','Command and Conquer Generals - Zero Hour')}
 $candidates=[Collections.Generic.List[string]]::new();$steam=[Collections.Generic.List[string]]::new()
 foreach($key in @('HKCU:\Software\Valve\Steam','HKLM:\SOFTWARE\WOW6432Node\Valve\Steam')){
  $v=Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
  foreach($value in @($v.SteamPath,$v.InstallPath)){if($value){$steam.Add($value)}}
 }
 foreach($base in @($steam.ToArray())){
  $vdf=Join-Path $base 'steamapps\libraryfolders.vdf'
  if(Test-Path -LiteralPath $vdf){foreach($m in [regex]::Matches([IO.File]::ReadAllText($vdf),'"path"\s+"([^"]+)"')){$steam.Add($m.Groups[1].Value.Replace('\\','\'))}}
 }
 foreach($base in $steam){foreach($name in $names){$candidates.Add((Join-Path $base "steamapps\common\$name"))}}
 foreach($base in @($env:ProgramFiles,${env:ProgramFiles(x86)},(Join-Path $env:SystemDrive 'EA Games'))){
  if($base){foreach($name in $names){$candidates.Add((Join-Path $base $name));$candidates.Add((Join-Path $base "EA Games\$name"))}}
 }
 foreach($root in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall','HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall','HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall')){
  foreach($key in Get-ChildItem -LiteralPath $root -ErrorAction SilentlyContinue){
   $v=Get-ItemProperty -LiteralPath $key.PSPath -ErrorAction SilentlyContinue
   if($v.DisplayName -match 'Generals' -and $v.InstallLocation){$candidates.Add($v.InstallLocation)}
  }
 }
 foreach($p in $candidates){if(Test-Path -LiteralPath (Join-Path $p $required)){return [IO.Path]::GetFullPath($p)}}
 return ''
}
function Choose-Game([string]$Which,[string]$Given){
 if($Given){return [IO.Path]::GetFullPath($Given).TrimEnd('\')}
 $found=Find-Game $Which
 if($found){Write-Host "$Which found: $found";return $found.TrimEnd('\')}
 Add-Type -AssemblyName System.Windows.Forms
 $dialog=New-Object Windows.Forms.FolderBrowserDialog
 $dialog.Description="Select your installed $Which game folder (contains $(if($Which -eq 'Generals'){'INI.big'}else{'INIZH.big'}))."
 $dialog.ShowNewFolderButton=$false
 try{if($dialog.ShowDialog() -ne [Windows.Forms.DialogResult]::OK){throw "$Which selection cancelled; nothing installed."};return $dialog.SelectedPath.TrimEnd('\')}finally{$dialog.Dispose()}
}
if(-not $Game){
 Write-Host "Fluid v1.0.0 Setup`n"
 Write-Host '1 = Both games (default), 2 = Generals only, 3 = Zero Hour only'
 switch(Read-Host 'Choose'){'2'{$Game='Generals'}'3'{$Game='ZeroHour'}''{$Game='Both'}'1'{$Game='Both'}default{throw 'Choose 1, 2 or 3.'}}
}
$specs=@(
 @{Game='Generals';Given=$GeneralsPath;Exe='generalsvsim.exe';Required='INI.big';Archives=@('Audio.big','AudioEnglish.big','English.big','gensec.big','INI.big','maps.big','Music.big','Patch.big','PatchData.big','PatchINI.big','shaders.big','Speech.big','SpeechEnglish.big','Terrain.big','Textures.big','W3D.big','Window.big')},
 @{Game='ZeroHour';Given=$ZeroHourPath;Exe='generalszhsim.exe';Required='INIZH.big';Archives=@('AudioEnglishZH.big','AudioZH.big','EnglishZH.big','gensecZH.big','INIZH.big','MapsZH.big','Music.big','MusicZH.big','PatchData.big','PatchINI.big','PatchZH.big','ShadersZH.big','SpeechEnglishZH.big','SpeechZH.big','TerrainZH.big','TexturesZH.big','W3DEnglishZH.big','W3DZH.big','WindowZH.big')}
)|Where-Object {$Game -eq 'Both' -or $_.Game -eq $Game}
# Preflight both selected games before any setup writes.
foreach($s in $specs){
 $s.Root=Choose-Game $s.Game $s.Given;Assert-PlainFolder $s.Root
 foreach($file in @($s.Required,'BINKW32.DLL','mss32.dll','MSS\msssoft.m3d','MSS\mssmp3.asi','MSS\mssvoice.asi','MSS\mssdsp.flt')){
  if(-not(Test-Path -LiteralPath (Join-Path $s.Root $file))){throw "$($s.Game) is missing $file. Select the actual game folder or verify its files in Steam/EA."}
 }
 $s.Destination=Join-Path $InstallBase $s.Game;Assert-PlainFolder $s.Destination
 if(Test-Path -LiteralPath $s.Destination){
  $statePath=Join-Path $s.Destination 'experimental-install-state.json'
  if(-not(Test-Path -LiteralPath $statePath)){throw "Existing incomplete setup at $($s.Destination). Use a fresh extracted package folder; nothing overwritten."}
  $st=Get-Content -LiteralPath $statePath -Raw|ConvertFrom-Json
  if($st.transfer_setup -ne 1 -or $st.game -ne $s.Game -or $st.original_root -ne $s.Root){throw 'Existing setup belongs to another game folder. Use a fresh extracted package.'}
  $exe=Join-Path $s.Destination "bin\$($s.Game)\$($s.Exe)"
  if((Get-FileHash -LiteralPath $exe).Hash -ne $manifest.PSObject.Properties["payload/bin/$($s.Game)/$($s.Exe)"].Value.sha256){throw 'Existing mod engine differs from this package. Use a fresh extracted package; saved profiles retained.'}
  $s.Exists=$true
 }
}
if(-not $SkipPrerequisite){
 $system=if([Environment]::Is64BitOperatingSystem){Join-Path $env:windir 'SysWOW64'}else{Join-Path $env:windir 'System32'}
 $needsRuntime=$false
 foreach($file in @('MSVCP140.dll','VCRUNTIME140.dll','MSVCP140_ATOMIC_WAIT.dll')){
  $p=Join-Path $system $file
  if(-not(Test-Path -LiteralPath $p) -or [version]((Get-Item -LiteralPath $p).VersionInfo.FileVersion -replace '[^\d.].*$','') -lt [version]'14.44.35207.0'){$needsRuntime=$true}
 }
 if($needsRuntime){
  $runtime=Join-Path $package 'runtime\vc_redist.x86.exe';$sig=Get-AuthenticodeSignature -LiteralPath $runtime
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'Microsoft Corporation'){throw 'Microsoft prerequisite signature invalid.'}
  Write-Host 'Installing the included Microsoft C++ x86 runtime. Windows may ask for permission.'
  $p=Start-Process -FilePath $runtime -ArgumentList '/install /passive /norestart' -Wait -PassThru
  if($p.ExitCode -notin @(0,3010,1638)){throw "Microsoft runtime installation failed ($($p.ExitCode)). Install runtime\vc_redist.x86.exe manually, then rerun INSTALL.cmd."}
  if($p.ExitCode -eq 3010){Write-Host 'Windows requests a restart before playing.'}
 }
}
Assert-PlainFolder $InstallBase
if(-not ('CncTransferFiles' -as [type])){
 Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CncTransferFiles {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 [return: MarshalAs(UnmanagedType.Bool)]
 public static extern bool CreateHardLink(string path,string existing,IntPtr security);
}
'@
}
foreach($s in $specs){
 $root=$s.Destination
 if(-not $s.Exists){
  $asset=Join-Path $root 'assets';$engineDir=Join-Path $root "bin\$($s.Game)"
  New-Item -ItemType Directory -Path $asset,$engineDir -Force|Out-Null
  $files=[Collections.Generic.List[string]]::new();$copies=0;$links=0
  function Add-Asset([string]$Source,[string]$Relative){
   $target=Join-Path $asset $Relative;New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force|Out-Null
   if([CncTransferFiles]::CreateHardLink($target,$Source,[IntPtr]::Zero)){$script:links++}
   else{Copy-Item -LiteralPath $Source -Destination $target;$script:copies++}
   $files.Add($Relative)
  }
  foreach($name in $s.Archives){$p=Join-Path $s.Root $name;if(Test-Path -LiteralPath $p){Add-Asset $p $name}}
  foreach($folder in @('Cursors','English','Movies','Scripts','WaterPlane')){
   $source=Join-Path $s.Root "Data\$folder"
   if(Test-Path -LiteralPath $source){foreach($p in Get-ChildItem -LiteralPath $source -File -Recurse){if(-not($p.Attributes -band [IO.FileAttributes]::ReparsePoint)){Add-Asset $p.FullName $p.FullName.Substring($s.Root.Length+1)}}}
  }
  New-Item -ItemType Directory -Path (Join-Path $asset 'bin') -Force|Out-Null
  foreach($dll in @('BINKW32.DLL','mss32.dll')){
   Copy-Item -LiteralPath (Join-Path $s.Root $dll) -Destination (Join-Path $asset "bin\$dll")
   Copy-Item -LiteralPath (Join-Path $s.Root $dll) -Destination (Join-Path $engineDir $dll)
  }
  @{schema=1;game=$s.Game;root=$asset;originalRoot=$s.Root;files=$files.ToArray();portable_assets=1}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $asset 'install-state.json') -Encoding UTF8
  foreach($file in @('Launch-Fluid.ps1','Ensure-OwnedMilesAudio.ps1','Ensure-AdaptiveCameraInput.ps1',"Launch-$($s.Game).cmd")){
   Copy-Item -LiteralPath (Join-Path $payload $file) -Destination (Join-Path $root $file)
  }
  Copy-Item -LiteralPath (Join-Path $payload "bin\$($s.Game)\$($s.Exe)") -Destination (Join-Path $engineDir $s.Exe)
  & (Join-Path $root 'Ensure-OwnedMilesAudio.ps1') -Game $s.Game -AssetRoot $asset -EnginePath (Join-Path $engineDir $s.Exe)|Out-Null
  $profile=Join-Path $root "profiles\$($s.Game)\sim-120";New-Item -ItemType Directory -Path $profile -Force|Out-Null
  Add-Type -AssemblyName System.Windows.Forms
  $screen=[Windows.Forms.Screen]::PrimaryScreen.Bounds
  @("Resolution = $($screen.Width) $($screen.Height)",'AntiAliasing = 0','StaticGameLOD = High','DynamicLOD = yes','DisplayMode = 0')|Set-Content -LiteralPath (Join-Path $profile 'Options.ini') -Encoding ASCII
  & (Join-Path $root 'Ensure-AdaptiveCameraInput.ps1') -Profile $profile
  $state=@{transfer_setup=1;kind='Fluid v1.0.0';game=$s.Game;root=$root;original_root=$s.Root;asset_root=$asset;exe=$s.Exe;sha256=(Get-FileHash -LiteralPath (Join-Path $engineDir $s.Exe)).Hash;default_simulation_hz=120;original_simulation_hz=30;experimental_simulation_rates=@(60,144,240);created=(Get-Date -Format o)}
  $state|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $root 'experimental-install-state.json') -Encoding UTF8
  Write-Host "$($s.Game) installed."
 }
 if(-not $NoShortcuts){
  $label=if($s.Game -eq 'ZeroHour'){'Command & Conquer: Fluid Generals Zero Hour'}else{'Command & Conquer: Fluid Generals'}
  $shortcut=(New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) "$($label.Replace(':',' -')).lnk"))
  $shortcut.TargetPath=Join-Path $root "Launch-$($s.Game).cmd";$shortcut.WorkingDirectory=$root;$shortcut.IconLocation=Join-Path $root "bin\$($s.Game)\$($s.Exe)";$shortcut.Description='120 Hz simulation; normal gameplay speed.';$shortcut.Save()
 }
 Write-Host "Play: $root\Launch-$($s.Game).cmd"
}
Write-Host "`nSetup complete. Keep this extracted folder in place. Use the desktop shortcuts to play."
