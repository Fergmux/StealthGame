<#
Tuning schema bootstrap regeneration (ADR-0004 section 2; story tdc-042).

When a tuning field is renamed or removed, the checked-in table
Source/StealthFoundation/Private/Tuning/StealthTuningSchema.generated.inl names a field that no longer exists,
StealthFoundation stops compiling, and the editor that hosts the generator cannot build. This script breaks that
deadlock:
  1. bootstrap-build  STEALTH_TUNING_SCHEMA_BOOTSTRAP=1 (read by StealthFoundation.Build.cs), delete the UBT makefile of
                      StealthGameEditor Win64 Development and build it: the module compiles an EMPTY table
  2. generate         UnrealEditor-Cmd <uproject> -ExecCmds="Stealth.Tuning.SchemaGen,QUIT_EDITOR": the generator
                      reflects the renamed declarations and rewrites the table (summary WROTE or UNCHANGED)
  3. final-build      the variable removed, makefile deleted again, rebuild: the regenerated table compiles
  4. verify           UnrealEditor-Cmd -ExecCmds="Stealth.Tuning.SchemaVerify,QUIT_EDITOR": PASS only when the build is
                      not a bootstrap build (catches a define left set), the compiled table is non-empty and equals
                      the reflection, and the checked-in file equals the generated text
Every stage failing (a build error, a generator FAIL line, no summary line at all) stops the script with exit 1.
A build with the define can never pass CI: StealthGame.Tuning.SchemaComplete fails on an empty table (story tdc-044).

Why the makefile is deleted: UBT 5.8 caches the evaluated Build.cs rules in
Intermediate/Build/Win64/<arch>/StealthGameEditor/Development/Makefile.bin and does not track environment variables
(it reloads the makefile even with -NoUBTMakefiles, verified 2026-10-25 against Modes/BuildMode.cs and
System/TargetMakefile.cs). Without the deletion a changed define is silently ignored; the verify stage is what
caught that on the first run.

This is NOT a per-commit step (two full UBT cycles plus two editor starts). Run it on demand, in the working tree
after a rename, or in a disposable git worktree for its own evidence (story tdc-042). It edits only the generated
table (through the generator). Summary line: "REGENERATE-TUNING-SCHEMA: PASS|FAIL [stage] <reason>".

