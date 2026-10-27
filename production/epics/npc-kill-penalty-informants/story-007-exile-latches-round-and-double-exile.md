# Story 007: Exile of a last Active player latches the round; two exiles in one drain

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-002`, `TR-pen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging (latch and drain order), with ADR-0019 §2.3 step 6 and §3.3
**ADR Decision Summary**: One total-order queue keyed `(ServerTickIndex, Seq)` drained in `TG_PostPhysics`, with a single-assignment terminal latch. The exile's Roster event is dispatched in the slot's Roster flush, where Round's elimination check runs: the exile of a team's last Active player latches `EliminationWin` for the other team at once, and Round then makes no `OnInformantAwaitingBody` call, so no respawn is ever scheduled inside a deciding slot. A second Attack after the latch is `Rejected_Latched` and counts nothing.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API in Penalty's code. ADR-0002 Verification Required (3): the drain tick function runs exactly once per server frame after CMC and AI ticks, including on the listen host (frame time ≤ 1/60 s at the profiled load); same-drain ordering tests run on the queue's test seam, not in real frames.

**Control Manifest Rules (this layer)**:
- Required: Roster transitions and strike accounting happen in the slot; the exile's elimination check runs in the same slot via the Roster event — source: ADR-0018, ADR-0002
- Forbidden: never schedule a respawn from a Penalty subscription to Roster state events, or when latched — source: ADR-0019
- Guardrail: a latched round accepts no further strike accounting — source: ADR-0002

These two cases are the exile's interaction with the win rule. An exile that empties a team must win the round for the other team in the same slot; two exiles that would empty both teams in one drain must not both count (the first in key order wins, the second is rejected with no strike and no tally).

**Prototype replaced:** none. The prototype's `ExilePlayer` ran its own elimination check after the exile on the game mode; the production check is Round's, in the same slot through the Roster event. The prototype's elimination-win path is exercised by prototype test C and is not Penalty's.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-08, AC-NKP-41), scoped to this story:*

- [ ] **AC1** (AC-NKP-08, [I] `StealthGame.Penalty.ExileLatchesRound`): **GIVEN** a team whose only Active player has L − 1 strikes and an opponent team with ≥ 1 Active **WHEN** that player's next ungraced NPC kill resolves **THEN** `EliminationWin` for the opponent latches in the exile's slot and no `RespawnScheduled` row is written for the exiled player (PEN-R4 step 4, Edge Cases; the two-exile drain is AC2).
- [ ] **AC2** (AC-NKP-41, [I] `StealthGame.Penalty.TwoExilesOneDrain`): **GIVEN** two exiling Attacks, one per team, each by its team's last Active player, in one drain **WHEN** the drain resolves **THEN** only the first in key order latches `EliminationWin` and the second Attack is `Rejected_Latched` with no strike and no tally (Edge Cases).

---

## Implementation Notes

*Derived from ADR-0019 Decision §2.3 step 6, §3.3 and ADR-0002 §4-§5:*

Penalty adds no win logic: Round owns the elimination check on the Roster event and the latch. Penalty's obligation in these cases is negative and testable: (1) the exile's transition (story 005) raises the Roster event inside the slot, so the check sees the post-exile `CountActive`; (2) Penalty never schedules from the state-event subscription, only from Round's `OnInformantAwaitingBody` (story 009), which Round skips when latched, so no `RespawnScheduled` row exists for the exiled player; (3) a `Rejected_Latched` Attack never emits `OnPawnKilled`, so no strike or tally is counted for the second Attack (the tally is not even called: `GetTallyForResult` is skipped for a rejected Attack).

