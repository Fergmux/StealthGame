<#
.SYNOPSIS
  Compile-probe runner (ADR-0008 section 7, story tdc-005). Emits observations only; the verdict
  is decided by gate-probes.sh from the observation file this script writes.

.DESCRIPTION
  1. Creates a disposable git worktree of HEAD (never the developer tree), with Git LFS smudge
     skipped (Content/ is not needed for a code-only UBT compile, so LFS pointers are enough).
  2. Overlays the developer tree's Source/ and StealthGame.uproject onto it, so uncommitted code
     under test is what gets compiled.
  3. Builds StealthGameEditor Win64 Development there once.
  4. For each tools/ci/compile-probes/*.cpp: copies it to Source/<TARGET-MODULE>/Private/CompileProbes/
     in the copy, compiles it alone with UnrealBuildTool -SingleFile= (through Build.bat, which selects
     the engine's bundled .NET; UnrealBuildTool.exe called directly fails on a machine whose installed
     .NET lacks the runtime UBT targets — observed 2026-10-25), records the outcome, deletes it;
     then does the same for its positive control (CONTROL-FILE or the probe itself, compiled in
     CONTROL-MODULE, default StealthGame).
  5. Removes the worktree (unless -KeepWorktree).

  Observation lines (stdout and -Out file):
    OBSERVATION probes-found=<n>
    OBSERVATION baseline-build=<succeeded|failed>
    OBSERVATION probe=<file> role=probe module=<m> outcome=<failed-expected|failed-other|compiled|not-compiled>
    OBSERVATION probe=<file> role=control module=<m> outcome=<compiled|failed|not-compiled>
  (not-compiled: UBT exited 0 without running a compile action for the file.)

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools/ci/compile-probes/run-probes.ps1 -Out Saved/probes.obs
  bash tools/ci/compile-probes/gate-probes.sh Saved/probes.obs
#>
param(
  [string]$Out = "",
  [string]$WorktreeDir = "",
  [string]$UERoot = "C:\Program Files\Epic Games\UE_5.8",
  [switch]$KeepWorktree
)

# Continue, not Stop: in Windows PowerShell 5.1 a native tool writing to stderr (git, UBT) would
# otherwise throw. Exit codes are checked explicitly instead.
$ErrorActionPreference = "Continue"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$probeDir = $PSScriptRoot
if (-not $Out) { $Out = Join-Path $root "Saved\CompileProbes\observations.obs" }
$logDir = Join-Path (Split-Path $Out) "logs"
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
New-Item -ItemType Directory -Force $logDir | Out-Null
Set-Content -Path $Out -Value $null -Encoding ascii

function Emit([string]$line) {
  $l = "OBSERVATION $line"
  $l
  Add-Content -Path $Out -Value $l -Encoding ascii
}

function Read-Header([string]$text, [string]$key) {
  $m = [regex]::Match($text, "(?m)^//\s*$key\s*:\s*(.+?)\s*$")
  if (-not $m.Success) { return "" }
  return $m.Groups[1].Value.Trim().Trim('"')
}

$probes = @(Get-ChildItem -Path $probeDir -Filter *.cpp -File)
Emit "probes-found=$($probes.Count)"
if ($probes.Count -eq 0) { exit 0 }

$buildBat = Join-Path $UERoot "Engine\Build\BatchFiles\Build.bat"
if (-not $WorktreeDir) { $WorktreeDir = Join-Path $env:TEMP ("stealth-probes-" + [DateTime]::Now.ToString("yyyyMMdd-HHmmss")) }

$env:GIT_LFS_SKIP_SMUDGE = "1"
& git -C $root worktree add --detach $WorktreeDir HEAD 2>&1 | Out-File (Join-Path $logDir "worktree.log") -Encoding utf8
if ($LASTEXITCODE -ne 0) { Emit "baseline-build=failed"; "worktree creation failed, see $logDir\worktree.log"; exit 0 }

try {
  # Overlay the developer tree's code (committed or not) onto the worktree.
  & robocopy (Join-Path $root "Source") (Join-Path $WorktreeDir "Source") /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
  Copy-Item (Join-Path $root "StealthGame.uproject") (Join-Path $WorktreeDir "StealthGame.uproject") -Force
  $proj = Join-Path $WorktreeDir "StealthGame.uproject"

  & $buildBat StealthGameEditor Win64 Development "-Project=$proj" -WaitMutex 2>&1 | Out-File (Join-Path $logDir "baseline-build.log") -Encoding utf8
  if ($LASTEXITCODE -ne 0) { Emit "baseline-build=failed"; exit 0 }
  Emit "baseline-build=succeeded"

  function Compile-One([string]$sourceFile, [string]$module, [string]$tag) {
    $destDir = Join-Path $WorktreeDir "Source\$module\Private\CompileProbes"
    if (-not (Test-Path (Join-Path $WorktreeDir "Source\$module"))) { return @{ Exit = -1; Text = "module $module not found in the copy" } }
    New-Item -ItemType Directory -Force $destDir | Out-Null
    $dest = Join-Path $destDir (Split-Path $sourceFile -Leaf)
    Copy-Item $sourceFile $dest -Force
    # UBT compiles a -SingleFile only when it is in the makefile's source list. Its "source directory
    # added" check did not always notice the new CompileProbes/ folder (observed 2026-10-25: "Target is
    # up to date", 0 actions, exit 0), so touch the module's Build.cs: a changed rules file always
    # invalidates the makefile and the rescan picks the probe up.
    (Get-Item (Join-Path $WorktreeDir "Source\$module\$module.Build.cs")).LastWriteTime = Get-Date
    $text = (& $buildBat StealthGameEditor Win64 Development "-Project=$proj" "-SingleFile=$dest" -WaitMutex 2>&1 | Out-String)
    $code = $LASTEXITCODE
    Set-Content -Path (Join-Path $logDir "$tag.log") -Value $text -Encoding utf8
    Copy-Item (Join-Path $env:LOCALAPPDATA "UnrealBuildTool\Log.txt") (Join-Path $logDir "$tag.ubt.txt") -ErrorAction SilentlyContinue
    Remove-Item $dest -Force
    if (-not (Get-ChildItem $destDir -Force)) { Remove-Item $destDir -Force }
    # Did UBT actually run a compile action for this file? A success with no action is not a compile.
    $ran = $text -match ("Compile \[[^\]]+\] " + [regex]::Escape((Split-Path $sourceFile -Leaf)))
    return @{ Exit = $code; Text = $text; Ran = $ran }
  }

  foreach ($probe in $probes) {
    $body = Get-Content $probe.FullName -Raw
    $expect = Read-Header $body "EXPECT-ERROR"
    $target = Read-Header $body "TARGET-MODULE"
    $controlModule = Read-Header $body "CONTROL-MODULE"; if (-not $controlModule) { $controlModule = "StealthGame" }
    $controlFile = Read-Header $body "CONTROL-FILE"
    $controlPath = if ($controlFile) { Join-Path $probeDir $controlFile } else { $probe.FullName }

    if (-not $expect -or -not $target) {
      Emit "probe=$($probe.Name) role=probe module=$target outcome=failed-other"
      continue
    }
    $r = Compile-One $probe.FullName $target "$($probe.BaseName).probe"
    $outcome = if ($r.Exit -eq 0 -and $r.Ran) { "compiled" } elseif ($r.Exit -eq 0) { "not-compiled" } elseif ($r.Text.Contains($expect)) { "failed-expected" } else { "failed-other" }
    Emit "probe=$($probe.Name) role=probe module=$target outcome=$outcome"

    if (-not (Test-Path $controlPath)) { Emit "probe=$($probe.Name) role=control module=$controlModule outcome=failed"; continue }
    $c = Compile-One $controlPath $controlModule "$($probe.BaseName).control"
    $coutcome = if ($c.Exit -eq 0 -and $c.Ran) { "compiled" } elseif ($c.Exit -eq 0) { "not-compiled" } else { "failed" }
    Emit "probe=$($probe.Name) role=control module=$controlModule outcome=$coutcome"
  }
}
finally {
  Remove-Item Env:GIT_LFS_SKIP_SMUDGE -ErrorAction SilentlyContinue
  if (-not $KeepWorktree) {
    & git -C $root worktree remove --force $WorktreeDir 2>&1 | Out-Null
    if (Test-Path $WorktreeDir) { Remove-Item -Recurse -Force $WorktreeDir -ErrorAction SilentlyContinue }
    & git -C $root worktree prune 2>&1 | Out-Null
  }
}
exit 0
