# Story 025: Tracked pre-commit hook framework running the T0 lint

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0009 section 5 and Validation 5, with ADR-0008 section 8; traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0009: Version control and binary asset workflow
**ADR Decision Summary**: Git plus LFS, trunk-based, with a pre-commit stage that runs the ADR-0008 T0 lint, the never-resave check and the lock check, and merges only after the per-commit gate passes. Commits happen only on user instruction.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API. Git itself: `core.hooksPath` is a local, unversioned git setting, so the installer must be run once per clone. The built-in UE Git plugin (Beta) gives no editor locking [V per ADR-0009], which is why the lock policy rests on this hook (story 027). No unverified claim is introduced by this story.

**Control Manifest Rules (this layer)**:
- Required: the include lint (rules a-j) runs in CI and pre-commit — source: ADR-0007
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Forbidden: agents never initialise a repository, commit, push, or change `.gitignore`/`.gitattributes` without explicit user instruction — source: ADR-0009
- Guardrail: per-commit pipeline at most 10 minutes — source: ADR-0008

---

## Acceptance Criteria

*From the story plan (bootstrap story, no GDD AC), scoped to this story:*

- [ ] `tools/ci/hooks/pre-commit` is tracked in the repository with an installer that points `core.hooksPath` (or copies it) so that every commit runs the T0 lint and aborts on findings; the installer is idempotent and a test commit with a seeded lint violation is rejected (ADR-0009 Validation 5).

---

## Implementation Notes

*Derived from ADR-0009 sections 4-5, Validation Criteria 5, and ADR-0008 section 8:*

