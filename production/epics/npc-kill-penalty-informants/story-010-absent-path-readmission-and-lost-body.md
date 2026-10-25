# Story 010: Absent path for disconnected victims, readmission and bodies lost without a kill

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§3.2 disconnected rule, §3.3 Absent rows)
**ADR Decision Summary**: A respawn cause on a participant whose Roster connection is `Disconnected` stores `t_lost` and `t_floor`, schedules nothing and puts the participant in `Absent`. `OnInformantReadmitted(P)` (Round R18) moves it to `Awaiting` if `now < T_due` else `Picking(0)`, with `T_due = max(t_lost + D, t_floor, now)` from the **kept** `t_lost` and `t_floor`; `OnInformantPawnLost(P)` (Round R17.9 informant case) re-pawns at once with `D = 0` and logs `PawnLost`, leaving strike and tally unchanged.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API; the lost-body event comes from Round's R17.9 path (KillZ or a destroyed body, a defect case), not from an engine callback in Penalty.

**Control Manifest Rules (this layer)**:
- Required: reclaim recomputes `T_due` from the kept `t_lost` and `t_floor`; every Penalty mutation runs inside a queue slot — source: ADR-0019
- Forbidden: never schedule a timer for a respawn cause on a `Disconnected` participant; never restart or skip the delay on reclaim — source: ADR-0019
- Guardrail: Penalty is O(1) inside the 0.5 ms queue line — source: ADR-0019

These are the connection edge cases: a disconnected `Active` player (in grace, AI-driven pawn) who is assassinated, then returns before or after the grace; an `Unresponsive` victim, which counts as connected; and an informant body lost to KillZ. Reclaiming is never faster than waiting, and never leaves a returner stranded.

**Prototype replaced:** none (the prototype's `RespawnInformant` ignored connection state: a disconnected controller's weak pointer simply expired).

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-48, AC-NKP-28), scoped to this story:*

- [ ] **AC1** (AC-NKP-48, [L] `StealthGame.Penalty.DisconnectedVictimAbsent`): **GIVEN** an `Active` participant with `Connection = Disconnected` (grace running, AI-driven pawn) assassinated at 100.0 s **WHEN** the slot resolves **THEN** Penalty state is Absent with `t_lost` = 100.0 and no `TimerFired(InformantRespawn)` item exists; **AND WHEN** the participant is reclaimed at 110.0 s **THEN** `T_due` = 115.0 (Awaiting); **AND** reclaimed instead at 130.0 s **THEN** Picking starts at once; **AND** the grace timer firing at 130.0 s changes nothing and is logged `Voided`; **AND GIVEN** the same kill on an `Unresponsive` participant **THEN** the state is Awaiting with `T_due` = 115.0 (PEN-R8, cross-review S-B3).
- [ ] **AC2** (AC-NKP-28, [L] `StealthGame.Penalty.LostBodyRepawn`): **GIVEN** an embodied informant whose body hits KillZ at 130.0 s **WHEN** the loss is reported **THEN** `T_due` = 130.0, PEN-R9 runs, strike and tally are unchanged, and `PawnLost` is logged (PEN-R11).

---

## Implementation Notes

*Derived from ADR-0019 Decision §3.2, §3.3 and PEN-R8, PEN-R11:*

State machine rows implemented here (the table is ADR-0019 §3.3): Active -> Absent on `OnInformantAwaitingBody` while `Disconnected`; Active -> Absent on Round's grace expiry (`Informant(Disconnect)` plus `ReleaseToCrowd`, Round R17.4; `t_lost` = expiry slot time, no item); Embodied -> Absent on a `Disconnected` connection event (Round R17.5 releases the pawn to the crowd in that slot; `t_lost` = that slot, `t_floor = t_lost`); Awaiting/Picking -> Absent on `Disconnected` (cancel the item, keep `t_lost`, log `RespawnCancelled{Disconnect}` through story 009's cancellation); Absent -> Awaiting/Picking on `OnInformantReadmitted(P)`, called by Round only after the returner's Session acknowledgement when its PlayerController is ready (Round R18; phase `Active`).

