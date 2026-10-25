<#
Nightly gate (ADR-0008 section 8, story tdc-023). Sequences the nightly components, collects their logs and
decides ONE verdict. The components emit observations; the decision is made here and in the scripts each
component already owns (gate-per-commit.ps1, gate-probes.sh, shipping-symbol-check.sh).

Components, in order (ids are the names printed and used by -Only and -StubVerdicts):
  gate             the per-commit gate (story tdc-021): T0 lint, build StealthGameEditor, ^StealthGame. automation run
  probes           the compile probes (story tdc-005): tools/ci/compile-probes/run-probes.ps1 on a disposable git
                   worktree copy (never the developer tree), decided by gate-probes.sh
  t3-StealthLeak2  the T3 multi-process smoke node (story tdc-020), cooked Development listen host + client, with
  t3-StealthClean  -PktEmulationProfile=<profile> applied to both processes (profiles: story tdc-017); the profile name
                   is recorded in the log and printed as "T3 profile=<name>"
  shipping-symbol  the Shipping-symbol check (story tdc-006) on a packaged Shipping build against a packaged
                   Development build (the positive control)
Groups: gate, probes, t3 (two components), shipping-symbol: all four run every night, in this order, even when an
earlier one fails (one night shows every problem).

Component verdict from the component's exit code: 0 PASS, 1 FAIL, 2 NOT ASSESSED; any other code, a component that
could not start, or one that did not run (not selected, no stub verdict) is NOT ASSESSED. NOT ASSESSED is never a pass.

Overall verdict and exit code (the printed line "NIGHTLY: <verdict>" always carries the word):
  0  PASS           every component ran and passed
  1  FAIL           at least one component FAILED (also when another is NOT ASSESSED)
  2  NOT ASSESSED   no component failed but at least one did not run or could not decide

Logs: -LogDir (default Saved/Nightly/<timestamp>) gets one <component>.log per component, the staging logs and
summary.txt; the workflow uploads Saved/Nightly/ and Saved/Logs/ (also on failure).

Usage:
  powershell -ExecutionPolicy Bypass -File tools/ci/nightly.ps1                 the real nightly (self-hosted runner)
  powershell -ExecutionPolicy Bypass -File tools/ci/nightly.ps1 -Only t3-StealthLeak2,t3-StealthClean -ReuseStage
        run chosen components only (the others report NOT ASSESSED "not selected": the overall verdict cannot be PASS);
        -ReuseStage reuses an existing staged build (-DevStageDir / -ShippingStageDir name the folders; default
        <StagingRoot>/NightlyDev and NightlyShipping, StagingRoot default Saved/Staged)
  powershell -ExecutionPolicy Bypass -File tools/ci/nightly.ps1 -DryRun -StubVerdicts "all=pass,probes=fail"
        every component replaced by tools/ci/selftest/nightly/stub-component.cmd with the given verdict
        (pass | fail | na | crash); a component with no verdict is NOT ASSESSED
  powershell -ExecutionPolicy Bypass -File tools/ci/nightly.ps1 -SelfTest        dry-run scenarios; never runs the real
                   components, so the automation wrapper StealthGame.Build.NightlyDryRun cannot recurse into a build
Environment: UE_ROOT (default C:\Program Files\Epic Games\UE_5.8), STEALTH_BASH.
#>
param(
  [switch]$SelfTest,
  [switch]$DryRun,
  [string]$StubVerdicts = '',
  [string]$Only = '',
  [string]$LogDir = '',
  [string]$StagingRoot = '',
  [string]$DevStageDir = '',
  [string]$ShippingStageDir = '',
  [switch]$ReuseStage
)
$ErrorActionPreference = 'Continue'
$repo = Split-Path (Split-Path $PSScriptRoot)
$proj = Join-Path $repo 'StealthGame.uproject'
$ExitPass = 0; $ExitFail = 1; $ExitNotAssessed = 2
$Components = @('gate', 'probes', 't3-StealthLeak2', 't3-StealthClean', 'shipping-symbol')
$Groups = [ordered]@{ 'gate' = @('gate'); 'probes' = @('probes'); 't3' = @('t3-StealthLeak2', 't3-StealthClean'); 'shipping-symbol' = @('shipping-symbol') }
$T3Profiles = @{ 't3-StealthLeak2' = 'StealthLeak2'; 't3-StealthClean' = 'StealthClean' }

