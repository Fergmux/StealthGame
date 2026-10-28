# Story 023: Playtest advisory metrics (pacing, exposure, tend share)

> **Epic**: Objectives Chain
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-28

## Context

**GDD**: `design/gdd/objectives-chain.md` (F1 chain duration, F2 exposure per stage, F3 tend posterior, Open Questions 4, 7, 9 and 10; AC-OC-19, AC-OC-20, AC-OC-21 playtest part)
**Requirement**: `TR-oc-007` (stall pressure is validated by measurement: F5 and the playtest metrics, never by a rule)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision section 5.4 `TendCompleted`; Validation Criteria)
**Other ADRs**: ADR-0023: Playtest telemetry hooks (the sinks that carry these logs)
**ADR Decision Summary**: All Objectives log entries go through the Round Event Log `Append`, server-only; the playtest metrics read those entries after the fact, so no gameplay path depends on them and none is a rule. The calibration inputs (kappa, phi, o, q, r, t_settle, N_probe, n_full, T_round) are provisional until logged leg times exist (GDD Open Question 4).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (offline log analysis; no engine API)
**Engine Notes**: None. The analyser is an offline tool or commandlet over server logs; its numbers are only trusted if it has a small T1 test with a fixture log (the QA-lead's condition), otherwise the report is unverified.

**Control Manifest Rules (this layer)**:
- Required: Objectives Event Log entries go through the Round Event Log `Append` only; never `UE_LOG`; never record telemetry client-side — source: ADR-0020, ADR-0023
- Forbidden: Never make a measured result a rule (no timer, reveal, ping or marker as a stall response) — source: GDD SP-7
- Forbidden: Never record telemetry client-side — source: ADR-0023
- Guardrail: none (offline)

The three metrics are advisory: they say whether the numbers the design assumed hold in play. F1 says whether objective-ended rounds take as long as modelled (about 9 minutes median; below 6 triggers the pacing levers, in the GDD's order: raise the LSC chain target toward 220 m, then `ObjectivesPerStageCount` 2, then a vaguer clue). F2 says whether the tell a tender gives (the pose, the 3 s) matches the design (`E_tell` about 4 s) and counts the wrong-court probes. F3 says whether a seen full-length tend is an opponent player about 70 percent of the time (band 0.65-0.75), reporting early releases and counted-versus-feint tends separately. The pass condition is the evidence file, not a build gate: these cannot run until the chain, Round, Crowd and the graybox exist and enough rounds have been played.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-19, AC-OC-20, AC-OC-21 playtest part), scoped to this story. All three are Playtest/ADVISORY in the GDD; the analyser carries a small T1 fixture test (QA-lead condition):*

- [x] **AC1** (AC-OC-19 [Playtest/ADVISORY] F1 pacing): **GIVEN** >= 20 objective-ended graybox rounds **THEN** the median `ObjectiveWin` duration and the per-team leg times are logged next to `T_chain` (L of the drawn combo); a median below 6 min triggers the F1 pacing levers; fewer than 20 rounds reports NOT ENOUGH DATA; a median of exactly 6 min is not flagged.
- [x] **AC2** (AC-OC-20 [Playtest/ADVISORY] F2 tell timing): **GIVEN** the server log of >= 10 graybox rounds **THEN** for each counted completion the time from the tender's last stop (speed below the walk threshold) to `Complete` is reported, the mean is within `E_tell` +- 1 s (target 4 s; 3.0 s and 5.0 s are in band, 5.01 is not), and probes (early releases after `TendCueDelaySec` at non-live courts) are counted per stage as `N_probe`; rounds with no counted completion are skipped.
- [x] **AC3** (AC-OC-21 [Playtest part] F3 measured share): **GIVEN** >= 60 internal graybox rounds, or an accelerated simulation of the same tend volume (labelled as simulation), **THEN** the measured share of opponent-player tends is reported separately for full-length tends (banded 0.65-0.75), early releases (`P_short`, alarm above 0.90) and, among player tends, counted versus feint (`TendCompleted` kind); `P_full` above 0.75 caused by heavy feinting is recorded as a chosen tell, not a defect; the report is saved to `production/qa/evidence/oc-playtest-metrics.md`.

---

## Implementation Notes

*Derived from the GDD formulas F1-F3 and Open Questions:*

Analyser inputs: the server Round Event Log entries (`ObjectiveStage`, `FinalCompleted`, `TendCompleted`, `StageSignalReleased`, `ClueTooBroad`) plus movement samples sufficient to find a tender's last stop (speed below the walk threshold) for F2, and the layout record (chain length L of the drawn combo, `T_chain` from F1 with the provisional inputs). Compute F1's `T_chain` for comparison (L 170 gives 402 s with the provisional inputs, a median of about 9 minutes with 2 minutes of final contest); compare the measured medians and the per-team leg times. The levers on a short median are pacing levers, not the SP-7 stall levers (the GDD says so explicitly: SP-7's order does not apply to pacing).

F2: `E_tell = t_settle + D` (default 4 s), `E_court` 9 s default; report the measured mean time from last stop to Complete per counted completion (within `E_tell` +- 1 s) and `N_probe` per stage (about 1 per stage, 3 per chain, provisional). F3: reuse story 017's `P_full`/`P_short` functions and the Crowd rate read from the registry; report full-length, early release (cancels plus probes) and counted-versus-feint separately. Heavy feinting pushing `P_full` above 0.75 is a chosen tell and not a defect. Accelerated simulation runs are labelled as simulation in the report so they are never confused with playtests.

The analyser itself has a T1 test with a fixture log (the QA-lead's condition) under `Source/StealthGameplay/Private/Tests/Objectives/`; the playtest evidence is the report file. Execution waits on the first playable graybox rounds (this story can be started, but the evidence closes only after enough rounds exist; the dependency list names what must exist first), and the calibration spike (the epic's "2-week calibration spike") feeds Open Question 4.

Prototype replaced: none. The first-playable report (`production/first-playable-report.md`) holds the prototype's only timing evidence and is a reference, not a baseline.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-017-stall-metrics-and-tend-posterior.md`: the pure F3 and F5 functions this story reuses.
- Telemetry (ADR-0023) epic: the sinks and detectors.
- Crowd epic: adjusting the early-release share if `P_short` exceeds 0.90 (GDD Open Question 10).
- LSC epic: raising the chain length target if the median is short.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Features/Objectives/Private/ObjectivesPlaytestAnalyser.h / .cpp` (F1 pacing, F2 tell timing and probes, F3 shares, markdown report)
- `Source/StealthGameplay/Private/Tests/Objectives/PlaytestAnalyserFixtureTest.cpp`
- `production/qa/evidence/oc-playtest-metrics.md` (record only: NOT ENOUGH DATA)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; all three ACs are Playtest/ADVISORY per the GDD, so the pass condition is the evidence file, not a build gate, and the analyser needs a small fixture test). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (F1 pacing metrics): median duration and leg times
  - Given: server logs of >= 20 objective-ended graybox rounds; `T_chain` for the drawn combo; analyser with a fixture log
  - When: the analyser runs
  - Then: median `ObjectiveWin` duration and per-team leg times are reported beside `T_chain`; a median below 6 min flags the F1 pacing levers
  - Edge cases: fewer than 20 rounds reports NOT ENOUGH DATA; median exactly 6 min is not flagged.
- **AC-2** (F2 tell timing): last stop to Complete and probes
  - Given: server logs of >= 10 graybox rounds
  - When: the analyser runs
  - Then: per counted completion the time from the tender's last stop to Complete is reported; the mean is within `E_tell` +- 1 s (target 4 s); `N_probe` (early releases after `TendCueDelaySec` at non-live courts) is counted per stage
  - Edge cases: no counted completion in a round is skipped; mean at 3.0 s and 5.0 s are in band, 5.01 is not.
- **AC-3** (F3 measured share): tend-share report
  - Given: >= 60 internal graybox rounds or an accelerated simulation of the same tend volume
  - When: the analyser runs
  - Then: opponent-player tend share is reported separately for full-length tends (band 0.65-0.75), early releases (`P_short`, alarm above 0.90) and, among player tends, counted vs feint (`TendCompleted` kind); the report is saved to `production/qa/evidence/oc-playtest-metrics.md`
  - Edge cases: `P_full` above 0.75 from heavy feinting is recorded as a chosen tell, not a defect; simulation runs labelled as such.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration (playtest, ADVISORY): `production/qa/evidence/oc-playtest-metrics.md` with the AC1-AC3 numbers; the analyser's fixture test under `Source/StealthGameplay/Private/Tests/Objectives/` must exist and pass

**Status**: [x] Created and passing (2026-10-28): StealthGame.Objectives.PlaytestAnalyserFixture; evidence production/qa/evidence/objectives-chain-013-023-2026-10-28/

Run result: N/A - an offline analyser; the evidence closes with human graybox playtests

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-017-stall-metrics-and-tend-posterior.md`, `production/epics/objectives-chain/story-015-rival-signal-release-scheduling.md`, `production/epics/objectives-chain/story-021-pie2-end-to-end-cacao-chain.md`; cross-epic: round-match-framework (Event Log), crowd-npc-behaviour (decoys; the F3 band needs them), level-socket-contract and procedural-city-generator (the graybox rounds); the first internal playtests
- Unlocks: None

## Completion Notes
**Completed**: 2026-10-28
**Criteria**: the analyser and its fixture test pass; AC1-AC3 are Playtest/ADVISORY and recorded as NOT ENOUGH DATA (0 graybox playtest rounds exist; human playtesters are a listed human item).
**Deviations**: none.
**Test Evidence**: production/qa/evidence/objectives-chain-013-023-2026-10-28/
**Reviewed by**: self code review 2026-10-28 (notes in the evidence)
