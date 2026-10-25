# Story 019: Placement, bay stacks and Manifest emission

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. The 3,500-entry cap itself is asserted by story 027; this story builds the entries and their size model.)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: `FLayoutManifest` is an integer, chunked, replicated structure with an FNV-1a `LayoutHash` and `Bytes = 8 + 9 x entries` (LSC D7); every module sits on the 2.0 m grid and 1.6 m course with exact 90-degree yaw, scale exactly 1, never mirrored; bay stacks are derived entries that keep the entry count under the cap.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. The Manifest entry struct and hash come from the LSC (`level-socket-contract/story-008-manifest-struct-and-layout-hash.md`); this story fills it.

**Control Manifest Rules (this layer)**:
- Required: Clients build only from a complete `FLayoutManifest`; the Manifest carries only the section 8.1 fields — source: ADR-0014
- Forbidden: Never add a Manifest field; never place a mirrored module (determinant > 0 asserted) — source: ADR-0014, GDD R-G6.1
- Guardrail: Manifest <= `CityGenManifestEntriesMaxCount` (3,500 = 31.5 KB raw); 13.5-31.5 KB once per round — source: ADR-0014

With bay stacks the F2 model gives about 2,400-2,900 entries (21.6-26.1 KB raw); without them about 4,100-4,800, which exceeds the cap, so stacks are required unless R1 and the Manifest wire-size spike both clear the larger figure.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G11, AC-G27, R-G6.1, R-G6.3, F2), scoped to this story:*

- [ ] **AC1** (AC-G11, `Placement.GridYawNoMirror`): **GIVEN** 50 seeds **THEN** every instance lies on the 2.0 m grid and 1.6 m course, yaw is in {0, 90, 180, 270}, scale is exactly 1 and the transform determinant is > 0.
- [ ] **AC2** (R-G6.3, `Manifest.BayStacksAndEntries`): **GIVEN** 50 seeds **THEN** a ground bay with its optional upper storey and cornice is one derived stack entry (k_bay = 1), vault and climb walls, M05, M07, M08 and M12 stay separate entries, and every entry is a valid LSC `FLayoutManifest` entry.
- [ ] **AC3** (AC-G27, `Formula.InstanceModel`, ADVISORY): **GIVEN** 50 seeds **THEN** the F2 estimate is within +-15% of the measured entry count and the Manifest size equals 8 + 9 x entries bytes exactly.

---

## Implementation Notes

*Derived from GDD R-G6.1, R-G6.3 and F2:*

Placement: every module on the 2.0 m grid and 1.6 m course, pivot bottom-left (state the pivot convention: cell corner), yaw from the facade normal (exact 4 rotations), scale exactly 1.0, never mirrored; projections (cornice, talud) overhang the neighbour's air only (art 6.2). Bay stacks: a ground bay plus its optional upper storey and cornice is one derived mesh baked by script from the same module sources (not a new authored variant; allowed before the mass-production gate by user decision 2026-10-25); kept separate: vault and climb walls (traversal exactness), M05, M07, M08, M12. F2: `N_entries = N_slab + N_bay * k_bay + N_corner + N_roof + N_prop + N_water + N_temple + N_trav + N_bush + N_fixture` with `N_bay ~ 2 * N_walk / w_bar - E_open`; example N_walk 2,160, w_bar 3.2, E_open 200 gives 2,615 entries and 23,543 bytes. M07, M08 and M12 exist only at tier G1, so the "stay separate" clause of AC2 uses a G1 fixture. AC3 is ADVISORY (a failure reports, never blocks); nothing here asserts the 3,500-entry cap (story 027 does) or `ManifestOverflow` (story 025).

Prototype replaced: the prototype spawned one actor per house/hall/wall; the Manifest + ISM model replaces that entirely.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-kit-tables-and-generator-kit-version.md`: stack and slab tables as data.
- `story-017-s12-sockets-socket-ids-slab-cover-and-skeleton-transform.md`: slab cover and socket records.
- `story-025-failure-modes-and-teardown.md`, `story-027-instance-entry-and-shape-budget-audit.md`: overflow handling and the cap audit.
- `story-026-server-kit-spawn-from-manifest.md`: spawning from the entries.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenEmit.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenPlacement.h`
- `Source/StealthGameplay/Private/Tests/CityGen/GridYawNoMirrorTest.cpp` (class `FCityGenPlacementGridYawNoMirrorTest`, name `StealthGame.CityGen.Placement.GridYawNoMirror`)
- `Source/StealthGameplay/Private/Tests/CityGen/BayStacksAndEntriesTest.cpp` (class `FCityGenManifestBayStacksAndEntriesTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/InstanceModelTest.cpp` (class `FCityGenFormulaInstanceModelTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Placement.GridYawNoMirror`
  - Given: 50 seeds.
  - When: placement runs.
  - Then: every instance is on the 2.0 m grid and 1.6 m course. Yaw is in {0, 90, 180, 270}, scale is exactly 1, and the determinant is above 0.
  - Edge cases: a scale of -1 or a 45-degree yaw fails. The pivot convention (cell corner) is stated.
- **AC2** `Manifest.BayStacksAndEntries`
  - Given: 50 seeds.
  - When: the Manifest is emitted.
  - Then: a ground bay with its optional upper storey and cornice is one entry (k_bay = 1). Vault and climb walls, M05, M07, M08 and M12 are separate entries. Every entry validates as an LSC `FLayoutManifest` entry.
  - Edge cases: a bay with no upper storey is still one entry. M07, M08 and M12 are absent at G0, so use a G1 fixture.
- **AC3** `Formula.InstanceModel` (ADVISORY)
  - Given: 50 seeds.
  - When: the Manifest is serialised.
  - Then: the F2 estimate is within +-15% of the measured entry count. Manifest size equals 8 + 9 x entries exactly.
  - Edge cases: a failure reports and doesn't block.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-012-s6-facade-bay-sequencer-and-f3-periodicity.md`, `story-017-s12-sockets-socket-ids-slab-cover-and-skeleton-transform.md`, `story-018-s12-crowd-route.md`, `story-004-kit-tables-and-generator-kit-version.md`; cross-epic `level-socket-contract/story-008-manifest-struct-and-layout-hash.md`
- Unlocks: `story-023-candidate-loop-work-unit-cap-and-metrics.md`, `story-026-server-kit-spawn-from-manifest.md`
