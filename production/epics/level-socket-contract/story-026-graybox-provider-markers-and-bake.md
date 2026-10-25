# Story 026: Graybox provider, editor-only markers and the opaque bake

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-001`, `TR-lsc-002`, `TR-lsc-008`
*(At write time: TR-lsc-001 = Layout service world subsystem with registered providers; Round talks only to it; TR-lsc-002 = Integer Layout Manifest; TR-lsc-008 = Pillar-1 placement constraints (no area type for spawns/objectives))*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Graybox marker actors live in `StealthGameEditor` with `bIsEditorOnlyActor = true` and carry `ZoneId` (0-3) at most; the editor Validate button and commandlet bake them into a `UStealthLayoutSocketData` asset holding `FormatVersion`, `GrayboxLayoutHash` and an opaque versioned `Records` byte blob with no role-named tagged properties; `FGrayboxLayoutProvider` loads the blob with no RNG and nothing spawned.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `AActor::bIsEditorOnlyActor` is [V]. Whether a cooked map referencing editor-module marker classes strips cleanly (no load warning, no dangling import) is [U], covered by the cook test in story 039. Listen-server reality: the blob is present in every client package; only the server-authority world loads it (acceptable under LSC R1's data classes; true exclusion comes with the Dedicated Server, ADR-0024).

**Why this story**: It is the provider that makes the MVP map a first-class layout and the permanent fallback. Read first: R1, R2 item 7, R11 items 1-3, R3 TraversalPoint row; ADR-0014 sections 2 and 6.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Graybox marker classes are editor-only (`StealthGameEditor`, `bIsEditorOnlyActor`); the baked asset stores records as an opaque versioned byte blob (ADR-0014)
- Forbidden: Never cook graybox markers as runtime actors or put them in a server-only data layer (ADR-0014)
- Guardrail: graybox provider build is instant (no RNG); nothing spawned (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-28 (Graybox.MarkersZoneOnly), R11 and ADR-0014 section 6 (revised after QL-STORY-READY: forbidden-token list defined), scoped to this story:*

- [ ] GIVEN the marker classes, THEN a reflection scan finds `ZoneId` and no property or class name containing the camelCase tokens Team, Stage, Role, Final, Prelim, Objective, Site or Respawn; markers live in `StealthGameEditor` with `bIsEditorOnlyActor` true (`Graybox.MarkersZoneOnly`)
- [ ] GIVEN the bake, THEN `UStealthLayoutSocketData` holds `FormatVersion`, `GrayboxLayoutHash` and an opaque versioned `Records` byte blob with no tagged property whose name contains any of those tokens, and a round-trip decode reproduces the socket set bit-exactly (`Graybox.BakeOpaque`, proposed)
- [ ] GIVEN coping-tagged kit pieces, THEN TraversalPoints are derived from them (Vault 0.8 m, Climb 1.6 m, both landings clear) and no hand-placed traversal marker class exists (`Graybox.TraversalDerived`, proposed)

---

## Implementation Notes

*Derived from ADR-0014 section 6 and GDD R11:*

- Marker classes (`AStealthLayoutMarker` and subclasses: emergence OriginCell/ExitPoint with baked W(O) debug view, start cell with `ZoneId` 0-3, court box only, bush planter cell with `EntryAxis`, route spline snapped to the grid, nav blocker) in `StealthGameEditor`. Token match is on camelCase tokens, not raw substrings (Website and Steam are not hits).
- `LevelSocketProvider` data component on one map actor; at server begin-play the provider reads the baked blob (soft reference from world settings) and registers as the `Graybox` provider (story 001). Clients read only the header (`GrayboxLayoutHash`) from their packaged blob and echo it as `LocalLayoutHash`.
- `GrayboxLayoutHash` is the D7 cook-time extract (40-byte records, story 008). An unknown `FormatVersion` is rejected. The markers carry no team, stage or role field, so the worst-case leak is "these cells are zone 2".
- Traversal points derive from coping-tagged kit variants (M01 vault and climb, platform edges, M10 quay walls); a blocked landing yields no point.
- **Prototype replaced, not promoted:** the prototype's hand-placed team starts, objectives, bushes, crowd centre and nav bounds in `/Game/StealthGame/Maps/L_Prototype`, `AStealthGameMode::FindCrowdPoint` and `AStealthCityGenerator` site relocation are superseded; the Graybox provider is built fresh from the markers. Editing the map itself is a Content step (story 028).

---

## Out of Scope

- Story 027: Validate commandlet/button parity and CI gate; story 028: authoring the map; story 039: the cook leak scan

---

## Files

- `Source/StealthGameplay/Public/Server/Core/GrayboxLayoutProvider.h`, `Private/Core/GrayboxLayoutProvider.cpp`
- `Source/StealthGameplay/Public/Server/Core/StealthLayoutSocketData.h`
- `Source/StealthGameEditor/Public/StealthLayoutMarker.h`, `Private/StealthLayoutMarker.cpp`, `Private/LayoutBake.cpp`
- `Source/StealthGameEditor/Private/Tests/GrayboxMarkersTest.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/GrayboxBakeTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Graybox.MarkersZoneOnly`
  - Given: all marker classes.
  - When: reflected.
  - Then: `ZoneId` exists; no property or class name has the camelCase token Team, Stage, Role, Final, Prelim, Objective, Site or Respawn; markers live in `StealthGameEditor` with `bIsEditorOnlyActor` true.
  - Edge cases: token match, not substring.
- **AC-2** `Graybox.BakeOpaque` (proposed)
  - Given: a baked `UStealthLayoutSocketData`.
  - When: reflected and round-tripped.
  - Then: holds FormatVersion, GrayboxLayoutHash and an opaque versioned Records blob; no tagged property name has a forbidden token; decode reproduces the socket set bit-exactly.
  - Edge cases: unknown FormatVersion is rejected.
- **AC-3** `Graybox.TraversalDerived` (proposed)
  - Given: coping-tagged kit pieces.
  - When: the bake runs.
  - Then: Vault 0.8 m and Climb 1.6 m points are derived with both landings clear; no hand-placed traversal marker class exists.
  - Edge cases: blocked landing yields no point.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameEditor/Private/Tests/GrayboxMarkersTest.cpp` and `Source/StealthGameplay/Private/Tests/Layout/GrayboxBakeTest.cpp` — `StealthGame.Layout.Graybox.MarkersZoneOnly`, `.BakeOpaque`, `.TraversalDerived` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-001-service-skeleton-provider-registry.md`, `production/epics/level-socket-contract/story-008-manifest-struct-and-layout-hash.md`, `production/epics/level-socket-contract/story-014-socket-records-determinism-required-data.md`
- Unlocks: `production/epics/level-socket-contract/story-027-graybox-validate-commandlet-and-ci-gate.md`, `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`, `production/epics/level-socket-contract/story-039-cook-leak-scan.md`
