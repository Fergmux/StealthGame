# Story 020: CrowdRoute graph with pre-baked path polylines

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`, `TR-lsc-009`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; TR-lsc-009 = Server-only query API for site assignment and team spawns)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Each `CrowdRoute` edge stores its navmesh-corridor polyline (both directions share one), baked once in Validate after the navmesh freeze and kept server-side with the route graph, never in `FLayoutManifest`, so Crowd follows an edge with no path query (user decision 2026-10-25, Crowd CR 17).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Polylines come from `FindPathSync` string-pulled corridors on the frozen navmesh; "on the navmesh" means within the bake's query extent (state the extent in the fixture).

**Why this story**: Crowd's route following must not depend on runtime path queries or on a navmesh that can change. Read first: R3 CrowdRoute row, R7 V-ROUTE; ADR-0014 7.4 and the 2026-10-25 user-decision amendments.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Round and every feature reach layout only through `UStealthLayoutService` (ADR-0014)
- Forbidden: Never add a field to `FLayoutManifest` beyond section 8.1; never let anything affect navigation after the freeze (ADR-0014)
- Guardrail: Layout runtime services at most 0.1 ms average in `Active` (ADR-0014)

---

## Acceptance Criteria

*From GDD R3 (CrowdRoute), R7 (V-ROUTE) and ADR-0014 7.4, scoped to this story:*

- [ ] GIVEN a frozen navmesh, THEN every `CrowdRoute` edge stores one path polyline baked once in Validate after the freeze, shared by both directions (`Route.PolylineBakedOnce`, proposed)
- [ ] THEN route data and polylines live only in the server socket view; `FLayoutManifest` bytes and `LayoutHash` are identical with and without routes (`Route.NotInManifest`, proposed)
- [ ] GIVEN a consumer following an edge, THEN it issues 0 path queries (counter) and the polyline endpoints lie on the navmesh (`Route.NoPathQueries`, proposed)

---

## Implementation Notes

- `FCrowdRoute`: nodes (cell, `AreaTypeId`, capacity) and edges (width, kind, pre-baked polyline). Bake counter is 1 per edge; a bake attempt before the freeze is rejected.
- Exposed through `GetSocketView_Server()` (story 023 enforces the header placement); a late-joining client has no route data.
- A Crowd stub traverses every edge in both directions in the test; the path-query counter must stay 0; every polyline endpoint projects onto the navmesh within the stated extent.
- The V-ROUTE predicate (coverage, gaps, cycles) is story 017; this story provides the baked data it and Crowd use.

---

## Out of Scope

- Crowd's route-following behaviour (`production/epics/crowd-npc-behaviour/EPIC.md`); V-ROUTE predicate (017)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/CrowdRouteBake.cpp`
- `Source/StealthGameplay/Public/Server/Core/StealthLayoutSocketView.h`
- `Source/StealthGameplay/Private/Tests/Layout/CrowdRouteTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Route.PolylineBakedOnce` (proposed)
  - Given: a frozen navmesh and a CrowdRoute graph.
  - When: Validate bakes routes.
  - Then: bake counter = 1 per edge, after the freeze; one polyline per edge, same object for both directions.
  - Edge cases: a bake attempt before the freeze is rejected.
- **AC-2** `Route.NotInManifest` (proposed)
  - Given: the same layout with and without routes.
  - When: the Manifest and LayoutHash are built.
  - Then: bytes and hash are identical; route data exists only in the server socket view.
  - Edge cases: late-join client has no route data.
- **AC-3** `Route.NoPathQueries` (proposed)
  - Given: a Crowd stub following an edge.
  - When: it traverses every edge in both directions.
  - Then: path-query counter = 0; every polyline endpoint projects onto the navmesh within the bake's query extent.
  - Edge cases: state the extent in the fixture.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/CrowdRouteTest.cpp` — `StealthGame.Layout.Route.PolylineBakedOnce`, `.NotInManifest`, `.NoPathQueries` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`, `production/epics/level-socket-contract/story-015-layout-measure-backend-world.md`
- Cross-epic: `production/epics/crowd-npc-behaviour/EPIC.md` (consumer; stub in tests)
- Unlocks: `production/epics/level-socket-contract/story-023-server-query-api-and-leak-blindness.md`
