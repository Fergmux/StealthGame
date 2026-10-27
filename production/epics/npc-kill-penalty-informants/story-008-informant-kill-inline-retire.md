# Story 008: Informant kill retires the binding inline and looks exactly like an NPC kill

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: In Review
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§2.1), with ADR-0001: Crowd identity rules (the invariant itself)
**ADR Decision Summary**: If `VictimKind = Informant`, Penalty calls Roster `RetireBinding(Victim)` inline in the slot (Roster "not a transition"), so a second strike on the falling body resolves `AlreadyResolved`, then schedules the victim participant's respawn (cause `InformantKilled`). No strike is added and the tally follows the `WrongTarget` result like any other, so every attacker-visible output is byte-identical to an NPC kill (Roster AC-PSR-52).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Risk is the highest of the governing ADRs: ADR-0001 is HIGH (owner-only replication of `PlayerState`, `Controller` and `Owner`; legacy NetDriver with Iris pinned off), ADR-0019 is MEDIUM. Spikes S1, S2 and S14 reported 2026-10-25 (`production/qa/spikes/2026-10-25_S1-S2-S14.md`): the three pointers are absent for non-owners (PASS) but whole-pawn parity FAILED in the prototype on `RemoteViewPitch16` and two sample-Blueprint properties (`CharacterInputState`, `TraversalResult`); those must be closed by the production pawn before AC2's byte comparison can pass. AC2 is a PIE test (ADR-0008 CQTest, PIE-3).

**Control Manifest Rules (this layer)**:
- Required: an informant kill is perceptually identical to an NPC kill; `FStrikeResult` is the same shape for every result — source: ADR-0001, ADR-0018, ADR-0019
- Forbidden: per-kill cue selection reads no `VictimKind`, `Graced` or `bExiled` input (reflection check); never send any outcome field besides `FStrikeResult`/`FDefenceResult` — source: ADR-0018
- Guardrail: Penalty O(1) inside the 0.5 ms queue line — source: ADR-0019

This is the Pillar-1 invariant at the attacker: killing an enemy informant must be indistinguishable, to the attacker and to its allies, from killing an NPC. The kill earns a `WrongTarget` plate and a tally tick, never a strike, and what Penalty adds (the inline retire and the victim's respawn) is invisible to the killer.

**Prototype replaced:** the informant branch of the prototype's assassination handler (`MessagePlayer(Attacker, "An enemy informant, silenced for now.")` and `EliminatePlayer(Target)` for an informant victim) and prototype test B (killing an enemy informant respawns them and is not an NPC kill). The prototype's *distinct message* for an informant kill is exactly the tell the invariant forbids and is not carried over; test B is ported in story 021 against the production path.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-15, AC-NKP-03, AC-NKP-26), scoped to this story:*

- [ ] **AC1** (AC-NKP-15, [L] `StealthGame.Penalty.InformantKillRetiresInline`): **GIVEN** two strikes on the same embodied informant in one drain **WHEN** both resolve **THEN** the first is a kill (`VictimKind = Informant`, binding retired in the slot) and the second resolves `NotTargetable(AlreadyResolved)` (`NoContact`, no tally, no strike) (PEN-R7).
- [ ] **AC2** (AC-NKP-03, [I][PIE-3] `StealthGame.Penalty.PIE.InformantKillParity`): **GIVEN** an attacker with tally 0 and k + 1 < L who kills an enemy informant and, in a second run from the same seed, an NPC **WHEN** the attacker's connection and each of the attacker's allies' connections log every received RPC payload and replicated property for the attacker's own PlayerController, PlayerState and C-TeamView, plus the attacker's HUD widget state and played cue ids **THEN** the logs compare equal field by field after normalising timestamps, the victim actor id and `ClientCommandSeq`, including `TallyAfter = 1` in both (PEN-R14; extends AC-PSR-52; the boundary case k + 1 = L is AC-NKP-39 in story 003).
- [ ] **AC3** (AC-NKP-26, [I][PIE-3] `StealthGame.Penalty.PIE.CueInputs`): **GIVEN** an NPC kill, an informant kill and a graced kill at equal `TallyAfter`, and an exiling kill **WHEN** audio and haptic calls are logged with their arguments **THEN** the three kills log identical cue ids; the cue selector's input is a reflected `USTRUCT` (`FKillCueInput`) whose only properties are `TallyAfter` and L, enumerated by the test, so no `VictimKind`, `Graced` or `bExiled` input exists; and the exile cue is invoked 0 times at the contact frame and once from HUD's card-entrance event (VA-NKP-1, VA-NKP-5; the ± 1 frame timing at `s + ExileBeatSec` is asserted by story 017's AC1).

---

## Implementation Notes

*Derived from ADR-0019 Decision §2.1, §3.2 and Implementation Guidelines:*

In `OnPawnKilled` (story 002) the `VictimKind = Informant` branch runs, in this order and inline in the slot: (1) Roster `RetireBinding(Victim, RetireAtServerTime, Reason = InformantKilled)` (roster `story-020-informant-kill-is-not-a-transition.md`; the retire time is the death write's time source, roster `story-021-retire-at-server-time-source.md`; the reason is written only to the Roster's ServerOnly log), so the second strike resolves `AlreadyResolved`; (2) `ScheduleRespawn(VictimParticipant, InformantKilled, slotTime)` through story 009's scheduler (this story calls it; if story 009 is not yet merged use a no-op seam and mark AC1's schedule assertion pending). No strike is added (the strike rule, story 002, already excludes `Informant`); the tally increments through `GetTallyForResult` because the result is `WrongTarget` (story 003).