Test construction: use the Round test seam (the stub/real queue with scripted keys) to place the two Attack items in one drain with explicit `(tick, seq)`; run the two teams' last-Active scenarios; assert the latch value, the `EndReason = Elimination`, the winner, the rows in the Event Log (no `RespawnScheduled`), and that the second Attack's `FStrikeResult` is not produced (`Rejected_Latched`). For AC1 also assert that the NPC body still falls and the plate result is `WrongTarget` (the exile does not suppress the kill's own presentation).

These are Integration because they cross Penalty, Roster, Round and Assassination; the real Round latch and the real Roster are used with fake Crowd, LSC and Character Pool gateways. No PIE run is needed.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-005-exile-sequence-in-attack-slot.md`: the exile itself.
- `story-009-respawn-due-time-scheduling-and-cancellation.md`: the provider call that Round skips when latched, and the round-end cancellation.
- round-match-framework: the elimination check and latch semantics (its own ACs; Penalty only observes them).

---

## Files

*Changed by /dev-story 2026-10-27.*

- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyEncounterRig.h` (shared with nkp-005: Penalty on the real queue, resolver and Round core)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyExileLatchesRoundTest.cpp` (`StealthGame.Penalty.ExileLatchesRound`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyTwoExilesOneDrainTest.cpp` (`StealthGame.Penalty.TwoExilesOneDrain`)
- No change to `StealthPenaltyService.cpp` was needed for the latched path.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-08, `StealthGame.Penalty.ExileLatchesRound`)
  - Given: real Round and Roster; P is the team's only Active player at k = L - 1, the opponent team has at least 1 Active.
  - When: P's next ungraced NPC kill resolves.
  - Then: `EliminationWin` for the opponent latches in that slot; no `RespawnScheduled` row or heap item exists for P and `OnInformantAwaitingBody` is never called; the `StrikeLimit` row is present (Round's); the NPC still falls and the plate reads `WrongTarget`.
  - Edge cases: contrast run with another teammate Active: no latch and the respawn is scheduled.
- **AC2** (AC-NKP-41, `StealthGame.Penalty.TwoExilesOneDrain`)
  - Given: two exiling Attacks, one per team, each by that team's last Active player, in one drain.
  - When: the drain resolves, once per key order.
  - Then: only the first in key order latches `EliminationWin`; the second Attack is `Rejected_Latched` with no strike, no tally and no exile.
  - Edge cases: `StrikeCounted` rows exist for the first Attack only.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two tests under `Source/StealthGameplay/Private/Tests/Penalty/` (`tests/integration/penalty/` in the studio docs) OR a documented playtest

**Status**: [x] Created and passing (2026-10-27): StealthGame.Penalty.ExileLatchesRound, TwoExilesOneDrain; evidence production/qa/evidence/npc-kill-penalty-informants-005-008-2026-10-27/

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-005-exile-sequence-in-attack-slot.md`; cross-epic: round-match-framework (elimination check, terminal latch, `Rejected_Latched`), participant-state-roster `story-017-transition-no-revival-no-late-events.md`
- Unlocks: None

## Dev Notes (2026-10-27)

**Readiness**: READY (nkp-005 in the same batch; Round's elimination check, latch and `Rejected_Latched` exist).

**Deviations**:
- Round writes the StrikeLimit transition as its `StateTransition` row (Code = `ERosterReason::StrikeLimit`); there is no separate `StrikeLimit` row type in use, so AC1 asserts that row.
- The Roster in these tests is the rig's fake (`FRoundEncounterRig`): Penalty's body gateway applies the StrikeLimit release and forwards the transition to Round's F2 check, as the store's event does. "OnInformantAwaitingBody is never called" is trivially true until nkp-009 adds the provider; asserted as no respawn call and no `RespawnScheduled` row.

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: 2/2 passing
**Deviations**: the StrikeLimit row is Round's StateTransition row (Code = StrikeLimit)
**Test Evidence**: production/qa/evidence/npc-kill-penalty-informants-005-008-2026-10-27/ (`tools/dev/cycle.ps1 -Tests -NoLaunch`: Result: Succeeded; 408/409 StealthGame. tests Success (the one failure is asn-027's uncommitted PrototypeScenariosACF.F_BlindStrike, not this batch)); run-and-observe OBSERVED in 3-player PIE (screenshots 01-05)
**Reviewed by**: self code review 2026-10-27 (APPROVED; notes in the evidence)