function ConvertTo-Slash([string]$p) { return ($p -replace '\\', '/') }

# Absolute path: a relative one is taken relative to the repository root (RunUAT runs elsewhere and cannot resolve it).
function Resolve-RepoPath([string]$p) {
  if ([System.IO.Path]::IsPathRooted($p)) { return $p }
  return [System.IO.Path]::GetFullPath((Join-Path $repo $p))
}

function Find-Bash {
  if ($env:STEALTH_BASH -and (Test-Path $env:STEALTH_BASH)) { return $env:STEALTH_BASH }
  foreach ($root in @($env:ProgramW6432, $env:ProgramFiles)) {
    if ($root) { $c = Join-Path $root 'Git\bin\bash.exe'; if (Test-Path $c) { return $c } }
  }
  return ''
}

function New-Result([string]$Id, [string]$Verdict, [string]$Reason, [bool]$Ran, [string]$Log) {
  return @{ Id = $Id; Verdict = $Verdict; Reason = $Reason; Ran = $Ran; Log = $Log }
}

# Verdict of a component from its exit code (the shared 0 / 1 / 2 convention).
function ConvertTo-ComponentResult([string]$Id, [int]$Code, [string]$Log, [string]$What) {
  switch ($Code) {
    0 { return New-Result $Id 'PASS' "$What exited 0" $true $Log }
    1 { return New-Result $Id 'FAIL' "$What exited 1 (it reported a failing observation; see the log)" $true $Log }
    2 { return New-Result $Id 'NOT ASSESSED' "$What exited 2 (it could not decide)" $true $Log }
    default { return New-Result $Id 'NOT ASSESSED' "$What exited $Code (not a verdict code: the component did not complete)" $true $Log }
  }
}

# Runs a native command, appends its output to the log and the console, returns its exit code (-1 = could not start).
function Invoke-Native([string]$LogFile, [string]$Exe, [string[]]$ArgList) {
  "> $Exe $($ArgList -join ' ')" | Out-File -FilePath $LogFile -Append -Encoding utf8
  try {
    & $Exe @ArgList 2>&1 | ForEach-Object {
      $line = "$_"
      Add-Content -Path $LogFile -Value $line -Encoding UTF8
      Write-Host "    $line"
    }
    return $LASTEXITCODE
  } catch {
    "could not start $Exe : $($_.Exception.Message)" | Out-File -FilePath $LogFile -Append -Encoding utf8
    Write-Host "    could not start $Exe : $($_.Exception.Message)"
    return -1
  }
}

# ---- dry run: a stub replaces every component --------------------------------------------------------------------
function Get-StubMap([string]$Text) {
  $map = @{}
  foreach ($pair in ($Text -split ',')) {
    $kv = $pair.Trim() -split '=', 2
    if ($kv.Count -eq 2 -and $kv[0]) { $map[$kv[0].Trim()] = $kv[1].Trim() }
  }
  return $map
}

function Invoke-StubComponent([string]$Id, $Stubs, [string]$Log) {
  $verdict = if ($Stubs.ContainsKey($Id)) { $Stubs[$Id] } elseif ($Stubs.ContainsKey('all')) { $Stubs['all'] } else { '' }
  if (-not $verdict) { return New-Result $Id 'NOT ASSESSED' 'no stub verdict given: the component did not run' $false $Log }
  $stub = Join-Path $PSScriptRoot 'selftest\nightly\stub-component.cmd'
  if ($T3Profiles.ContainsKey($Id)) {
    "PROFILE: $($T3Profiles[$Id])" | Out-File -FilePath $Log -Append -Encoding utf8
    Write-Host "T3 profile=$($T3Profiles[$Id])"
  }
  $code = Invoke-Native $Log $stub @($Id, $verdict)
  return ConvertTo-ComponentResult $Id $code $Log "stub component ($verdict)"
}

# ---- real components ---------------------------------------------------------------------------------------------
function Get-UeRoot { if ($env:UE_ROOT) { return $env:UE_ROOT } else { return 'C:\Program Files\Epic Games\UE_5.8' } }

