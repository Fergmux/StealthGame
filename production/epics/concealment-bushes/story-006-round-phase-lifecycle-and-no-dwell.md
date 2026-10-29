# Story 006: Ended and Reset leave concealment consistent and no dwell timer exists

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (States and Transitions, CON-R7 no dwell timer, AC-CON-17, AC-CON-12 simulation part, Edge Cases lifecycle)
**Requirement**: `TR-con-001`, `TR-con-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§3 phases, post-latch exemption), with ADR-0002 (latch, epoch, reset)
**ADR Decision Summary**: In `Ended`, Hide commands are rejected `NotNow`, in-flight transitions complete through the post-latch exemption, and concealed pawns stay culled; `Reset` destroys every pawn, drops every pending item by epoch and clears every record on `OnRosterReset`. A stale commit, crossing or end resolves `Voided`. There is no dwell, capacity, reveal or timeout key or timer anywhere.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; plain C++ over the Round queue here)
**Engine Notes**: No engine API. The test drives a fake clock and a fake queue (IIF/Round test seams), never real time.

**Control Manifest Rules (this layer)**:
- Required: After the latch, transition items complete only through the ADR-0002 post-latch exemption — source: ADR-0017
- Forbidden: Never add a dwell, capacity, reveal or timeout key — source: ADR-0017
- Guardrail: Concealed pawns stay culled in `Ended` until `Reset` (no PostRound position release in MVP)

Hiding is a pause: no timer, no warning, no reveal. Stall pressure belongs to the Objectives Chain.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-17, AC-CON-12, Edge Cases), scoped to this story:*

- [ ] **AC1** (AC-CON-17, [U] `StealthGame.Concealment.RoundEndAndReset`): **GIVEN** transitions in flight at Ended **THEN** they complete (the post-latch exemption: they may write only Concealment state, the cull record and the Owner/Team flags), later Hide commands reply `NotNow`, and concealed pawns stay culled; at `OnRosterReset` every record is empty and every queued Concealment item is dropped by its generation id.
- [ ] **AC2** (AC-CON-12 simulation part, [U] `StealthGame.Concealment.NoDwellTimer`): **GIVEN** a player pawn concealed for 30 min of simulated time in phase Active (Crowd disabled for that pawn, no other input) **THEN** it is still concealed and no event, reply, log warning or state change occurs except the log's Commit row.
- [ ] **AC3** (derived, Edge Cases, States): **GIVEN** a commit, crossing or end item firing after its pawn was killed, reset or reversed **THEN** it is dropped by generation id with no effect (resolves `Voided`, no state, cull, flag or log write).

---

## Implementation Notes

*Derived from ADR-0017 Decision §3 (Phases) and the ADR-0002 post-latch exemption:*

The `ConcealmentTransition` timer kind is a third post-latch exemption: its items keep resolving after the latch and may write only Concealment state, the cull record and the Owner/Team concealment flags (never an outcome, a Roster transition or a win), and are dropped by generation/epoch at `Reset`. Without it a Commit rejected after the latch would leave an Entering pawn unculled, its position on the wire in `Ended`. A guard-failure exit after the latch is an internal transition run by the same resolver, never a `PawnCommand`. Transitions are atomic, and commits, crossings and ends are ordered by (due time, sequence id) with commands and kills. The culled pawn's destruction at Reset sends nothing to non-allies (no channel).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-032-ownership-change-while-concealed.md`: disconnect, release to Crowd and rejoin.
- `story-025-cull-push-wiring.md`: the real cull behaviour in `Ended`.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentTransitionResolver.cpp`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentCore.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/RoundEndAndResetTest.cpp` (class `FConcealmentRoundEndAndResetTest`, name `StealthGame.Concealment.RoundEndAndReset`)
- `Source/StealthGameplay/Private/Tests/Concealment/NoDwellTimerTest.cpp` (class `FConcealmentNoDwellTimerTest`, name `StealthGame.Concealment.NoDwellTimer`)
- `Source/StealthGameplay/Private/Tests/Concealment/StaleItemsVoidedTest.cpp` (class `FConcealmentStaleItemsVoidedTest`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: RoundEndAndReset
  - Given: enter and exit transitions in flight at Ended.
  - When: the phase flips and later `OnRosterReset` fires.
  - Then: in-flight transitions complete and write only Concealment state, the cull record and the Owner/Team flags (seam spy); later Hide gives `NotNow` and concealed pawns stay culled; after reset every record is empty and queued items are dropped by generation.
  - Edge cases: a reset on the same tick as a due item.
- **AC2**: NoDwellTimer
  - Given: a player Concealed with a fake clock advanced 30 min in Active (Crowd off).
  - When: the queue is drained each tick.
  - Then: still Concealed; count of events, replies, log warnings and state changes is 0, except one Commit row.
  - Edge cases: no real time is used.
- **AC3**: StaleItemsVoided
  - Given: Commit, Cross and End items.
  - When: each fires after kill, reset or reversal (9 combinations).
  - Then: each resolves Voided with no state, cull, flag or log write.
  - Edge cases: the pawn handle is reused with a new generation.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the three tests above under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [x] Created and passing (2026-10-29): StealthGame.Concealment.RoundEndAndReset, StealthGame.Concealment.NoDwellTimer, StealthGame.Concealment.StaleItemsVoided; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: N/A - headless core rule (no player-observable surface of its own); the integrated behaviour is observed in the batch's 3-player PIE run (con-*.png)

---

## Dependencies

- Depends on: `story-003-conceal-state-machine-and-occupancy-record.md`, `story-004-hide-enter-execution-and-commit.md`, `story-005-hide-exit-execution-and-blocked-side.md`; round-match-framework epic phase-gate and latch stories; IIF `production/epics/input-interaction-framework/story-095-command-test-seams.md`
- Unlocks: `story-032-ownership-change-while-concealed.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1-AC3 passing (latch exemption for in-flight transitions, reset empties every record, 30 min no-dwell simulation, stale items voided).
**Deviations**: None.
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
