# Story 033: An acknowledgement with an old nonce or replaced controller is dropped and does not satisfy the barrier

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-006` (Preparing readiness sequence, spawn gate and watchdog); GDD AC 8c; rule R6 step P5.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies the acknowledgement keying: ack records are keyed by `ParticipantId` and this Preparing's nonce (`FAckEpoch { RoundIndex; TuningRevision; AckSeq }`), never by controller, so an acknowledgement from an earlier round or a replaced controller is stale and dropped.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: None touched. The test drives the barrier with fake Session acknowledgements. The facts behind it are ADR-0005 E5 (seamless travel replaces each PlayerController, so an acknowledgement sent through the old controller is stale) and E19, verified by S3 in the Session epic, not here.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules, section Session and travel):
- Required: Ack records, kick list, silence clocks and connection maps are keyed by `ParticipantId` (or `UNetConnection` for the live mapping) and never replicated (ADR-0005).
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState (ADR-0005); a replaced controller must therefore not be able to satisfy the barrier.
- Guardrail: Session ack bookkeeping stays inside the 0.5 ms line (ADR-0005).

**Prototype disposition**: Replaces the prototype start-up readiness in `Source/StealthGame/Private/StealthGameMode.cpp` (`TryScheduleRoundStart`); production acknowledgement keying is new Round Core code over Session's events.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 8c, scoped to this story:*

- [ ] GIVEN an acknowledgement carrying an old nonce or a replaced controller, THEN it is dropped and does not satisfy the barrier.

---

## Implementation Notes

*Derived from GDD R6 P5, ADR-0002 section 8.6 and ADR-0005 (Key Interfaces):*

- **Keyed acknowledgement.** The P5 barrier tracks acknowledgements by `ParticipantId` plus this Preparing's nonce (`FAckEpoch`, ADR-0005 Key Interfaces). An acknowledgement whose epoch does not match the current Preparing (an earlier round) or whose delivering controller was replaced is dropped: it does not mark the participant acknowledged and does not change the wait set.
- **No inference from a PlayerState.** Travel-complete never comes from a PlayerState existing (PlayerStates are carried through travel, ADR-0005 E6).
- **Bounded, not extended.** A dropped acknowledgement does not start, extend or restart any deadline (ADR-0005 guideline 7): the participant's Session F7 `Deadline` runs unchanged.
- **Valid acknowledgement afterwards.** A correct acknowledgement from the same participant after the dropped ones satisfies the barrier normally.
- **Counting.** Open: confirm against GDD R6 P5 and ADR-0002 section 8.6 whether a dropped acknowledgement is logged or counted; both state only that stale acknowledgements "are dropped" and do not mention logging, so log nothing unless the contract is amended.
- **Test shape.** Preparing with the barrier satisfied except for one client; inject, in turn, an acknowledgement with an old nonce and one from a replaced controller (fake Session raising `OnParticipantTravelComplete` with a stale `FAckEpoch`), assert the barrier is unsatisfied and the phase stays `Preparing`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 032 (`production/epics/round-match-framework/story-032-preparing-barrier.md`): the barrier itself and the world-ready stage (this story's dependency).
- Story 034 (`production/epics/round-match-framework/story-034-preparing-aborts.md`): abort codes; `ClientHash` / `PoolHash` mismatches never abort and only disconnect that client (`ParamsMismatch`), handled by Session.
- Session internals (the `ServerRoundAck` RPC, `FAckEpoch` issue, controller replacement) and TDC internals (client hash computation).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Round/Preparing/PreparingBarrier.cpp` (shared with story 032)
- `Source/StealthGameplay/Private/Tests/Round/StaleAckDroppedTest.cpp` (class `FRoundStaleAckDroppedTest`, test name `StealthGame.Round.StaleAckDropped`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: An acknowledgement with an old nonce or replaced controller is dropped and does not satisfy the barrier.
  - Given: Preparing with the barrier satisfied except for one client's acknowledgement.
  - When: that client sends an acknowledgement with an old nonce, then one from a replaced controller.
  - Then: each is dropped.
  - Then: the barrier is not satisfied and the phase stays `Preparing`.
  - Edge cases: a valid acknowledgement afterwards still satisfies the barrier; Open: confirm against GDD R6 P5 whether the dropped acknowledgements are logged or counted (only if the contract says so).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/StaleAckDroppedTest.cpp` (automation test `StealthGame.Round.StaleAckDropped`, class `FRoundStaleAckDroppedTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.StaleAckDropped`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-032-preparing-barrier.md`
- Unlocks: None
- Cross-epic: `production/epics/session-travel` (acknowledgement payload, `FAckEpoch`, controller replacement; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (old nonce and replaced-controller acknowledgements dropped; no deadline moved; nothing logged; a valid acknowledgement afterwards satisfies the barrier)
**Deviations**: Open point resolved: nothing is logged for a dropped acknowledgement (GDD R6 P5 / ADR-0002 8.6 name no entry).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/StaleAckDroppedTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: N/A - stale acknowledgements cannot be produced in L_Prototype (no travel, no replaced controllers before the session-travel epic); covered headless
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