Usage:
  powershell -ExecutionPolicy Bypass -File tools/ci/regenerate-tuning-schema.ps1 [-ProjectRoot <dir>] [-CloseEditor]
  powershell -ExecutionPolicy Bypass -File tools/ci/regenerate-tuning-schema.ps1 -DryRun     prints the planned steps
  powershell -ExecutionPolicy Bypass -File tools/ci/regenerate-tuning-schema.ps1 -SelfTest   stub-tool selftest (G2)

  -ProjectRoot      the project to regenerate (default: this script's repository)
  -CloseEditor      close a running UnrealEditor first (it locks the game DLLs of the working tree)
  -UbtPath, -EditorCmdPath, -LogDir   overrides used by the selftest (stub tools)
Environment: UE_ROOT (default C:\Program Files\Epic Games\UE_5.8).
#>
param(
  [string]$ProjectRoot = '',
  [switch]$CloseEditor,
  [switch]$DryRun,
  [switch]$SelfTest,
  [string]$UbtPath = '',
  [string]$EditorCmdPath = '',
  [string]$LogDir = ''
)
$ErrorActionPreference = 'Continue'
$DefineName = 'STEALTH_TUNING_SCHEMA_BOOTSTRAP'
$repo = if ($ProjectRoot) { (Resolve-Path $ProjectRoot).Path } else { Split-Path (Split-Path $PSScriptRoot) }
$proj = Join-Path $repo 'StealthGame.uproject'
$ue = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }
$ubt = if ($UbtPath) { $UbtPath } else { Join-Path $ue 'Engine\Build\BatchFiles\Build.bat' }
$editorCmd = if ($EditorCmdPath) { $EditorCmdPath } else { Join-Path $ue 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe' }
$logs = if ($LogDir) { $LogDir } else { $env:TEMP }

function Write-Result([string]$verdict, [string]$stage, [string]$reason) {
  "REGENERATE-TUNING-SCHEMA: $verdict [$stage] $reason"
}

# Build stage: sets $script:StageOk to $true on "Result: Succeeded" and exit code 0 (functions here emit their
# progress lines as output, so results travel in script variables, never as return values).
function Invoke-BuildStage([string]$stage) {
  "STAGE ${stage}: $ubt StealthGameEditor Win64 Development -Project=$proj -WaitMutex ($DefineName=$([Environment]::GetEnvironmentVariable($DefineName)))"
  if (-not $UbtPath) {
    # The makefile caches the Build.cs evaluation (and so the define); delete it so the rules are re-read.
    Get-ChildItem -Path (Join-Path $repo 'Intermediate\Build\Win64') -Recurse -Filter 'Makefile.bin' -ErrorAction SilentlyContinue |
      Where-Object { $_.FullName -match '[\\/]StealthGameEditor[\\/]Development[\\/]Makefile\.bin$' } |
      ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force; "    removed UBT makefile $($_.FullName)" }
  }
  $out = & $ubt StealthGameEditor Win64 Development "-Project=$proj" -WaitMutex 2>&1 | ForEach-Object { "$_" }
  $code = $LASTEXITCODE
  $out | Select-String -Pattern 'error|warning C|Result:|stub build' | ForEach-Object { "    $($_.Line)" }
  $script:StageOk = (($code -eq 0) -and [bool]($out -match 'Result: Succeeded'))
}

