# Story 014: Socket records, determinism and required data

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-002`, `TR-lsc-004`, `TR-lsc-008`
*(At write time: TR-lsc-002 = Integer Layout Manifest; TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; TR-lsc-008 = Pillar-1 placement constraints (no area type for spawns/objectives))*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: A candidate is a socket set plus Manifest entries (or the graybox's baked records); the socket set and Manifest are a pure function of (Seed, GeneratorKitVersion, LayoutParamsHash), never of player count, join order, time or world state; V-SOCK rejects missing required data and never snaps a socket onto the grid.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Plain data structs and integer validation; no engine API.

**Why this story**: The socket vocabulary (R3) is the shape every provider, predicate and runtime service shares. Read first: R1 Terms, R2 item 4, R3 table, R10, AC-1, AC-4.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every V-* predicate exists once, written against `ILayoutMeasureBackend`; draw sites use ADR-0006 shapes (ADR-0014)
- Forbidden: Never fork predicate logic between backends; providers never read `FSiteAssignment` or world state outside their own geometry (ADR-0014)
- Guardrail: Layout work bounded by `LayoutSliceWorkUnitsLimit` (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-1, AC-4 and R1/R10, scoped to this story:*

- [ ] GIVEN fixed (Seed, GeneratorKitVersion, LayoutParamsHash), WHEN the layout is built twice in fresh worlds and once more with a different player count and join order, THEN socket sets (SocketId, type, cell, yaw, R3 data) and Manifest bytes are identical (`Determinism.SameSeedSameSockets`)
- [ ] GIVEN one fixture per R3 row with a required field missing or a socket off the 2.0 m grid, WHEN validated, THEN V-SOCK rejects, names the field, and does not snap the socket onto the grid (`Validate.SocketRequiredData.<Type>`)
- [ ] THEN the `AreaTypeId` enum is exactly Causeway, Market, Temple, Residential and no value (or marker field) exists for spawns, starts or objectives (`Pillar1.NoSpawnAreaType`)

---

## Implementation Notes

- Records: `FEmergenceSocket`, `FStartCell`, `FTeamSpawnCandidate`, `FObjectiveCourt`, `FBushSite`, `FCrowdRoute`, `FTraversalPoint`, `FNavAreaCell`, each with a stable integer `SocketId` in canonical order, a 2.0 m grid cell (x, y, course), a yaw index 0-3 and the R3 data. `ZoneId` (0-3) is the only grouping field on starts; never a team, stage or role.
- V-SOCK counts meet D5 (story 007) and every required field is present and on the grid; the rejection names the field and leaves the cell unchanged (1 cm off-grid is rejected too).
- A court record carries no role hint; eligibility for each role derives only from distance predicates (R9). The role-blind `ClueTag` is story 019.
- Also declare the `ILayoutMeasureBackend` interface and the integer grid fixture backend here (shared by stories 015-019 and 021; the world implementation is story 015).
- Determinism test builds three times (fresh worlds; different player count and join order; different frame count or world time between builds) with a synthetic stub provider; the real graybox repeats it in story 028.
- Closed-set test: reflection over `AreaTypeId` fails when a fifth value or any spawn/start/objective-named value or field appears.

---

## Out of Scope

- Story 016-019: the other V-* predicates; story 026: graybox records from markers; story 019: ClueTag

---

## Files

- `Source/StealthGameplay/Public/View/Core/StealthLayoutSocketTypes.h`
- `Source/StealthGameplay/Private/Core/Layout/LayoutMeasureBackend.h` (interface) and `GridMeasureBackend.cpp` (fixture backend)
- `Source/StealthGameplay/Private/Core/Layout/SocketValidation.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/SocketRecordsTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Determinism.SameSeedSameSockets`
  - Given: fixed (Seed, GeneratorKitVersion, LayoutParamsHash).
  - When: built twice in fresh worlds, then once with a different player count and join order.
  - Then: socket sets (SocketId, type, cell, yaw, R3 data) and Manifest bytes are identical in all 3 builds.
  - Edge cases: different frame count or world time between builds.
- **AC-2** `Validate.SocketRequiredData.<Type>`
  - Given: one fixture per R3 row (EmergenceSocket, StartCell, TeamSpawn, ObjectiveCourt, BushSite, CrowdRoute, TraversalPoint, NavArea) with one required field missing, plus a socket off the 2.0 m grid.
  - When: V-SOCK validates.
  - Then: reject, the missing field is named, the off-grid socket is not snapped (cell unchanged).
  - Edge cases: 1 cm off-grid; every field missing individually.
- **AC-3** `Pillar1.NoSpawnAreaType`
  - Given: the `AreaTypeId` enum and the per-cell area record.
  - When: reflected.
  - Then: values are exactly {Causeway, Market, Temple, Residential}; no value or field names spawn, start or objective.
  - Edge cases: adding a fifth value fails the test.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/SocketRecordsTest.cpp` — `StealthGame.Layout.Determinism.SameSeedSameSockets`, `StealthGame.Layout.Validate.SocketRequiredData.<Type>`, `StealthGame.Layout.Pillar1.NoSpawnAreaType` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-001-service-skeleton-provider-registry.md`, `production/epics/level-socket-contract/story-007-formulas-d5-d6-d6b-d9.md`
- Unlocks: `production/epics/level-socket-contract/story-015-layout-measure-backend-world.md`, `production/epics/level-socket-contract/story-016-predicates-emergence-witness-alley.md`, `production/epics/level-socket-contract/story-026-graybox-provider-markers-and-bake.md`
