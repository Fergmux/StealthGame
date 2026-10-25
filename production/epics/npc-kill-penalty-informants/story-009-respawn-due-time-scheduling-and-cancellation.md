# Story 009: Respawn due time (F2), scheduling from Round's provider call, and cancellation

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§1 seams, §3.2-§3.3), with ADR-0002 (due-time heap, `FTimerPayload`, stale-epoch rejection)
**ADR Decision Summary**: A respawn is one `TimerFired(InformantRespawn)` item in Round's due-time heap at `T_due = max(t_lost + D, t_floor, ServerNow)` with `D = InformantRespawnDelaySec` (or 0 for a body lost without a kill) and `t_floor = DeathServerTime + DeathFallSec + NoPawnFadeSec` for a dying body, else `t_lost`. `Active -> Informant` causes are scheduled only from Round's `IInformantBodyProvider::OnInformantAwaitingBody`, called after its elimination check and never when latched. Penalty registers the `TimerFired(InformantRespawn)` resolver; `OnRosterRoundEnded`, `Reset` and a disconnect cancel the item, and a stale item resolves `Rejected_StaleEpoch`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API; the heap and payload types are ADR-0002's (`FTimerPayload {Kind = InformantRespawn, Subject = FParticipantId, Generation = 0, Seq = RespawnRequestSeq, Phase = 0}`, `RoundEpoch` as `FQueueItem::Expected`). Penalty uses no engine timer, tick or latent action (manifest forbidden approach).

**Control Manifest Rules (this layer)**:
- Required: respawns for `Active -> Informant` causes are scheduled only from Round's `OnInformantAwaitingBody` call; reclaim recomputes `T_due` from the kept `t_lost` and `t_floor` — source: ADR-0019
- Forbidden: never schedule a respawn from a Penalty subscription to Roster state events, or when latched; never use an engine timer, feature tick or latent action in Penalty — source: ADR-0019
- Guardrail: Penalty is O(1) inside the 0.5 ms queue line — source: ADR-0019

The delay is counted from the cause, not from the fall's end, so every cause waits the same time (counting from the fall would give an exiled player, whose body does not fall, a 1.6 s head start); a disconnect never shortens it. The floor is a guard that never decides for `D ≥ 5` at the hard maxima (4.7 s).

**Prototype replaced:** the prototype's `EliminatePlayer` timer (`GetWorldTimerManager().SetTimer(... RespawnInformant ...)` in `AStealthGameMode`), an engine timer with a fixed delay, no floor, no epoch and no disconnect handling. Removal is story 021.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-10, AC-NKP-25, AC-NKP-11) and ADR-0019 §3.2 (derived item marked), scoped to this story:*

- [ ] **AC1** (AC-NKP-10, [L] `StealthGame.Penalty.RespawnDueTime`): **GIVEN** the F2 cases (kill slot at 100.0 s with defaults; exile at 100.0 s; an informant killed at 100.0 s that disconnects while awaiting at 103.0 s and reclaims at 120.0 s (the F2 example); embodied informant disconnected at 103.0 s and reclaimed at 120.0 s; disconnected at 103.0 s and reclaimed at 110.0 s; body lost without a kill at 130.0 s) **WHEN** `T_due` is computed **THEN** it equals 115.0, 115.0, 120.0, 120.0, 118.0 and 130.0 respectively; **AND** with D = 5 and every floor input at its hard maximum, for both the open strike and the bush variant (`T_v` = 1.2), the floor (≤ 4.7 s after `t_lost`) is never later than `t_lost + D` (F2).
- [ ] **AC2** (AC-NKP-25, [L] `StealthGame.Penalty.DisconnectNeverShortens`): **GIVEN** an embodied informant that disconnects at t and reclaims at t + 5 s **WHEN** its respawn is scheduled **THEN** `T_due = t + InformantRespawnDelaySec`, and the body it held was released at t (PEN-R11, F2).
- [ ] **AC3** (AC-NKP-11 as clarified at QL-STORY-READY, [L] `StealthGame.Penalty.RespawnCancellation`): **GIVEN** a pending respawn item **WHEN** the event below occurs before it resolves **THEN** zero `BindPawn` calls are made in every row and the row's outcome holds (PEN-R10): participant disconnect -> the item is cancelled (`Cancel(TimerHandle)`), `RespawnCancelled{Disconnect}` is logged, state `Absent` with `t_lost` kept; `OnRosterRoundEnded` -> every item is cancelled, `RespawnCancelled{RoundEnded}` is logged per pending item, state `RoundEnded`; `OnRosterReset` -> `RespawnCancelled{Reset}` is logged for each pending item, state `Active(0)`, and a leftover item from the earlier epoch that still fires resolves `Rejected_StaleEpoch`. *(Reason values `Disconnect`, `RoundEnded`, `Reset` are this story's proposal for the GDD's "with the reason"; confirm at `/story-readiness`.)*
- [ ] **AC4** (derived, ADR-0019 §3.2 and Implementation Guidelines): **GIVEN** each Active-to-Informant cause (`Assassinated`, `ReversedAgainst`, `StrikeLimit`) **WHEN** Round calls `IInformantBodyProvider::OnInformantAwaitingBody` after its elimination check **THEN** exactly one `TimerFired(InformantRespawn)` item is scheduled with payload `{Subject = P, Seq = RespawnRequestSeq, Generation = 0, Phase = 0}` and `RoundEpoch` as its Expected, and the state is `Awaiting`; **AND** no item is scheduled when the round latched (Round makes no call), none from a Roster state-event subscription, and no engine timer or tick is used.

