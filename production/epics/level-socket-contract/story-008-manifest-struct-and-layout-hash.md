# Story 008: Layout Manifest struct and LayoutHash (D7)

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-002`
*(At write time: TR-lsc-002 = Integer Layout Manifest; clients build with no RNG and recompute `LayoutHash`)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: `FLayoutManifest` is a single GameState property with only the section 8.1 fields and chunked 9-byte entries; `LayoutHash` is an FNV-1a 64 over canonically sorted entries (generated) or one 40-byte record per graybox placement.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Plain structs and integer hashing in a client-safe `View/` header; the graybox extract hash reads collision-relevant placements from the map package (editor/cook tool).

**Why this story**: The hash is the desync detector the handshake (story 024) and the graybox bake (story 026) depend on. Read first: R2 (items 1, 5, 7), R4 item 4, Formula D7, AC-3 and AC-30 (D7 row); ADR-0014 section 6 (the 40-byte record layout) and section 8.1.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Clients build only from a complete `FLayoutManifest` (ADR-0014)
- Forbidden: Never add a field to `FLayoutManifest` beyond section 8.1 (Visibility Policy change request + schema change, always-ask) (ADR-0014)
- Guardrail: Manifest 13.5-31.5 KB once per round for generated layouts (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-3 (Manifest.GeometryOnly, Manifest.GrayboxCookHash) and AC-30 (D7), scoped to this story:*

- [ ] D7: empty -> `0xCBF29CE484222325`; `0x61` -> `0xAF63DC4C8601EC8C`; shuffled entries give the same hash; any one-field change differs; 9 bytes per generated entry and 40 per graybox record, little-endian (`Formula.D7`)
- [ ] GIVEN a published layout, THEN `FLayoutManifest` holds only the ADR-0014 8.1 fields (`LayoutId`, `ProviderKind`, `GeneratorKitVersion`, `LayoutAttempt`, `LayoutHash`, `EntryCount`, `Chunks`) and the graybox Manifest has 0 entries (`Manifest.GeometryOnly`)
- [ ] GIVEN the graybox, WHEN one collision mesh moves 1 cm, THEN `LayoutHash` changes (`Manifest.GrayboxCookHash`)

---

## Implementation Notes

*Derived from ADR-0014 sections 6 and 8.1:*

- `StealthGameplay/Public/View/Core/StealthLayoutManifest.h` (client-safe): `FLayoutManifestEntry` (int16 CellX, CellY; uint8 Course; uint16 ModuleId; uint8 VariantId; uint8 Yaw = 9 bytes), `FLayoutManifestChunk`, `FLayoutManifest`; `ComputeLayoutHash(const FLayoutManifest&)`.
- Generated hash: FNV-1a 64 over `u32 GeneratorKitVersion || u32 count || entries` after canonical sort, little-endian. Graybox hash: one 40-byte record per placement sorted ascending by `(X, Y, Z, MeshIdHash)`: `u32 MeshIdHash, i32 X, Y, Z (cm), i32 Yaw, Pitch, Roll (0.01 degree), i32 ScaleX, ScaleY, ScaleZ (parts per 1,000)`. A 1 cm move must change the hash; sub-cm moves follow the stated rounding.
- A reflection test lists the `UPROPERTY`s of `FLayoutManifest` and fails when any field is added. The Manifest carries no role data (R2.5).
- Golden bytes for the 9-byte entry and the 40-byte record are committed as test fixtures.
- Edge: negative `CellX` (two's complement little-endian), `EntryCount` 0 and 3,500, each of the 10 graybox record fields changed.

---

## Out of Scope

- Story 009: chunk `NetSerialize`, replication, `OnRep` handler, allowlist row
- Story 013: the client builder that consumes the Manifest
- Story 026: the editor bake that produces the graybox extract

---

## Files

- `Source/StealthGameplay/Public/View/Core/StealthLayoutManifest.h`
- `Source/StealthGameplay/Private/View/StealthLayoutManifest.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/ManifestHashTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Formula.D7`
  - Given: FNV-1a 64 over u32 kit || u32 count || 9-byte entries (graybox: u32 fmt || u32 count || 40-byte records), little-endian.
  - When: empty bytes; byte 0x61; one entry list in two shuffled orders; each of the 6 entry fields changed by 1.
  - Then: `0xCBF29CE484222325`; `0xAF63DC4C8601EC8C`; equal hash for both orders; 6 changes give 6 hashes != original; entry is 9 B and graybox record 40 B (golden bytes).
  - Edge cases: EntryCount 0 and 3,500; negative i16 CellX; each of the 10 graybox record fields changed.
- **AC-2** `Manifest.GeometryOnly`
  - Given: a published graybox layout and a generated fixture.
  - When: `FLayoutManifest` UPROPERTYs are reflected.
  - Then: exactly {LayoutId, ProviderKind, GeneratorKitVersion, LayoutAttempt, LayoutHash, EntryCount, Chunks}; graybox EntryCount 0 and no entries.
  - Edge cases: adding any field fails the test.
- **AC-3** `Manifest.GrayboxCookHash`
  - Given: an extract fixture of N mesh records.
  - When: one record moves +1 cm in X, then Y, then Z, then yaw +0.01 degree.
  - Then: LayoutHash differs for each; unchanged input gives an identical hash.
  - Edge cases: sub-cm moves follow the stated rounding. The real-map version is covered by stories 027/028.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/ManifestHashTest.cpp` — `StealthGame.Layout.Formula.D7`, `StealthGame.Layout.Manifest.GeometryOnly`, `StealthGame.Layout.Manifest.GrayboxCookHash` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-001-service-skeleton-provider-registry.md`
- Unlocks: `production/epics/level-socket-contract/story-009-manifest-chunked-replication-late-join.md`, `production/epics/level-socket-contract/story-013-client-geometry-builder-and-decoration.md`, `production/epics/level-socket-contract/story-026-graybox-provider-markers-and-bake.md`
