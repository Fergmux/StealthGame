# Story 024: VCS conformance check for ignore and attribute rules

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0009 sections 2-3; traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0009: Version control and binary asset workflow
**ADR Decision Summary**: Git plus Git LFS, trunk-based, with `.uasset`/`.umap` stored via LFS and lockable. The ignore list must hide regenerable and local-safety folders and must not hide Unreal's `Build/` folder (the template's Godot and Unity sections did), and `.gitattributes` lists one pattern per line.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: ADR-0009 uses no post-cutoff engine API. Relevant verified facts: UAT/Gauntlet scripts live under `<Project>/Build/` [V per ADR-0008], so a `.gitignore` rule that hides `Build/` would silently drop them; the template's `[Bb]uild/` rule does that regardless of `core.ignorecase`. The ADR-0009 Verification Required items (provider plugin, LFS of the 3.3 GB sample tree, host quota) are [U] and are not touched by this check.

**Control Manifest Rules (this layer)**:
- Required: `.uasset` and `.umap` are stored via LFS and are lockable — source: ADR-0009
- Required: before the first commit the template's Godot/Unity ignore sections are removed and `build/`/`bin/` anchored or removed (`git check-ignore -v Build/Scripts/x.cs` prints nothing); `.gitattributes` lists one pattern per line — source: ADR-0009
- Forbidden: never commit `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, `_snapshots/`, `_backup_pre_rebase/` — source: ADR-0009
- Forbidden: agents never change `.gitignore`/`.gitattributes` or commit without explicit user instruction — source: ADR-0009

---

## Acceptance Criteria

*From the story plan and amendments (bootstrap story, no GDD AC), scoped to this story:*

- [ ] A script verifies that `git check-ignore -v Build/Scripts/x.cs` prints nothing, `_snapshots/` and `_backup_pre_rebase/` and `Plugins/*/Binaries/` `Plugins/*/Intermediate/` are ignored, `Binaries/ Intermediate/ DerivedDataCache/ Saved/` are ignored, `.gitattributes` lists one pattern per line with `lockable` on `*.uasset` and `*.umap`, and no Godot/Unity ignore section remains (recognised as any `.gitignore` section whose header comment matches `Godot` or `Unity`, case-insensitive, or any of the patterns `.godot/`, `*.import`, `Library/`, `Temp/`, `UnityLockfile`); it exits nonzero with a finding per violation.

---

## Implementation Notes

*Derived from ADR-0009 sections 2-3, Key Interfaces and Implementation Guidelines:*

- **Script.** `tools/ci/lint/vcs-conformance.sh` (the plan's `TOOLS/lint/`). It emits one `file:line rule message` observation per violation through the shared emitter of story 011 when that exists, and exits nonzero when any violation is found; a zero-file or no-repo situation is NOT ASSESSED (nonzero), never a silent pass.
- **Checks, one observation per violation.** (1) `git check-ignore -v Build/Scripts/x.cs` prints nothing; (2) ignored: `_snapshots/`, `_backup_pre_rebase/`, `Plugins/*/Binaries/`, `Plugins/*/Intermediate/`, `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`; (3) `.gitattributes`: each non-comment line holds exactly one pattern followed by attributes (the failure mode ADR-0009 names: a line such as `*.fbx *.blend ... filter=lfs` makes `*.blend` an attribute name and sends those files to no LFS), and `*.uasset` and `*.umap` lines carry `lockable`; (4) no Godot/Unity section by the recognition rule in the AC.
- **Current tree (observation, not a verdict).** At the time of writing `.gitignore` shows an anchored `/bin/`, a comment that no `build/` rule exists, `Plugins/*/Binaries/`, `Plugins/*/Intermediate/` and `_snapshots/`, and `.gitattributes` has an ADR-0009 LFS block with `lockable` on `*.uasset` and `*.umap`, so the setup edits appear already applied and the script is expected to pass. The script is the authority, not this note; I did not verify `_backup_pre_rebase/` or the Godot/Unity patterns.
- **Edits to `.gitignore`/`.gitattributes` are conditional.** Only if the check finds a gap on the real tree, and agents change those two files only on explicit user instruction (ADR-0009 Implementation Guidelines): stop and ask.
- **G2 (script story).** Ship (a) a selftest under `tools/ci/lint/selftest/` that runs the script on a compliant temporary repository (exit 0) and on a seeded copy with one violation per check (nonzero, one finding each), and (b) the automation wrapper `StealthGame.Build.VcsConformance` in `Source/StealthFoundation/Private/Tests/Build/` that runs the script through `FPlatformProcess` and asserts exit code and observations. The wrapper needs Foundation (story 001), which the plan does not list as a dependency; locating bash on Windows from the wrapper is open.
- **Prototype.** None. A repository with `origin` already exists, so the check runs against the real repository.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`: the tracked pre-commit hook and installer
- `production/epics/tuning-data-config/story-026-never-resave-commit-check.md`: rejecting commits that touch never-resave assets
- `production/epics/tuning-data-config/story-027-lfs-lock-commit-check.md`: the LFS lock comparison
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: relies on `Build/` not being ignored but does not check it

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/lint/vcs-conformance.sh`
- `tools/ci/lint/selftest/` (vcs-conformance selftest)
- `Source/StealthFoundation/Private/Tests/Build/VcsConformanceTest.cpp` (G2 wrapper `StealthGame.Build.VcsConformance`)
- `.gitignore`, `.gitattributes` (only if the check finds a gap, and only on explicit user instruction)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the conformance script passes on the compliant tree and fails with one finding per violation on a seeded copy (selftest plus wrapper per G2)
  - Setup: the repository `.gitignore` and `.gitattributes`
  - Verify: `git check-ignore -v Build/Scripts/x.cs` prints nothing; `_snapshots/`, `_backup_pre_rebase/`, `Plugins/*/Binaries/`, `Plugins/*/Intermediate/`, `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/` are ignored; `.gitattributes` has one pattern per line with `lockable` on `*.uasset` and `*.umap`; no `.gitignore` section with a header matching Godot or Unity (case-insensitive) and none of `.godot/`, `*.import`, `Library/`, `Temp/`, `UnityLockfile`
  - Pass condition: exit 0 on the compliant tree; a seeded copy exits nonzero with one finding per violation

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass recorded in `production/qa/smoke-*.md`, plus (G2) the selftest under `tools/ci/lint/selftest/` and the wrapper `StealthGame.Build.VcsConformance` (`Source/StealthFoundation/Private/Tests/Build/VcsConformanceTest.cpp`) which must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. Open: the G2 wrapper sits in the Foundation module (story 001), which is not a declared dependency; bash discovery from the wrapper on Windows; unverified here are `_backup_pre_rebase/` and the Godot/Unity patterns on the current tree.
