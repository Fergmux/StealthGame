<#
Per-commit gate (ADR-0008 section 8; story tdc-021). Stages, in order, each one stopping the run on failure:
  1. T0 lint      tools/ci/lint/gate.sh over the include lint rules (a-j) and the VCS conformance check
  2. Build        Build.bat (as tools/dev/cycle.ps1; UnrealBuildTool.exe called directly lacks the dotnet environment) StealthGameEditor Win64 Development
  3. Automation   UnrealEditor-Cmd.exe <uproject> -nullrhi -nosound -ExecCmds="Automation RunTests ^StealthGame.; Quit"
                  -unattended -stdout -FullStdOutLogOutput   (prefix filter, all T1 and T2 tests; never RunFilter)
  4. Verdict      parsed from the automation OUTPUT, never from the editor exit code (it is 255 both when a test
                  fails and when nothing matched)

Verdict and exit code (the printed summary always carries the verdict word):
  0  PASS           at least one "Test Completed" line and no "Result={Fail}"
  1  FAIL           lint findings, a failed build, or any "Result={Fail}" (also when the log holds passes too)
  2  NOT ASSESSED   "No automation tests matched", no "Test Completed" line, an empty log, a lint run that
                    scanned nothing, or a tool that could not be started: never a pass

The gate only observes and decides here; the lint scripts and the engine emit the observations. It does not
use the MCP toolsets or Python editor scripting. Budget: 10 minutes of wall time per commit; the elapsed time
of every stage and the total are printed and "BUDGET: EXCEEDED" is printed when it is passed (a suite over the
budget moves to nightly by ADR amendment, never by disabling it).

Compile probes (ADR-0008 section 8: when a Build.cs, the guard header or any Public/Server header changed) are
a documented hook point at the "# HOOK: compile probes" comment below; the plan puts them in the nightly
(story tdc-023), so nothing runs there yet.

Usage:
  powershell -ExecutionPolicy Bypass -File tools/ci/gate-per-commit.ps1 [-CloseEditor]
  powershell -ExecutionPolicy Bypass -File tools/ci/gate-per-commit.ps1 -AnalyzeLog <automation-output.log>
  powershell -ExecutionPolicy Bypass -File tools/ci/gate-per-commit.ps1 -SelfTest

  -CloseEditor      close a running editor first (a running editor locks the game DLLs); without it the
                    gate reports NOT ASSESSED when an editor is running
  -AnalyzeLog       decide on a recorded automation log only (no lint, build or run)
  -SelfTest         canned-log and stub-stage selftest (story tdc-021 G2); never runs the real build or tests,
                    so the automation wrapper that calls it cannot recurse
  -LintRoot, -UbtPath, -EditorCmdPath, -Bash, -LogFile   overrides used by the selftest
Environment: UE_ROOT (default C:\Program Files\Epic Games\UE_5.8), STEALTH_BASH.
#>
param(
  [switch]$CloseEditor,
  [string]$AnalyzeLog = '',
  [switch]$SelfTest,
  [string]$LintRoot = '',
  [string]$UbtPath = '',
  [string]$EditorCmdPath = '',
  [string]$Bash = '',
  [string]$LogFile = ''
)
$ErrorActionPreference = 'Continue'
$repo = Split-Path (Split-Path $PSScriptRoot)
$proj = Join-Path $repo 'StealthGame.uproject'
$BudgetSeconds = 600
$ExitPass = 0; $ExitFail = 1; $ExitNotAssessed = 2

# Lint rule scripts the gate runs (T0): include lint rules a-j and the VCS conformance check.
$LintRules = @('include-lint-abc.sh', 'include-lint-def.sh', 'include-lint-gh.sh', 'include-lint-ij.sh', 'vcs-conformance.sh')

function ConvertTo-Slash([string]$p) { return ($p -replace '\\', '/') }

