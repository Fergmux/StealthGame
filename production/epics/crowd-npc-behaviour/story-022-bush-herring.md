# Story 022: Bush red herring: occupancy-blind shade-seeking with uncapped stays

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-010 = Baseline and Tell Matrix rates as data with calibration counters.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0017 (Concealment: the one `Hide` path, no capacity or occupancy, `BushReenterCooldownSec`, `BushSpotPick`), information-visibility-policy GDD (F5 `P_hide <= k*b`, AC-IVP-25)
**ADR Decision Summary**: NPC bush entry is a real `Hide(Enter/Exit)` through the one Concealment call site, chosen without reading occupancy or player positions; the stay is drawn at entry from an uncapped log-normal (median 15 s, sigma 0.8). If Concealment is absent, bush red herrings are disabled.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Concealment per-viewer presentation is ADR-0017's; the crowd only issues the command and later the exit. NPC concealment hides the NPC from NPC perception through the filter of story 008.

**Why this story**: The bush tell (T5) must have a ceiling of 3/22 (about 0.136) for "seen entering a bush implies player", which holds only at a configured NPC bush rate of at least 0.19 per minute with occupancy-blind choice and an uncapped stay. Read GDD CNB-R7 bush bullet, R16 row T5 and AC-CNB-15 (the advisory dwell comparison is not gated).

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: every `Hide` enters through `UPawnCommandComponent`; the NPC bush choice reads no occupancy, occupants or player positions; the stay is drawn at entry from the uncapped log-normal (ADR-0016, ADR-0017)
- Forbidden: Hide legality, replies or timing reading occupancy, controller kind or team; adding a dwell, capacity, reveal or timeout key (ADR-0017)
- Guardrail: bush selection once per due entry; cost inside `Crowd.Perception`

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-15 and CNB-R7 (bush), scoped to this story:*

- [x] (AC-CNB-15a) GIVEN an NPC-only headless run of at least 120 simulated minutes with fixed seeds, WHEN accepted NPC bush entries (after the chain-gap guard) are counted from the Concealment entry log, THEN the configured accepted rate `60 / CrowdBushIntervalSec` is at least `r_N,req` (0.19/min at r_P 0.3 and 50 NPCs; a deterministic config check) and the measured per-NPC rate lies inside the two-sided 99 % Poisson interval of the configured rate.
- [x] (AC-CNB-15b) Every NPC entry uses the one Concealment call site, the bush chosen for a fixed seed is identical with the bush empty and occupied, NPC stays show no upper truncation (the empirical 99th percentile is within 15 % of the log-normal's), and no NPC re-enters within `BushReenterCooldownSec`.
- [x] The bush chosen is the nearest open-side bush within 10 m of the current leg and reads no occupancy; the stay is drawn at entry from the uncapped log-normal (`CrowdBushDwellMedianSec` 15, `CrowdBushDwellSigma` 0.8).

---

## Implementation Notes

*Derived from GDD CNB-R7 bush bullet and CNB-R16 T5:*

- "Stepping into the shade": when an entry is due the NPC enters the nearest bush with an open side within 10 m of its current leg. Any number of pawns may share a bush; a second entrant tells an observer nothing (Concealment has no capacity check).
- Rate: `CrowdBushIntervalSec` 300 (0.20/min, lowered from 315 by decision D7 to absorb r_P drift); constraint `60 / CrowdBushIntervalSec >= r_N,req`; the config check is deterministic because a measured comparison at 120 simulated minutes is too noisy.
- NPCs obey the same cooldown and exit-side rule as humans; exits go through `Hide(Exit)` on the drawn stay end.
- The advisory comparison of NPC versus logged player dwell (Policy F7) is reported, not gated.
- Concealed NPCs are removed from NPC perception through `ICrowdPerceptionFilter` (story 008); do not duplicate that logic.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the prototype's incidental "NPCs passing through bushes are concealed" behaviour (`Source/StealthGame/Private/StealthBush.cpp` overlap hiding). The production path is `Hide` through the Concealment epic; the prototype overlap logic is not promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The Concealment epic (`production/epics/concealment-bushes/EPIC.md`): the `Hide` path and presentation
- Story 018: the clock and guard
- Story 023: the 600-minute rate gate

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdBushTasks.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdBushHerringTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.NpcBushParity` (config and rate)
  - Given: an NPC-only headless run of at least 120 simulated minutes, fixed seeds
  - When: the config is checked and entries are counted from the Concealment log
  - Then: `60 / CrowdBushIntervalSec` >= 0.19/min; the measured per-NPC rate inside the two-sided 99 % Poisson interval of the configured rate
  - Edge cases: chain-gap-discarded entries excluded; the config check is deterministic
- **AC-2**: `StealthGame.Crowd.NpcBushParity` (call site and stays)
  - Given: the same run, plus a fixed-seed scene with the bush empty and occupied
  - When: entries, choices, stays and re-entries are analysed
  - Then: one Concealment call site; identical bush choice empty versus occupied; stay p99 within 15 % of the log-normal's; no re-entry within `BushReenterCooldownSec`
  - Edge cases: stays longer than any old cap are retained
- **AC-3**: `StealthGame.Crowd.BushChoiceOccupancyBlind`
  - Given: an NPC on a leg with open-side bushes at various distances, including beyond 10 m
  - When: the bush is chosen and occupancy is toggled
  - Then: the nearest open-side bush within 10 m of the current leg is chosen; no occupancy is read; the stay is drawn at entry from the uncapped log-normal
  - Edge cases: none within 10 m means no entry; tied distances

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdBushHerringTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Bush.RateAndStays`, `StealthGame.Crowd.Bush.OccupancyBlind`, `StealthGame.Crowd.Bush.NearestWithinTenM`; 2026-10-27)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-018-tell-sampler-and-calibration-counters.md`, `production/epics/crowd-npc-behaviour/story-008-spatial-hash-and-perception-filter.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-023-tell-calibration-gate.md`

## Completion Notes
**Completed**: 2026-10-27 (Crowd & NPC Behaviour batch, stories cnb-019..031)
**Criteria**: 3/3 passing
**Deviations**: The Concealment entry log and the one Hide call site are the sim stand-ins (the Concealment epic is not built); the production Director runs no bush clock until Concealment registers its filter and bushes (the GDD rule: bush herrings disabled without Concealment). BushReenterCooldownSec 6 s is read as a pending owner key.
**Test Evidence**: `StealthGame.Crowd.Bush.RateAndStays`, `StealthGame.Crowd.Bush.OccupancyBlind`, `StealthGame.Crowd.Bush.NearestWithinTenM`; full suite and measured lines: `production/qa/evidence/crowd-npc-behaviour-019-031-2026-10-27/`
**Run result**: 3-player PIE on L_Prototype with 50 NPCs: `production/qa/evidence/crowd-npc-behaviour-019-031-2026-10-27/evidence.md`
**Reviewed by**: self-review against the code-review skill (findings in the evidence note)