# Editor stage: runs one console command headless and sets $script:Summary to the last summary line containing
# $summary (empty when the editor printed none, which counts as a failure).
function Invoke-EditorStage([string]$stage, [string]$command, [string]$summary) {
  $log = Join-Path $logs "stealth_regen_$stage.log"
  "STAGE ${stage}: $editorCmd $proj -ExecCmds=`"$command,QUIT_EDITOR`" -unattended -nullrhi -nosound -stdout (log: $log)"
  & $editorCmd $proj -ExecCmds="$command,QUIT_EDITOR" -unattended -nullrhi -nosound -stdout -FullStdOutLogOutput 2>&1 | Out-File $log -Encoding utf8
  "    editor exit code $LASTEXITCODE (ignored: the verdict is the summary line)"
  $text = if (Test-Path $log) { Get-Content -Path $log } else { @() }
  $text | Where-Object { $_ -match 'LogStealthTuningSchemaGen' -and $_ -match 'Error' } | ForEach-Object { "    $_" }
  $line = $text | Where-Object { $_ -match [regex]::Escape($summary) } | Select-Object -Last 1
  if ($line) { "    $line" }
  $script:Summary = "$line"
}

function Invoke-Regenerate {
  if (-not (Test-Path $proj)) { Write-Result 'FAIL' 'preflight' "no StealthGame.uproject under $repo"; exit 1 }
  $callerDefine = [Environment]::GetEnvironmentVariable($DefineName)
  if ($callerDefine) { "note: $DefineName=$callerDefine was already set by the caller; it is removed before the final build" }

  if ($DryRun) {
    "DRY RUN (nothing is built or run):"
    "  1. bootstrap-build  set $DefineName=1; delete the StealthGameEditor Development Makefile.bin; $ubt StealthGameEditor Win64 Development -Project=$proj -WaitMutex"
    "  2. generate         $editorCmd $proj -ExecCmds=`"Stealth.Tuning.SchemaGen,QUIT_EDITOR`" -unattended -nullrhi -nosound -stdout; want 'STEALTH TUNING SCHEMA GEN: WROTE|UNCHANGED'"
    "  3. final-build      remove $DefineName; delete the makefile again; $ubt StealthGameEditor Win64 Development -Project=$proj -WaitMutex"
    "  4. verify           $editorCmd $proj -ExecCmds=`"Stealth.Tuning.SchemaVerify,QUIT_EDITOR`" ...; want 'STEALTH TUNING SCHEMA VERIFY: PASS'"
    Write-Result 'PASS' 'dry-run' 'planned steps printed'
    exit 0
  }

  $running = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($running -and -not $UbtPath) {
    if ($CloseEditor) {
      $null = $running.CloseMainWindow()
      if (-not $running.WaitForExit(20000)) { Stop-Process -Id $running.Id -Confirm:$false; Start-Sleep -Seconds 3; 'editor force-closed (save prompt)' } else { 'editor closed' }
    } else {
      "note: an UnrealEditor process is running; if it holds this project's DLLs the build fails (pass -CloseEditor)"
    }
  }

  try {
    [Environment]::SetEnvironmentVariable($DefineName, '1', 'Process')
    Invoke-BuildStage 'bootstrap-build'
    if (-not $script:StageOk) { Write-Result 'FAIL' 'bootstrap-build' 'the editor did not build with the empty bootstrap table'; exit 1 }

    Invoke-EditorStage 'generate' 'Stealth.Tuning.SchemaGen' 'STEALTH TUNING SCHEMA GEN:'
    $gen = $script:Summary
    if ($gen -notmatch 'STEALTH TUNING SCHEMA GEN: (WROTE|UNCHANGED)') {
      $why = if ($gen) { $gen.Trim() } else { 'no summary line (the editor or the console command did not run)' }
      Write-Result 'FAIL' 'generate' "generator failed: $why"
      exit 1
    }

    [Environment]::SetEnvironmentVariable($DefineName, $null, 'Process')
    Invoke-BuildStage 'final-build'
    if (-not $script:StageOk) { Write-Result 'FAIL' 'final-build' 'the regenerated table does not compile'; exit 1 }

    Invoke-EditorStage 'verify' 'Stealth.Tuning.SchemaVerify' 'STEALTH TUNING SCHEMA VERIFY:'
    $verify = $script:Summary
    if ($verify -notmatch 'STEALTH TUNING SCHEMA VERIFY: PASS') {
      $why = if ($verify) { $verify.Trim() } else { 'no summary line' }
      Write-Result 'FAIL' 'verify' "verification failed: $why"
      exit 1
    }
    Write-Result 'PASS' 'verify' 'table regenerated, rebuilt without the define and verified'
    exit 0
  }
  finally {
    [Environment]::SetEnvironmentVariable($DefineName, $null, 'Process')
  }
}

# ------------------------------------------------------------------------------------------------
# Selftest (G2): stub build and editor tools. Never starts the real build or the editor, so the
# automation wrapper (StealthGame.Build.RegenerateTuningSchema) cannot rebuild the editor it runs in.
# ------------------------------------------------------------------------------------------------
function Invoke-SelfTest {
  $fx = Join-Path $PSScriptRoot 'selftest/regenerate-tuning-schema'
  $script = $PSCommandPath
  $script:bad = 0; $script:total = 0
  function Report([string]$name, [bool]$ok, [string]$detail) {
    $script:total++
    if ($ok) { "SELFTEST $name ok" } else { "SELFTEST $name FAILED: $detail"; $script:bad++ }
  }
  $cases = @(
    @{ Name = 'full-flow-pass'; Ubt = 'build-ok.cmd'; Ed = 'editor-ok.cmd'; Pre = ''; Extra = @();
       Code = 0; Has = @('REGENERATE-TUNING-SCHEMA: PASS [verify]'); Not = @(); Order = @('bootstrap=[1]', 'STAGE generate', 'STAGE final-build', 'bootstrap=[]', 'STAGE verify') },
    @{ Name = 'commandlet-failure-fails-script'; Ubt = 'build-ok.cmd'; Ed = 'editor-gen-fail.cmd'; Pre = ''; Extra = @();
       Code = 1; Has = @('REGENERATE-TUNING-SCHEMA: FAIL [generate]'); Not = @('STAGE final-build'); Order = @() },
    @{ Name = 'editor-without-summary-fails'; Ubt = 'build-ok.cmd'; Ed = 'editor-silent.cmd'; Pre = ''; Extra = @();
       Code = 1; Has = @('REGENERATE-TUNING-SCHEMA: FAIL [generate]', 'no summary line'); Not = @('STAGE final-build'); Order = @() },
    @{ Name = 'bootstrap-build-failure-stops'; Ubt = 'build-fail.cmd'; Ed = 'editor-ok.cmd'; Pre = ''; Extra = @();
       Code = 1; Has = @('REGENERATE-TUNING-SCHEMA: FAIL [bootstrap-build]'); Not = @('STAGE generate'); Order = @() },
    @{ Name = 'final-build-failure-stops'; Ubt = 'build-fail-unless-bootstrap.cmd'; Ed = 'editor-ok.cmd'; Pre = ''; Extra = @();
       Code = 1; Has = @('REGENERATE-TUNING-SCHEMA: FAIL [final-build]'); Not = @('STAGE verify'); Order = @() },
    @{ Name = 'define-left-set-is-caught'; Ubt = 'build-ok.cmd'; Ed = 'editor-verify-bootstrap.cmd'; Pre = ''; Extra = @();
       Code = 1; Has = @('REGENERATE-TUNING-SCHEMA: FAIL [verify]', 'bootstrap'); Not = @(); Order = @() },
    @{ Name = 'caller-define-removed-before-final-build'; Ubt = 'build-ok.cmd'; Ed = 'editor-ok.cmd'; Pre = '1'; Extra = @();
       Code = 0; Has = @('was already set by the caller', 'REGENERATE-TUNING-SCHEMA: PASS [verify]'); Not = @(); Order = @('STAGE final-build', 'bootstrap=[]') },
    @{ Name = 'dry-run-runs-nothing'; Ubt = 'build-fail.cmd'; Ed = 'editor-gen-fail.cmd'; Pre = ''; Extra = @('-DryRun');
       Code = 0; Has = @('DRY RUN', 'REGENERATE-TUNING-SCHEMA: PASS [dry-run]'); Not = @('stub build', 'STAGE bootstrap-build'); Order = @() }
  )
  foreach ($c in $cases) {
    [Environment]::SetEnvironmentVariable($DefineName, $(if ($c.Pre) { $c.Pre } else { $null }), 'Process')
    $args2 = @('-UbtPath', (Join-Path $fx $c.Ubt), '-EditorCmdPath', (Join-Path $fx $c.Ed), '-LogDir', $env:TEMP) + $c.Extra
    $out = & powershell -NoProfile -ExecutionPolicy Bypass -File $script @args2 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    [Environment]::SetEnvironmentVariable($DefineName, $null, 'Process')
    $joined = $out -join "`n"
    $ok = ($code -eq $c.Code)
    foreach ($h in $c.Has) { if (-not $joined.Contains($h)) { $ok = $false } }
    foreach ($n in $c.Not) { if ($joined.Contains($n)) { $ok = $false } }
    $at = -1
    foreach ($o in $c.Order) {
      $next = $joined.IndexOf($o, [Math]::Max($at, 0))
      if ($next -lt 0 -or $next -lt $at) { $ok = $false; break }
      $at = $next + $o.Length
    }
    Report $c.Name $ok "exit $code (want $($c.Code)); want $($c.Has -join ', ') in order $($c.Order -join ' > '), not $($c.Not -join ', ')"
    $out | ForEach-Object { "    $_" }
  }
  if ($script:bad -eq 0) { "SELFTEST RESULT: PASS ($script:total cases)"; exit 0 }
  "SELFTEST RESULT: FAIL ($script:bad of $script:total cases)"
  exit 1
}

if ($SelfTest) { Invoke-SelfTest }
Invoke-Regenerate
