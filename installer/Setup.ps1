# Fluid installer backend, 0BSD. UI runs this without a console window.
[CmdletBinding()]
param(
 [ValidateSet('','Both','Generals','ZeroHour')][string]$Game='',
 [string]$GeneralsPath='',[string]$ZeroHourPath='',[string]$InstallBase='',
 [ValidateSet('Install','Repair','Reset','Uninstall')][string]$Action='Install',
 [string]$MigrateGeneralsFrom='',[string]$MigrateZeroHourFrom='',
 [switch]$Discover,[switch]$NoShortcuts,[switch]$SkipPrerequisite,[switch]$RemoveSaves,
 [string]$RegistryPath=''
)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$package=[IO.Path]::GetFullPath($PSScriptRoot)
$payload=Join-Path $package 'payload'
$version='1.0.1'
if(-not $RegistryPath){$RegistryPath=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Fluid\installations.json'}
function Full([string]$p){$full=[IO.Path]::GetFullPath($p);if($full -eq [IO.Path]::GetPathRoot($full)){return $full};return $full.TrimEnd('\')}
function Within([string]$p,[string]$parent){$p=Full $p;$parent=Full $parent;return $p.Equals($parent,[StringComparison]::OrdinalIgnoreCase) -or $p.StartsWith($parent.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)}
function Assert-Plain([string]$p){
 $current=Full $p
 while($current){
  if(Test-Path -LiteralPath $current){$i=Get-Item -LiteralPath $current -Force;if(-not $i.PSIsContainer -or ($i.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Use an ordinary folder: $current"}}
  $parent=Split-Path $current -Parent;if(-not $parent -or $parent -eq $current){break};$current=$parent
 }
}
function Read-FluidState([string]$root,[string]$which){
 if(-not $root){return $null}
 try{
  $root=Full $root;$p=Join-Path $root 'experimental-install-state.json'
  if(-not(Test-Path -LiteralPath $p)){return $null}
  $s=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json
  if($s.kind -notlike 'Fluid *' -or $s.game -ne $which -or (Full $s.root) -ne $root){return $null}
  return $s
 }catch{return $null}
}
function Find-Game([string]$which){
 $required=if($which -eq 'Generals'){'INI.big'}else{'INIZH.big'}
 $names=if($which -eq 'Generals'){@('Command and Conquer Generals','Command & Conquer Generals','Command and Conquer Generals (TM)')}else{@('Command & Conquer Generals - Zero Hour','Command and Conquer Generals Zero Hour','Command and Conquer Generals Zero Hour (TM)','Command and Conquer Generals - Zero Hour')}
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
 foreach($base in @($env:ProgramFiles,${env:ProgramFiles(x86)},(Join-Path $env:SystemDrive 'EA Games'))){if($base){foreach($name in $names){$candidates.Add((Join-Path $base $name));$candidates.Add((Join-Path $base "EA Games\$name"))}}}
 foreach($registry in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall','HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall','HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall')){
  foreach($key in Get-ChildItem -LiteralPath $registry -ErrorAction SilentlyContinue){$v=Get-ItemProperty -LiteralPath $key.PSPath -ErrorAction SilentlyContinue;if($v.DisplayName -match 'Generals' -and $v.InstallLocation){$candidates.Add($v.InstallLocation)}}
 }
 foreach($p in $candidates){if(Test-Path -LiteralPath (Join-Path $p $required)){return Full $p}}
 return ''
}
function Find-Fluid([string]$which){
 $candidates=[Collections.Generic.List[string]]::new()
 if(Test-Path -LiteralPath $RegistryPath){try{$saved=Get-Content -LiteralPath $RegistryPath -Raw|ConvertFrom-Json;if($saved.$which){$candidates.Add([string]$saved.$which)}}catch{}}
 $candidates.Add((Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) "Programs\Fluid\$which"))
 $label=if($which -eq 'Generals'){'Command & Conquer - Fluid Generals.lnk'}else{'Command & Conquer - Fluid Generals Zero Hour.lnk'}
 $link=Join-Path ([Environment]::GetFolderPath('Desktop')) $label
 if(Test-Path -LiteralPath $link){try{$target=(New-Object -ComObject WScript.Shell).CreateShortcut($link).TargetPath;if($target){$candidates.Add((Split-Path $target -Parent))}}catch{}}
 foreach($root in $candidates){$s=Read-FluidState $root $which;if($s){return $s}}
 return $null
}
function Fluid-Status($state,[string]$which){
 if($state.uninstalled -eq 1){return 'Saved settings only'}
 $exeName=if($which -eq 'Generals'){'generalsvsim.exe'}else{'generalszhsim.exe'}
 $exe=Join-Path $state.root "bin\$which\$exeName"
 if(-not(Test-Path -LiteralPath $exe) -or ($state.sha256 -and (Get-FileHash -LiteralPath $exe).Hash -ne $state.sha256)){return 'Needs repair'}
 if($state.managed_hashes){foreach($e in $state.managed_hashes.PSObject.Properties){$p=Join-Path $state.root $e.Name;if(-not(Test-Path -LiteralPath $p) -or (Get-FileHash -LiteralPath $p).Hash -ne $e.Value){return 'Needs repair'}}}
 if($state.permanent_install -eq 1 -and $state.setup_complete -ne $true){return 'Needs repair'}
 $installedVersion=if($state.version){$state.version}else{'1.0.0'}
 if([version]$installedVersion -lt [version]$version){return 'Update available'}
 return 'Installed'
}
if($Discover){
 $games=@{};$installed=@{}
 foreach($which in @('Generals','ZeroHour')){
  $s=if($InstallBase){Read-FluidState (Join-Path (Full $InstallBase) $which) $which}else{Find-Fluid $which};$gameRoot=Find-Game $which
  if($s){$installed[$which]=@{root=$s.root;version=$(if($s.version){$s.version}else{'1.0.0'});original_root=$s.original_root;permanent=($s.permanent_install -eq 1);active=($s.uninstalled -ne 1);status=(Fluid-Status $s $which)};if(-not $gameRoot){$gameRoot=$s.original_root}}
  $games[$which]=$gameRoot
 }
 @{games=$games;installed=$installed;default_install=(Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Programs\Fluid')}|ConvertTo-Json -Depth 5
 return
}
if(-not $Game -or -not $InstallBase){throw 'Open FluidSetup.exe and select your games and install location.'}
if(Test-Path -LiteralPath (Join-Path $package 'setup-config.json')){$config=Get-Content -LiteralPath (Join-Path $package 'setup-config.json') -Raw|ConvertFrom-Json;if($config.game -ne 'Both' -and $config.game -ne $Game){throw 'Use the setup package for the selected game.'}}
$InstallBase=Full $InstallBase
if($Action -ne 'Uninstall' -and ((Within $InstallBase $package) -or (Within $package $InstallBase))){throw 'Choose a permanent folder separate from the extracted setup folder.'}
if($InstallBase -eq (Full ([IO.Path]::GetPathRoot($InstallBase)))){throw 'Choose a Fluid folder, not a drive root.'}
Assert-Plain $InstallBase
if($Action -eq 'Uninstall'){
 $plan=@();$running=@(Get-CimInstance Win32_Process -Filter "Name='generalsvsim.exe' OR Name='generalszhsim.exe'" -ErrorAction SilentlyContinue)
 foreach($which in @('Generals','ZeroHour')){
  if($Game -ne 'Both' -and $Game -ne $which){continue}
  $root=Join-Path $InstallBase $which
  if(-not(Test-Path -LiteralPath $root)){continue}
  Assert-Plain $root;$state=Read-FluidState $root $which
  if(-not $state -or (Within $root $state.original_root) -or (Within $state.original_root $root)){throw 'Uninstall only removes a recognized Fluid folder outside the original game.'}
  $exe=Join-Path $root "bin\$which\$($state.exe)"
  if(@($running|Where-Object {$_.ExecutablePath -and $_.ExecutablePath.Equals($exe,[StringComparison]::OrdinalIgnoreCase)}).Count){throw "Close Fluid $which before uninstalling."}
  $items=@('assets','bin','FluidLauncher.exe','Launch-Fluid.ps1','Ensure-OwnedMilesAudio.ps1','Ensure-AdaptiveCameraInput.ps1',"Launch-$which.cmd")
  if($RemoveSaves){$items+=@('profiles','backups',"simulation-$which.ini")}
  $targets=@()
  foreach($relative in $items){
   $target=Join-Path $root $relative
   if(-not(Within $target $root)){throw 'Invalid uninstall path.'}
   if(Test-Path -LiteralPath $target){
    $entry=Get-Item -LiteralPath $target -Force
    $entries=@($entry);if($entry.PSIsContainer){$entries+=@(Get-ChildItem -LiteralPath $target -Recurse -Force)}
    if(@($entries|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count){throw 'Redirected files/folders cannot be uninstalled automatically.'}
    if(@($entries|Where-Object {-not $_.PSIsContainer -and ($_.Attributes -band [IO.FileAttributes]::ReadOnly)}).Count){throw 'Read-only files prevent safe removal. Original game file attributes will not be changed.'}
    $targets+=$target
   }
  }
  $plan+=@{Game=$which;Root=$root;State=$state;Targets=$targets}
 }
 foreach($item in $plan){
  Write-Output "Uninstalling $($item.Game)..."
  foreach($target in $item.Targets){if(-not(Within $target $item.Root)){throw 'Invalid uninstall path.'};Remove-Item -LiteralPath $target -Recurse -Confirm:$false}
  $label=if($item.Game -eq 'ZeroHour'){'Command & Conquer - Fluid Generals Zero Hour.lnk'}else{'Command & Conquer - Fluid Generals.lnk'}
  $link=Join-Path ([Environment]::GetFolderPath('Desktop')) $label
  if(Test-Path -LiteralPath $link){$target=(New-Object -ComObject WScript.Shell).CreateShortcut($link).TargetPath;if($target.Equals((Join-Path $item.Root 'FluidLauncher.exe'),[StringComparison]::OrdinalIgnoreCase) -or $target.Equals((Join-Path $item.Root "Launch-$($item.Game).cmd"),[StringComparison]::OrdinalIgnoreCase)){Remove-Item -LiteralPath $link}}
  if($RemoveSaves){Remove-Item -LiteralPath (Join-Path $item.Root 'experimental-install-state.json');if(-not@(Get-ChildItem -LiteralPath $item.Root -Force).Count){Remove-Item -LiteralPath $item.Root}}
  else{$state=$item.State;$state|Add-Member -NotePropertyName uninstalled -NotePropertyValue 1 -Force;$state|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $item.Root 'experimental-install-state.json') -Encoding UTF8}
 }
 Write-Output 'Uninstalled. Original games unchanged. Source/licenses and maintenance utility remain in the chosen location.'
 return
}
if(Test-Path -LiteralPath (Join-Path $package 'maintenance-config.json')){throw 'Use the downloaded FluidSetup.exe to install, update or repair.'}
Write-Output 'Checking package...'
$manifest=Get-Content -LiteralPath (Join-Path $package 'transfer-manifest.json') -Raw|ConvertFrom-Json
foreach($e in $manifest.PSObject.Properties){
 $p=[IO.Path]::GetFullPath((Join-Path $package $e.Name))
 if(-not $p.StartsWith($package+'\',[StringComparison]::OrdinalIgnoreCase) -or -not(Test-Path -LiteralPath $p) -or (Get-FileHash -LiteralPath $p).Hash -ne $e.Value.sha256){throw "Package damaged: $($e.Name). Extract the complete ZIP again."}
}
$specs=@(
 @{Game='Generals';Given=$GeneralsPath;Migrate=$MigrateGeneralsFrom;Exe='generalsvsim.exe';Required='INI.big';Archives=@('Audio.big','AudioEnglish.big','English.big','gensec.big','INI.big','maps.big','Music.big','Patch.big','PatchData.big','PatchINI.big','shaders.big','Speech.big','SpeechEnglish.big','Terrain.big','Textures.big','W3D.big','Window.big')},
 @{Game='ZeroHour';Given=$ZeroHourPath;Migrate=$MigrateZeroHourFrom;Exe='generalszhsim.exe';Required='INIZH.big';Archives=@('AudioEnglishZH.big','AudioZH.big','EnglishZH.big','gensecZH.big','INIZH.big','MapsZH.big','Music.big','MusicZH.big','PatchData.big','PatchINI.big','PatchZH.big','ShadersZH.big','SpeechEnglishZH.big','SpeechZH.big','TerrainZH.big','TexturesZH.big','W3DEnglishZH.big','W3DZH.big','WindowZH.big')}
)|Where-Object {$Game -eq 'Both' -or $_.Game -eq $Game}
$running=@(Get-CimInstance Win32_Process -Filter "Name='generalsvsim.exe' OR Name='generalszhsim.exe'" -ErrorAction SilentlyContinue)
foreach($s in $specs){
 $s.Root=if($s.Given){Full $s.Given}else{Find-Game $s.Game};if(-not $s.Root){throw "Select your installed $($s.Game) game folder."};Assert-Plain $s.Root
 foreach($f in @($s.Required,'BINKW32.DLL','mss32.dll','MSS\msssoft.m3d','MSS\mssmp3.asi','MSS\mssvoice.asi','MSS\mssdsp.flt')){if(-not(Test-Path -LiteralPath (Join-Path $s.Root $f))){throw "$($s.Game) is missing $f. Verify the original game or select its correct folder."}}
 if(-not(Test-Path -LiteralPath (Join-Path $payload "bin\$($s.Game)\$($s.Exe)"))){throw 'This package does not contain the selected game engine.'}
 $s.Destination=Join-Path $InstallBase $s.Game;Assert-Plain $s.Destination
 if((Within $s.Destination $s.Root) -or (Within $s.Root $s.Destination)){throw 'Install Fluid outside the original game folder.'}
 $s.Exists=Test-Path -LiteralPath $s.Destination
 if($s.Exists -and -not (Read-FluidState $s.Destination $s.Game)){throw "Not a recognized Fluid installation: $($s.Destination). Choose another location."}
 if($s.Migrate){$s.Migrate=Full $s.Migrate;Assert-Plain $s.Migrate;if(-not(Read-FluidState $s.Migrate $s.Game)){throw 'The previous folder is not a recognized Fluid installation.'}}
 foreach($root in @($s.Destination,$s.Migrate)){
  if($root){$exe=Join-Path $root "bin\$($s.Game)\$($s.Exe)";if(@($running|Where-Object {$_.ExecutablePath -and $_.ExecutablePath.Equals($exe,[StringComparison]::OrdinalIgnoreCase)}).Count){throw "Close Fluid $($s.Game) before updating or migrating it."}}
 }
}
if(-not $SkipPrerequisite){
 $system=if([Environment]::Is64BitOperatingSystem){Join-Path $env:windir 'SysWOW64'}else{Join-Path $env:windir 'System32'};$need=$false
 foreach($f in @('MSVCP140.dll','VCRUNTIME140.dll','MSVCP140_ATOMIC_WAIT.dll')){$p=Join-Path $system $f;if(-not(Test-Path -LiteralPath $p) -or [version]((Get-Item -LiteralPath $p).VersionInfo.FileVersion -replace '[^\d.].*$','') -lt [version]'14.44.35207.0'){$need=$true}}
 if($need){
  $p=Join-Path $package 'runtime\vc_redist.x86.exe';$sig=Get-AuthenticodeSignature -LiteralPath $p
  if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'Microsoft Corporation'){throw 'Microsoft runtime signature invalid.'}
  Write-Output 'Installing Microsoft runtime...'
  $proc=Start-Process -FilePath $p -ArgumentList '/install /quiet /norestart' -WindowStyle Hidden -Wait -PassThru
  if($proc.ExitCode -notin @(0,3010,1638)){throw "Microsoft runtime installation failed ($($proc.ExitCode))."}
  if($proc.ExitCode -eq 3010){Write-Output 'Restart Windows before playing.'}
 }
}
if(-not('FluidFiles' -as [type])){
 Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class FluidFiles {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool CreateHardLink(string path,string existing,IntPtr security);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool MoveFileEx(string existing,string target,int flags);
}
'@
}
function Put-File([string]$source,[string]$target,[bool]$link=$false){
 Assert-Plain (Split-Path $target -Parent)
 if(Test-Path -LiteralPath $target){$i=Get-Item -LiteralPath $target -Force;if($i.PSIsContainer -or ($i.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Redirected target rejected: $target"}}
 New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force|Out-Null
 $temporary=$target+'.fluid-'+[Guid]::NewGuid().ToString('N')
 $readOnly=((Get-Item -LiteralPath $source -Force).Attributes -band [IO.FileAttributes]::ReadOnly)
 if(-not $link -or $readOnly -or -not[FluidFiles]::CreateHardLink($temporary,$source,[IntPtr]::Zero)){[IO.File]::Copy($source,$temporary,$false);[IO.File]::SetAttributes($temporary,([IO.File]::GetAttributes($temporary) -band (-bnot [IO.FileAttributes]::ReadOnly)))}
 if(-not[FluidFiles]::MoveFileEx($temporary,$target,9)){throw "Cannot replace $target. Close Fluid or choose a writable folder."}
}
function Copy-Tree([string]$source,[string]$target,[bool]$onlyMissing=$false){
 if(-not(Test-Path -LiteralPath $source)){return};Assert-Plain $source;Assert-Plain $target
 foreach($file in Get-ChildItem -LiteralPath $source -File -Recurse){
  if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Redirected profile/file rejected.'}
  $destination=Join-Path $target $file.FullName.Substring($source.Length+1)
  if(-not $onlyMissing -or -not(Test-Path -LiteralPath $destination)){Put-File $file.FullName $destination}
 }
}
New-Item -ItemType Directory -Path $InstallBase -Force|Out-Null
$sourceArchive=Join-Path $InstallBase "Source\EngineSource-v$version.zip";Put-File (Join-Path $payload 'EngineSource.zip') $sourceArchive
Copy-Tree (Join-Path $payload 'licenses') (Join-Path $InstallBase 'licenses')
Put-File (Join-Path $package 'FluidSetup.exe') (Join-Path $InstallBase 'FluidUninstall.exe')
Put-File (Join-Path $package 'Setup.ps1') (Join-Path $InstallBase 'Setup.ps1')
@{maintenance=$true;game='Both';version=$version}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $InstallBase 'maintenance-config.json') -Encoding UTF8
$records=@{};if(Test-Path -LiteralPath $RegistryPath){try{$old=Get-Content -LiteralPath $RegistryPath -Raw|ConvertFrom-Json;foreach($p in $old.PSObject.Properties){$records[$p.Name]=$p.Value}}catch{}}
foreach($s in $specs){
 $root=$s.Destination;$asset=Join-Path $root 'assets';$engineDir=Join-Path $root "bin\$($s.Game)"
 Write-Output "Installing $($s.Game)..."
 New-Item -ItemType Directory -Path $asset,$engineDir -Force|Out-Null
 if(-not $s.Exists){@{transfer_setup=1;permanent_install=1;version=$version;kind="Fluid v$version";game=$s.Game;root=$root;original_root=$s.Root;asset_root=$asset;exe=$s.Exe;setup_complete=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'experimental-install-state.json') -Encoding UTF8;$records[$s.Game]=$root;New-Item -ItemType Directory -Path (Split-Path $RegistryPath -Parent) -Force|Out-Null;$records|ConvertTo-Json|Set-Content -LiteralPath $RegistryPath -Encoding UTF8}
 $owned=[Collections.Generic.List[string]]::new()
 foreach($name in $s.Archives){$p=Join-Path $s.Root $name;if(Test-Path -LiteralPath $p){Put-File $p (Join-Path $asset $name) $true;$owned.Add($name)}}
 foreach($folder in @('Cursors','English','Movies','Scripts','WaterPlane')){
  $dir=Join-Path $s.Root "Data\$folder"
  if(Test-Path -LiteralPath $dir){Assert-Plain $dir;foreach($p in Get-ChildItem -LiteralPath $dir -File -Recurse){if($p.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Redirected asset rejected.'};$relative=$p.FullName.Substring($s.Root.Length+1);Put-File $p.FullName (Join-Path $asset $relative) $true;$owned.Add($relative)}}
 }
 foreach($dll in @('BINKW32.DLL','mss32.dll')){Put-File (Join-Path $s.Root $dll) (Join-Path $asset "bin\$dll");Put-File (Join-Path $s.Root $dll) (Join-Path $engineDir $dll)}
 @{schema=1;game=$s.Game;root=$asset;originalRoot=$s.Root;files=$owned.ToArray();portable_assets=1}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $asset 'install-state.json') -Encoding UTF8
 foreach($f in @('Launch-Fluid.ps1','Ensure-OwnedMilesAudio.ps1','Ensure-AdaptiveCameraInput.ps1','FluidLauncher.exe')){Put-File (Join-Path $payload $f) (Join-Path $root $f)}
 Put-File (Join-Path $payload "bin\$($s.Game)\$($s.Exe)") (Join-Path $engineDir $s.Exe)
 # Audio repair replaces damaged providers without writing through shared game assets.
 foreach($name in @('msssoft.m3d','mssmp3.asi','mssvoice.asi','mssdsp.flt','mssa3d.m3d','mssds3d.m3d','mssdx7.m3d','msseax.m3d','mssrsx.m3d')){$p=Join-Path $s.Root "MSS\$name";if(Test-Path -LiteralPath $p){Put-File $p (Join-Path $engineDir "MSS\$name")}}
 & (Join-Path $root 'Ensure-OwnedMilesAudio.ps1') -Game $s.Game -AssetRoot $asset -EnginePath (Join-Path $engineDir $s.Exe)|Out-Null
 if($Action -eq 'Reset'){
  $backup=Join-Path $root ('backups\'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6))
  foreach($name in @('profiles',"simulation-$($s.Game).ini")){
   $from=Join-Path $root $name;$to=Join-Path $backup $name
   if(Test-Path -LiteralPath $from){if(-not(Within $from $root) -or -not(Within $to $root)){throw 'Invalid reset path.'};if($name -eq 'profiles'){Assert-Plain $from};New-Item -ItemType Directory -Path $backup -Force|Out-Null;Move-Item -LiteralPath $from -Destination $to}
  }
  Write-Output 'Settings reset; previous saves/settings are in backups.'
 }elseif($s.Migrate -and $s.Migrate -ne $root){
  Copy-Tree (Join-Path $s.Migrate 'profiles') (Join-Path $root 'profiles') $true
  $old=Join-Path $s.Migrate "simulation-$($s.Game).ini";$new=Join-Path $root "simulation-$($s.Game).ini"
  if((Test-Path -LiteralPath $old) -and -not(Test-Path -LiteralPath $new)){Put-File $old $new}
 }
 $profile=Join-Path $root "profiles\$($s.Game)\sim-120";New-Item -ItemType Directory -Path $profile -Force|Out-Null
 if(-not(Test-Path -LiteralPath (Join-Path $profile 'Options.ini'))){
  Add-Type -AssemblyName System.Windows.Forms;$screen=[Windows.Forms.Screen]::PrimaryScreen.Bounds
  @("Resolution = $($screen.Width) $($screen.Height)",'AntiAliasing = 0','StaticGameLOD = High','DynamicLOD = yes','DisplayMode = 0')|Set-Content -LiteralPath (Join-Path $profile 'Options.ini') -Encoding ASCII
  & (Join-Path $root 'Ensure-AdaptiveCameraInput.ps1') -Profile $profile
 }
 $managed=@{};foreach($f in @('Launch-Fluid.ps1','Ensure-OwnedMilesAudio.ps1','Ensure-AdaptiveCameraInput.ps1','FluidLauncher.exe',"bin\$($s.Game)\$($s.Exe)","bin\$($s.Game)\BINKW32.DLL", "bin\$($s.Game)\mss32.dll", "bin\$($s.Game)\MSS\msssoft.m3d", "bin\$($s.Game)\MSS\mssmp3.asi", "bin\$($s.Game)\MSS\mssvoice.asi", "bin\$($s.Game)\MSS\mssdsp.flt")){$managed[$f]=(Get-FileHash -LiteralPath (Join-Path $root $f)).Hash}
 @{transfer_setup=1;permanent_install=1;setup_complete=$true;version=$version;kind="Fluid v$version";game=$s.Game;root=$root;original_root=$s.Root;asset_root=$asset;exe=$s.Exe;sha256=(Get-FileHash -LiteralPath (Join-Path $engineDir $s.Exe)).Hash;managed_hashes=$managed;source_archive=$sourceArchive;default_simulation_hz=120;original_simulation_hz=30;experimental_simulation_rates=@(60,144,240);updated=(Get-Date -Format o)}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $root 'experimental-install-state.json') -Encoding UTF8
 if(-not $NoShortcuts){
  $label=if($s.Game -eq 'ZeroHour'){'Command & Conquer - Fluid Generals Zero Hour'}else{'Command & Conquer - Fluid Generals'}
  $shortcut=(New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) "$label.lnk"))
  $shortcut.TargetPath=Join-Path $root 'FluidLauncher.exe';$shortcut.WorkingDirectory=$root;$shortcut.IconLocation=Join-Path $engineDir $s.Exe;$shortcut.Description='120 Hz simulation; normal gameplay speed.';$shortcut.Save()
 }
 $records[$s.Game]=$root
 Write-Output "$($s.Game) ready."
}
New-Item -ItemType Directory -Path (Split-Path $RegistryPath -Parent) -Force|Out-Null
$records|ConvertTo-Json|Set-Content -LiteralPath $RegistryPath -Encoding UTF8
Write-Output 'Complete. Use the Fluid desktop shortcuts. You can delete the downloaded ZIP and extracted setup folder.'
