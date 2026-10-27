# Story 027: Kill vacancy, victim-blind replacement delay and exit cancellation

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-008 = Ambient emergence and replacement via shared sockets and DrawReplacement.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0018 (death-write timing: `KillServerTime` = `GetScheduledDeathServerTime(Victim)`; no kill effect observable before the death write), ADR-0002 (`Voided(Revalidation)`)
**ADR Decision Summary**: `OnPawnKilled` runs inline in the Attack slot with `KillServerTime` the scheduled death-write time. The Director adds the vacancy at `t_k`, cancels one upcoming exit chosen uniformly among those due in [t_k, t_k + 60 s] (an `AmbientSchedule` draw), and binds the replacement to the first scheduled entry at or after `t_k + CrowdReplacementMinDelaySec`, with the `CrowdReplacementMaxDelaySec` fallback. The ledger reads only kill times, never who died.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0002 has no cancel for a scheduled item, so a cancelled exit is recorded in the Director ledger and its item resolves `Voided(Revalidation)` when the resolver re-checks the ledger at the slot.

**Why this story**: Replacement timing is the public signature of "someone was killed": its delay distribution must be identical for a killed player, NPC and informant, or the city refill itself reveals who died (GDD CNB-R9 item 3, CPA CP-12). Read GDD CNB-R9 items 2 and 3, F3 and AC-CNB-08, AC-CNB-35.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: a cancelled schedule item resolves `Voided(Revalidation)` by ledger re-check; schedule draws use `AmbientSchedule` counter-as-Attempt; an NPC victim is driven exactly as if the strike had whiffed until its death write (ADR-0016, ADR-0018)
- Forbidden: reading `VictimKind` or any victim identity in the ledger; any kill effect observable before the death write; a Penalty subscription that schedules respawns from Roster events (ADR-0016, ADR-0018, ADR-0019)
- Guardrail: kill handling is O(sockets) inside the Attack slot; inside the 0.5 ms queue line

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-08, AC-CNB-35 and CNB-R9 items 2 and 3, scoped to this story:*

- [x] (AC-CNB-08) GIVEN at least 1,236 simulated kills of player pawns and at least 1,236 of NPC pawns (and of informants), WHEN the kill-to-replacement-entry delay D is measured, THEN for each pair of classes |d| <= 0.1, variance ratio is in [0.8, 1.25] and KS D <= 0.1; the pooled mean is 16 +-2 s and p95 <= 32 s at defaults (F3); no replacement occurs earlier than `CrowdReplacementMinDelaySec`.
- [x] (AC-CNB-35) GIVEN 200 kills at random times, WHEN the ambient schedule is logged, THEN each kill cancels exactly one exit scheduled within the next 60 s (uniform over sockets, chi-square p > 0.01) within LSC R5.7's bounds, an exiting NPC killed inside an alley adds a vacancy without counting a cancel, and a cancelled item resolves `Voided(Revalidation)`.
- [x] `OnPawnKilled` runs inline in the Attack slot with `KillServerTime` equal to the scheduled death-write time; no effect of a kill (vacancy, exit cancellation, reaction) is observable before the death write, and an NPC victim keeps its driver and follower until then.

---

## Implementation Notes

*Derived from ADR-0016 section 6 and GDD CNB-R9:*

- t_k = `GetScheduledDeathServerTime(Victim)` (the strike's `ContactServerTime`, ADR-0018). The kill handling may change only exits scheduled at or after t_k.
- Replacement: bound to the first scheduled entry at or after `t_k + CrowdReplacementMinDelaySec` (10 s); if none has served it by `CrowdReplacementMaxDelaySec` (60 s), the fallback entry is scheduled. F3's delay model assumes the Poisson schedule of story 026.
- Cancel: one exit chosen uniformly among the exits scheduled in the next 60 s at all sockets (`AmbientSchedule` draw), subject to LSC R5.7 bounds; the 2.4/min exit-cancel capacity is the ceiling the 2 kills/min test stays under.
- Ledger input is the kill time only (CNB-R9.3): same vacancy, delay distribution and socket pool for a killed informant, player or NPC.
- Statistical tests use fixed pre-registered seeds; 1,236 samples per class gives 80 % power for the 0.1 effect-size margins.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the replacement-after-kill logic in `Source/StealthGame/Private/StealthGameMode.cpp` (`ScheduleNpcReplacement`, engine-timer based and victim-dependent). Not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 026: the schedule itself
- Story 029 and the Penalty epoch (`production/epics/npc-kill-penalty-informants/EPIC.md`): exile exit slot and informant respawns
- The Assassination epoch: the death write and `GetScheduledDeathServerTime`

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdKillHandling.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdReplacementDelayTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.ReplacementDelayVictimBlind`
  - Given: at least 1,236 simulated kills each of player, NPC and informant pawns
  - When: kill-to-replacement-entry delay D is measured
  - Then: for each class pair |d| <= 0.1, variance ratio in [0.8, 1.25], KS D <= 0.1; pooled mean 16 +-2 s, p95 <= 32 s; none before `CrowdReplacementMinDelaySec`
  - Edge cases: minimum-delay boundary
- **AC-2**: `StealthGame.Crowd.ExitCancelOnKill`
  - Given: 200 kills at random times
  - When: the ambient schedule is logged
  - Then: each kill cancels exactly one exit scheduled in the next 60 s (uniform over sockets, chi-square p > 0.01, within LSC R5.7 bounds); an exiting NPC killed in an alley adds a vacancy without a cancel; a cancelled item resolves `Voided(Revalidation)`
  - Edge cases: no exit within 60 s
- **AC-3**: `StealthGame.Crowd.KillEffectsNotBeforeDeathWrite`
  - Given: scripted kills of player and NPC victims
  - When: `OnPawnKilled` runs in the Attack slot and effects are polled each frame
  - Then: `KillServerTime` equals the scheduled death-write time; no kill effect observable before the death write; an NPC victim keeps its driver until then
  - Edge cases: a kill at the end of a leg

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdReplacementDelayTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Replacement.VictimBlindDelay`, `StealthGame.Crowd.Replacement.ExitCancelOnKill`, `StealthGame.Crowd.Replacement.NothingBeforeDeathWrite`; 2026-10-27)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-026-ambient-schedule-and-emergence.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-038-performance-pass-50-60-character.md`

## Completion Notes
**Completed**: 2026-10-27 (Crowd & NPC Behaviour batch, stories cnb-019..031)
**Criteria**: 3/3 passing headless
**Deferred**: OnPawnKilled wiring in production (the Attack slot and GetScheduledDeathServerTime are the Assassination epic's); FCrowdAmbientSchedule::NoteKill takes the death-write time.
**Deviations**: A cancelled exit keeps its cycle's entry (the kill vacancy takes the exit's place) so entries stay a Poisson process as F3 assumes: D mean 16.3 s, p95 29.1 s (decision log 2026-10-27). AC-CNB-08 kills are spaced about 2 minutes apart (F3's single-vacancy delay; the AC gives no kill rate).
**Test Evidence**: `StealthGame.Crowd.Replacement.VictimBlindDelay`, `StealthGame.Crowd.Replacement.ExitCancelOnKill`, `StealthGame.Crowd.Replacement.NothingBeforeDeathWrite`; full suite and measured lines: `production/qa/evidence/crowd-npc-behaviour-019-031-2026-10-27/`
**Run result**: 3-player PIE on L_Prototype with 50 NPCs: `production/qa/evidence/crowd-npc-behaviour-019-031-2026-10-27/evidence.md`
**Reviewed by**: self-review against the code-review skill (findings in the evidence note)