Nothing Penalty does here may change a byte the attacker receives. The parity test (AC2) runs the same scripted seed twice: kill an informant vs. kill an NPC, with recording on the attacker's and each ally's connection (the PIE-3 harness: 3v3, `StealthTestHarness`, capture helpers per ADR-0008); compare field by field after normalising timestamps, victim actor id and `ClientCommandSeq`. The roster epic's `story-050-pie-informant-kill-looks-like-npc-kill.md` covers the Roster side of AC-PSR-52; this story covers Penalty's additions (the inline retire and respawn scheduling must not alter any attacker-side byte) and the `TallyAfter` equality.

AC3 reads the per-kill cue selection code by reflection: it must take only `TallyAfter` and the latched `L` as inputs. The exile cue is not a per-kill cue: HUD raises it from the card-entrance event, never from `bExiled` directly (otherwise it would fire at the contact frame, VA-NKP-5). HUD and audio are other epics; this story provides the test and fixes any Penalty-side input that leaks.

Provisional note: AC2 and AC3 were tagged against spikes S1/S14. Both reported (see Engine Notes), so they are not blocked; re-run on the production pawn with the multi-process capture still open in the spike report's conditions.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-009-respawn-due-time-scheduling-and-cancellation.md`: the `InformantRespawn` item and its due-time rule.
- `story-016-strike-count-never-leaves-and-removal-audiences.md`: the `StrikeCount` differential capture and removal audiences.
- participant-state-roster `story-050-pie-informant-kill-looks-like-npc-kill.md`: the Roster side of the same parity.

---

## Files

*Changed by /dev-story 2026-10-27.*

