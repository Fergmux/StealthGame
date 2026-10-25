# Story 026: Late binds and transitions after a round boundary are rejected

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story proves that work queued before a round boundary cannot leak across it. After `EndRoundAll`, a pending informant respawn cannot bind a pawn (`Rejected_WrongState`) and a queued `Assassinated` transition cannot exile a participant who is already `RoundEnded` (`Rejected_IllegalTransition`); after `ResetAll`, a stale respawn carrying its old epoch is rejected as `Rejected_StaleEpoch`. It realises GDD rules R8 and R9 (`BindPawn` epoch guard and precedence), R7 and the Edge Cases "Round boundaries" (a queued event resolves after `EndRoundAll`; a timer started in round N fires after `ResetAll`; an informant killed and `EndRoundAll` resolves first).

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-004` (also `TR-roster-002`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: The Roster is one of four game-instance stores and the only authoritative owner of participant records, bindings, tombstones, `MatchId`, `RoundEpoch` and `EventSeq`; its logic lives in the plain-C++ `FRosterCore`. ADR-0010 makes the binding the only link between a participant and a pawn and requires that `BindPawn` precedes visibility, with the crowd-adoption respawn bound through `TakeOverCrowdPawn` carrying an `ExpectedEpoch` so a stale respawn is refused by the Roster itself.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking domain (HIGH per VERSION.md). No new post-cutoff APIs; new logging uses `UE_LOGF` (ADR-0007). Verification required by ADR-0005 is behavioural: spikes S3, S4 and S5 (not yet reported); ADR-0010's S8, S14, H1 and traversal spike do not concern this core. This story runs entirely inside the engine-free `FRosterCore` and is not gated by any of them.

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: Test names: `StealthGame.<System>.<Scenario>` (T1); T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` — source: ADR-0005
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Guardrail: Tests: per-commit pipeline <= 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-36** [Core] `StealthGame.Roster.RoundBoundaryLateEvents`:
  - GIVEN a killed informant with a pending respawn, WHEN `EndRoundAll` resolves first, THEN `BindPawn` returns `Rejected_WrongState`.
  - WHEN instead `ResetAll` happens and then the stale respawn fires with its old epoch, THEN `Rejected_StaleEpoch`.
  - GIVEN `EndRoundAll` has run, WHEN a queued `RequestTransition(victim, Informant, Assassinated)` arrives, THEN `Rejected_IllegalTransition` (Edge Cases).

---

## Implementation Notes

*Derived from ADR-0010 Decision sections 3 and 4 and Implementation Guidelines (bind carries `ExpectedEpoch`), and GDD rules R7, R8, R9 and the Edge Cases "Round boundaries":*

- No new API: this story composes `BindPawn(Id, Pawn, ExpectedEpoch)` (story 010), `RetireBinding` (story 020), `EndRoundAll` (story 023), `ResetAll` (story 022) and the epoch check (story 024), and fixes whatever precedence mistake the test reveals in `FRosterCore` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`).
- Scenario (a): informant killed (`RetireBinding`, `AwaitingPawn(T)` +1, respawn epoch captured at that time). `EndRoundAll` runs. `BindPawn(Id, UnboundPawn, CapturedEpoch)` returns `Rejected_WrongState` because the participant is `RoundEnded`; `EndRoundAll` does not change `RoundEpoch`, so the captured epoch still matches and step "epoch" does not mask the result. The binding stays retired.
- Scenario (b): from the same starting state, `ResetAll` runs instead (epoch +1; the participant becomes `Active` with no pawn and the tombstone is cleared). The stale respawn's `BindPawn` with the old epoch returns `Rejected_StaleEpoch`: BindPawn precedence checks the epoch before `WrongState` (R9), so an `Active` participant is still refused. Without the guard the stale respawn would bind.
- Scenario (c): after `EndRoundAll`, `RequestTransition(victim, Informant, Assassinated)` with the matching current epoch returns `Rejected_IllegalTransition` (precedence step 8; the victim is `RoundEnded`). The epoch must match or step 4 would mask the result.
- In each case snapshot the store (states, binding table, `RoundEpoch`, `EventSeq`) and assert nothing changed and no event fired.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source, fault sink, pawn-liveness provider that reports the Unbound pawn alive). Use separate fixtures for (a), (b) and (c).
- Prototype disposition: no prototype counterpart.
- Cross-epic note: the pending respawn belongs to NPC-Kill Penalty & Informants (`production/epics/npc-kill-penalty-informants/EPIC.md`); this core test calls `BindPawn` directly and needs no stub.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event): the reset batch itself.
- Story 023 (EndRoundAll ends all records once and is idempotent): the end-round batch itself.
- Story 024 (Calls carrying an old round epoch or match id are rejected): the general epoch-token behaviour.
- Story 017 (No revival and no round-ended-to-informant transitions are possible): the general `RoundEnded -> Informant` rejection for each entering reason.
- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant): `BindPawn` guards outside round-boundary context.
- Story 043 (Player pawns are bound before BeginPlay and a failed bind destroys the pawn before FinishSpawning): the spawn-site behaviour on rejection.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (only if a precedence correction is needed)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if a precedence correction is needed)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterRoundBoundaryLateEventsTest.cpp` (class `FRosterRoundBoundaryLateEventsTest`, test name `StealthGame.Roster.RoundBoundaryLateEvents`)
- ADR-0007 header naming: no new header is added; any new one uses the `Stealth` prefix and a project-unique file name.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: late binds and transitions after a round boundary are rejected.
  - Given: a killed informant (retired, respawn epoch captured).
  - When: (a) EndRoundAll runs, then BindPawn with matching epoch. (b) Separately ResetAll runs, then the stale respawn's BindPawn with the old epoch. (c) After EndRoundAll, RequestTransition(victim, Informant, Assassinated).
  - Then: (a) Rejected_WrongState, (b) Rejected_StaleEpoch, (c) Rejected_IllegalTransition. Nothing changes.
  - Edge cases: in (b) the participant is Active without a pawn and epoch is checked first; in (c) the epoch must match or step 4 masks the result.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterRoundBoundaryLateEventsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-022-reset-all-batch.md`, `production/epics/participant-state-roster/story-023-end-round-all-batch-idempotent.md`, `production/epics/participant-state-roster/story-024-stale-epoch-rejected.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: None
