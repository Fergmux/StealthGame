# Story 027: Pre-commit check compares changed assets with git lfs locks

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

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
- `tools/ci/hooks/selftest/run-lfs-locks-selftest.sh` (19 cases, injected lock source)
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

**Status**: [x] Created and passing (`run-lfs-locks-selftest.sh` 19/19 cases; `StealthGame.Build.LfsLockCheck`; one real-remote observation recorded below)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`
- Unlocks: None
- Open verification claims: from the plan Note, real lock behaviour on the GitHub remote is [U] until observed once, and this story records that observation. Also [U]: ADR-0009 Verification Required (1) (provider plugin) and (3) (host quota). Open: the `git lfs locks` invocation and owner matching, and the Foundation-module dependency of the G2 wrapper.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1 through an injectable lock source `STEALTH_LFS_LOCKS_CMD` (default `git lfs locks --verify`): (a) a lock held by the committer (`O <path>` line) accepted; (b) no lock, or another owner's lock (two-space line), rejected naming the path with the hint `git lfs lock <path> first`; (c) a remote that reports no lock support (`does not support`, `not supported`, `locksverify`, or `missing protocol` for a repo with no remote) accepted with a printed `NOTE single-writer` reminder. Also: any other source failure (authentication, network) and an unparseable line are NOT ASSESSED (exit 2); a change with no `.uasset`/`.umap` never queries the source; several assets with one unlocked name only that one; `.umap` and an upper-case extension are checked; lock paths compare ignoring case; a deletion and the old path of a rename need the lock; an added asset needs no lock unless someone else holds that path; the same check inside the installed hook rejects an unlocked and accepts a locked asset)
**Real observation (the story's [U], 2026-10-25, `production/qa/evidence/story-027-lfs-lock-commit-check/real-remote-observation.txt`)**: the GitHub remote `https://github.com/Fergmux/StealthGame.git` SUPPORTS Git LFS locking. `git lfs lock Content/Audio/Ambient/Starter_Birds01.uasset` -> `Locked ...`; `git lfs locks --verify` printed `O <path><TAB>Fergmux<TAB>ID:59189522` (the `O ` prefix = held by the person running git; others get two spaces; `--json` gives `{"ours":[...],"theirs":[...]}`); the check with the DEFAULT source rejected the asset before the lock and accepted it after; `git lfs unlock` -> `Unlocked ...`; the list was empty again afterwards. `lockable` is in `.gitattributes` (story 024). Not observed against a second account (another owner's lock) or the host quota (ADR-0009 Verification Required 3): the other-owner path is covered by the injected source only. Because the remote supports locking the degrade path is NOT the live behaviour here, but it is kept for a clone without a remote.
**Decisions**: ownership comes from git-lfs's own `O ` marker in `--verify` output, so no matching of lock owner names to git identities is needed (the story left that open); only `.uasset`/`.umap` are checked (`STEALTH_LOCK_EXTENSIONS` overrides) though `.gitattributes` marks more types lockable; never-resave assets are not special-cased (story 026); no remote at all counts as "no lock support" (git-lfs says `missing protocol: ""`, observed in a temporary repo), which would otherwise block every commit there
**Deviations**: none
**Test Evidence**: `production/qa/evidence/story-027-lfs-lock-commit-check/` (`selftest.txt` 19/19, `real-remote-observation.txt`, `test-run.txt` 43/43 Success, `stamp.txt`)
**Run result**: N/A — git hook tooling; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): the missing-protocol hazard above was found by a selftest commit in a repo with no remote and fixed; no further defects found
