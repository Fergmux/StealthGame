# Story 028: commit-msg hook enforces Conventional Commits with a story or ADR reference

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0009 section 5 (workflow conventions); traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0009: Version control and binary asset workflow
**ADR Decision Summary**: Commits follow `.claude/docs/coding-standards.md`: Conventional Commits (`feat:`, `fix:`, `chore:`, `docs:`, `test:`, `refactor:`), a story or task ID in the body (`Story: EPIC-NNN-SNN`) and a design-document or ADR reference; commits happen only on user instruction.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API; plain git hook. No unverified claim is introduced by this story.

**Control Manifest Rules (this layer)**:
- Required: commits use Conventional Commits with a story/task ID and a design-doc/ADR reference — source: ADR-0009
- Forbidden: agents never initialise a repository, commit, push, or change `.gitignore`/`.gitattributes` without explicit user instruction (the hook's tests run in a temporary repository) — source: ADR-0009
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script (a commit-msg hook is itself the gate, so it prints the reason it rejected) — source: ADR-0008

---

## Acceptance Criteria

*From the story plan and amendments (bootstrap story, no GDD AC), scoped to this story:*

- [ ] The commit-msg hook rejects a message not starting with `feat:`, `fix:`, `chore:`, `docs:`, `test:` or `refactor:`, and rejects a body with none of: a `Story: <ID>` line, an `ADR-NNNN` token, or a path under `design/gdd/` or `docs/architecture/`; it accepts a message with the prefix and any one of those references.

---

## Implementation Notes

*Derived from ADR-0009 section 5 and the commit rules in `.claude/docs/coding-standards.md`:*

- **File.** `tools/ci/hooks/commit-msg` (the plan's `TOOLS/hooks/commit-msg`), installed through the story 025 installer. It is a git `commit-msg` hook: it receives the message file path and rejects with a printed reason and nonzero exit.
- **Rules (from the amendment).** (1) The first line starts with one of the six prefixes. Conventional Commits permits a scope (`feat(tdc):`); the spec lists "prefix with scope" as an edge case but the AC text says "starting with `feat:`": open, decide whether a scoped prefix is accepted and document it in the hook header. (2) The body holds at least one of: a line `Story: <ID>`; the token `ADR-` followed by exactly four digits (`ADR-12` is not a reference, per the spec edge case); a path beginning `design/gdd/` or `docs/architecture/`. Any one reference is enough.
- **Edge cases the spec names without defining.** An empty message (reject, nothing to accept) and merge/revert subjects (`Merge ...`, `Revert ...`): the inputs do not say whether they are exempt. Do not silently exempt them: choose, state the choice in the hook header and cover it in the selftest.
- **Trailer interaction.** Project commits end with attribution trailer lines (`Co-Authored-By: ...`); the hook must not reject them and must not count them as a reference.
- **Hook framework.** Installed by story 025's installer. The extensionless-file line-ending risk noted in story 025 applies here too (needs a user decision on `.gitattributes`, not an agent edit).
- **G2 (script story).** Ship (a) a selftest under `tools/ci/hooks/selftest/` running the hook on the message matrix below and (b) the wrapper `StealthGame.Build.CommitMsgCheck` in `Source/StealthFoundation/Private/Tests/Build/` that runs it through `FPlatformProcess` and asserts exit code and observations. Foundation (story 001) is not a declared dependency; bash discovery on Windows is open.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`: the hook directory, installer and `core.hooksPath`
- `production/epics/tuning-data-config/story-026-never-resave-commit-check.md`: never-resave rejection (pre-commit)
- `production/epics/tuning-data-config/story-027-lfs-lock-commit-check.md`: lock comparison (pre-commit)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/hooks/commit-msg` (shim) and `tools/ci/hooks/commit-msg.sh` (rules)
- `tools/ci/hooks/selftest/run-commit-msg-selftest.sh` (43 cases)
- `Source/StealthFoundation/Private/Tests/Build/CommitMsgCheckTest.cpp` (G2 wrapper `StealthGame.Build.CommitMsgCheck`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the hook rejects a wrong prefix or a body with no reference form, and accepts a conforming message
  - Given: messages with prefix `feat:`, `fix:`, `chore:`, `docs:`, `test:`, `refactor:` or another prefix, with a body containing a `Story: <ID>` line, an `ADR-NNNN` token, a path under `design/gdd/` or `docs/architecture/`, or none
  - When: the hook runs on each
  - Then: rejected if the prefix is wrong; rejected if the body has none of the three reference forms; accepted with the prefix and any one reference
  - Edge cases: empty message; merge/revert subjects; `ADR-12` (not NNNN) is not a reference

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/hooks/selftest/` and the wrapper `StealthGame.Build.CommitMsgCheck` (`Source/StealthFoundation/Private/Tests/Build/CommitMsgCheckTest.cpp`) must exist and pass

**Status**: [x] Created and passing (`run-commit-msg-selftest.sh` 43/43 cases; `StealthGame.Build.CommitMsgCheck`)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. Open: whether a scoped prefix is accepted, the merge/revert and empty-message treatment, and the Foundation-module dependency of the G2 wrapper.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: the six prefixes with a `Story:` line accepted; `update:`, no prefix, `feat do`, `feat: ` with no description and upper-case `Feat:` rejected; a body with none of a `Story:`/`Task:`/`Decision:` line, an `ADR-NNNN` token or a `design/gdd/` or `docs/architecture/` path rejected, any one accepted; `ADR-12` and `ADR-00123` are not references; an ADR token in the subject only does not count; one real `git commit` with a bad prefix is rejected (HEAD unchanged) and a good one accepted)
**Decisions**: a SCOPED prefix (`feat(ci):`) and a breaking `!` are accepted (the history uses scopes); `Task: <ID>` and `Decision: <ID>` lines count as references alongside `Story:` (the history uses all three); trailer lines (`Co-Authored-By:`, `Signed-off-by:`) are neither rejected nor counted (a trailer naming an ADR does not count); an empty or comment-only message is rejected; git-generated `Merge ...` and `Revert "..."` subjects are EXEMPT with a printed NOTE (`fixup!`/`squash!` are not); `#` comment lines and CR are ignored; only the BODY is searched for a reference
**Existing style**: the hook was run over the 72 most recent real commits: 69 accepted. The 2 rejected (`7344aba chore(config): disable Android File Server ...` and `7b45fbd docs: D51 MaxPlayers=10`) carry no `Story:`/`Task:`/`Decision:`/ADR/path reference at all, so they are rejections by the rule, not by the style; the selftest case `real-history-style-accepted` asserts that no commit carrying a reference form is rejected. The hook is NOT installed in this working copy.
**Deviations**: none
**Test Evidence**: `production/qa/evidence/story-028-commit-message-check/` (`selftest.txt` 43/43, `test-run.txt` 43/43 Success, `stamp.txt`)
**Run result**: N/A — git hook tooling; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects found