---

## Implementation Notes

*Derived from ADR-0019 Decision §1, §3.2, §3.3 and ADR-0002 §2a, §4:*

Implement `ScheduleRespawn(P, Cause, t_lost)`, always inside a slot:
- `t_floor = GetScheduledDeathServerTime(DyingPawn) + DeathFallSec + NoPawnFadeSec` when the participant's body is dying (`Assassinated`, `ReversedAgainst`, `InformantKilled`; ADR-0018 §4.1 is valid from the kill slot), else `t_floor = t_lost` (exile, disconnect release).
- `T_due = max(t_lost + D, t_floor, ServerNow)`; `D = InformantRespawnDelaySec`, or `D = 0` for a body lost without a kill (story 010).
- If the participant's Roster connection is `Disconnected` at this slot (`Unresponsive` counts as connected, Roster R11): store `t_lost` and `t_floor`, schedule nothing, state `Absent` (story 010 asserts the Absent path).
- Otherwise `TimerHandle = ScheduleAt(T_due, TimerFired(InformantRespawn, {P, RoundEpoch, RespawnRequestSeq}))`, state `Awaiting`, log `RespawnScheduled{P, Cause, t_lost, T_due}` through story 004's writer, and raise `OnDueTimeChanged(P, T_due)` for story 011's adapter.

Implement `IInformantBodyProvider::OnInformantAwaitingBody(P, Cause, LostServerTime)` as the only scheduler for the three Roster-transition causes, and register the `IQueueResolver` for `ETimerKind::InformantRespawn` (the resolver body is story 012). Round declares `IInformantBodyProvider` in `StealthGameplay/Public/Server/Core/` (round-match-framework declares it) with `ERespawnCause { Assassinated, ReversedAgainst, StrikeLimit, InformantKilled, PawnLost, Readmitted }`; Penalty implements it and the composition root registers it weakly. Do not subscribe to `OnParticipantStateChanged` for scheduling: delegate order inside the flush is unspecified.

Cancellation: `OnRosterRoundEnded`, `OnRosterReset` and a connection `Disconnected` cancel the item (`Cancel(TimerHandle)` or let it go stale via the epoch). A stale item resolves `Rejected_StaleEpoch` (ADR-0002 §4) and a stale `BindPawn` is rejected by the Roster's epoch guard; log `RespawnCancelled{Participant, Reason}` with the reason enum.

