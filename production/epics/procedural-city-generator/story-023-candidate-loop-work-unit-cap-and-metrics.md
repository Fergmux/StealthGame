# Story 023: Candidate loop, work-unit cap and attempt metrics

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Within LSC attempt k the Generator draws up to J = 8 candidates, each running S1-S12 as data then G-PRE; the loop ends by J or a deterministic work-unit cap and **never by wall-clock**, so a slow host cannot publish a different city. Wall-clock bounds only the whole attempt (LSC `LayoutAttemptTimeoutSec`).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain C++)
**Engine Notes**: No engine API. The 3x-slowdown test uses a test seam that scales a counted work budget, never a real `Sleep` (a timing-based test would itself be wall-clock dependent).

**Control Manifest Rules (this layer)**:
- Required: Generator worker <= 1 s per attempt off the game thread (J <= 8) — source: ADR-0014
- Forbidden: Never bound the Generator candidate loop by wall clock; never time-slice the Generator on the game thread; wall-clock may only defer work, never change a result — source: ADR-0014
- Guardrail: all candidates of an attempt <= 1 s on the worker; E[j_used] about 3.3 at `p_g` 0.30 (F5)

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G9, R-G5.1/4/5/6), scoped to this story:*

- [ ] **AC1** (AC-G9, `Determinism.CandidateLoopNotClockBound`): **GIVEN** a golden seed **WHEN** the run repeats with an injected 3x slowdown of every stage **THEN** the accepted candidate index and `LayoutHash` are unchanged.
- [ ] **AC2** (R-G5.1/4/5, `Loop.CandidatesAndCap`): **GIVEN** `CityGenCandidateDrawsCount` 8 **THEN** candidates j = 0..7 each draw their own N and area mix, the first passing candidate is spawned, the loop ends at J or at the work-unit cap (a constant of `GeneratorKitVersion`, set from spike R4 / story 042, a generous placeholder until then), reaching either with no pass ends as `GPreExhausted` with 0 instances spawned, and a static check finds no clock API reachable from the loop.
- [ ] **AC3** (R-G5.6, `Loop.MetricsSnapshot`): **GIVEN** an attempt **THEN** `CandidateIndex`, `j_used`, stage timings and G-PRE failure ids are written into `FLayoutAttemptRecord.MetricsSnapshot` (server-only) and never into the Manifest or `LogStealthLayout`.

---

## Implementation Notes

*Derived from GDD R-G5 and the QL-STORY-READY review:*

Each candidate j runs S1-S12 as data with `StageSeed(s, j)` streams (story 003) then G-PRE (stories 020-022) and, on pass, emission (story 019). Candidate independence: each draws its own N and area mix. The work-unit cap: the GDD says "a deterministic work-unit cap" without a key or value, and R-G5.2 puts candidate count, thresholds, guard band and collision strategy into `LayoutParams` or `GeneratorKitVersion`; this story makes the cap a constant of `GeneratorKitVersion` (so it enters `LayoutHash`) and defines a work unit in the dev-story notes (proposal: one BFS expansion or one DDA ray step, counted by the backend, checked at stage boundaries and inside G-PRE sweeps). The placeholder is generous until spike R4 and story 042 measure `t_cand`; reaching the cap is deterministic across hosts because the counter, not the clock, drives it. Reaching J or the cap with no pass ends `GPreExhausted`. `MetricsSnapshot` carries `CandidateIndex`, `j_used`, per-stage timings (the only wall-clock values, never fed back into any decision, never in the hash) and G-PRE failure ids; server-only, never replicated, never logged by name to `LogStealthLayout` (control manifest: attempt records and failed predicate ids may be logged, so confirm the allowed set before asserting "never": the AC says the metric names and failure ids stay out of `LogStealthLayout` for this snapshot's wall-clock timings; take the stricter reading and flag it).

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-020-g-pre-grid-measure-backend-and-guard-bands.md`, `story-021-g-pre-predicates-a.md`, `story-022-g-pre-predicates-b-and-role-counts.md`: the G-PRE verdicts the loop consumes.
- `story-024-worker-task-cancellation-and-state-machine.md`: the task wrapper.
- `story-025-failure-modes-and-teardown.md`: failure code semantics.
- `story-042-attempt-time-budget-and-t-cand-measurement.md`: measuring `t_cand` and setting the cap.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenCandidateLoop.h`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenCandidateLoop.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/CandidateLoopNotClockBoundTest.cpp` (class `FCityGenDeterminismCandidateLoopNotClockBoundTest`, name `StealthGame.CityGen.Determinism.CandidateLoopNotClockBound`)
- `Source/StealthGameplay/Private/Tests/CityGen/CandidatesAndCapTest.cpp` (class `FCityGenLoopCandidatesAndCapTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/MetricsSnapshotTest.cpp` (class `FCityGenLoopMetricsSnapshotTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC3 ADEQUATE; AC2 revised and ADEQUATE).*

- **AC1** `Determinism.CandidateLoopNotClockBound`
  - Given: a golden seed.
  - When: it runs once normally and once with a 3x slowdown injected through a test seam in every stage.
  - Then: the accepted candidate index and `LayoutHash` are equal.
  - Edge cases: verify the test fails against a clock-bound loop stub. Do not use real `Sleep` timing.
- **AC2** `Loop.CandidatesAndCap`
  - Given: `CityGenCandidateDrawsCount` 8, and a test seam that injects a small work-unit cap.
  - When: the loop runs.
  - Then: candidates j = 0..7 each draw their own N and area mix. The first passing candidate is spawned. With none passing, or with the injected cap reached first, the attempt ends `GPreExhausted` with 0 instances spawned. The static check finds no clock API reachable from the loop.
  - Edge cases: outcomes are identical across runs, with and without the 3x slowdown. The work-unit counter and its check granularity are named in the dev-story notes. The shipped cap is a placeholder until spike R4 and story 042.
- **AC3** `Loop.MetricsSnapshot`
  - Given: an attempt with at least one failed candidate.
  - When: it completes.
  - Then: `FLayoutAttemptRecord.MetricsSnapshot` holds `CandidateIndex`, `j_used`, 12 stage timings and the G-PRE failure ids. Manifest bytes and `LayoutHash` are unchanged with metrics zeroed. Captured `LogStealthLayout` output contains none of the metric names or failure ids.
  - Edge cases: the metrics are server-only and never in a replicated struct. The wall-clock timings stay out of the hash.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-019-placement-bay-stacks-and-manifest-emission.md`, `story-020-g-pre-grid-measure-backend-and-guard-bands.md`, `story-021-g-pre-predicates-a.md`, `story-016-s11-repairs-and-occurrence-rule.md` (story 022's predicates plug into the loop when it is released; the loop is built against the predicate-set interface)
- Unlocks: `story-024-worker-task-cancellation-and-state-machine.md`, `story-025-failure-modes-and-teardown.md`, `story-030-pure-function-determinism-and-stage-hashes.md`, `story-042-attempt-time-budget-and-t-cand-measurement.md`
