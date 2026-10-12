# 0BSD; see licenses/HELPERS-LICENSE.txt. Only adaptive profile input defaults.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Profile)
$ErrorActionPreference = 'Stop'
$Profile = [IO.Path]::GetFullPath($Profile)
$options = Join-Path $Profile 'Options.ini'
$marker = Join-Path $Profile 'camera-edge-input-v1.applied'
if ((Test-Path -LiteralPath $marker) -or -not (Test-Path -LiteralPath $options)) { return }
$original = [IO.File]::ReadAllBytes($options)
$encoding = [Text.UTF8Encoding]::new($false)
if ($original.Length -ge 3 -and $original[0] -eq 239 -and $original[1] -eq 187 -and $original[2] -eq 191) { $encoding = [Text.UTF8Encoding]::new($true) }
elseif ($original.Length -ge 2 -and $original[0] -eq 255 -and $original[1] -eq 254) { $encoding = [Text.UnicodeEncoding]::new($false,$true) }
elseif ($original.Length -ge 2 -and $original[0] -eq 254 -and $original[1] -eq 255) { $encoding = [Text.UnicodeEncoding]::new($true,$true) }
$text = [IO.File]::ReadAllText($options)
$newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
foreach ($key in @('ScreenEdgeScrollEnabledInFullscreenApp','ScreenEdgeScrollEnabledInWindowedApp','CursorCaptureEnabledInFullscreenGame','CursorCaptureEnabledInWindowedGame')) {
    $pattern = '(?mi)^(\s*' + [regex]::Escape($key) + '\s*=\s*)[^\r\n]*'
    if ([regex]::IsMatch($text,$pattern)) { $text = [regex]::Replace($text,$pattern,'${1}yes') }
    else { if (-not $text.EndsWith($newline)) { $text += $newline }; $text += "$key = yes$newline" }
}
# Preserve the original once and avoid overriding later deliberate preferences.
$backup = Join-Path $Profile 'Options.before-camera-edge-input-v1.ini'
if (-not (Test-Path -LiteralPath $backup)) { [IO.File]::WriteAllBytes($backup,$original) }
[IO.File]::WriteAllText($options,$text,$encoding)
[IO.File]::WriteAllText($marker,"Adaptive edge navigation defaults migrated once.$newline",[Text.UTF8Encoding]::new($false))
