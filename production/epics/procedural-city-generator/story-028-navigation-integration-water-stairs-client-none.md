# Story 028: Navigation integration: water, stairs, client-none

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-004`, `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The navmesh is built **once** per accepted attempt on the server (Recast `Dynamic` runtime generation behind the initial build lock, then one non-blocking rebuild by removing the `Custom` lock) and frozen at `OnLayoutReady`; clients hold no navigation data (`bAllowClientSideNavigation=false`, `CanEverAffectNavigation` false on client instances). Water exports no nav; a pawn-only blocking volume covers it (visibility and camera channels ignore it) and the LSC NavArea is Blocked.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; Navigation has no 5.6-5.8 breaking change listed in `VERSION.md`)
**Engine Notes**: Verified in 5.8.4 source (ADR-0014): `ENavigationBuildLock::Custom`, `RemoveNavigationBuildLock(..., ELockRemovalRebuildAction::Rebuild)`, `bAllowClientSideNavigation`, `SetCanEverAffectNavigation`, ISM per-instance navigation export. ADR-0014 verification items (4) ISM nav export of per-instance simple collision produces the same navmesh as equivalent static actors and (6) `bAllowClientSideNavigation=false` leaves clients with no navigation data are exercised here. Use the agent size, cell and tile values spike R4 recommends (`level-socket-contract/story-011-spike-r4-graybox-navmesh-timing.md`) once it reports; engine defaults before.

**Control Manifest Rules (this layer)**:
- Required: `bAllowClientSideNavigation=false`; the only runtime navmesh build trigger is the service's `RemoveNavigationBuildLock(Custom, Rebuild)` — source: ADR-0014
- Forbidden: Never call `UNavigationSystemV1::Build()` at runtime; clients never hold navigation data; no dynamic nav modifier — source: ADR-0014
- Guardrail: server spawn + nav <= 3.0 s (story 043)

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G13, AC-G14, AC-G15), scoped to this story:*

- [ ] **AC1** (AC-G13, `Nav.ClientNone`): **GIVEN** a 2-client PIE on a golden seed **THEN** clients hold no navigation data, the client build path's RNG counter is 0, and the server holds exactly one built navmesh.
- [ ] **AC2** (AC-G14, `Nav.WaterNotWalkable`): **GIVEN** 20 seeds **THEN** water footprints carry zero nav polygons, a pawn-only blocking volume covers the water (visibility and camera channels ignore it, NavArea Blocked), and a scripted pawn walking at the kerb cannot enter the water.
- [ ] **AC3** (AC-G15, `Nav.StairsAndKerb`): **GIVEN** 20 seeds **THEN** every stair (civic and temple) connects the navmesh across it and no pawn crosses a quay kerb into the water (the 0.4 m kerb alone is stepped at the 45 cm step height, so the blocking volume is the blocker).

---

## Implementation Notes

*Derived from GDD R-G7 and ADR-0014 section 4:*

The Generator owns the water blocking volume and the stair geometry; the service owns the navmesh lifecycle (`level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`). Water: no nav exported from the water component; a pawn-only blocking volume over the water; LSC NavArea = Blocked. Shared Character & Locomotion answered (GDD change request 4): step height is 45 cm, so the 0.4 m canal kerb is **stepped**, not blocked, and the walkable floor angle stays 44.765 degrees (the 40 degree temple stair passes). Canals are disabled at tier G0, so AC2 and AC3's water clauses run on a canal-enabled G1 fixture; the test reports the count of seeds with 0 stairs rather than passing them silently. The 2-client PIE in AC1 is T2 (CQTest, `StealthGame.CityGen.PIE.NavClientNone.TwoClientsNoNavData`, in `StealthTestHarness`); the water and stairs checks run in a server-only editor world with a scripted pawn pushed toward the water at three angles for 10 s.

Prototype replaced: the prototype rebuilt the navmesh dynamically after placement and logged reachability; neither is carried over.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`: lock lifecycle and the freeze counters.
- `story-035-navmesh-frozen-after-hand-off.md`: the freeze over 60 s of play.
- `story-026-server-kit-spawn-from-manifest.md`: kit spawn.
- LSC V-NAV predicates on the navmesh: LSC story 017.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenWater.cpp` (blocking volume, NavArea)
- `Source/StealthTestHarness/Private/CityGen/NavClientNonePieTest.cpp` (CQTest PIE, `StealthGame.CityGen.PIE.NavClientNone.TwoClientsNoNavData`)
- `Source/StealthGameplay/Private/Tests/CityGen/WaterNotWalkableTest.cpp` (class `FCityGenNavWaterNotWalkableTest`, name `StealthGame.CityGen.Nav.WaterNotWalkable`)
- `Source/StealthGameplay/Private/Tests/CityGen/StairsAndKerbTest.cpp` (class `FCityGenNavStairsAndKerbTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `StealthGame.CityGen.PIE.NavClientNone.TwoClientsNoNavData`
  - Given: a listen server plus 2 clients on a golden seed.
  - When: the round reaches Active.
  - Then: each client world has 0 `ANavigationData` actors and the client build RNG counter is 0. The server has exactly 1 built navmesh with >= 1 tile.
  - Edge cases: a second leftover graybox navmesh on the server fails.
- **AC2** `Nav.WaterNotWalkable`
  - Given: 20 fixed seeds in a server-only world. Precondition: at least one seed has water footprints, because canals are disabled at tier G0.
  - When: the navmesh is built, then a scripted pawn walks along the kerb for 10 s.
  - Then: water footprints have 0 nav polygons. The blocking volume exists with Pawn = Block, visibility and camera = Ignore, and NavArea Blocked. The pawn's XY never overlaps water.
  - Edge cases: push the pawn toward the water at 3 angles. The 0.4 m kerb is stepped at 45 cm, so only the volume blocks.
- **AC3** `Nav.StairsAndKerb`
  - Given: the same 20 seeds.
  - When: path queries run base to top for every civic and temple stair, plus pawn move-to into water from the kerb.
  - Then: every stair path succeeds across the stair and no pawn enters the water.
  - Edge cases: report the count of seeds with 0 stairs rather than passing them silently.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the T2 PIE test and the two editor-world tests above OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-026-server-kit-spawn-from-manifest.md`, `story-014-s8-traversal-points.md`; cross-epic `level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`, `story-015-layout-measure-backend-world.md`
- Unlocks: `story-035-navmesh-frozen-after-hand-off.md`, `story-039-g-pre-false-pass-rate-and-guard-band-sign.md`, `story-043-kit-gpu-cost-and-server-spawn-nav-time.md`
