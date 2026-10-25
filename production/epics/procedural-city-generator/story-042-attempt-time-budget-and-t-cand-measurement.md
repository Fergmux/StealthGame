# Story 042: Attempt time budget and t_cand measurement

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: spike R4 (server spawn + navmesh build time on the listen host, `level-socket-contract/story-011-spike-r4-graybox-navmesh-timing.md` plus the generator-scale run in story 043) has not reported; `t_spawn + t_nav <= 3.0 s` (F5) and the attempt budget are provisional (dagger) until it does. The standalone `t_cand` measurement (AC2, GDD Open Question 10) needs no spike but is released with the story.
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Attempt time `T_attempt = j_used x t_cand + t_spawn + t_nav + t_val + t_sel <= LayoutAttemptTimeoutSec` (7 s) and `T_total <= LayoutTotalBudgetSec` (45 s); the Generator worker takes <= 1 s per attempt off the game thread, kit spawn + collision + navmesh <= 3.0 s wall time, Validate + Select <= about 1.5 s in <= 4 ms game-thread slices.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014)
**Engine Notes**: Measured on the ADR-0022 reference listen-server host (name the machine in the report); `t_cand` is also measured in a standalone harness (no world) so it isolates the pure pipeline. Percentiles are nearest-rank (the 48th of 50 for p95).

**Control Manifest Rules (this layer)**:
- Required: Layout (server, `Preparing`): game-thread frames <= 33 ms; Generator worker <= 1 s per attempt; <= 45 s worst case including reseeds — source: ADR-0014
- Forbidden: Never adapt J, N or any budget to the host (a slow host must fail over to the fallback, not publish a different city) — source: ADR-0014, GDD R-G10
- Guardrail: `t_cand` <= 0.12 s is an unmeasured assumption until AC2 runs

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G30, F5, Open Question 10), scoped to this story:*

- [ ] **AC1** (AC-G30, `Formula.AttemptTimeBudget`, provisional): **GIVEN** 50 seeds on the reference listen-server host **THEN** no attempt exceeds 7 s and server time from `RequestLayout` to `OnLayoutReady` is <= 6 s median and <= 10 s p95.
- [ ] **AC2** (OQ 10, `Formula.CandidateTimeMeasured`): **GIVEN** a standalone harness on the ADR-0022 reference PC and a 50-seed batch counting all candidates **THEN** t_cand (S1-S12 plus G-PRE) median and p95 are recorded and the host named; the 0.12 s assumption holds iff the median is <= 0.12 s; if not, the report sets J and the work-unit cap as hard limits and gives the recomputed F5 worst case.

---

## Implementation Notes

*Derived from GDD F5, R-G9 and the QL-STORY-READY review:*

F5 worst case: 8 x 0.12 + 3.0 (spawn + nav, R4) + 1.5 + 0.3 = 5.76 s <= 7 s (1.24 s slack); if R4 reports only `t_nav` <= 3.0 with spawn on top, the worst case is 6.76 s (0.24 s slack) and J and `t_cand` become hard limits. Expected about 4.1 s (`j_used` about 1/p_g about 3.3). AC1: Gauntlet T3 on the reference listen-server host (`StealthGame.CityGen.MP.AttemptTimeBudget`), 50 seeds, per-attempt duration and `RequestLayout` -> `OnLayoutReady`; define whether a seed that falls back counts toward the percentiles (take: it counts at its elapsed time and is also reported separately). AC2: the standalone harness times every candidate (accepted and rejected) for 50 seeds; "exactly 0.12 s holds"; the report must include the recomputed F5 worst case when the assumption fails, and a missing section when over 0.12 s fails the test. AC-G30's p95 <= 10 s also needs `p` >= about 0.78 (two attempts cover 96%); at the LSC floor `p` = 0.565 the p95 is about 16 s: record `p` from story 037/039 beside the result. The work-unit cap (story 023) is set or confirmed from the AC2 numbers.

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-043-kit-gpu-cost-and-server-spawn-nav-time.md`: the spawn + nav and GPU measurements.
- `story-041-client-build-time-and-first-frame-completeness.md`: client side.
- `story-023-candidate-loop-work-unit-cap-and-metrics.md`: the cap mechanism.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/CityGen/AttemptTimeBudgetMpTest.cpp` (Gauntlet node)
- `Source/StealthTestHarness/Private/CityGen/CandidateTimeHarness.cpp` (standalone t_cand harness)
- `production/qa/evidence/citygen/attempt-time-and-tcand-[date].md` (report)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 ADEQUATE; AC2 revised and ADEQUATE; AC1 DEFERRED until the reference host exists).*

- **AC1** `StealthGame.CityGen.MP.AttemptTimeBudget`
  - Given: the reference listen-server host, 50 seeds, provisional.
  - When: per-attempt duration and `RequestLayout` to `OnLayoutReady` are recorded.
  - Then: no attempt exceeds 7 s. The median is <= 6 s and p95 <= 10 s (nearest-rank, the 48th of 50).
  - Edge cases: define whether a seed that falls back counts toward the percentiles.
- **AC2** `Formula.CandidateTimeMeasured`
  - Given: a standalone harness on the ADR-0022 reference PC, 50 seeds, every candidate timed (S1-S12 plus G-PRE).
  - When: median and p95 of t_cand are computed.
  - Then: the report records the median and p95 and names the host. If median <= 0.12 s, the assumption holds. If not, the report states that J and the work-unit cap are hard limits and gives the recomputed F5 worst case. A missing section when over 0.12 s fails the test.
  - Edge cases: exactly 0.12 s holds. Count rejected candidates too. p95 uses nearest-rank.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the Gauntlet test, the harness test and the committed report OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-023-candidate-loop-work-unit-cap-and-metrics.md`, `story-026-server-kit-spawn-from-manifest.md`, `story-039-g-pre-false-pass-rate-and-guard-band-sign.md`; spike report: `level-socket-contract/story-011-spike-r4-graybox-navmesh-timing.md`
- Unlocks: `story-046-ten-connection-gauntlet-integration-pass.md`
