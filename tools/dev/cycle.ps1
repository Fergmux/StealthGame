param([switch]$Tests, [switch]$NoLaunch)
# Dev loop: close the editor -> build the editor target -> (optionally run the ^StealthGame. prefix-filtered automation tests, ADR-0008)
# -> relaunch the editor and wait for its MCP server. Run from anywhere.
# If the editor shows "Save Content" for the sample camera assets on close, it is force-closed: those
# assets must NOT be resaved in 5.8 (see production/first-playable-report.md, Changed project content).
$root = Split-Path (Split-Path $PSScriptRoot)
$proj = Join-Path $root "StealthGame.uproject"
$ue = "C:\Program Files\Epic Games\UE_5.8"
$log = Join-Path $root "Saved\Logs\StealthGame.log"

$p = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Select-Object -First 1
if ($p) {
  $null = $p.CloseMainWindow()
  if (-not $p.WaitForExit(20000)) { Stop-Process -Id $p.Id -Confirm:$false; Start-Sleep -Seconds 3; "editor force-closed (save prompt)" } else { "editor closed" }
}
$b = & "$ue\Engine\Build\BatchFiles\Build.bat" StealthGameEditor Win64 Development "-Project=$proj" -WaitMutex 2>&1 | Select-String -Pattern 'error|warning C|Result:'
$b | ForEach-Object { $_.Line }
if (-not ($b -match 'Succeeded')) { "BUILD FAILED"; exit 1 }

if ($Tests) {
  $testLog = Join-Path $env:TEMP "stealth_tests.log"
  & "$ue\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $proj -ExecCmds="Automation RunTests ^StealthGame.; Quit" -unattended -nullrhi -stdout -FullStdOutLogOutput 2>&1 | Out-File $testLog -Encoding utf8
  "tests exit=$LASTEXITCODE"
  Select-String -Path $testLog -Pattern 'Test Completed' | ForEach-Object { if ($_.Line -match 'Result=\{(\w+)\} Name=\{([^}]*)\} Path=\{([^}]*)\}') { "$($matches[1])  $($matches[3])" } }
}
if ($NoLaunch) { exit 0 }

Start-Process -FilePath "$ue\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"$proj`""
Start-Sleep -Seconds 25
$deadline = (Get-Date).AddMinutes(8)
do { Start-Sleep -Seconds 8; $ok = Select-String -Path $log -Pattern 'Starting MCP server' -Quiet -ErrorAction SilentlyContinue } until ($ok -or (Get-Date) -gt $deadline)
Start-Sleep -Seconds 12
"mcp started: $ok"
