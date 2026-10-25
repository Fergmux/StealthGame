# Story 026: Pre-commit check rejects changes to never-resave assets

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0009 section 4 (never-resave list) and Validation 4; traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0009: Version control and binary asset workflow
**ADR Decision Summary**: Assets the 5.8 editor must not resave are listed in the checked-in `production/never-resave-assets.md`, are never locked for editing, and a pre-commit check rejects any commit that modifies one.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API. Background fact from `production/never-resave-assets.md`: resaving the sample's GameplayCameras assets in 5.8 broke the CameraDirector import on 2026-10-24, and they were restored from a snapshot. The editor can bypass read-only state ("Make Writable") [V per ADR-0009], which is why a commit-time check is needed.

**Control Manifest Rules (this layer)**:
- Forbidden: never save an asset on the never-resave list (reject a commit modifying one); never use Make Writable — source: ADR-0009
- Required: until a locking-capable remote exists, only one session saves `Content/` assets at a time — source: ADR-0009
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008

---

## Acceptance Criteria

*From the story plan (bootstrap story, no GDD AC), scoped to this story:*

- [ ] The pre-commit check reads `production/never-resave-assets.md` and rejects a commit that modifies any listed asset path (ADR-0009 Validation 4), demonstrated by a staged change to a listed path being rejected and an unlisted change accepted.

---

## Implementation Notes

*Derived from ADR-0009 section 4 and Validation Criteria 4:*

- **File.** `tools/ci/hooks/check-never-resave.sh` (the plan's `TOOLS/hooks/`), invoked from the story 025 pre-commit hook through the contract defined there. `production/never-resave-assets.md` is read only; this story never edits it.
- **Parsing.** The list is a Markdown table whose first column is the asset path (at writing: five `Content/Blueprints/Cameras/*.uasset` rows under the header `Asset path | Reason`). Only table rows in the first column that look like asset paths are paths: the header row, the separator row and the prose lines around the table must not be parsed as paths (spec edge case).
- **Match.** Compare each staged path (git's changed-file list) with the listed paths, tolerating case and slash differences; a rename of a listed asset counts as modifying it (spec edge cases). A deletion rule is not stated in the inputs: open.
- **Result.** A rejection names the offending path. Exit nonzero on a rejection, zero otherwise, and emit the finding as an observation; the hook decides. If the list file is missing or yields no paths, report NOT ASSESSED rather than accept silently (an empty list must not look like a pass).
- **Related rules.** Never-resave assets are never locked for editing (ADR-0009 section 4); this check enforces the commit side only. Sample assets are never edited in place.
- **G2 (script story).** Ship (a) a selftest under `tools/ci/hooks/selftest/` using a temporary list and temporary repository (a staged listed path rejected, an unlisted path accepted) and (b) the wrapper `StealthGame.Build.NeverResaveCheck` in `Source/StealthFoundation/Private/Tests/Build/` that runs it through `FPlatformProcess` and asserts exit code and observations. Foundation (story 001) is not a declared dependency; bash discovery on Windows is open.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`: the hook and installer that call this check
- `production/epics/tuning-data-config/story-027-lfs-lock-commit-check.md`: the lock comparison for other `.uasset`/`.umap` changes
- `production/epics/tuning-data-config/story-028-commit-message-check.md`: commit message rules

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/hooks/check-never-resave.sh`
- `tools/ci/hooks/selftest/` (never-resave selftest)
- `Source/StealthFoundation/Private/Tests/Build/NeverResaveCheckTest.cpp` (G2 wrapper `StealthGame.Build.NeverResaveCheck`)
- `production/never-resave-assets.md` (read only)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a staged change to a listed asset is rejected and an unlisted change is accepted (ADR-0009 Validation 4)
  - Given: `production/never-resave-assets.md` (a table of asset paths in the first column)
  - When: a staged change to a listed path; then a staged change to an unlisted path
  - Then: the listed change is rejected naming the path; the unlisted change is accepted
  - Edge cases: path case and slash differences; a rename of a listed asset; the table header and prose lines are not parsed as paths

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/hooks/selftest/` and the wrapper `StealthGame.Build.NeverResaveCheck` (`Source/StealthFoundation/Private/Tests/Build/NeverResaveCheckTest.cpp`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. Open: the deletion rule, and the Foundation-module dependency of the G2 wrapper.