# Verdict from automation output text. Returns @{ Verdict; Code; Reason; Completed; Failed }.
function Get-AutomationVerdict([string]$Text) {
  # Anchored on the engine's completion line, so text that merely quotes a verdict (a test echoing its own output) is not counted.
  $completed = ([regex]::Matches($Text, 'Test Completed\. Result=\{\w+\}')).Count
  $failed = ([regex]::Matches($Text, 'Test Completed\. Result=\{Fail\}')).Count
  if ($failed -gt 0) {
    return @{ Verdict = 'FAIL'; Code = $ExitFail; Reason = "$failed test(s) reported Result={Fail}"; Completed = $completed; Failed = $failed }
  }
  if ($Text -match 'No automation tests matched') {
    return @{ Verdict = 'NOT ASSESSED'; Code = $ExitNotAssessed; Reason = 'No automation tests matched the ^StealthGame. filter'; Completed = $completed; Failed = 0 }
  }
  if ($completed -eq 0) {
    return @{ Verdict = 'NOT ASSESSED'; Code = $ExitNotAssessed; Reason = 'no "Test Completed" line: the run did not execute tests'; Completed = 0; Failed = 0 }
  }
  return @{ Verdict = 'PASS'; Code = $ExitPass; Reason = "$completed test(s) completed, none failed"; Completed = $completed; Failed = 0 }
}

function Find-Bash {
  if ($Bash) { return $Bash }
  if ($env:STEALTH_BASH -and (Test-Path $env:STEALTH_BASH)) { return $env:STEALTH_BASH }
  foreach ($root in @($env:ProgramW6432, $env:ProgramFiles)) {
    if ($root) { $c = Join-Path $root 'Git\bin\bash.exe'; if (Test-Path $c) { return $c } }
  }
  return ''
}

function Write-Verdict($v, [string]$Stage, $Timer) {
  $total = [int]$Timer.Elapsed.TotalSeconds
  "GATE-PER-COMMIT: $($v.Verdict) [$Stage] $($v.Reason)"
  "ELAPSED: total=${total}s budget=${BudgetSeconds}s"
  if ($total -gt $BudgetSeconds) { "BUDGET: EXCEEDED (${total}s > ${BudgetSeconds}s): move the slowest suite to nightly by ADR amendment, never disable it" }
}

