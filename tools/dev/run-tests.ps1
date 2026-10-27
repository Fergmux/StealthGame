param([Parameter(Mandatory)] [string]$Filter, [string]$Log = "$env:TEMP\stealth_filtered_tests.log")
# Runs the automation tests whose names start with $Filter (e.g. "StealthGame.Crowd.Herring") headless, without building or
# closing anything: the editor target must be built and the editor closed. Prints one line per test plus the AddInfo lines
# of the run (the specs' measured observations). The full gate is still tools/dev/cycle.ps1 -Tests.
$root = Split-Path (Split-Path $PSScriptRoot)
$proj = Join-Path $root "StealthGame.uproject"
$ue = "C:\Program Files\Epic Games\UE_5.8"
& "$ue\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $proj -ExecCmds="Automation RunTests ^$Filter; Quit" -unattended -nullrhi -stdout -FullStdOutLogOutput 2>&1 | Out-File $Log -Encoding utf8
"tests exit=$LASTEXITCODE"
Select-String -Path $Log -Pattern 'Test Completed' | ForEach-Object { if ($_.Line -match 'Result=\{(\w+)\} Name=\{([^}]*)\} Path=\{([^}]*)\}') { "$($matches[1])  $($matches[3])" } }
Select-String -Path $Log -Pattern 'LogAutomationController: (Display|Error|Warning): .*(Expected|Info|failed|Error)' | Select-Object -First 80 | ForEach-Object { $_.Line.Substring([Math]::Min(60, $_.Line.Length)) }