Readmission: `T_due = max(t_lost + D, t_floor, now)` using the *kept* values, never recomputed from the reclaim time. If `now < T_due` the state is Awaiting with a new heap item and `OnDueTimeChanged`; otherwise `Picking(0)` and story 012's commit runs in the same slot. A reclaim during `Active` for an `Active` participant who was never an informant (disconnect inside grace, pawn still alive) is not a Penalty event: strikes and tally carry (story 001).

Lost body: `OnInformantPawnLost(P)`: `Embodied -> Picking(0)`, `T_due = now`, `D = 0`, log `PawnLost{Participant, Cause}`, run story 012's commit in the same slot; strike and tally unchanged. Round R17.9 covers `Active` players and routes the informant case here (Penalty CR-8).

Round's later grace expiry for an already-`Informant` participant is Round's own logged no-op (`Voided`, Round R17.4); the AC1 assertion that it "changes nothing" is asserted on Penalty's state (unchanged) and on the Event Log row Round writes, not on Round internals.

Test data: the literal times in the ACs (100.0, 110.0, 115.0, 130.0) are boundary values; use a fake server clock and a stub Roster that reports `Connected`, `Disconnected` and `Unresponsive` as the case requires.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-009-respawn-due-time-scheduling-and-cancellation.md`: the F2 computation, scheduler and cancellation machinery.
- `story-012-respawn-commit-sequence.md`: the commit that runs when Picking starts.
- participant-state-roster `story-030-informant-disconnect-release-reclaim.md`: the Roster-side release and reclaim (a different pawn is adopted).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Penalty/StealthPenaltyCore.cpp` (Absent transitions, kept `t_lost`/`t_floor`)
- `Source/StealthGameplay/Private/Features/Penalty/StealthPenaltyService.cpp` (`OnInformantReadmitted`, `OnInformantPawnLost`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyDisconnectedVictimAbsentTest.cpp` (class `FPenaltyDisconnectedVictimAbsentTest`, name `StealthGame.Penalty.DisconnectedVictimAbsent`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyLostBodyRepawnTest.cpp` (class `FPenaltyLostBodyRepawnTest`, name `StealthGame.Penalty.LostBodyRepawn`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-48, `StealthGame.Penalty.DisconnectedVictimAbsent`)
  - Given: an Active P with `Connection = Disconnected` and an AI-driven pawn, assassinated at 100.0.
  - When: (i) the slot resolves; (ii) P is reclaimed at 110.0; (iii) instead reclaimed at 130.0; (iv) the grace timer fires at 130.0; (v) the same kill on an `Unresponsive` P.
  - Then: (i) Absent, `t_lost` = 100.0, no `TimerFired(InformantRespawn)` item; (ii) Awaiting, `T_due` = 115.0; (iii) Picking starts at once; (iv) Penalty state unchanged; (v) Awaiting, `T_due` = 115.0.
  - Edge cases: the "logged `Voided`" clause is Round's row: assert it in an integration run with the real Round, in this fixture assert only that Penalty is unchanged; a reclaim at exactly `T_due` goes to Picking; `t_floor` is kept across the Absent period; `OnInformantReadmitted` is called only after the Session acknowledgement.
- **AC2** (AC-NKP-28, `StealthGame.Penalty.LostBodyRepawn`)
  - Given: an Embodied informant whose body hits KillZ at 130.0.
  - When: `OnInformantPawnLost(P)` is called.
  - Then: `T_due` = 130.0 (D = 0), state Picking(0), the fake LSC receives `PickRespawnPawn` in that same slot with a fresh `RespawnRequestSeq`; k and t unchanged; a `PawnLost{P, Cause}` row is written.
  - Edge cases: the pawn is lost while Disconnected (state becomes Absent instead); a second loss report is idempotent.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the two tests under `Source/StealthGameplay/Private/Tests/Penalty/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-009-respawn-due-time-scheduling-and-cancellation.md`; cross-epic: round-match-framework (`OnInformantReadmitted`, `OnInformantPawnLost`, grace expiry R17.4/R17.5/R17.9, `story-072-informant-disconnect-releases.md`), participant-state-roster (connection state)
- Unlocks: `production/epics/npc-kill-penalty-informants/story-012-respawn-commit-sequence.md`
