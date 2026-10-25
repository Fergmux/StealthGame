# Story 023: EndRoundAll ends all records once and is idempotent

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story delivers `EndRoundAll()`, the batch call Round makes in its round-end step: every `Active` or `Informant` record becomes `RoundEnded` in one step with one `OnRosterRoundEnded` event, and a second call does nothing and emits nothing. It exists so a round ends for every participant at the same instant and a duplicate end can never re-fire listeners or consume an `EventSeq`. It realises GDD rule R7 (`EndRoundAll`), the "RoundEnded to RoundEnded" table row, the Edge Case "`EndRoundAll` called twice", and F4's rule that batch events are never evaluated as an elimination.

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: The Roster is one of four game-instance stores and the only authoritative owner of participant records, bindings, tombstones, `MatchId`, `RoundEpoch` and `EventSeq`; its logic lives in the plain-C++ `FRosterCore` behind a thin `URosterStore` facade. Round calls `EndRoundAll` from its round-end step (ADR-0002 section 8.4(b)), as a batch that is never evaluated as an elimination, and ADR-0002's flush barrier dispatches its event after the slot.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking domain (HIGH per VERSION.md). No new post-cutoff APIs; new logging uses `UE_LOGF` (ADR-0007). Verification required by ADR-0005 is behavioural: spikes S3, S4 and S5 (not yet reported). This story runs entirely inside the engine-free `FRosterCore` and is not gated by any of them.

**Control Manifest Rules (this layer)**:
- Required: The latch step replicates and presents nothing; the round-end step runs in one server step, bound to the deciding slot's latest death write by `CauseKey` (or inline when the slot staged none) — source: ADR-0002
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Test names: `StealthGame.<System>.<Scenario>` (T1); T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never end the round in one step at the latch, or through an `OnRoundEnded` subscriber chain — reveals the end before contact; subscriber order is unspecified — source: ADR-0002
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-12** [Core] `StealthGame.Roster.EndRoundAllBatchIdempotent`: GIVEN Active and Informant records, WHEN `EndRoundAll` runs twice, THEN all are `RoundEnded` after the first call with exactly one `OnRosterRoundEnded`, and the second call emits nothing and does not advance `EventSeq` (R7).

---

## Implementation Notes

*Derived from ADR-0002 sections 5 and 8.4 and GDD rules R7, R8, R12, F4:*

- Add `EndRoundAll()` to `FRosterCore` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`) and the `URosterStore` facade. Every `Active` or `Informant` record becomes `RoundEnded` in one step; one `OnRosterRoundEnded{RoundEpoch, EventSeq}` is queued through the story-008 FIFO queue and consumes one `EventSeq`. No per-participant `OnParticipantStateChanged` is emitted.
- Idempotence: if no record is `Active` or `Informant` the call is a no-op for every record, emits no event and leaves `EventSeq` unchanged. `Dormant` records stay `RoundEnded`. A call on a fresh roster (everyone `RoundEnded`) is therefore also a no-op.
- Bindings and tombstones are not touched by `EndRoundAll` (only `ResetAll` clears them): a retired informant binding stays retired, which is what makes a late `BindPawn` return `Rejected_WrongState` (story 026).
- Batch events are never evaluated as an elimination (F4); the elimination trigger consumes only single-participant transition payloads (story 035). `IsTeamEliminated` returns false after the call because the phase is no longer `Active`.
- The forced net update of every written carrier (ADR-0002 section 8.4(b)) belongs to the push step (stories 038 and 041); the core only mutates state and queues the event.
- Authority and thread checks apply as for every mutator (`Rejected_NotAuthority`, `Rejected_WrongThread`).
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source, fault sink). Records via stories 002, 005 and 014 (one Active, one Informant). Record `EventSeq` before the first call, attach an event spy, call twice, and assert counts.
- Prototype disposition: no prototype counterpart. The prototype ends rounds through game-mode win evaluation, not a roster batch, and nothing is ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event): the reset batch.
- Story 019 (RoundEnd and Reset reasons are rejected on single transitions): the single-transition rejection of `RoundEnd`.
- Story 026 (Late binds and transitions after a round boundary are rejected): behaviour of later `BindPawn` and `RequestTransition` calls after this batch.
- Story 035 (The elimination trigger fires only for a strike-out win and never for disconnect or empty sides): the trigger and the `IsTeamEliminated` check after `EndRoundAll`.
- Story 034 (A scripted 200-step sequence keeps F1-F3 and I1-I3 equal to a brute-force recount): conservation through end-round steps.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h`
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Public/Server/StealthRosterService.h` (facade method `EndRoundAll`; includes `StealthServerOnlyGuard.h` first)
- `Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp`
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterEndRoundAllBatchIdempotentTest.cpp` (class `FRosterEndRoundAllBatchIdempotentTest`, test name `StealthGame.Roster.EndRoundAllBatchIdempotent`)
- ADR-0007 header naming: `Stealth` prefix and project-unique file names; no new header beyond the shared roster headers.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: EndRoundAll ends all records once and is idempotent.
  - Given: Active and Informant records, EventSeq recorded.
  - When: EndRoundAll runs twice.
  - Then: after the first all RoundEnded, exactly one OnRosterRoundEnded, EventSeq +1. Second call emits nothing, EventSeq unchanged.
  - Edge cases: no per-participant events; Dormant records stay RoundEnded.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterEndRoundAllBatchIdempotentTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: `production/epics/participant-state-roster/story-026-round-boundary-late-events.md`, `production/epics/participant-state-roster/story-034-counts-and-conservation.md`, `production/epics/participant-state-roster/story-035-elimination-trigger-core.md`
