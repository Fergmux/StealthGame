# Story 022: ResetAll resets every non-dormant record with exactly one roster-reset event

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story delivers `ResetAll(RoundIndex)`, the per-round batch call Round makes in its `StartRound` (after `SetTeamsLocked(true)`, before Session's `RequestRoundTravel`). It makes every non-Dormant record `Active` and `InRound`, keeps Dormant records out of the round as `RoundEnded`, wipes all bindings, tombstones, last-reason and appearance-assignment state, advances `RoundEpoch`, records `RoundIndex` for `IsRoundScopeReset`, and emits exactly one `OnRosterReset` instead of per-participant events. It realises GDD rule R7 (`ResetAll`), R3 (match versus round fields), I1 and I2, and the "Reset" row of the legal-transition table.

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**ADR Decision Summary**: The Roster is one of four game-instance stores and the only authoritative owner of participant records, bindings, tombstones, `MatchId`, `RoundEpoch` and `EventSeq`; its logic lives in the plain-C++ `FRosterCore`. Session's `ResetRoundScope` maps to the Roster's `ResetAll`, and its readiness query to `IsRoundScopeReset`. ADR-0002 adds that the Roster implements `IEventFlushBarrier::FlushPending()` so events queued during the outermost call are dispatched FIFO after each queue slot, and a batch call raises batch events rather than per-participant ones.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking domain (HIGH per VERSION.md). No new post-cutoff APIs; new logging uses `UE_LOGF` (ADR-0007). Verification required by ADR-0005 is behavioural: spikes S3, S4 and S5 (not yet reported); ADR-0002's verification items concern the queue drain, not this core. This story runs entirely inside the engine-free `FRosterCore` and is not gated by any of them.

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: `WrongTargetTally`/`LastResultSeq` are written to C-TeamView only on reclaim, rejoin and `OnRosterReset` — source: ADR-0001, ADR-0018, ADR-0019
- Required: Test names: `StealthGame.<System>.<Scenario>` (T1); T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-11** [Core] `StealthGame.Roster.ResetAllBatch`: GIVEN teams locked and mixed states, bindings, tombstones and one Dormant record, WHEN `ResetAll(RoundIndex = 2)` runs, THEN:
  - every non-Dormant record is `Active` with `InRound = true`, and the Dormant record is `RoundEnded` with `InRound = false`;
  - `IsRoundScopeReset(2)` = true;
  - bindings, tombstones, `LastTransitionReason` and `AssignedAppearanceId` are cleared;
  - `Team` and `Connection` are kept;
  - `RoundEpoch` is +1;
  - exactly one `OnRosterReset` fires and no per-participant event fires (R7).

---

## Implementation Notes

*Derived from ADR-0005 section 2 (Session API mapping, `ResetRoundScope`) and ADR-0002 section 5 (Roster event flush), and GDD rules R7, R5, R3, R12, I1, I2:*

- Add `ResetAll(RoundIndex)` and `IsRoundScopeReset(RoundIndex)` to `FRosterCore` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`) and the `URosterStore` facade. Sequence inside one call: precondition teams locked (the unlocked result `Rejected_TeamsUnlocked` is asserted in story 025; implement the check here because it is the same function), then for every non-Dormant record set `State = Active`, `InRound = true`; every Dormant record becomes or stays `RoundEnded` with `InRound = false`.
- Clear all bindings and tombstones, every record's `LastTransitionReason` and `AssignedAppearanceId`. Keep `Team` and `Connection` (including `DisconnectedAt` and `ConnectionGeneration`) untouched. Increment `RoundEpoch` and record `RoundIndex` so `IsRoundScopeReset(RoundIndex)` returns true afterwards.
- Emit exactly one `OnRosterReset{RoundEpoch, EventSeq}` through the story-008 FIFO event queue; it consumes one `EventSeq` (I6) and no per-participant `OnParticipantStateChanged` fires. Batch events are never evaluated as an elimination (F4).
- Authority and thread checks apply as for every mutator (`Rejected_NotAuthority`, `Rejected_WrongThread`).
- R7 also says a repeat call with the same `RoundIndex` after an aborted start is applied in full again (new `RoundEpoch`, new event) and `IsRoundScopeReset` stays true. The AC does not assert this; implement it, and do not rely on it being tested here.
- Owner-view cache fields (tally, `LastResultSeq`, respawn due) are zeroed by `ResetAll` per the owner-view feed rule; their tests belong to story 038, and they must not be stored on the record as a replicated field (R3 whitelist, story 001).
- Open point: the Dormant record needs a core test seam because `NotifyConnectionChanged(... Dormant)` belongs to story 009, which this story does not depend on. Likewise the writer API for `AssignedAppearanceId` is unnamed in the GDD (Round writes it from the allocator output); the test sets it through a seam compiled only under `WITH_STEALTH_TEST_SEAMS`. The qa-lead flagged both as missing seams; do not invent a production writer beyond the seam.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source, fault sink). Set up a mix of Active, Informant and RoundEnded records with bindings (story 010) and tombstones (story 011), teams locked, then call `ResetAll(2)`; assert `EventSeq` rose by exactly 1.
- Prototype disposition: no prototype counterpart; the prototype's round-restart bookkeeping in `Source/StealthGame/Private/StealthGameMode.cpp` is replaced by this batch call and not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 023 (EndRoundAll ends all records once and is idempotent): the round-end batch.
- Story 025 (Undersized rosters count correctly and lock and reset preconditions hold): the `Rejected_TeamsUnlocked` result and the lock precondition tests.
- Story 024 (Calls carrying an old round epoch or match id are rejected): epoch staleness after a reset.
- Story 038 (SetOwnerTally and SetOwnerRespawnDue feed only the owner view with the full precedence): zeroing of owner-view fields on reset.
- Story 008 (Registration, team, removal and reclaim each fire exactly one sequenced event): the FIFO event queue this story emits through.
- Story 009 (Session connection states project onto Connection fields only): the real Lobby `Dormant` projection.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h`
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Public/Server/StealthRosterService.h` (facade methods `ResetAll` and `IsRoundScopeReset`; includes `StealthServerOnlyGuard.h` first)
- `Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp`
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterResetAllBatchTest.cpp` (class `FRosterResetAllBatchTest`, test name `StealthGame.Roster.ResetAllBatch`)
- ADR-0007 header naming: `Stealth` prefix and project-unique file names; no new header beyond the shared roster headers.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: ResetAll resets every non-Dormant record with exactly one reset event.
  - Given: teams locked; mixed states, bindings, tombstones, AssignedAppearanceId; one Dormant record (via core seam, writer API unnamed).
  - When: ResetAll(2).
  - Then: non-Dormant records Active and InRound = true; the Dormant one RoundEnded and InRound = false. IsRoundScopeReset(2) true. Bindings, tombstones, LastTransitionReason, AssignedAppearanceId cleared. Team and Connection kept. RoundEpoch +1. Exactly one OnRosterReset and no per-participant event.
  - Edge cases: EventSeq +1 only. Repeating with the same RoundIndex is not in the AC.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterResetAllBatchTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`, `production/epics/participant-state-roster/story-011-tombstone-generation-and-release.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: `production/epics/participant-state-roster/story-024-stale-epoch-rejected.md`, `production/epics/participant-state-roster/story-025-undersized-rosters-and-lock.md`, `production/epics/participant-state-roster/story-026-round-boundary-late-events.md`, `production/epics/participant-state-roster/story-035-elimination-trigger-core.md`, `production/epics/participant-state-roster/story-038-owner-view-feed.md`