function Invoke-GateComponent([string]$Log) {
  $script = Join-Path $PSScriptRoot 'gate-per-commit.ps1'
  $code = Invoke-Native $Log 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $script, '-LogFile', (Join-Path $script:LogDirResolved 'gate-automation-output.log'))
  return ConvertTo-ComponentResult 'gate' $code $Log 'the per-commit gate'
}

function Invoke-ProbesComponent([string]$Log) {
  $bash = Find-Bash
  if (-not $bash) { return New-Result 'probes' 'NOT ASSESSED' 'no bash found for gate-probes.sh (set STEALTH_BASH)' $false $Log }
  $obs = Join-Path $script:LogDirResolved 'probes.obs'
  $runner = Join-Path $repo 'tools\ci\compile-probes\run-probes.ps1'
  # The runner only emits observations (its exit code is not a verdict); gate-probes.sh decides.
  $null = Invoke-Native $Log 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $runner, '-Out', $obs, '-UERoot', (Get-UeRoot))
  $gate = ConvertTo-Slash (Join-Path $repo 'tools\ci\compile-probes\gate-probes.sh')
  $code = Invoke-Native $Log $bash @($gate, (ConvertTo-Slash $obs))
  return ConvertTo-ComponentResult 'probes' $code $Log 'gate-probes.sh'
}

# BuildCookRun of one configuration into a staging folder. Returns @{ Ok; Reason; Dir }.
function Invoke-Stage([string]$Config, [string]$Dir, [string]$Log) {
  if ($ReuseStage -and (Test-Path (Join-Path $Dir 'Windows'))) { return @{ Ok = $true; Reason = "reusing the existing stage $Dir"; Dir = $Dir } }
  $uat = Join-Path (Get-UeRoot) 'Engine\Build\BatchFiles\RunUAT.bat'
  if (-not (Test-Path $uat)) { return @{ Ok = $null; Reason = "RunUAT not found: $uat"; Dir = $Dir } }
  # -IgnoreCookErrors: the cook reports one error from the sample's broken CameraDirector_SandboxCharacter import
  # (present before this project's work; recorded in story tdc-020). -pak: -nopak stages no content in 5.8.4 here.
  $code = Invoke-Native $Log $uat @('BuildCookRun', "-project=$proj", '-noP4', '-platform=Win64', "-clientconfig=$Config", '-build', '-cook', '-stage', '-pak', '-IgnoreCookErrors', "-stagingdirectory=$Dir")
  if ($code -ne 0) { return @{ Ok = $false; Reason = "BuildCookRun $Config exited $code"; Dir = $Dir } }
  return @{ Ok = $true; Reason = "staged $Config into $Dir"; Dir = $Dir }
}

function Get-Stage([string]$Config) {
  if (-not $script:Stages.ContainsKey($Config)) {
    $override = if ($Config -eq 'Shipping') { $ShippingStageDir } else { $DevStageDir }
    $dir = if ($override) { Resolve-RepoPath $override } else { Join-Path $script:StagingRootResolved $(if ($Config -eq 'Shipping') { 'NightlyShipping' } else { 'NightlyDev' }) }
    $log = Join-Path $script:LogDirResolved "stage-$Config.log"
    $script:Stages[$Config] = Invoke-Stage $Config $dir $log
  }
  return $script:Stages[$Config]
}

function Invoke-T3Component([string]$Id, [string]$Log) {
  $profileName = $T3Profiles[$Id]
  "PROFILE: $profileName" | Out-File -FilePath $Log -Append -Encoding utf8
  Write-Host "T3 profile=$profileName"
  $stage = Get-Stage 'Development'
  if ($null -eq $stage.Ok) { return New-Result $Id 'NOT ASSESSED' $stage.Reason $false $Log }
  if (-not $stage.Ok) { return New-Result $Id 'FAIL' "the Development stage failed: $($stage.Reason)" $true $Log }
  $uat = Join-Path (Get-UeRoot) 'Engine\Build\BatchFiles\RunUAT.bat'
  $code = Invoke-Native $Log $uat @("-ScriptsForProject=$proj", 'RunUnreal', "-project=$proj", '-test=StealthMultiProcessSmoke', '-Namespaces=StealthGame.Harness.MP',
    "-build=$($stage.Dir)", '-platform=Win64', '-configuration=Development', "-StealthRoleArgs=-PktEmulationProfile=$profileName")
  $text = if (Test-Path $Log) { Get-Content -Raw -Path $Log } else { '' }
  # Text anchors: the node prints "<name> (Win64 Development Client) result=Passed|Failed". Without one the run did not assess anything.
  if ($text -match 'StealthGame\.Harness\.MP\.Smoke[^\r\n]*result=Failed' -or $text -match 'BUILD FAILED') { return New-Result $Id 'FAIL' "the T3 smoke node failed under profile $profileName (exit $code)" $true $Log }
  if ($code -eq 0 -and $text -match 'StealthGame\.Harness\.MP\.Smoke[^\r\n]*result=Passed') { return New-Result $Id 'PASS' "the T3 smoke node passed under profile $profileName" $true $Log }
  if ($code -ne 0) { return New-Result $Id 'FAIL' "RunUAT exited $code under profile $profileName" $true $Log }
  return New-Result $Id 'NOT ASSESSED' "no 'StealthGame.Harness.MP.Smoke ... result=' line under profile ${profileName}: the node assessed nothing" $true $Log
}

