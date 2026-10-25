# Story 027: Pre-commit check compares changed assets with git lfs locks

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0009 section 4 (lock policy) and Validation 2; traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0009: Version control and binary asset workflow
**ADR Decision Summary**: Once a locking-capable remote exists, an unmergeable `.uasset`/`.umap` is locked before it is edited, and a pre-commit check compares every changed asset with `git lfs locks` and rejects changes to assets the committer does not hold; until then the single-writer rule applies.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API. Verified: `lockable` makes unlocked files read-only on checkout but is not a hard stop (the editor's save dialog offers Make Writable), and the built-in UE Git plugin (Beta) does no locking [V per ADR-0009], so this hook is the enforcement. Unverified [U]: real `git lfs lock` behaviour on the GitHub remote (`origin` is `https://github.com/Fergmux/StealthGame.git`) and ADR-0009 Verification Required (1) and (3); the plan requires this story to record one real observation.

**Control Manifest Rules (this layer)**:
- Required: once locking exists, lock an unmergeable asset before editing and unlock after commit/abandon; the pre-commit lock check passes — source: ADR-0009
- Required: until a locking-capable remote exists, only one session saves `Content/` assets at a time — source: ADR-0009
- Forbidden: never use Make Writable — source: ADR-0009

---

## Acceptance Criteria

*From the story plan (bootstrap story, no GDD AC), scoped to this story:*

- [ ] The pre-commit check rejects a commit changing a `.uasset`/`.umap` that the committer does not hold according to `git lfs locks` (parsed through an injectable lock-list source so the test needs no remote), accepts an asset the committer holds, and degrades to a printed single-writer reminder when the remote reports no lock support.

---

## Implementation Notes

*Derived from ADR-0009 sections 3-4, Key Interfaces (agent lock protocol) and Validation Criteria 2:*

- **Files.** `tools/ci/hooks/check-lfs-locks.sh` (the plan's `TOOLS/hooks/`) invoked from the story 025 hook, and selftests under `tools/ci/hooks/selftest/`.
- **Injectable source.** The check reads the lock list through a replaceable source: production runs `git lfs locks` and parses it into records (path, owner); tests supply canned output. The exact `git lfs locks` flags and the way the committer's identity is established (lock owner name versus git user) are not in the inputs: open, decide against the real output and record it. The test must need no remote.
- **Three source states.** (a) a lock held by the committer: accept; (b) no lock or another owner: reject, naming the path; (c) the remote reports locking unsupported: accept with a printed single-writer reminder (the interim policy of ADR-0009 section 4: one session edits `Content/` at a time). A non-asset change is ignored; several assets with one unlocked is rejected naming the unlocked path.
- **Never-resave assets.** Those are covered by story 026; this check does not special-case them.
- **Real observation (the [U]).** After the check works against canned data, run it once against the real remote and record in this story whether the host supports locking and what the observed lock/unlock and rejection behaviour was. If the host does not support locking, record that and keep the degrade path as the live behaviour.
- **Agent protocol (for the reader).** `git lfs lock <explicit paths>`, edit and save explicit paths, commit on user instruction, `git lfs unlock`. Two sessions never lock the same asset.
- **G2 (script story).** Ship (a) the selftest with the three lock-source states above and (b) the wrapper `StealthGame.Build.LfsLockCheck` in `Source/StealthFoundation/Private/Tests/Build/` that runs it through `FPlatformProcess` and asserts exit code and observations. Foundation (story 001) is not a declared dependency; bash discovery on Windows is open.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`: the hook framework and installer
- `production/epics/tuning-data-config/story-026-never-resave-commit-check.md`: the never-resave list check
- `production/epics/tuning-data-config/story-028-commit-message-check.md`: commit message rules
- `production/epics/tuning-data-config/story-024-vcs-conformance-check.md`: verifying that `.gitattributes` marks `*.uasset` and `*.umap` as `lockable`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/hooks/check-lfs-locks.sh`
- `tools/ci/hooks/selftest/` (lock-source selftests)
- `Source/StealthFoundation/Private/Tests/Build/LfsLockCheckTest.cpp` (G2 wrapper `StealthGame.Build.LfsLockCheck`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a changed asset not held by the committer is rejected, a held one accepted, and an unsupported-lock remote degrades to a reminder
  - Given: a lock source returning (a) a lock held by the committer, (b) none or another owner, (c) "locks unsupported"
  - When: a `.uasset`/`.umap` change is committed under each
  - Then: (a) accepted; (b) rejected; (c) accepted with a printed single-writer reminder
  - Edge cases: a non-asset change is ignored; several assets with one unlocked; real GitHub lock behaviour is [U] and is recorded once in the story

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/hooks/selftest/` and the wrapper `StealthGame.Build.LfsLockCheck` (`Source/StealthFoundation/Private/Tests/Build/LfsLockCheckTest.cpp`) must exist and pass, plus the one recorded real-remote observation in this story

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`
- Unlocks: None
- Open verification claims: from the plan Note, real lock behaviour on the GitHub remote is [U] until observed once, and this story records that observation. Also [U]: ADR-0009 Verification Required (1) (provider plugin) and (3) (host quota). Open: the `git lfs locks` invocation and owner matching, and the Foundation-module dependency of the G2 wrapper.
