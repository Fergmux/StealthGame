# Story 009: S3 Area types

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
**ADR Decision Summary**: S1-S12 are pure integer functions of earlier stages; area types use the LSC R1 vocabulary and are assigned on the skeleton before fill, so zone parity (V-ZONE) holds by construction.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. The Market plaza is a mirror pair because a rectangle on the axis would not be transpose-symmetric, and a square >= 24 m fails V-SIGHT on its diagonal.

**Control Manifest Rules (this layer)**:
- Required: Pure data, integer streams, canonical iteration — source: ADR-0014, ADR-0006
- Forbidden: Never let an area type depend on a role, team or zone assignment (the Generator never sees `FSiteAssignment`) — source: ADR-0014
- Guardrail: part of the 0.12 s candidate budget

S3 places one Temple precinct on the axis near the centre, two Market plazas (a mirror pair, 12 x 8 c each), Causeway blocks lining the canals, and Residential for the rest (45-55% of walkable).

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (R-G4 S3, Edge Cases "mirror pair of a zone"), scoped to this story:*

- [ ] **AC1** (R-G4 S3, `AreaTypes.TempleAndPlazas`): **GIVEN** 50 seeds **THEN** one Temple precinct has its centre on x = y within 10 c of the grid centre (one block pitch), pyramid base <= 8 x 8 c, platform 1-2 courses, walkable only at courses 0-2 (<= 3.2 m), tiers above sealed and uncoped; and two Market plazas are a mirror pair across x = y, each 12 x 8 c (diagonal 28.8 m) with no ingress collinear with the long axis unless it jogs within 4 c of the edge.
- [ ] **AC2** (`AreaTypes.ShareAndBlocks`): **GIVEN** 50 seeds **THEN** Residential is 45-55% of walkable cells, Causeway blocks line the canals, and an alley or court takes its block's area type.
- [ ] **AC3** (R-G4 S3 / Edge Cases, `AreaTypes.AssignedBeforeFill`): **GIVEN** a candidate **THEN** area types are assigned on the skeleton before fill so mirror cells carry identical types (V-ZONE parity by construction), and fill changes only bays, props, bushes and alleys within a half.

---

## Implementation Notes

*Derived from GDD R-G4 S3:*

Temple: pyramid base <= 8 x 8 c, platform of 1-2 courses, walkable only at courses 0-2 (<= 3.2 m), tiers above sealed and uncoped. "Near the centre" is not quantified by the GDD: the story uses within 10 c (one block pitch, Euclidean between cell centres, the grid centre being the point between the four central cells since N is even) and flags it for the level-designer. Markets: a mirror pair (the GDD's reason: a rectangle on the axis would not be transpose-symmetric), 12 x 8 c (24 x 16 m, diagonal 28.8 m); no ingress collinear with a plaza's long axis unless it jogs within 4 c of the edge (V-SIGHT). Causeway blocks line the canals (vacuous at tier G0, where canals are disabled and Causeway blocks become Residential, so the Causeway clause runs on a canal-enabled fixture). Residential is the rest: 45-55% of walkable, inclusive; state whether the denominator is measured at S3 or on the final grid (take the final walkable grid after S11 for the batch audit, and the S3 estimate as a guard here). The S3 output is a type grid; the S6 bay recipe (Residential <= 2 storeys, Market 1, Causeway low) and the G-PRE V-ZONE parity read it.

Prototype replaced: the prototype's halls, temple, stalls and gardens were placed in a fixed grid without parity or area-share rules; only the idea of a central temple carries over.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-006-s2-skeleton-lattice-widths-jogs-transpose-and-jitter.md`: the lattice the types are assigned on.
- `story-010-s4-courts.md`, `story-011-s5-zones-anchors-and-start-cells.md`: courts and zones (including the temple-within-15 m anchor slide).
- `story-012-s6-facade-bay-sequencer-and-f3-periodicity.md`: recipes per area type.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageAreaTypes.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/TempleAndPlazasTest.cpp` (class `FCityGenAreaTypesTempleAndPlazasTest`, name `StealthGame.CityGen.AreaTypes.TempleAndPlazas`)
- `Source/StealthGameplay/Private/Tests/CityGen/ShareAndBlocksTest.cpp` (class `FCityGenAreaTypesShareAndBlocksTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/AssignedBeforeFillTest.cpp` (class `FCityGenAreaTypesAssignedBeforeFillTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after the revision of AC1).*

- **AC1** `AreaTypes.TempleAndPlazas`
  - Given: 50 seeds.
  - When: S3 runs.
  - Then: the temple centre is on x = y and within 10 c of the grid centre. The pyramid base is at most 8x8 c, the platform is 1-2 courses, and it is walkable only at courses 0-2 (3.2 m or less). Tiers above are sealed and carry no coping tag. Two Market plazas, 12x8 c each, are exact transposes of each other. No ingress is collinear with a plaza's long axis unless it jogs within 4 c of the edge.
  - Edge cases: the distance is Euclid between cell centres. Boundaries: a distance of 10.0 c passes and 10.1 c fails. A 9x8 c base fails. A plaza at 12x9 c fails.
- **AC2** `AreaTypes.ShareAndBlocks`
  - Given: 50 seeds.
  - When: S3 runs.
  - Then: Residential is 45-55% of walkable cells, inclusive. Every block flanking a canal is Causeway. Alley and court cells carry their block's type.
  - Edge cases: G0 has no canals, so the Causeway check is vacuous. Run it on a canal-enabled params fixture.
- **AC3** `AreaTypes.AssignedBeforeFill`
  - Given: a candidate.
  - When: S3 completes, then S4-S12 run.
  - Then: the type grid has type(x,y) == type(y,x) for every cell, with 0 mismatches. The area-type layer hash is identical before and after fill.
  - Edge cases: mutating one mirror cell fails parity (negative control).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-006-s2-skeleton-lattice-widths-jogs-transpose-and-jitter.md`, `story-007-f7-skeleton-fairness-pre-check.md`
- Unlocks: `story-010-s4-courts.md`, `story-011-s5-zones-anchors-and-start-cells.md`, `story-012-s6-facade-bay-sequencer-and-f3-periodicity.md`
