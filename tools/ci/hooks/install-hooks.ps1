<#
.SYNOPSIS
  Opt-in installer for the tracked git hooks (ADR-0009 section 5; story tdc-025): PowerShell twin of
  install-hooks.sh. Sets this clone's core.hooksPath to tools/ci/hooks. Idempotent; never edits a tracked file.
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools/ci/hooks/install-hooks.ps1
  powershell -ExecutionPolicy Bypass -File tools/ci/hooks/install-hooks.ps1 -Status
  powershell -ExecutionPolicy Bypass -File tools/ci/hooks/install-hooks.ps1 -Uninstall
#>
param([switch]$Status, [switch]$Uninstall, [switch]$Force)
$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
$top = (& git -C $here rev-parse --show-toplevel 2>$null)
if ($LASTEXITCODE -ne 0 -or -not $top) { Write-Error "install-hooks: $here is not inside a git work tree"; exit 1 }
$want = ((& git -C $here rev-parse --show-prefix) -replace '/$', '')
$current = (& git -C $top config --local --get core.hooksPath 2>$null)
if ($Status) {
  if ($current -eq $want) { "STATUS: installed (core.hooksPath=$current)"; exit 0 }
  "STATUS: not installed (core.hooksPath=$(if ($current) { $current } else { '<unset>' }))"; exit 1
}
if ($Uninstall) {
  if ($current -eq $want) { & git -C $top config --local --unset core.hooksPath; 'STATUS: uninstalled'; exit 0 }
  "STATUS: nothing to uninstall (core.hooksPath=$(if ($current) { $current } else { '<unset>' }))"; exit 0
}
foreach ($hook in 'pre-commit', 'commit-msg', 'pre-commit.sh', 'commit-msg.sh') {
  if (-not (Test-Path (Join-Path $here $hook))) { Write-Error "install-hooks: $hook is missing from $here"; exit 1 }
}
if ($current -and ($current -ne $want) -and -not $Force) { Write-Error "install-hooks: core.hooksPath is already set to '$current'; pass -Force to replace it"; exit 1 }
$result = 'already installed'
if ($current -ne $want) {
  & git -C $top config --local core.hooksPath $want
  if ($LASTEXITCODE -ne 0) { Write-Error 'install-hooks: git config failed'; exit 1 }
  $result = 'installed'
}
# Git runs only the hooks in core.hooksPath once it is set, so the Git LFS hooks (pre-push uploads LFS objects)
# must be there too: `git lfs install --local` writes them (generated per clone, untracked, never committed).
$attr = Join-Path $top '.gitattributes'
if ((Test-Path $attr) -and (Select-String -Path $attr -Pattern 'filter=lfs' -Quiet)) {
  & git -C $top lfs install --local *> $null
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path (Join-Path $here 'pre-push'))) {
    Write-Error "install-hooks: this repository uses Git LFS but the LFS hooks were not written to $want; a push would NOT upload LFS objects. Install git-lfs and run this installer again."
    exit 1
  }
}
"STATUS: $result (core.hooksPath=$want)"
exit 0
