# Story 017: Predicates V-NAV, V-AREA, V-SIGHT, V-ROUTE and V-TRAV

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; <= 33 ms slices)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Each V-* predicate has an id, a severity (`Reject` fails the attempt, `Warn` is logged) and a data threshold, written once against `ILayoutMeasureBackend`; all path lengths are navmesh paths without traversal unless a predicate says otherwise.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Fixtures run on the grid fixture backend (story 015) so the matrix is headless; the world-backend repeat happens in story 028 on the real graybox.

**Why this story**: These are the map-shape predicates: connectivity, clear width, detour, area band, sightlines, route coverage and traversal landings. Read first: R7 table rows V-NAV-1..3, V-AREA/V-DENSITY, V-SIGHT, V-ROUTE, V-TRAV; AC-22.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every V-* predicate exists once against `ILayoutMeasureBackend` (ADR-0014)
- Forbidden: Never fork predicate logic between backends (ADR-0014)
- Guardrail: work bounded by `LayoutSliceWorkUnitsLimit` (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-22 (predicate matrix, subset), revised after QL-STORY-READY with exact boundaries, scoped to this story:*

- [ ] GIVEN one fixture per predicate V-NAV-1 (including a 1-cell, 2 x 2 m island), V-NAV-2 (one-pawn chokepoint at a court ingress; 0.6 x 0.6 m recess), V-NAV-3 and V-AREA, each breaking only that predicate, THEN the attempt fails with exactly that id (`Validate.PredicateMatrix.<Id>`)
- [ ] GIVEN V-SIGHT and V-ROUTE fixtures, THEN a longest sightline of 3,200 cm passes and 3,201 cm fails in axis-aligned and diagonal directions (a jog whose offset is under the corridor clear width is not a break); V-ROUTE fails for coverage under 95% within 12 m, a player-standable cell more than 10 m from an edge, fewer than 8 cycles or fewer than 2 per quadrant, fewer than 95% of nodes with degree 2 or more, or a socket more than 3 m from a node (`Validate.PredicateMatrix.V-SIGHT`, `Validate.PredicateMatrix.V-ROUTE`)
- [ ] GIVEN V-TRAV fixtures, THEN a 46 cm walk-over obstacle fails, 45 cm passes, a landing or climb-top footprint shallower than `TraversalLandingClearCm` fails, and the all-pass fixture passes (`Validate.PredicateMatrix.V-TRAV`)

---

## Implementation Notes

- V-NAV-1: one connected walkable component; no island of `NavIslandRejectCellsCount` (1) cell or more. V-NAV-2: clear width at least `NavMinClearWidthCm` (2.8 m); no dead-end court under 6 x 6 m; no recess 0.6 m wide and 0.6 m deep (0.59 m passes); every court ingress and every bridge/stair on a `CrowdRoute` path between courts and zones at least 2.8 m wide; every fixture 1.2 m clear on all four faces. V-NAV-3: D3 (story 006).
- V-AREA: `LayoutWalkableCellsMinCount` 2,000 <= A <= min(3,250, 45 cells x (`NpcCount` + 2 x `TeamSize`)); default band 8,000-10,800 m2 at `NpcCount` 50; exactly 8,000 m2 passes.
- V-SIGHT: longest straight walkable sightline at most `SightlineMaxCm` (32 m), measured in every direction including diagonals across plazas and junctions; a one-module jog is not a break unless its offset is at least the corridor clear width.
- V-ROUTE: connected; at least 95% of walkable cells within 12 m of an edge and every player-standable cell within `RouteMaxGapCm` (10 m); at least 8 independent cycles and 2 per quadrant; at least 95% of nodes degree 2 or more; every socket within 3 m of a node. Boundary values pass.
- V-TRAV: both landings on the navmesh and clear; each landing and climb top a capsule-diameter footprint at least `TraversalLandingClearCm` (100 cm) deep (99 cm fails, 100 cm passes); no walk-over obstacle taller than `TraversalMinHeightCm` (45 cm, SCL-owned); 46 cm up to the vault band blocks.
- Each fixture breaks only its predicate; the all-pass fixture is shared with stories 018 and 019.

---

## Out of Scope

- V-SEP/V-REACH/V-RESPAWN/V-ZONE/V-ZONEFILL (018); V-COURT/V-POOL/V-BUSH/V-CLUE (019); route polyline baking (020)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/Predicates/NavAreaSightRoutePredicates.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/PredicateMatrixNavTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Validate.PredicateMatrix.<V-NAV-1|V-NAV-2|V-NAV-3|V-AREA>`
  - Given: one fixture per predicate: a 1-cell (2 x 2 m) island; a one-pawn court-ingress chokepoint; a 0.6 x 0.6 m recess; a detour p95 > 1.6; area outside 8,000-10,800 m2.
  - When: validated.
  - Then: the attempt fails with exactly that id.
  - Edge cases: 0.6 m recess fails and 0.59 m passes; V-AREA at exactly 8,000 m2 passes.
- **AC-2** `Validate.PredicateMatrix.V-SIGHT`, `.V-ROUTE`
  - Given: sightline fixtures (axis-aligned, diagonal, one-module jog); route fixtures.
  - When: validated.
  - Then: 3,200 cm passes, 3,201 cm fails in both directions; a jog narrower than the corridor clear width is not a break; V-ROUTE fails for each listed condition.
  - Edge cases: boundary values pass (95%, 10 m, 8 cycles, 3 m).
- **AC-3** `Validate.PredicateMatrix.V-TRAV`
  - Given: obstacle 46 cm and 45 cm; landing/climb-top depths 99 and 100 cm; all-pass fixture.
  - When: validated.
  - Then: 46 cm fails, 45 cm passes; 99 cm fails, 100 cm passes; all-pass passes.
  - Edge cases: both landings checked separately.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/PredicateMatrixNavTest.cpp` — `StealthGame.Layout.Validate.PredicateMatrix.<Id>` for V-NAV-1, V-NAV-2, V-NAV-3, V-AREA, V-SIGHT, V-ROUTE, V-TRAV must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-006-formulas-d1-to-d4.md`, `production/epics/level-socket-contract/story-007-formulas-d5-d6-d6b-d9.md`, `production/epics/level-socket-contract/story-014-socket-records-determinism-required-data.md`
- Unlocks: `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
