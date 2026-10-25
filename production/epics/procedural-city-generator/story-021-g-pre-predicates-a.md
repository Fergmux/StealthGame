# Story 021: G-PRE predicates A: navigation, reach, sight, emergence, clue, counts

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
**ADR Decision Summary**: G-PRE evaluates the LSC predicates on the cell grid with the same predicate code the service uses on the navmesh (behind `ILayoutMeasureBackend`), plus guard bands; V-CLUE is a Reject predicate computed by one shared integer function used by G-PRE and the service.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. Predicate ids and thresholds come from the LSC GDD (R7); this story proves the Generator's grid evaluation of the first group, each with a crafted failing fixture.

**Control Manifest Rules (this layer)**:
- Required: Every V-* predicate exists once; V-CLUE is one shared integer-only function used by G-PRE and the service; `ClueTag` is a pure function of the published geometry and its rule version is part of `GeneratorKitVersion` — source: ADR-0014
- Forbidden: Never fork predicate logic between backends; never add `ClueTag` to the wire — source: ADR-0014
- Guardrail: part of the 0.12 s candidate budget

Predicate group A (GDD R-G5.3): V-NAV-1/2/3 (detour on the grid sample pairs) and V-AREA; V-REACH (complete grid path from every start cell to every court within the D2 budget); V-CLUE (`ClueTag` shared by <= `ClueMatchMaxCount`, 3, Objectives-owned and read only); V-SOCK and D5 counts; V-EMERGE (W(O) <= 30); V-SIGHT (DDA, every direction). Also the no-single-pawn-chokepoint check of AC-G38.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (R-G5.3, AC-G38 last clause, LSC change request 1b), scoped to this story:*

- [ ] **AC1** (R-G5.3, `GPre.PredicateSetA`): **GIVEN** a crafted fixture per predicate **THEN** V-NAV-1/2/3 (detour on grid sample pairs), V-AREA, V-REACH (complete grid path from every start cell to every court within the D2 budget), V-CLUE (<= `ClueMatchMaxCount` 3 via the shared integer function), V-SOCK and D5 counts, V-EMERGE (W(O) <= 30) and V-SIGHT each fail with their named failure id in the metrics, and a clean fixture passes all.
- [ ] **AC2** (AC-G38 last clause, `Courts.NoSinglePawnChokepoint`): **GIVEN** 50 seeds **THEN** on a 0.5 m sub-cell nav raster, blocking a 0.8 m-diameter disc at every walkable raster position never disconnects a fixture from either zone anchor, every bridge and route stair has >= 2.8 m clear, and a one-cell pinch fixture fails (negative control).
- [ ] **AC3** (LSC request 1b, `GPre.SightEveryDirection`): **GIVEN** a plaza of 24 x 16 m **THEN** V-SIGHT measures its 28.8 m diagonal and a fixture with a clear diagonal above `SightlineMaxCm` fails.

---

## Implementation Notes

*Derived from GDD R-G5.3 and the QL-STORY-READY review:*

Evaluate each predicate through the story-020 grid backend; do not copy predicate logic. One failing fixture per predicate (V-NAV-1: a one-cell island; V-NAV-2: a 2.6 m ingress against the 2.8 m clear width; V-NAV-3: detour above the LSC bound for pairs far enough apart; V-AREA: area outside the band; V-REACH: a path over the D2 budget; V-CLUE: a `ClueTag` shared by 4 courts; V-SOCK/D5: a count off by one; V-EMERGE: W(O) = 31; V-SIGHT: a clear line above `SightlineMaxCm` 32 m) plus a just-pass fixture for each (W(O) = 30, a ClueTag shared by 3). Read the exact LSC thresholds from the LSC GDD R7 and keys, not from this list.

Chokepoint check (AC-G38): the GDD does not give a resolution for "removing any single 0.8 m-diameter disc from the grid navmesh". Use a 0.5 m sub-cell raster eroded by the capsule radius, block a disc at every walkable raster position (about 34,000 positions per seed: use an articulation-point method or a bounded time budget rather than brute force), and define the "one-cell pinch" fixture as a neck under 1.6 m (a 0.8 m disc plus a 0.8 m capsule disconnects any neck under 1.6 m). Penalty CR-13 and Objectives CR-8 are the sources of the rule. Both resolution and erosion choices are flagged in the dev-story notes for the level-designer. SightlineMaxCm default 32 m (3,200 cm, LSC): a 24 x 16 m plaza diagonal is 28.8 m and passes; a 28 x 16 m fixture (32.25 m) fails; exactly 32.0 m passes.

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-022-g-pre-predicates-b-and-role-counts.md`: court, pool, bush, route, trav, zone and fairness predicates and the S4 role-count pre-check.
- `story-020-g-pre-grid-measure-backend-and-guard-bands.md`: the backend and guard values.
- `story-010-s4-courts.md`: the geometric rules (ingress width, fixture clearance) as placement.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenGPrePredicatesA.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenChokepoint.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/PredicateSetATest.cpp` (class `FCityGenGPrePredicateSetATest`, name `StealthGame.CityGen.GPre.PredicateSetA`)
- `Source/StealthGameplay/Private/Tests/CityGen/NoSinglePawnChokepointTest.cpp` (class `FCityGenCourtsNoSinglePawnChokepointTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/SightEveryDirectionTest.cpp` (class `FCityGenGPreSightEveryDirectionTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC3 ADEQUATE; AC2 revised and ADEQUATE).*

- **AC1** `GPre.PredicateSetA`
  - Given: one crafted single-failure fixture per predicate.
  - When: G-PRE runs.
  - Then: each fails with its named id in the metrics snapshot, and a clean fixture passes all. V-NAV-1: a 1-cell island. V-NAV-2: a 2.6 m ingress. V-NAV-3: detour above the LSC bound. V-AREA: area outside the band. V-REACH: a path over the budget. V-CLUE: a `ClueTag` shared by 4 courts. V-SOCK / D5: a count off by one. V-EMERGE: W(O) = 31. V-SIGHT: a 32.2 m sightline.
  - Edge cases: a just-pass fixture for each, for example W(O) = 30 and a ClueTag shared by 3.
- **AC2** `Courts.NoSinglePawnChokepoint`
  - Given: 50 seeds on a 0.5 m sub-cell nav raster, plus a neck fixture 1.5 m wide.
  - When: the disc is blocked at every walkable raster position, one at a time.
  - Then: no fixture is ever disconnected from either zone anchor. Every bridge and route stair is at least 2.8 m clear. The 1.5 m neck fixture fails the disc check on its own (negative control).
  - Edge cases: a neck under 1.6 m disconnects; the raster is eroded by the capsule radius; the method must fit the time budget (articulation-point or equivalent).
- **AC3** `GPre.SightEveryDirection`
  - Given: a 24x16 m plaza and a 28x16 m fixture.
  - When: V-SIGHT runs.
  - Then: the 24x16 diagonal is measured at 28.8 m (+-1 cell) and passes. The 28x16 fixture (32.25 m) fails with the V-SIGHT id.
  - Edge cases: exactly 32.0 m passes. A jog smaller than the clear width doesn't break a sightline.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-020-g-pre-grid-measure-backend-and-guard-bands.md`, `story-013-s7-emergence-alleys-and-exit-points.md`, `story-018-s12-crowd-route.md`
- Unlocks: `story-023-candidate-loop-work-unit-cap-and-metrics.md`, `story-037-batch-harness-and-f4-f6-audits.md`
