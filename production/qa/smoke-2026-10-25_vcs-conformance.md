# Smoke record: VCS conformance check for the ignore and attribute rules (story tdc-024)

> **Story:** `production/epics/tuning-data-config/story-024-vcs-conformance-check.md` · **Date:** 2026-10-25 · **Tool:** `tools/ci/lint/vcs-conformance.sh` (observations) decided by `tools/ci/lint/gate.sh`
> **Result:** **PASS** — the real repository's `.gitignore` and `.gitattributes` conform on every check; the selftest proves each check fails on a seeded violation (25 cases) and that a folder that is not a git checkout reads NOT ASSESSED.

## Checks on the real repository

| Check | Method | Observed | Result |
|---|---|---|---|
| `Build/` is not ignored | `git check-ignore -v Build/Scripts/x.cs` (global excludes disabled, `--no-index`) | prints nothing | PASS |
| Regenerable and local-safety folders are ignored | `git check-ignore -q` for `_snapshots/`, `_backup_pre_rebase/`, `Plugins/*/Binaries/`, `Plugins/*/Intermediate/`, `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/` | all eight ignored | PASS |
| `.gitattributes` one pattern per line; `lockable` on `*.uasset` and `*.umap` | every non-comment line is a pattern followed by attribute tokens only; the two lines carry `lockable` | no stray pattern token; both lines lockable | PASS |
| No Godot or Unity section | a decorated heading (`# === ... ===`) naming Godot or Unity (case-insensitive), or any of `.godot/`, `*.import`, `Library/`, `Temp/`, `UnityLockfile` | none (the ordinary comment that mentions Unity under the `Test Output` heading is not a section header) | PASS |

`bash tools/ci/lint/vcs-conformance.sh --check` printed `GATE: PASS (2 file(s) scanned, 0 findings)`. No edit to `.gitignore` or `.gitattributes` was needed or made.

## Seeded controls (watched to fail)

`tools/ci/lint/selftest/run-vcs-conformance-selftest.sh` builds throwaway repositories from a compliant pair and applies one violation per case: `Build/` ignored; each of the eight folders un-ignored in turn; two patterns on one `.gitattributes` line (and a path-like token); `*.uasset` or `*.umap` without `lockable`; the `*.umap` line missing; a Godot header, a lower-case Unity header and each of the five patterns. Each case reports exactly one finding at its file and line; the compliant pair passes; a plain folder is `GATE: NOT ASSESSED`.

Wrapper: `StealthGame.Build.VcsConformance` (`Source/StealthFoundation/Private/Tests/Build/VcsConformanceTest.cpp`), passing in the 37/37 run recorded in `production/qa/evidence/story-024-vcs-conformance-check/`.