- `Source/StealthGameplay/Features/Penalty/Public/Server/StealthPenaltySeams.h` (`IPenaltyBodyGateway`, `IPenaltyRespawnScheduler`, `IPenaltyPawnGateway`, fault details)
- `Source/StealthGameplay/Features/Penalty/Private/StealthPenaltyCore.h`, `StealthPenaltyCore.cpp` (`RunExile`, `RetireInformant`, `FPenaltyKill` body fields, released-body tally mapping, `FindReleaseTime`)
- `Source/StealthGameplay/Features/Penalty/Public/Server/StealthPenaltyService.h`, `Private/StealthPenaltyService.cpp` (`FBodies` adapter over URosterStore / ICrowdControl / IPenaltyPawnGateway, lockout and death-time reads)
- `Source/StealthFoundation/Public/Server/Roster/StealthRosterCore.h`, `Private/Roster/StealthRosterCore.cpp`, `Public/Server/Roster/StealthRosterStore.h`, `Private/Roster/StealthRosterStore.cpp` (`RetireBinding`, Roster story psr-020 landed here)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterInformantKillIsNotATransitionTest.cpp` (`StealthGame.Roster.InformantKillIsNotATransition`, psr-020 AC-PSR-21)
- `Source/StealthFoundation/Private/Tests/Roster/StealthClientCallsRejectedTest.cpp` (the RetireBinding client row)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyInformantKillRetiresInlineTest.cpp` (`StealthGame.Penalty.InformantKillRetiresInline`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyInformantKillParityTest.cpp` (`StealthGame.Penalty.InformantKillParity`, the server-side half of AC2)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-15, `StealthGame.Penalty.InformantKillRetiresInline`)
  - Given: a real Roster with R9 semantics, a fake clock, an embodied informant, two strikes on it in one drain.
  - When: both resolve in key order.
  - Then: the first is a kill (`VictimKind = Informant`, binding retired in the slot, respawn scheduled); the second resolves `NotTargetable(AlreadyResolved)` as `NoContact` with no tally and no strike.
  - Edge cases: same attacker for both strikes and different attackers; no second `RespawnScheduled`.
- **AC2** (AC-NKP-03, `StealthGame.Penalty.PIE.InformantKillParity.<Method>`)
  - Given: 3v3 PIE, fixed seed, attacker tally 0 and k + 1 < L; run 1 kills an enemy informant, run 2 kills an NPC, same prior state.
  - When: the attacker's and each ally's connection log all RPCs, replicated properties, HUD widget state and cue ids.
  - Then: logs equal field by field after normalising timestamps, victim actor id and `ClientCommandSeq`, with `TallyAfter = 1` in both.
  - Edge cases: the k + 1 = L boundary is story 003 AC2; execution needs HUD.
- **AC3** (AC-NKP-26, `StealthGame.Penalty.PIE.CueInputs.<Method>`)
  - Given: an NPC kill, an informant kill and a graced kill at equal `TallyAfter`, plus an exiling kill.
  - When: audio and haptic calls are logged with their arguments.
  - Then: the three non-exile kills log identical cue ids; reflecting `FKillCueInput` lists exactly `{TallyAfter, L}`; the exile cue is called 0 times at the contact frame and once from HUD's card-entrance event.
  - Edge cases: the +/- 1 frame timing is story 017's; depends on HUD's card-entrance event existing.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two CQTest PIE tests under `Source/StealthTestHarness/Private/Penalty/` and the unit test under `Source/StealthGameplay/Private/Tests/Penalty/` OR a documented playtest

**Status**: [x] Created and passing (2026-10-27): StealthGame.Penalty.InformantKillRetiresInline, InformantKillParity (+ StealthGame.Roster.InformantKillIsNotATransition); evidence production/qa/evidence/npc-kill-penalty-informants-005-008-2026-10-27/

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-002-strike-rule-and-release-grace.md`, `story-003-tally-rule-and-strike-result-supply.md`; cross-epic: participant-state-roster `story-020-informant-kill-is-not-a-transition.md`, `story-021-retire-at-server-time-source.md`, `story-050-pie-informant-kill-looks-like-npc-kill.md`; assassination-defence-reversal (`VictimKind = Informant`, byte-identical `WrongTarget`); hud (cue selection reads `TallyAfter` only); tuning-data-config `story-019-cqtest-headless-proof.md`
- Unlocks: `production/epics/npc-kill-penalty-informants/story-021-retire-prototype-penalty-code.md`

## Dev Notes (2026-10-27)

**Readiness**: READY with gaps: Roster psr-020 (`RetireBinding`) was backlog, so it is landed here with its own test (decision D10, confirmed by the user: InformantKilled is raised on RetireBinding, never as a transition). The HUD cue selector (`FKillCueInput`), HUD widget state and the PIE-3 3v3 capture do not exist.

**What runs**: every informant kill (Cause = Strike, VictimKind = Informant) calls `RetireBinding(Victim, RetireAtServerTime = the death write's time, InformantKilled)` inline in the slot, then `ScheduleRespawn(VictimParticipant, InformantKilled, slot time)` through the nkp-009 seam (null in production until nkp-009: the prototype respawn after the body's release still runs). No strike is added; the tally follows WrongTarget. A retire that does not apply faults once (`InformantRetireRejected`).

**Deviations**:
- AC1: as written; the drain part uses the rig's Roster fake for the encounter side and a second part runs the retire on the REAL Roster core (state stays Informant, no event, no EventSeq, AlreadyResolved, the tombstone carries the death time).
- AC2: the server-side half runs headless (`StealthGame.Penalty.InformantKillParity`: the attacker's FStrikeResult reply, FActionState writes and the victim's FDeathState compare equal between an informant kill and an NPC kill; the ally receives nothing). The PIE-3 capture of every RPC, replicated property, HUD widget state and cue id is DEFERRED (HUD epic; spike S1/S14 multi-process capture).
- AC3: DEFERRED in full: no per-kill cue selector or `FKillCueInput` exists (HUD / audio epics). Penalty adds no cue input (it sends nothing but FStrikeResult's TallyAfter / bExiled).

## Completion Notes
**Completed**: 2026-10-27 (status In Review)
**Criteria**: AC1 passing; AC2 server-side half passing (the PIE-3 capture DEFERRED); AC3 DEFERRED in full (no HUD cue selector / FKillCueInput yet)
**Deviations**: the open halves need the HUD epic and the PIE-3 multi-process capture; reopen when they exist
**Test Evidence**: production/qa/evidence/npc-kill-penalty-informants-005-008-2026-10-27/ (`tools/dev/cycle.ps1 -Tests -NoLaunch`: Result: Succeeded; 408/409 StealthGame. tests Success (the one failure is asn-027's uncommitted PrototypeScenariosACF.F_BlindStrike, not this batch)); run-and-observe OBSERVED in 3-player PIE (screenshots 01-05)
**Reviewed by**: self code review 2026-10-27 (APPROVED; notes in the evidence)
