# Story 017: S12 Sockets, SocketIds, slab cover and skeleton transform

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
**ADR Decision Summary**: The Generator returns `{socket set, Manifest entries, skeleton transform, metrics}` through `OnCandidateReady(FLayoutCandidate&&)`; the socket set carries the LSC R3 records, the Manifest carries only the section 8.1 fields (a new field is a Visibility Policy change request and a schema change), and the floor slab cover is computed on the server and emitted so clients derive nothing.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. SocketIds are `uint16` LSC ids assigned canonically; the skeleton symmetry transform is server-only and is what LSC R9.3 uses to reject mirror-intercept combos.

**Control Manifest Rules (this layer)**:
- Required: Canonical SocketId assignment; the Manifest holds only the section 8.1 fields — source: ADR-0014
- Forbidden: Never add a field to `FLayoutManifest` beyond section 8.1; never put a role-named field in a socket record — source: ADR-0014, GDD R-G1
- Guardrail: part of the 0.12 s candidate budget

S12 (excluding CrowdRoute, story 018) derives the LSC R3 socket records from the stages before it, assigns SocketIds canonically, covers walkable floor with M06 slabs and emits the candidate.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (R-G4 S12, R-G6.4, R-G1, LSC R9.3), scoped to this story:*

- [ ] **AC1** (R-G4 S12, `Sockets.CanonicalIdsAndSchema`): **GIVEN** 50 seeds **THEN** the LSC R3 records are derived (emergence, StartCell, candidate zones, ObjectiveCourt, BushSite, TraversalPoint, NavArea per cell) with no role-named field, SocketIds are assigned canonically and are identical across two runs and two insertion orders.
- [ ] **AC2** (R-G6.4, `Sockets.SlabCover`): **GIVEN** 50 seeds **THEN** walkable floor is covered exactly once, greedily in canonical order, by M06 slabs of 2x2, 2x4, 2x8, 4x4 and 8x8 on 2 m multiples, scored and drain-channel cells stay 2 x 2, and the cover is emitted in the Manifest so clients derive nothing.
- [ ] **AC3** (LSC R9.3 / mirror intercept, `Sockets.SkeletonTransformServerOnly`): **GIVEN** a candidate **THEN** the skeleton symmetry transform is carried in `FLayoutCandidate` only and is absent from the Manifest entry bytes and `LayoutHash`; the decoration rule-set id is returned in `FLayoutCandidate`, equals a constant covered by `GeneratorKitVersion`, and is not a Manifest entry.

---

## Implementation Notes

*Derived from GDD R-G4 S12, R-G6.4 and R-G1:*

Derive the LSC R3 records from the earlier stages (courts S4 as `ObjectiveCourt` records with no role, zones and start cells S5, emergence sockets S7, traversal S8, bushes S9) plus a NavArea per cell. SocketIds are assigned in canonical order (by record type then row-major position or stable block id); an `UnorderedMap` must never feed the assignment. Slab cover (R-G6.4): walkable floor is covered greedily in canonical order by M06 slabs of 2x2, 2x4, 2x8, 4x4 and 8x8 on 2 m multiples (joints stay hidden by the world-aligned material); scored and drain-channel cells stay 2 x 2 (define "scored" as LSC's scored cells and state the greedy size order, largest first, in the notes); the cover is computed on the server and emitted in the Manifest, so clients derive nothing (N_slab 550-800 per F2). The skeleton symmetry transform (the transpose and optional 180-degree rotation S2 used) goes in `FLayoutCandidate` for the layout service; it is not a Manifest field and not part of `LayoutHash`. The decoration rule-set id the GDD says the Generator returns is a constant tied to `GeneratorKitVersion` (the GDD does not name a destination field; this story returns it in `FLayoutCandidate` and flags the choice).

Prototype replaced: none (the prototype had no socket records).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-018-s12-crowd-route.md`: CrowdRoute nodes and edges.
- `story-019-placement-bay-stacks-and-manifest-emission.md`: placing modules and emitting Manifest entries.
- LSC socket record definitions and the required-data check: `level-socket-contract/story-014-socket-records-determinism-required-data.md`; mirror-intercept Selection: `story-021-selection-combos-and-fairness.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageSockets.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenSlabCover.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/CanonicalIdsAndSchemaTest.cpp` (class `FCityGenSocketsCanonicalIdsAndSchemaTest`, name `StealthGame.CityGen.Sockets.CanonicalIdsAndSchema`)
- `Source/StealthGameplay/Private/Tests/CityGen/SlabCoverTest.cpp` (class `FCityGenSocketsSlabCoverTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/SkeletonTransformServerOnlyTest.cpp` (class `FCityGenSocketsSkeletonTransformServerOnlyTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC2 ADEQUATE; AC3 revised and ADEQUATE).*

- **AC1** `Sockets.CanonicalIdsAndSchema`
  - Given: 50 seeds.
  - When: S12 runs.
  - Then: records exist for emergence, StartCell, candidate zones, ObjectiveCourt, BushSite, TraversalPoint and NavArea per cell. Reflected property names are a subset of the LSC R3 schema, with no role-named field. SocketIds are equal across 2 runs and 2 insertion orders.
  - Edge cases: a fixture record with a `bIsFinal` field fails (negative control).
- **AC2** `Sockets.SlabCover`
  - Given: 50 seeds.
  - When: S12 runs.
  - Then: every walkable cell is covered exactly once (0 gaps, 0 overlaps). Sizes are only {2x2, 2x4, 2x8, 4x4, 8x8} on 2 m multiples. The cover equals a reference greedy implementation in canonical order. Scored and drain-channel cells stay 2x2. Slab entries are in the Manifest and the client derives 0.
  - Edge cases: "scored" is LSC's scored cells and the greedy size order is stated in the dev-story notes.
- **AC3** `Sockets.SkeletonTransformServerOnly`
  - Given: a candidate.
  - When: S12 emits the candidate and Manifest.
  - Then: the transform is in `FLayoutCandidate` only. Changing it after emission leaves Manifest bytes and `LayoutHash` unchanged. A byte scan finds no transform serialisation in the Manifest. The decoration rule-set id is in `FLayoutCandidate`, equals the constant covered by `GeneratorKitVersion`, and is not a Manifest entry.
  - Edge cases: bumping `GeneratorKitVersion` changes the id (links it to the kit constant). The Manifest entry count is unchanged by emitting the id.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-013-s7-emergence-alleys-and-exit-points.md`, `story-014-s8-traversal-points.md`, `story-015-s9-bushes-and-s10-props-role-blind.md`, `story-016-s11-repairs-and-occurrence-rule.md`; cross-epic `level-socket-contract/story-008-manifest-struct-and-layout-hash.md`, `story-014-socket-records-determinism-required-data.md`
- Unlocks: `story-018-s12-crowd-route.md`, `story-019-placement-bay-stacks-and-manifest-emission.md`