F2 test data: boundary values are the point (the AC states the numbers), so use the literal 100.0/103.0/110.0/120.0/130.0 seconds on the fake clock, with default knobs injected through the tuning fake (`D = 15`, `DeathFallSec`, `NoPawnFadeSec`, `StrikeContactSec` as in the GDD formulas header). The hard-maximum floor check uses `DeathFallSec = 2.0`, `NoPawnFadeSec = 1.5`, `T_v = 0.8` and `1.2`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-010-absent-path-readmission-and-lost-body.md`: Absent state, readmission and the lost-body `D = 0` case.
- `story-011-t-due-owner-carrier-adapter.md`: writing `T_due` to the owner part.
- `story-012-respawn-commit-sequence.md`: the resolver body (pick, release, takeover, card).
- `story-013-starvation-retries-and-widening.md`: retries and widening.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Penalty/StealthPenaltyCore.cpp` (`ComputeDueTime`, `ScheduleRespawn`)
- `Source/StealthGameplay/Private/Features/Penalty/StealthPenaltyService.cpp` (`IInformantBodyProvider`, queue resolver registration, cancellation handlers)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyRespawnDueTimeTest.cpp` (class `FPenaltyRespawnDueTimeTest`, name `StealthGame.Penalty.RespawnDueTime`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyDisconnectNeverShortensTest.cpp` (class `FPenaltyDisconnectNeverShortensTest`, name `StealthGame.Penalty.DisconnectNeverShortens`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyRespawnCancellationTest.cpp` (class `FPenaltyRespawnCancellationTest`, name `StealthGame.Penalty.RespawnCancellation`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltySchedulingSourcesTest.cpp` (class `FPenaltySchedulingSourcesTest`, name `StealthGame.Penalty.SchedulingSources`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-10, `StealthGame.Penalty.RespawnDueTime`)
  - Given: the six F2 cases on a fake clock.
  - When: `T_due` is computed.
  - Then: 115.0, 115.0, 120.0, 120.0, 118.0, 130.0; with D = 5 and every floor input at its maximum, the floor (<= 4.7 s) is never later than `t_lost + D` for both the open strike and the bush variant (`T_v` = 1.2).
  - Edge cases: take the open-strike `T_v` from the F2 fixture; a reclaim exactly at `T_due`.
- **AC2** (AC-NKP-25, `StealthGame.Penalty.DisconnectNeverShortens`)
  - Given: an embodied informant that disconnects at t and reclaims at t + 5 s.
  - When: its respawn is scheduled.
  - Then: `T_due = t + InformantRespawnDelaySec` and the body was released at t.
  - Edge cases: a reclaim after `T_due` is recomputed by story 010; the delay is never restarted or skipped.
- **AC3** (AC-NKP-11, `StealthGame.Penalty.RespawnCancellation`, three rows)
  - Given: a pending item per row.
  - When: a disconnect, `OnRosterRoundEnded` or `OnRosterReset` occurs.
  - Then: 0 `BindPawn` calls in every row; disconnect: `Cancel(TimerHandle)`, `RespawnCancelled{Disconnect}`, Absent, `t_lost` kept; round end: all items cancelled, `RespawnCancelled{RoundEnded}` per item, `RoundEnded`; reset: `RespawnCancelled{Reset}` per item, `Active(0)`, an injected leftover item resolves `Rejected_StaleEpoch`.
  - Edge cases: the rows are written inside the end handling, not rejected by the freeze; two pending items give two rows; the reason values are a story proposal.
- **AC4** (derived, `StealthGame.Penalty.RespawnScheduling`, three causes plus a lint)
  - Given: stub Round calls for `Assassinated`, `ReversedAgainst` and `StrikeLimit`.
  - When: `OnInformantAwaitingBody` is called after the elimination check.
  - Then: exactly one `TimerFired(InformantRespawn)` item with `{Subject = P, Seq, Generation = 0, Phase = 0}` and `Expected = RoundEpoch`, state Awaiting; when the round latched there is no call and no item; a raised Roster state event schedules nothing; the lint finds no engine timer, tick or `OnParticipantStateChanged` binding.
  - Edge cases: add the `InformantKilled` cause, which Penalty schedules itself; a Disconnected P goes to Absent (story 010).

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the four tests under `Source/StealthGameplay/Private/Tests/Penalty/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-001-penalty-core-store-and-service-shell.md`, `story-004-event-log-rows.md`; cross-epic: round-match-framework (due-time heap, `IInformantBodyProvider` declaration; no story path yet), assassination-defence-reversal (`GetScheduledDeathServerTime`, ADR-0018 §4.1), participant-state-roster (connection state, `story-022-reset-all-batch.md`, `story-023-end-round-all-batch-idempotent.md`)
- Unlocks: `production/epics/npc-kill-penalty-informants/story-010-absent-path-readmission-and-lost-body.md`, `story-011-t-due-owner-carrier-adapter.md`, `story-012-respawn-commit-sequence.md`
