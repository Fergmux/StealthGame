# Story 017: Stall metrics (F5) and the tend posterior band (F3)

> **Epic**: Objectives Chain
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-28

## Context

**GDD**: `design/gdd/objectives-chain.md` (F3 tend posterior, F5 stall metrics, SP-6, SP-7; Edge Cases; AC-OC-21 unit part, AC-OC-23)
**Requirement**: `TR-oc-007` (stall pressure: reporting only, never a rule), `TR-oc-004` (F3's tend counts depend on the `TendCompleted` entries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision section 5.4 `TendCompleted`; Validation Criteria)
**Other ADRs**: ADR-0023: Playtest telemetry hooks (the F5 fields read from the Round Event Log)
**ADR Decision Summary**: Objectives writes server-only Event Log entries (`ObjectiveStage`, `FinalCompleted`, `TendCompleted`, `StageSignalReleased`, `ClueTooBroad`) through the Round Event Log `Append` only; F5 and F3 are computed from them as reporting metrics. F5 never changes a rule: a mutual stall is an Objectives defect answered only by the SP-7 levers.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (pure arithmetic over a log; no engine API)
**Engine Notes**: None. The functions take log records and tuning values as arguments (Crowd's `CrowdDecoyTendIntervalSec` through a stub registry in the test, the real registry in the product), so no gameplay value is hardcoded.

**Control Manifest Rules (this layer)**:
- Required: Objectives Event Log entries go through the Round Event Log `Append` only; none goes through `UE_LOG` — source: ADR-0020, Round R19
- Forbidden: Never make F5 a rule (never a timer, a reveal, a ping or a marker; SP-7) — source: ADR-0020, GDD
- Forbidden: Never hardcode Crowd's decoy rate; read the registered key — source: GDD F3, coding standards (data-driven values)
- Guardrail: metrics are computed offline or at round end, not per frame — source: ADR-0023

F3 and F5 are the two reporting formulas the design leans on to say whether the mechanism works. F5 says when a round stalled (`G_mutual` the longest gap between counted completions of either team; `G_team(T)` the same over one team's completions, reported but not a defect on its own; `FinalDwell` the time from the first team reaching its final stage to the round end) and flags a defect against `ObjectiveStallGapDefectSec` (300) and `ObjectiveFinalDwellDefectSec` (240). F3 says how often a seen full-length tend is an opponent player rather than a decoy NPC: `P_full = R_full / (R_full + N_npc * r_N * c)`, which the design holds in the band 0.65-0.75 (target 0.70) so a tend is evidence, never proof (Pillar 1). The band depends on Crowd's rate, so a regression in Crowd's interval must fail this story's test.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-23, AC-OC-21 unit part, F5 note on cross-review C-W35), scoped to this story. QL-STORY-READY revised AC1 (thresholds stated) and AC2 (formula and source):*

- [x] **AC1** (AC-OC-23 [U] `StallMetrics`): with `ObjectiveStallGapDefectSec` = 300 and `ObjectiveFinalDwellDefectSec` = 240 (the Tuning Knobs defaults, read from the registry stub), **GIVEN** fixture logs (team 0: P1 95, P2 260, final 420; team 1: P1 180, P2 330) **THEN** `G_mutual` 95, `G_team(0)` 165, `G_team(1)` 180, `FinalDwell` 160 and no defect; **GIVEN** no completion and an elimination at 400 s **THEN** `StallDefect` (`G_mutual` 400 > 300).
- [x] **AC2** (AC-OC-21 [U] `TendPosteriorBand`): **GIVEN** `R_full` 0.40 (F1 `T_round` 9 min), `NpcCount` 50, completion share `c` 0.75 and r_N 0.0046 **THEN** `P_full = R_full / (R_full + N_npc * r_N * c)` is in [0.65, 0.75] and stays there for `T_round` 7 and 11 min; **GIVEN** Crowd's registered `CrowdDecoyTendIntervalSec` (13,125 s, read through a stub registry, not hardcoded) **THEN** the computed `P_full` is in band, and a regression to 18,750 s gives 0.77 and fails (negative control).
- [x] **AC3** (F5 note, cross-review C-W35 [U] `RejectedFinalNotCounted`): a final `Complete` rejected by the latch is logged `TendCompleted(..., NoEffect)` and counts in tend statistics (two such completions count twice), not as an extra `FinalCompleted` row, and does not move `t_end` or `FinalDwell`.

---

## Implementation Notes

*Derived from the GDD formulas F3 and F5 and ADR-0020 section 5.4:*

F5, as stated: `G_mutual` = max gap between consecutive counted completions of either team with t = 0 (`Active` start) and `t_end` included; `G_team(T)` the same over team T's completions only; `FinalDwell = t_end - min_T(t_P2,T)`; `StallDefect = StallFlag or (G_mutual > ObjectiveStallGapDefectSec) or (FinalDwell > ObjectiveFinalDwellDefectSec)`, where `StallFlag` is Round's F7 long-round flag. The fixture's figures are internally consistent (team 0's longest gap including the start is 165; the merged completions give 95, 85, 80, 70, 90 and the largest is 95; `FinalDwell` = 420 - 260 = 160). A team with no completion uses the start-to-end gap; a gap exactly at the threshold follows the knob semantics (strictly greater than is a defect).

F3: `P_full = R_full / (R_full + N_npc * r_N * c)`, inverse `r_N = R_full * (1 - P_full) / (N_npc * c * P_full)`; `R_full = n_full / T_round` (n_full 3.6, `T_round` 9 min gives 0.40/min); `r_N` is Crowd's `CrowdDecoyTendIntervalSec` converted (13,125 s gives 0.0046 decoy tends per NPC per minute at 50 NPCs); the band edges are inclusive. `P_short` (early releases) is reported on its own line and never banded (alarm above 0.90, Open Question 10). Boundaries from the GDD: `T_round` 7 min gives `R_full` 0.51 and `P_full` 0.75; 11 min gives 0.33 and 0.66.

The log fields come from the Round Event Log: `ObjectiveStage`, `FinalCompleted`, `TendCompleted(ParticipantId, Court, Counted|NoEffect)` (story 008), `StageSignalReleased`. A final `Complete` rejected by the latch is logged as `TendCompleted(..., NoEffect)` (story 009) and counts in tend statistics only: it is not an extra `FinalCompleted` row and does not move `t_end` or `FinalDwell` (cross-review C-W35). These functions are pure, so they can run in the playtest analyser (story 023) and in the telemetry epic without a world.

Prototype replaced: none (the prototype logged nothing of this kind).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-023-playtest-advisory-metrics.md`: the measured playtest reports (AC-OC-19/20/21 playtest parts).
- `story-009-final-completion-and-win-declaration.md`: producing the latch-rejected `TendCompleted` entry.
- Crowd epic: the decoy rate and its tuning key.
- Telemetry (ADR-0023): the sinks that consume these metrics.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Features/Objectives/Private/ObjectivesMetrics.h / .cpp` (F5 ComputeStall, StallInputFromLog, CountTends; F3 FullTendPosterior, DecoyRatePerNpcPerMinute, inverse, band)
- `Source/StealthGameplay/Private/Tests/Objectives/StallMetricsTest.cpp, TendPosteriorBandTest.cpp, RejectedFinalNotCountedTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS; the AC1 thresholds and the AC2 formula and source were applied above and the specs cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`StallMetrics`): gap and dwell metrics
  - Given: fixture logs team 0 (P1 95, P2 260, final 420), team 1 (P1 180, P2 330); stub defect thresholds (300 and 240)
  - When: F5 metrics are computed; second case no completion with an elimination at 400 s
  - Then: `G_mutual` 95, `G_team(0)` 165, `G_team(1)` 180, `FinalDwell` 160, no defect; second case `StallDefect`
  - Edge cases: team with no completion uses the start-to-end gap; a gap exactly at the defect threshold (boundary per knob semantics).
- **AC-2** (`TendPosteriorBand`): `P_full` band
  - Given: `R_full` 0.40 at `T_round` 9 min, `NpcCount` 50, completion share 0.75, r_N 0.0046; Crowd stub registry giving `CrowdDecoyTendIntervalSec` 13,125
  - When: `P_full` is computed at `T_round` 9, 7, 11; then with the interval 18,750
  - Then: `P_full` in [0.65, 0.75] for all three durations; at 18,750 the result is 0.77 and the band check fails (negative control)
  - Edge cases: band edges 0.65 and 0.75 inclusive; the value read from the live registry changes the result.
- **AC-3** (`RejectedFinalNotCounted`): latch-rejected final accounting
  - Given: a final Complete rejected by the latch; log capture
  - When: stall and tend statistics are computed
  - Then: `TendCompleted(..., NoEffect)` is logged and counted in tend statistics; no extra `FinalCompleted` row; `t_end` and `FinalDwell` unchanged
  - Edge cases: two rejected completions count twice in tend stats.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [x] Created and passing (2026-10-28): StealthGame.Objectives.StallMetrics, TendPosteriorBand, RejectedFinalNotCounted; evidence production/qa/evidence/objectives-chain-013-023-2026-10-28/

Run result: N/A - pure reporting metrics (no player-visible behaviour)

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md`, `production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md`, `production/epics/objectives-chain/story-009-final-completion-and-win-declaration.md`; cross-epic: crowd-npc-behaviour (`CrowdDecoyTendIntervalSec` registered key; stories being written in parallel), round-match-framework (Event Log and F7 `StallFlag`)
- Unlocks: `production/epics/objectives-chain/story-021-pie2-end-to-end-cacao-chain.md` (AC3 excludes 1v0 rounds from F5), `production/epics/objectives-chain/story-023-playtest-advisory-metrics.md`

## Completion Notes
**Completed**: 2026-10-28
**Criteria**: AC1-AC3 passing (fixture: G_mutual 95, G_team 165 / 180, FinalDwell 160; idle 400 s round StallDefect; P_full 0.700 at the registered 13,125 s, 0.769 at 18,750 s fails; T_round 7 / 9 / 11 min give 0.749 / 0.699 / 0.655).
**Deviations**: debug-flagged entries are left out of F5 (R22); rounds flagged by ObjectivePacing(Code 1) are excluded (story 021 AC3).
**Test Evidence**: production/qa/evidence/objectives-chain-013-023-2026-10-28/
**Reviewed by**: self code review 2026-10-28 (notes in the evidence)