function Invoke-ShippingComponent([string]$Log) {
  $bash = Find-Bash
  if (-not $bash) { return New-Result 'shipping-symbol' 'NOT ASSESSED' 'no bash found for shipping-symbol-check.sh (set STEALTH_BASH)' $false $Log }
  $ship = Get-Stage 'Shipping'
  if ($null -eq $ship.Ok) { return New-Result 'shipping-symbol' 'NOT ASSESSED' $ship.Reason $false $Log }
  if (-not $ship.Ok) { return New-Result 'shipping-symbol' 'FAIL' "the Shipping stage failed: $($ship.Reason)" $true $Log }
  $dev = Get-Stage 'Development'
  if ($null -eq $dev.Ok) { return New-Result 'shipping-symbol' 'NOT ASSESSED' $dev.Reason $false $Log }
  if (-not $dev.Ok) { return New-Result 'shipping-symbol' 'FAIL' "the Development stage (the positive control) failed: $($dev.Reason)" $true $Log }
  $check = ConvertTo-Slash (Join-Path $repo 'tools\ci\shipping-symbol-check.sh')
  $code = Invoke-Native $Log $bash @($check, (ConvertTo-Slash $ship.Dir), (ConvertTo-Slash $dev.Dir))
  return ConvertTo-ComponentResult 'shipping-symbol' $code $Log 'shipping-symbol-check.sh'
}

function Invoke-RealComponent([string]$Id, [string]$Log) {
  switch ($Id) {
    'gate' { return Invoke-GateComponent $Log }
    'probes' { return Invoke-ProbesComponent $Log }
    'shipping-symbol' { return Invoke-ShippingComponent $Log }
    default { return Invoke-T3Component $Id $Log }
  }
}

