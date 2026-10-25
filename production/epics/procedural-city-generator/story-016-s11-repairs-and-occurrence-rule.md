# Story 016: S11 Repairs and the occurrence rule

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: S1-S12 are pure, bounded integer stages; repairs are deterministic and bounded so the candidate loop never depends on wall-clock, and a repair that would change connectivity in one half only is refused so the transpose symmetry survives.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. The sun-pool regeneration uses the fixed-sun raycast of the grid backend (story 020); art bible 6.8's shadow-trace check is the S11.3 repair target while LSC V-POOL is the gate (GDD cross-reference 5).

**Control Manifest Rules (this layer)**:
- Required: Pure data, integer streams, canonical iteration — source: ADR-0014, ADR-0006
- Forbidden: Never repair a sightline with a stall or prop (awnings have a clear underside); never wall-clock-bound a repair — source: GDD S11, ADR-0014
- Guardrail: part of the 0.12 s candidate budget; sun-pool regeneration <= `CityGenSunPoolRepairMaxCount` (20)

S11 runs four bounded repairs in order: (1) sightline breaker (a jog or a full-height wall segment), (2) prune (unreachable cells, spurs and nooks become building; registered S7 L-alleys excepted), (3) sun-pool host regeneration round a failing court, (4) occurrence fix (a variant used 1-2 times becomes the most common same-slot variant, lowest id on ties). This story also proves AC-G12, the facade rules over 1,000 seeds.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G8, AC-G12, R-G4 S11), scoped to this story:*

- [ ] **AC1** (AC-G8, `Repair.RulesHold`): **GIVEN** 50 seeds **THEN** no sightline repair placed a prop or stall, no recess >= 0.6 x 0.6 m or dead-end court < 6 x 6 m exists, and no repair changed connectivity in one half only.
- [ ] **AC2** (AC-G12, `Facade.PeriodicityAndOccurrence`): **GIVEN** 1,000 seeds **THEN** every facade run has PE <= 1, door gaps are 2-5 with no value three times in a row, every variant occurs 0 or >= 3 times (S11.4: a variant used 1-2 times becomes the most common same-slot variant, lowest id on ties), and no unique variant lies within 6 m of a socket.
- [ ] **AC3** (R-G4 S11, `Repair.OrderAndCaps`): **GIVEN** a failing court **THEN** repairs run in the stated order (sightline breaker, prune, sun-pool host regeneration up to `CityGenSunPoolRepairMaxCount` 20, occurrence fix) proved by a stage trace, steps 1, 2 and 4 are each bounded to one canonical pass (proposed: the GDD says "caps per pass" without numbers), and a repair that would change connectivity in one half only is refused.

---

## Implementation Notes

*Derived from GDD R-G4 S11 and the QL-STORY-READY review:*

Order and content as in the S11 row of the GDD. Prune rules: no recess >= 0.6 x 0.6 m, no dead-end court < 6 x 6 m, no alcove behind a stair; registered S7 L-alleys are exempt from the spur prune (story 013). A repair that would change connectivity in one half only is refused (transpose symmetry). The GDD says "caps per pass" without values: this story bounds steps 1, 2 and 4 to one canonical pass each and step 3 to `CityGenSunPoolRepairMaxCount` (20) regenerations; state in the dev-story notes whether the 20 is per court or per candidate (take per failing court) and ask the GDD owner to ratify both. The 0.6 m recess test needs sub-cell geometry on a 2 m grid: measure it on the Manifest-level geometry (module footprints) and state the basis in the notes.

AC2 needs a multi-variant kit: G0's single variant makes the occurrence rule trivially true, so the 1,000-seed run uses a synthetic multi-variant table (the same data-asset route as story 004 AC3). "No unique variant within 6 m of a socket" uses socket positions from S4, S5, S7 and S8 (ExitPoints, courts, start cells, traversal points), which exist before S12.

Prototype replaced: none (the prototype had a logged reachability check, not repairs).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-s6-facade-bay-sequencer-and-f3-periodicity.md`: the sequencer itself (F3, door gaps).
- `story-021-g-pre-predicates-a.md`, `story-022-g-pre-predicates-b-and-role-counts.md`: G-PRE verdicts after repair.
- `story-037-batch-harness-and-f4-f6-audits.md`: the entropy audit.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageRepairs.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/RepairRulesHoldTest.cpp` (class `FCityGenRepairRulesHoldTest`, name `StealthGame.CityGen.Repair.RulesHold`)
- `Source/StealthGameplay/Private/Tests/CityGen/PeriodicityAndOccurrenceTest.cpp` (class `FCityGenFacadePeriodicityAndOccurrenceTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/OrderAndCapsTest.cpp` (class `FCityGenRepairOrderAndCapsTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC2 ADEQUATE; AC3 revised and ADEQUATE).*

- **AC1** `Repair.RulesHold`
  - Given: 50 seeds.
  - When: S11 completes.
  - Then: sightline repairs are only jogs or full-height wall segments, with 0 props or stalls. No recess of 0.6x0.6 m or more and no dead-end court under 6x6 m exists. Each repair's connectivity change is identical in both halves.
  - Edge cases: a fixture for each violation fails (negative control). The basis for measuring a 0.6 m recess on a 2 m grid (Manifest geometry or sub-cell) is stated.
- **AC2** `Facade.PeriodicityAndOccurrence`
  - Given: 1,000 seeds on a synthetic multi-variant kit (G0's single variant makes occurrence trivial).
  - When: S6 and S11 run.
  - Then: every run has PE <= 1. Door gaps are 2-5 with no value three times in a row. Every variant occurs 0 or at least 3 times. No unique variant is within 6 m of a socket.
  - Edge cases: a variant used once or twice becomes the most common same-slot variant. A fixture with a tie takes the lowest id.
- **AC3** `Repair.OrderAndCaps`
  - Given: a failing-court fixture that needs all four repairs.
  - When: S11 runs.
  - Then: the stage trace order is sightline breaker, prune, sun-pool regeneration, occurrence fix. Steps 1, 2 and 4 each run at most one canonical pass. Sun-pool regeneration runs at most 20 times per failing court. A repair that would change connectivity in one half only is refused, and the trace records the refusal.
  - Edge cases: a fixture where a repair creates a new violation shows no second pass. A fixture where sun-pool regeneration needs 21 tries ends at 20.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-012-s6-facade-bay-sequencer-and-f3-periodicity.md`, `story-013-s7-emergence-alleys-and-exit-points.md`, `story-014-s8-traversal-points.md`, `story-015-s9-bushes-and-s10-props-role-blind.md`, `story-020-g-pre-grid-measure-backend-and-guard-bands.md` (sun raycast)
- Unlocks: `story-017-s12-sockets-socket-ids-slab-cover-and-skeleton-transform.md`, `story-023-candidate-loop-work-unit-cap-and-metrics.md`
