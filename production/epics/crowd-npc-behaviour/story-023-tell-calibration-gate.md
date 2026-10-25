# Story 023: Tell Matrix calibration gate: 600-minute rates and the evidence-compounds property test

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-010 = Baseline and Tell Matrix rates as data with calibration counters.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0023 (telemetry counters and statistics), ADR-0008 (harness for statistical gates)
**ADR Decision Summary**: Rates are data from the frozen tuning snapshot with calibration counters; accepted NPC tell rates must equal F2's targets (through compensated draw rates), and no NPC may show two different tells within `CrowdTellChainGapSec`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Headless accelerated simulation with fixed pre-registered seeds; no post-cutoff API. A failing seed is a defect, never reseeded.

**Why this story**: This is the epic's calibration gate for Pillar 2: the red-herring system either produces the intended baseline or the whole tell budget (and the MVP hypothesis) is built on sand. It is a gate story, so it adds no new behaviour. Read GDD AC-CNB-10 and AC-CNB-31, F2 and R16.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: Baseline and Tell Matrix rates come from the frozen snapshot; draws via `FCrowdDraws`; calibration counters server-only (ADR-0016, ADR-0023)
- Forbidden: adapting rates at runtime from observed behaviour (steps re-derived offline per playtest build, capped at 25 %); reseeding a failing seed (ADR-0016, GDD test rules)
- Guardrail: the 600-minute runs are accelerated headless and must complete within the nightly CI budget

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-10 and AC-CNB-31, scoped to this story:*

- [ ] (AC-CNB-10) GIVEN 600 simulated minutes of the reference scene (headless, accelerated, fixed pre-registered seeds, no strikes so R8 adds nothing), WHEN accepted NPC runs, vaults, climbs, braces, decoy tends, bush entries, reversals and long head holds (after the chain-gap guard) are counted, THEN each count is above 0 and each city-wide rate lies within the two-sided 99 % Poisson interval of F2's accepted default (decoy tends: expected about 137 events).
- [ ] (AC-CNB-31a) GIVEN 600 simulated minutes with scripted strikes every 30 s, WHEN every NPC's accepted tells (T1-T5, T12, T13, R8 panic runs and startle braces included) are logged, THEN no NPC shows two different tells within `CrowdTellChainGapSec`.
- [ ] (AC-CNB-31b) The gap times between an NPC's consecutive different tells show no excess just after `CrowdTellChainGapSec` (the count in [gap, gap + 5 s] is within the 99 % Poisson interval of the count expected from the restarted exponential clocks); the resulting posterior for "two different tells within the gap" is reported as a playtest metric, not gated (it depends on the measured r_P, OQ6).

---

## Implementation Notes

*Derived from GDD AC-CNB-10, AC-CNB-31 and F2:*

- The harness reads the calibration counters of story 018 and the Concealment, IIF and Objectives logs; it does not add its own instrumentation inside Crowd code.
- Pre-register the seed list in the test fixture before the first run; record it in the evidence note.
- Interval for each rate: two-sided 99 % Poisson interval around `NPCs * minutes * accepted rate`. Decoy tends are expected at about 137 events over 600 min x 50 NPCs at r_N 0.0046.
- AC-CNB-31b's reported posterior goes into the evidence note for the playtest metric; it is not an assertion.
- This story only turns green once stories 016, 019, 020, 021, 022 and 024 are done; it is the integration point of the herring set.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Stories 019-022 and 024: the behaviours under test
- Story 018: the sampler and counters
- Story 035: the classifier gate on human data

---

## Files

- `Source/StealthTestHarness/Private/CrowdTellCalibrationTest.cpp`
- `Source/StealthTestHarness/Private/CrowdEvidenceCompoundsTest.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/Fixtures/CrowdCalibrationSeeds.json`
- `production/qa/evidence/crowd-tell-calibration-<date>.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.TellBaselineNonZero`
  - Given: 600 sim min of the reference scene, no strikes, pre-registered seeds
  - When: runs, vaults, climbs, braces, decoy tends, bush entries, reversals and long head holds are counted
  - Then: each count above 0 and each city-wide rate in the two-sided 99 % Poisson interval of F2's accepted default
  - Edge cases: decoy tends expect about 137 events; never reseed
- **AC-2**: `StealthGame.Crowd.EvidenceCompounds` (chain gap)
  - Given: 600 sim min with a scripted strike every 30 s
  - When: every NPC's accepted tells are logged (T1-T5, T12, T13, R8 panic runs, startle braces)
  - Then: no NPC shows two different tells within `CrowdTellChainGapSec`
  - Edge cases: the same tell repeating is allowed; a tell at exactly the gap
- **AC-3**: `StealthGame.Crowd.EvidenceCompounds` (no excess after the gap)
  - Given: the same log
  - When: gap times between consecutive different tells are tabulated
  - Then: the count in [gap, gap + 5 s] lies in the 99 % Poisson interval of the restarted-clock expectation; the posterior for two tells within the gap is reported, not gated
  - Edge cases: low counts; the report is always emitted

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthTestHarness/Private/CrowdTellCalibrationTest.cpp` and `CrowdEvidenceCompoundsTest.cpp` — must exist and pass; the evidence note `production/qa/evidence/crowd-tell-calibration-<date>.md` records seeds and the reported posterior

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-016-gaze-catalogue.md`, `production/epics/crowd-npc-behaviour/story-019-run-traversal-reversal-herrings.md`, `production/epics/crowd-npc-behaviour/story-020-decoy-tending-herring.md`, `production/epics/crowd-npc-behaviour/story-021-brace-and-startle-herring.md`, `production/epics/crowd-npc-behaviour/story-022-bush-herring.md`, `production/epics/crowd-npc-behaviour/story-024-strike-stimulus-and-reactions.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-035-classifier-gate.md`, `production/epics/crowd-npc-behaviour/story-038-performance-pass-50-60-character.md`