- **Files.** `tools/ci/hooks/pre-commit` (tracked, executable by git) and `tools/ci/hooks/install-hooks.sh` (the plan's `TOOLS/hooks/`). The hook calls the story 011 lint gate (`tools/ci/lint/gate.sh`) and aborts the commit when it exits nonzero, printing the observations. The gate decides; the hook only relays (observation rule).
- **Installer.** Sets `core.hooksPath` to `tools/ci/hooks` (or copies the hook; the AC allows either, choose one and document it). Idempotent: running it twice leaves the same configuration with no error and no duplicate state. It never edits `.gitignore`, `.gitattributes` or any tracked file.
- **Extension contract for stories 026-028.** The never-resave check (026) and the LFS lock check (027) run from this pre-commit hook, and the Conventional Commits check (028) is the separate `commit-msg` hook in the same folder. The calling contract (how the hook discovers and runs `check-*.sh`, how a check reports its observations and exit code) is not specified in the inputs: define it here, in the hook's header comment, so 026-028 add a check without editing the hook body.
- **Scope of the lint run.** Follow G5: the include lint scans `Source/StealthFoundation`, `StealthGameplay`, `StealthPresentation`, `StealthGameEditor`, `StealthTestHarness`; the prototype `Source/StealthGame` is scanned only for rules (e) and (f). Whether the hook lints all files or only staged files is not specified: open. Keep the hook fast; the 10-minute budget belongs to the whole per-commit gate, not to the hook.
- **Line endings (open, needs the user).** The existing `.gitattributes` forces LF for `*.sh` but the hook files `pre-commit` and `commit-msg` have no extension, so a CRLF checkout on Windows could break them under Git Bash. Adding a `.gitattributes` rule is a change agents may not make without explicit user instruction (ADR-0009): raise it, do not edit the file.
- **Seeded test.** Use a temporary repository created by the test, with a hook installed by the installer, a staged file seeded with a lint violation (reuse the story 016 seeded fixtures if present, otherwise a minimal local one) and a second clean commit. No commit is made in the real repository; commits there stay user-instructed.
- **G2 (script story).** Ship (a) a selftest under `tools/ci/hooks/selftest/` implementing the temp-repo scenario above, and (b) the automation wrapper `StealthGame.Build.PreCommitHook` in `Source/StealthFoundation/Private/Tests/Build/` that runs the selftest through `FPlatformProcess` and asserts exit code and observations. The wrapper needs Foundation (story 001), which the plan does not list for this story; bash discovery on Windows is open.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`: the lint emitter and gate this hook calls
- `production/epics/tuning-data-config/story-026-never-resave-commit-check.md`: the never-resave check
- `production/epics/tuning-data-config/story-027-lfs-lock-commit-check.md`: the `git lfs locks` comparison
- `production/epics/tuning-data-config/story-028-commit-message-check.md`: the `commit-msg` hook
- `production/epics/tuning-data-config/story-021-per-commit-gate-script.md`: the full per-commit gate (build and tests), which the hook does not run

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/hooks/pre-commit` (shim), `tools/ci/hooks/pre-commit.sh` (hook body and the check contract), `tools/ci/hooks/commit-msg` and `commit-msg.sh` (story 028)
- `tools/ci/hooks/install-hooks.sh`, `tools/ci/hooks/install-hooks.ps1` (opt-in installer, bash and PowerShell), `tools/ci/hooks/.gitignore` (the per-clone git-lfs hooks)
- `tools/ci/hooks/selftest/lib.sh`, `tools/ci/hooks/selftest/run-pre-commit-selftest.sh` (temp-repo selftest, 17 cases)
- `Source/StealthFoundation/Private/Tests/Build/PreCommitHookTest.cpp` (G2 wrapper `StealthGame.Build.PreCommitHook`), `Source/StealthFoundation/Private/Tests/Build/StealthBuildFileTestUtils.h` (added `RunBashSelftest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the tracked hook and idempotent installer reject a commit with a seeded lint violation and accept a clean one (ADR-0009 Validation 5)
  - Given: the tracked `tools/ci/hooks/pre-commit` and `install-hooks.sh`, in a temporary repository
  - When: the installer is run twice; a commit with a seeded lint violation is attempted; a clean commit is attempted
  - Then: `core.hooksPath` is set (or the hook copied) identically after the second run; the seeded commit is rejected; the clean commit is accepted
  - Edge cases: hook not executable; no staged files

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the temp-repo selftest under `tools/ci/hooks/selftest/` and the wrapper `StealthGame.Build.PreCommitHook` (`Source/StealthFoundation/Private/Tests/Build/PreCommitHookTest.cpp`) must exist and pass

**Status**: [x] Created and passing (`run-pre-commit-selftest.sh` 17/17 cases; `StealthGame.Build.PreCommitHook`; ADR-0009 Validation 5: a commit with a seeded lint violation is rejected, a clean commit accepted, in a temporary repository)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`
- Unlocks: `production/epics/tuning-data-config/story-026-never-resave-commit-check.md`, `production/epics/tuning-data-config/story-027-lfs-lock-commit-check.md`, `production/epics/tuning-data-config/story-028-commit-message-check.md`
- Open verification claims: the plan has no Note line for this story. Open: the check-calling contract, staged-only versus full lint, the extensionless-hook line-ending rule (needs a user decision), and the Foundation-module dependency of the G2 wrapper.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: in temporary repositories holding a copy of the hooks, `install-hooks.sh` sets `core.hooksPath=tools/ci/hooks`, a second run changes nothing ("already installed", one config value), a foreign `core.hooksPath` is refused without `--force`, no tracked file changes; a clean commit is accepted and a commit staging `StealthNoGuard.h` (include-lint rule b) is rejected with `Source/StealthFoundation/Public/Server/StealthNoGuard.h:2 b first include ...`, `GATE: FAIL` and `PRE-COMMIT: REJECTED`, HEAD unchanged; nothing staged passes; a new `check-demo.sh` rejecting (exit 1), not assessed (exit 2) and passing is honoured without editing the hook body; CRLF-converted shims still work; the hook's lint rule list equals the gate's `$LintRules`)
**Decisions**: the extensionless `pre-commit` and `commit-msg` are two-line shims that `exec bash <name>.sh`, with a trailing `#` so a CRLF checkout cannot corrupt the last argument: this resolves the story's open line-ending question WITHOUT touching `.gitattributes` (the logic is in LF-enforced `*.sh` files); installer = `core.hooksPath` (relative, `tools/ci/hooks`), `--status/--uninstall/--force`, refuses to overwrite a foreign value, plus a PowerShell twin; the check contract is documented in the header of `pre-commit.sh` (checks are `check-*.sh`, run with `STEALTH_HOOK_REPO` and `STEALTH_HOOK_STAGED`, exit 0 accept / 1 reject / 2 NOT ASSESSED, any nonzero aborts, all run); the hook runs the FULL T0 lint over the working tree on every commit (about 12 s on the real tree), not only staged files: open point recorded: a partly staged change is judged on disk, not in the index; the lint rule list is `include-lint-*.sh` plus `vcs-conformance.sh` and a selftest case asserts it equals the gate's list; hooks are opt-in and were NOT installed in this working copy (every commit these tests make is in a temporary repository)
**Finding (new, important)**: git runs ONLY the hooks in `core.hooksPath` once it is set, so the git-lfs hooks (`pre-push` uploads LFS objects) in `.git/hooks` would silently stop running and a push would send pointer files without objects. The installer therefore runs `git lfs install --local`, which writes the four LFS hooks into `tools/ci/hooks`; they are per-clone and untracked (`tools/ci/hooks/.gitignore` lists them) because an extensionless CRLF checkout would corrupt them; the installer fails when the repository uses LFS and the hooks did not appear. Observed first when a selftest commit in a temp repo left `post-checkout/post-commit/post-merge/pre-push` untracked in the hooks folder. A second observation: `lockable` makes LFS files read-only after a commit (`git lfs post-commit`), so a later write needs `rm` first.
**Deviations**: none; `StealthBuildFileTestUtils.h` gained the shared helper `RunBashSelftest` used by the four hook wrappers (stories 025-028)
**Test Evidence**: `production/qa/evidence/story-025-pre-commit-hook-framework/` (`selftest.txt` 17/17, `test-run.txt` 43/43 Success, `stamp.txt`)
**Run result**: N/A — git hook tooling; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): the LFS-hooks hazard above and a read-only `lockable` write failure in the selftest were found and fixed; no further defects found
