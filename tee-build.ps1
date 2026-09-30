# Live-tee helper for build.bat. Invoked with VC_BAT and VC_LOG set.
$ErrorActionPreference = "Continue"
$bat = $env:VC_BAT
$log = $env:VC_LOG
if (-not $bat -or -not $log) { Write-Error "VC_BAT / VC_LOG not set"; exit 1 }
cmd.exe /c "call `"$bat`" --inner" 2>&1 | Tee-Object -FilePath $log
$code = $LASTEXITCODE
if ($null -eq $code) { $code = 0 }
exit $code