function Invoke-Gate {
  $timer = [System.Diagnostics.Stopwatch]::StartNew()
  $ue = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }

  if ($AnalyzeLog) {
    if (-not (Test-Path $AnalyzeLog)) { Write-Verdict @{ Verdict = 'NOT ASSESSED'; Code = $ExitNotAssessed; Reason = "log not found: $AnalyzeLog" } 'analyze' $timer; exit $ExitNotAssessed }
    $v = Get-AutomationVerdict (Get-Content -Raw -Path $AnalyzeLog)
    Write-Verdict $v 'analyze' $timer
    exit $v.Code
  }

  # ---- Stage 1: T0 lint -------------------------------------------------------------------------
  $bashExe = Find-Bash
  if (-not $bashExe) { Write-Verdict @{ Verdict = 'NOT ASSESSED'; Code = $ExitNotAssessed; Reason = 'no bash found for the lint scripts (set STEALTH_BASH)' } 'lint' $timer; exit $ExitNotAssessed }
  $lintDir = ConvertTo-Slash (Join-Path $repo 'tools/ci/lint')
  $root = if ($LintRoot) { ConvertTo-Slash $LintRoot } else { ConvertTo-Slash $repo }
  "STAGE lint: bash gate.sh --root $root $($LintRules -join ' ')"
  $lintArgs = @("$lintDir/gate.sh", '--root', $root) + ($LintRules | ForEach-Object { "$lintDir/$_" })
  & $bashExe @lintArgs
  $lintCode = $LASTEXITCODE
  if ($lintCode -eq 1) { Write-Verdict @{ Verdict = 'FAIL'; Code = $ExitFail; Reason = 'T0 lint reported findings (see above); the build was not started' } 'lint' $timer; exit $ExitFail }
  if ($lintCode -ne 0) { Write-Verdict @{ Verdict = 'NOT ASSESSED'; Code = $ExitNotAssessed; Reason = "T0 lint exited $lintCode (not assessed); the build was not started" } 'lint' $timer; exit $ExitNotAssessed }
  "STAGE lint: PASS ($([int]$timer.Elapsed.TotalSeconds)s)"

  # HOOK: compile probes. When a Build.cs, StealthServerOnlyGuard.h or a Public/Server header changed,
  # ADR-0008 section 8 runs tools/ci/compile-probes/gate-probes.sh here. Left to the nightly (tdc-023).

  # ---- Stage 2: build ---------------------------------------------------------------------------
  $running = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($running -and -not $UbtPath) {
    if ($CloseEditor) {
      $null = $running.CloseMainWindow()
      if (-not $running.WaitForExit(20000)) { Stop-Process -Id $running.Id -Confirm:$false; Start-Sleep -Seconds 3; 'editor force-closed (save prompt)' } else { 'editor closed' }
    } else {
      Write-Verdict @{ Verdict = 'NOT ASSESSED'; Code = $ExitNotAssessed; Reason = 'UnrealEditor is running and locks the game DLLs; close it or pass -CloseEditor' } 'build' $timer
      exit $ExitNotAssessed
    }
  }
  $ubt = if ($UbtPath) { $UbtPath } else { Join-Path $ue 'Engine/Build/BatchFiles/Build.bat' }
  if (-not (Test-Path $ubt)) { Write-Verdict @{ Verdict = 'NOT ASSESSED'; Code = $ExitNotAssessed; Reason = "build tool not found: $ubt" } 'build' $timer; exit $ExitNotAssessed }
  "STAGE build: $ubt StealthGameEditor Win64 Development -Project=$proj"
  $buildOut = & $ubt StealthGameEditor Win64 Development "-Project=$proj" -WaitMutex 2>&1 | ForEach-Object { "$_" }
  $buildCode = $LASTEXITCODE
  $buildOut | Select-String -Pattern 'error|warning C|Result:' | ForEach-Object { $_.Line }
  if ($buildCode -ne 0 -or -not ($buildOut -match 'Result: Succeeded')) {
    Write-Verdict @{ Verdict = 'FAIL'; Code = $ExitFail; Reason = "build failed (UBT exit $buildCode); the automation run was not started" } 'build' $timer
    exit $ExitFail
  }
  "STAGE build: PASS ($([int]$timer.Elapsed.TotalSeconds)s elapsed)"

  # ---- Stage 3: automation run ------------------------------------------------------------------
  $editorCmd = if ($EditorCmdPath) { $EditorCmdPath } else { Join-Path $ue 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe' }
  if (-not (Test-Path $editorCmd)) { Write-Verdict @{ Verdict = 'NOT ASSESSED'; Code = $ExitNotAssessed; Reason = "UnrealEditor-Cmd not found: $editorCmd" } 'automation' $timer; exit $ExitNotAssessed }
  $log = if ($LogFile) { $LogFile } else { Join-Path $env:TEMP 'stealth_gate_per_commit.log' }
  "STAGE automation: $editorCmd $proj -nullrhi -nosound -ExecCmds=`"Automation RunTests ^StealthGame.; Quit`" -unattended -stdout -FullStdOutLogOutput"
  & $editorCmd $proj -nullrhi -nosound -ExecCmds="Automation RunTests ^StealthGame.; Quit" -unattended -stdout -FullStdOutLogOutput 2>&1 | Out-File $log -Encoding utf8
  "editor exit code $LASTEXITCODE (ignored: the verdict is taken from the output); log: $log"

  # ---- Stage 4: verdict -------------------------------------------------------------------------
  $text = if (Test-Path $log) { Get-Content -Raw -Path $log } else { '' }
  $v = Get-AutomationVerdict $text
  Select-String -InputObject $text -Pattern 'Result=\{(\w+)\} Name=\{([^}]*)\} Path=\{([^}]*)\}' -AllMatches | ForEach-Object { $_.Matches } | ForEach-Object { "$($_.Groups[1].Value)  $($_.Groups[3].Value)" }
  Write-Verdict $v 'automation' $timer
  exit $v.Code
}

# ------------------------------------------------------------------------------------------------
# Selftest (G2): canned logs and stub stages. Never starts the real build or the editor.
# ------------------------------------------------------------------------------------------------
function Invoke-SelfTest {
  $fx = Join-Path $PSScriptRoot 'selftest/gate-per-commit'
  $script = $PSCommandPath
  $script:selfBad = 0; $script:selfTotal = 0
  function Report([string]$name, [bool]$ok, [string]$detail) {
    $script:selfTotal++
    if ($ok) { "SELFTEST $name ok" } else { "SELFTEST $name FAILED: $detail"; $script:selfBad++ }
  }
  # Verdict from each canned log, through the real parser and through the -AnalyzeLog entry point.
  $cases = @(
    @{ Name = 'no-tests-matched'; Log = 'no-tests-matched.txt'; Verdict = 'NOT ASSESSED'; Code = 2 },
    @{ Name = 'one-failure'; Log = 'one-failure.txt'; Verdict = 'FAIL'; Code = 1 },
    @{ Name = 'all-pass'; Log = 'all-pass.txt'; Verdict = 'PASS'; Code = 0 },
    @{ Name = 'pass-and-fail-is-fail'; Log = 'pass-and-fail.txt'; Verdict = 'FAIL'; Code = 1 },
    @{ Name = 'empty-log-not-assessed'; Log = 'empty.txt'; Verdict = 'NOT ASSESSED'; Code = 2 },
    @{ Name = 'no-completion-lines-not-assessed'; Log = 'no-completion.txt'; Verdict = 'NOT ASSESSED'; Code = 2 },
    @{ Name = 'missing-log-not-assessed'; Log = 'does-not-exist.txt'; Verdict = 'NOT ASSESSED'; Code = 2 }
  )
  foreach ($c in $cases) {
    $out = & powershell -NoProfile -ExecutionPolicy Bypass -File $script -AnalyzeLog (Join-Path $fx $c.Log) 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $line = ($out | Where-Object { $_ -like 'GATE-PER-COMMIT:*' }) -join ''
    Report $c.Name (($code -eq $c.Code) -and ($line -like "GATE-PER-COMMIT: $($c.Verdict) *")) "exit $code (want $($c.Code)), output: $($out -join ' | ')"
    $out | ForEach-Object { "    $_" }
  }
  # Stage ordering with stub tools. A clean tiny lint root keeps the lint stage fast and passing.
  $cleanRoot = Join-Path $PSScriptRoot 'lint/selftest/include-abc/fixtures/c-clean'
  $seededRoot = Join-Path $PSScriptRoot 'lint/fixtures/seeded'
  $stages = @(
    @{ Name = 'lint-failure-stops-before-build'; Args = @('-LintRoot', $seededRoot, '-UbtPath', (Join-Path $fx 'build-ok.cmd'), '-EditorCmdPath', (Join-Path $fx 'editor-all-pass.cmd'));
       Code = 1; Verdict = 'FAIL'; Has = 'GATE-PER-COMMIT: FAIL [lint]'; Not = 'STAGE build' },
    @{ Name = 'build-failure-stops-before-automation'; Args = @('-LintRoot', $cleanRoot, '-UbtPath', (Join-Path $fx 'build-fail.cmd'), '-EditorCmdPath', (Join-Path $fx 'editor-all-pass.cmd'));
       Code = 1; Verdict = 'FAIL'; Has = 'GATE-PER-COMMIT: FAIL [build]'; Not = 'STAGE automation' },
    @{ Name = 'full-flow-pass'; Args = @('-LintRoot', $cleanRoot, '-UbtPath', (Join-Path $fx 'build-ok.cmd'), '-EditorCmdPath', (Join-Path $fx 'editor-all-pass.cmd'));
       Code = 0; Verdict = 'PASS'; Has = 'GATE-PER-COMMIT: PASS [automation]'; Not = '' },
    @{ Name = 'full-flow-no-tests-matched'; Args = @('-LintRoot', $cleanRoot, '-UbtPath', (Join-Path $fx 'build-ok.cmd'), '-EditorCmdPath', (Join-Path $fx 'editor-no-match.cmd'));
       Code = 2; Verdict = 'NOT ASSESSED'; Has = 'GATE-PER-COMMIT: NOT ASSESSED [automation]'; Not = '' },
    @{ Name = 'full-flow-one-failure'; Args = @('-LintRoot', $cleanRoot, '-UbtPath', (Join-Path $fx 'build-ok.cmd'), '-EditorCmdPath', (Join-Path $fx 'editor-one-failure.cmd'));
       Code = 1; Verdict = 'FAIL'; Has = 'GATE-PER-COMMIT: FAIL [automation]'; Not = '' }
  )
  $logPath = Join-Path $env:TEMP 'stealth_gate_selftest.log'
  foreach ($s in $stages) {
    $out = & powershell -NoProfile -ExecutionPolicy Bypass -File $script @($s.Args) -LogFile $logPath 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $joined = $out -join "`n"
    $ok = ($code -eq $s.Code) -and $joined.Contains($s.Has) -and ((-not $s.Not) -or (-not $joined.Contains($s.Not)))
    Report $s.Name $ok "exit $code (want $($s.Code)); want '$($s.Has)' and not '$($s.Not)'; output: $($out -join ' | ')"
    $out | ForEach-Object { "    $_" }
  }
  if ($script:selfBad -eq 0) { "SELFTEST RESULT: PASS ($script:selfTotal cases)"; exit 0 }
  "SELFTEST RESULT: FAIL ($script:selfBad of $script:selfTotal cases)"
  exit 1
}

if ($SelfTest) { Invoke-SelfTest }
Invoke-Gate
