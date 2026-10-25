# Story 018: S12 CrowdRoute

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
**ADR Decision Summary**: `CrowdRoute` is an LSC R3 record the Generator emits; legs along a route edge follow the edge's pre-baked path (server-only, stored with the route graph, ADR-0014/0016), other legs use `FindPathAsync`. Route coverage is what lets the crowd reach every legal player position (Crowd CR-17).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; the route graph is plain integer data here; the pre-baked paths are the LSC's)
**Engine Notes**: No engine API in this story.

**Control Manifest Rules (this layer)**:
- Required: Legs along a `CrowdRoute` edge follow the edge's pre-baked path (server-only) — source: ADR-0014, ADR-0016
- Forbidden: Never let route data carry a role or a team — source: ADR-0014
- Guardrail: part of the 0.12 s candidate budget

Route edges run on corridor centrelines and through every court and plaza; nodes at every junction, court centre, plaza and ExitPoint and every <= `CityGenRouteNodeSpacingCount` (3) cells along each edge, so the LSC R6.1 NPC pool holds several hundred cells; plazas and courts wider than 6 c get a second parallel edge so every walkable cell lies within 10 m of an edge (Crowd CR-17, stricter than V-ROUTE's 95% within 12 m). Start cells (S5) are route nodes.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G37, R-G4 S12), scoped to this story:*

- [ ] **AC1** (AC-G37 first clause, `Route.NodeSpacingAndCoverage`): **GIVEN** 50 seeds **THEN** no two consecutive nodes on a route edge are more than `CityGenRouteNodeSpacingCount` (3) cells apart and every walkable cell is within 10 m of a route edge.
- [ ] **AC2** (AC-G37 second clause, `Route.StartCellsAndNpcPool`): **GIVEN** 50 seeds **THEN** every start cell is a route node and the R6.1 NPC pool (cells within 3 m of a node, greedy >= 3 m spacing in canonical order) holds >= 4 x (`NpcCount` + 2 x `TeamSize`) = 240 cells at defaults.
- [ ] **AC3** (R-G4 S12, `Route.NodePlacement`): **GIVEN** 50 seeds **THEN** edges run on corridor centrelines and through every court and plaza, nodes sit at every junction, court centre, plaza and emergence ExitPoint, and plazas and courts wider than 6 c get a second parallel edge.

---

## Implementation Notes

*Derived from GDD R-G4 S12 (review 2026-10-24) and AC-G37:*

Build the route graph on the integer grid in canonical order. State the distance metric for "within 10 m" (take Euclidean from the cell centre to the nearest edge segment, in cm) and the meaning of "wider than 6 c" (the shorter side of a plaza or court: a 12 x 8 c plaza qualifies, a 4 x 4 c court does not) in the dev-story notes. The pool threshold is recomputed from `NpcCount` and `TeamSize` at the keys' hard-range extremes as well as the defaults (240 at 50 and 5). If LSC adopts touchable-prop records (Crowd CR-8: PENDING-USER at LSC), they are emitted here from props and walls already in the Manifest, role-blind; not in scope until then. The pre-baked per-edge paths (server-only, stored with the route graph) are produced by the LSC story `level-socket-contract/story-020-crowd-route-prebaked-paths.md` from the graph this story emits.

Prototype replaced: the prototype crowd walked to random navmesh points (`StealthGameMode::FindCrowdPoint`); no route graph exists.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-017-s12-sockets-socket-ids-slab-cover-and-skeleton-transform.md`: the other S12 records.
- `story-021-g-pre-predicates-a.md`, `story-022-g-pre-predicates-b-and-role-counts.md`: V-ROUTE pre-check on the grid.
- Pre-baked paths and Crowd's use of the graph: LSC story 020, `crowd-npc-behaviour/story-007-path-source-and-start-budget.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageCrowdRoute.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/NodeSpacingAndCoverageTest.cpp` (class `FCityGenRouteNodeSpacingAndCoverageTest`, name `StealthGame.CityGen.Route.NodeSpacingAndCoverage`)
- `Source/StealthGameplay/Private/Tests/CityGen/StartCellsAndNpcPoolTest.cpp` (class `FCityGenRouteStartCellsAndNpcPoolTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/NodePlacementTest.cpp` (class `FCityGenRouteNodePlacementTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Route.NodeSpacingAndCoverage`
  - Given: 50 seeds.
  - When: S12 runs.
  - Then: consecutive nodes are at most `CityGenRouteNodeSpacingCount` (3) cells apart. Every walkable cell is within 10 m of a route edge.
  - Edge cases: a 4-cell gap and a cell 10.5 m away each fail. The distance metric is stated. Spacing is also run at hard-range 2 and 5.
- **AC2** `Route.StartCellsAndNpcPool`
  - Given: 50 seeds.
  - When: S12 runs.
  - Then: every start cell is a route node. The R6.1 pool (cells within 3 m of a node, greedy at least 3 m apart, canonical order) holds at least 4 x (`NpcCount` + 2 x `TeamSize`) cells, which is 240 at defaults.
  - Edge cases: the pool is identical across insertion orders. The threshold is recomputed at hard-range extremes.
- **AC3** `Route.NodePlacement`
  - Given: 50 seeds.
  - When: S12 runs.
  - Then: edges run on corridor centrelines and through every court and plaza. Nodes sit at every junction, court centre, plaza and ExitPoint. Plazas and courts wider than 6 c get a second parallel edge.
  - Edge cases: "wider" is the shorter side. A 12x8 c plaza qualifies and a 4x4 c court doesn't.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-011-s5-zones-anchors-and-start-cells.md`, `story-013-s7-emergence-alleys-and-exit-points.md`, `story-017-s12-sockets-socket-ids-slab-cover-and-skeleton-transform.md`
- Unlocks: `story-019-placement-bay-stacks-and-manifest-emission.md`, `story-021-g-pre-predicates-a.md`, `story-022-g-pre-predicates-b-and-role-counts.md`