# ---- the nightly -------------------------------------------------------------------------------------------------
function Invoke-Nightly {
  $timer = [System.Diagnostics.Stopwatch]::StartNew()
  $script:LogDirResolved = if ($LogDir) { Resolve-RepoPath $LogDir } else { Join-Path $repo ('Saved\Nightly\' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
  $script:StagingRootResolved = if ($StagingRoot) { Resolve-RepoPath $StagingRoot } else { Join-Path $repo 'Saved\Staged' }
  $script:Stages = @{}
  New-Item -ItemType Directory -Force -Path $script:LogDirResolved | Out-Null
  $stubs = Get-StubMap $StubVerdicts
  $selected = if ($Only) { @($Only -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ }) } else { $Components }
  $mode = if ($DryRun) { 'DRY RUN (stub components)' } else { 'real' }
  "NIGHTLY: starting ($mode); logs in $($script:LogDirResolved)"

  $results = @()
  foreach ($id in $Components) {
    $log = Join-Path $script:LogDirResolved "$id.log"
    "COMPONENT $id`: running"
    if ($selected -notcontains $id) {
      $r = New-Result $id 'NOT ASSESSED' 'not selected (-Only): the component did not run' $false $log
    } elseif ($DryRun) {
      $r = Invoke-StubComponent $id $stubs $log
    } else {
      $r = Invoke-RealComponent $id $log
    }
    $results += , $r
    "COMPONENT $id`: $($r.Verdict) - $($r.Reason) (log: $($r.Log))"
  }

  $ranGroups = @()
  foreach ($g in $Groups.Keys) {
    $members = @($results | Where-Object { $Groups[$g] -contains $_.Id })
    if (@($members | Where-Object { $_.Ran }).Count -eq $members.Count) { $ranGroups += $g }
  }
  "GROUPS RAN: $($ranGroups -join ' ') ($($ranGroups.Count) of $($Groups.Count))"

  $failed = @($results | Where-Object { $_.Verdict -eq 'FAIL' })
  $notAssessed = @($results | Where-Object { $_.Verdict -eq 'NOT ASSESSED' })
  if ($failed.Count -gt 0) {
    $verdict = 'FAIL'; $code = $ExitFail
    $reason = "failing: $(($failed | ForEach-Object { $_.Id }) -join ', ')" + $(if ($notAssessed.Count -gt 0) { "; not assessed: $(($notAssessed | ForEach-Object { $_.Id }) -join ', ')" } else { '' })
  } elseif ($notAssessed.Count -gt 0) {
    $verdict = 'NOT ASSESSED'; $code = $ExitNotAssessed
    $reason = "not assessed: $(($notAssessed | ForEach-Object { $_.Id }) -join ', ') (a component that did not run is never a pass)"
  } else {
    $verdict = 'PASS'; $code = $ExitPass
    $reason = 'every component ran and passed'
  }
  $summaryLines = @($results | ForEach-Object { "COMPONENT $($_.Id): $($_.Verdict) - $($_.Reason)" }) + @("GROUPS RAN: $($ranGroups -join ' ')", "NIGHTLY: $verdict - $reason")
  $summaryLines | Out-File -FilePath (Join-Path $script:LogDirResolved 'summary.txt') -Encoding utf8
  "LOGS: $($script:LogDirResolved)"
  "ELAPSED: total=$([int]$timer.Elapsed.TotalSeconds)s"
  "NIGHTLY: $verdict - $reason"
  exit $code
}

# ---- selftest (G2): dry-run scenarios; never runs a real component ----------------------------------------------------
function Invoke-SelfTest {
  $script = $PSCommandPath
  $script:selfBad = 0; $script:selfTotal = 0
  function Report([string]$name, [bool]$ok, [string]$detail) {
    $script:selfTotal++
    if ($ok) { "SELFTEST $name ok" } else { "SELFTEST $name FAILED: $detail"; $script:selfBad++ }
  }
  $tmpRoot = Join-Path $env:TEMP ('stealth_nightly_selftest_' + [guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Force -Path $tmpRoot | Out-Null
  $script:n = 0
  function Run-Dry([string]$stubs, [string]$extra = '') {
    $script:n++
    $dir = Join-Path $tmpRoot "run$($script:n)"
    $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $script, '-DryRun', '-LogDir', $dir)
    if ($stubs) { $argList += @('-StubVerdicts', $stubs) }
    if ($extra) { $argList += @('-Only', $extra) }
    $out = & powershell @argList 2>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    return @{ Out = ($out -join "`n"); Code = $code; Dir = $dir }
  }
  function Has($r, [string]$text) { return $r.Out.Contains($text) }
  $allIds = @('gate', 'probes', 't3-StealthLeak2', 't3-StealthClean', 'shipping-symbol')

  # Clean run: every component passes, all four groups ran, logs are collected, the job passes.
  $r = Run-Dry 'all=pass'
  $logsOk = $true; foreach ($id in $allIds) { if (-not (Test-Path (Join-Path $r.Dir "$id.log"))) { $logsOk = $false } }
  $logsOk = $logsOk -and (Test-Path (Join-Path $r.Dir 'summary.txt'))
  $allPass = $true; foreach ($id in $allIds) { if (-not (Has $r "COMPONENT ${id}: PASS")) { $allPass = $false } }
  Report 'clean-run-passes' (($r.Code -eq 0) -and (Has $r 'NIGHTLY: PASS') -and $allPass) "exit $($r.Code): $($r.Out)"
  Report 'all-four-groups-ran' ((Has $r 'GROUPS RAN: gate probes t3 shipping-symbol (4 of 4)')) $r.Out
  Report 'logs-collected-per-component-and-summary' $logsOk "missing a log under $($r.Dir)"
  $r.Out -split "`n" | ForEach-Object { "    $_" }

  # The profile applied to each T3 run is recorded in the log (and printed).
  $leakLog = Get-Content -Raw -Path (Join-Path $r.Dir 't3-StealthLeak2.log')
  $cleanLog = Get-Content -Raw -Path (Join-Path $r.Dir 't3-StealthClean.log')
  Report 't3-profile-recorded-in-log' ((Has $r 'T3 profile=StealthLeak2') -and (Has $r 'T3 profile=StealthClean') -and $leakLog.Contains('PROFILE: StealthLeak2') -and $cleanLog.Contains('PROFILE: StealthClean')) "leak log: $leakLog / clean log: $cleanLog"

  # A seeded failing observation from any ONE component fails the job, and the others still ran.
  foreach ($failing in $allIds) {
    $r = Run-Dry "all=pass,$failing=fail"
    $others = $true; foreach ($id in $allIds) { if ($id -ne $failing -and -not (Has $r "COMPONENT ${id}: PASS")) { $others = $false } }
    $ok = ($r.Code -eq 1) -and (Has $r "COMPONENT ${failing}: FAIL") -and (Has $r 'NIGHTLY: FAIL') -and $others -and (Has $r 'GROUPS RAN: gate probes t3 shipping-symbol (4 of 4)') -and (Test-Path (Join-Path $r.Dir "$failing.log")) -and (Test-Path (Join-Path $r.Dir 'summary.txt'))
    Report "seeded-failure-in-$failing-fails-the-job" $ok "exit $($r.Code): $($r.Out)"
    if ($failing -eq 'probes') { $r.Out -split "`n" | ForEach-Object { "    $_" } }
  }
  # The seeded failure's observation reaches the log.
  $r = Run-Dry 'all=pass,probes=fail'
  $obsLog = Get-Content -Raw -Path (Join-Path $r.Dir 'probes.log')
  Report 'seeded-failing-observation-is-in-the-component-log' ($obsLog.Contains('OBSERVATION probes seeded failing observation')) $obsLog

  # NOT ASSESSED is never a pass: a component that cannot decide, did not run, or crashed.
  $r = Run-Dry 'all=pass,gate=na'
  Report 'not-assessed-component-gives-not-assessed' (($r.Code -eq 2) -and (Has $r 'NIGHTLY: NOT ASSESSED') -and (Has $r 'COMPONENT gate: NOT ASSESSED')) "exit $($r.Code): $($r.Out)"
  $r = Run-Dry 'gate=pass,probes=pass,t3-StealthLeak2=pass,t3-StealthClean=pass'
  Report 'component-with-no-verdict-did-not-run-not-assessed' (($r.Code -eq 2) -and (Has $r 'COMPONENT shipping-symbol: NOT ASSESSED') -and (Has $r 'did not run') -and (Has $r 'GROUPS RAN: gate probes t3 (3 of 4)')) "exit $($r.Code): $($r.Out)"
  $r = Run-Dry 'all=pass,t3-StealthClean=crash'
  Report 'crashed-component-is-not-assessed-not-pass' (($r.Code -eq 2) -and (Has $r 'COMPONENT t3-StealthClean: NOT ASSESSED') -and (Has $r 'not a verdict code')) "exit $($r.Code): $($r.Out)"
  $r = Run-Dry 'all=pass,probes=fail,shipping-symbol=na'
  Report 'fail-wins-over-not-assessed' (($r.Code -eq 1) -and (Has $r 'NIGHTLY: FAIL') -and (Has $r 'not assessed: shipping-symbol')) "exit $($r.Code): $($r.Out)"
  $r = Run-Dry 'all=pass' 'gate'
  Report 'only-subset-can-never-pass' (($r.Code -eq 2) -and (Has $r 'COMPONENT probes: NOT ASSESSED') -and (Has $r 'not selected')) "exit $($r.Code): $($r.Out)"

  Remove-Item -Recurse -Force -Path $tmpRoot -ErrorAction SilentlyContinue
  if ($script:selfBad -eq 0) { "SELFTEST RESULT: PASS ($script:selfTotal cases)"; exit 0 }
  "SELFTEST RESULT: FAIL ($script:selfBad of $script:selfTotal cases)"
  exit 1
}

if ($SelfTest) { Invoke-SelfTest }
Invoke-Nightly
