# Story 006: S2 Skeleton: lattice, widths, jogs, transpose and jitter

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
**ADR Decision Summary**: S1-S12 are pure integer functions of their stage stream and earlier stages, so the whole city is a deterministic function of the attempt seed, kit version and params; the grid pre-validator rejects most bad cities before any geometry exists.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. Skeleton symmetry is the transpose across x = y, the sun axis (art bible 2.1, azimuth 45 degrees to grid X), which preserves sun and shade exactly.

**Control Manifest Rules (this layer)**:
- Required: Single cancellable worker task over pure data; integer PCG32 stage streams; canonical iteration — source: ADR-0014, ADR-0006
- Forbidden: No floats in a draw; no unordered container as draw input — source: ADR-0006
- Guardrail: the whole candidate (S1-S12 + G-PRE) <= 0.12 s on one worker (F5; measured in story 042)

S2 builds the street/alley/canal/causeway skeleton: a regular-pitch lattice generated for the half x >= y and reflected across x = y with identical topology, cut positions jittered by up to one cell per half (`CityGenSkeletonJitterCount`), optionally rotated 180 degrees (the only rotation that keeps the axis on the sun line).

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G4 first clause, AC-G5, R-G4 S2, F1 example), scoped to this story:*

- [ ] **AC1** (AC-G4 first clause, `Skeleton.TransposeSymmetry`): **GIVEN** 50 seeds **THEN** every skeleton's topology is invariant under the x = y transpose (cut positions jittered +-1 c per half with identical topology) and the only seed rotation is 180 degrees.
- [ ] **AC2** (AC-G5, `Skeleton.RunAndJunctionRules`): **GIVEN** 50 seeds **THEN** no straight corridor run exceeds 14 c wall to wall (12 c plus the junction), runs are broken by a jog of at least the corridor's clear width (2 c alleys, 3 c streets and canals), and every junction is a 3 x 3 c T (no 4-way crossing).
- [ ] **AC3** (R-G4 S2, `Skeleton.LatticeAndMix`): **GIVEN** 50 seeds **THEN** the lattice has 5 block rows and 5 columns of 7-10 c (25 lattice blocks; the temple precinct and the two plazas may merge blocks so 20-25 separate blocks remain), corridor widths are alley 2 c, street 3 c, canal 3 c of water plus quays carved 1 c into flanking blocks, causeway 6 c; **AND GIVEN** a canal-enabled params fixture (tier G1) **THEN** canals are <= 12% of corridor length with exactly one per half crossed at intervals of at most 12 c along its length, causeway <= 5% (<= 12 c), and a canal never meets the outer skirt without a sealed end.

---

## Implementation Notes

*Derived from GDD R-G4 S2 and the F1 example:*

Generate the half x >= y: 5 blocks per axis of 7-10 c (mean 8; `CityGenBlockSizeMinCount`/`MaxCount`), corridor widths alley 2 c, street 3 c, canal 3 c of water plus quays carved 1 c into the flanking blocks, causeway 6 c; every straight run <= `CityGenRunMaxCount` (12) c of corridor plus the junction (<= 14 c = 28 m wall to wall); runs are broken by a jog offset by at least the corridor's clear width; every junction is a staggered T of 3 x 3 c, no 4-way. Mix by length: streets ~50%, alleys ~33%, canals <= `CityGenCanalShareMaxPct` (12%, one per half, crossing at least every 12 c), causeway <= 5% (<= 12 c). Reflect across x = y (transpose), then jitter cut positions by +-`CityGenSkeletonJitterCount` c per half with identical topology; the seed may rotate the result 180 degrees. N_int = N - 6 (56-60 cells) must hold the 5 x 5 lattice (the F1 example: `5 * 8 + 6 * 3 = 58`).

Expose jitter as an explicit per-cut record (cut id, jitter offset, half) so the OQ9 script (story 008) and S4 (story 010) can apply either anchor policy ("anchors fixed" or "anchors move with the jitter"); the GDD leaves that choice OPEN until story 008 reports. Canal and causeway checks are vacuous at tier G0 (canals disabled), so the canal clause of AC3 runs on a canal-enabled params fixture. The 20-25 separate block count is the author's reading of "5 blocks per axis, 20-25 blocks": 25 lattice blocks before S3 merges blocks for the temple precinct and the plazas (flagged for the GDD owner).

Prototype replaced: the prototype's fixed grid city (no skeleton, no jogs, no canals) is not carried over.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-f7-skeleton-fairness-pre-check.md`: the node-level fairness gate on the skeleton.
- `story-009-s3-area-types.md`: temple, plazas and area types (and block merging).
- `story-010-s4-courts.md`, `story-011-s5-zones-anchors-and-start-cells.md`: courts, anchors and the jitter-anchor policy.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageSkeleton.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenSkeleton.h` (node graph, cut records)
- `Source/StealthGameplay/Private/Tests/CityGen/TransposeSymmetryTest.cpp` (class `FCityGenSkeletonTransposeSymmetryTest`, name `StealthGame.CityGen.Skeleton.TransposeSymmetry`)
- `Source/StealthGameplay/Private/Tests/CityGen/RunAndJunctionRulesTest.cpp` (class `FCityGenSkeletonRunAndJunctionRulesTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/LatticeAndMixTest.cpp` (class `FCityGenSkeletonLatticeAndMixTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after the revision of AC3).*

- **AC1** `Skeleton.TransposeSymmetry`
  - Given: 50 seeds.
  - When: S2 runs.
  - Then: the topology (nodes, edges, corridor types) is identical under (x,y) to (y,x), with cut positions jittered at most +-1 c per half. Seed rotation is only 0 or 180 degrees.
  - Edge cases: an unmirrored-corridor fixture fails. A jitter of 2 c fails. A 90-degree rotation fails.
- **AC2** `Skeleton.RunAndJunctionRules`
  - Given: 50 seeds.
  - When: S2 runs.
  - Then: the longest straight run is at most 14 c wall to wall. Runs are broken by a jog of at least the clear width (alley 2 c, street or canal 3 c). Every junction is a 3x3 c T and there are 0 four-way crossings.
  - Edge cases: fixtures with a 15 c run, a 1 c jog in a street, and a 4-way crossing each fail.
- **AC3** `Skeleton.LatticeAndMix`
  - Given: 50 G0 seeds, then 50 seeds on a canal-enabled G1 fixture.
  - When: S2 runs, and S3 runs for the block count.
  - Then: the lattice has 5 rows by 5 columns of 7-10 c, which is 25 lattice blocks. After S3 merges (temple and plazas), 20-25 separate blocks remain. Widths are alley 2 c and street 3 c. On the G1 fixture, canal is 3 c water plus 1 c quays and causeway is 6 c; canals are at most 12% of corridor length with exactly 1 per half; consecutive crossings are at most 12 c apart along the canal; causeway is at most 5% (at most 12 c). A canal never meets the skirt without a sealed end.
  - Edge cases: canal and causeway width checks are vacuous at G0, so they run on the G1 fixture. A canal stretch of 13 c between crossings fails. The canal-end to first-crossing distance counts toward the 12 c. Block sizes must fit N_int (56-60).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-005-grid-model-s1-frame-and-f1-walkable-target.md`, `story-003-integer-stage-streams-and-canonical-iteration.md`
- Unlocks: `story-007-f7-skeleton-fairness-pre-check.md`, `story-008-oq9-final-court-enumeration-script.md`, `story-009-s3-area-types.md`
