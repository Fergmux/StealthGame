# Story 019: Predicates V-COURT, V-POOL, V-BUSH and V-CLUE with the court ClueTag

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`, `TR-lsc-010`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; TR-lsc-010 = Per-court public `ClueTag` as a pure function of published geometry, rule version in `GeneratorKitVersion`, V-CLUE Reject predicate reading `ClueMatchMaxCount`. Note: TR-lsc-010 exists in the registry but is not in the epic's requirements table of 9; the epic table should be updated.)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Sun and sight predicates use geometric raycasts on the `ViewBlock` channel at a fixed sun direction (not Lumen); the per-court `ClueTag` is a role-blind pure function of published geometry exposed server-side in `FLayoutSocketView` (no Manifest or wire change) once the GDD delivers it (delivered 2026-10-25).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `SunAzimuthDeg` is injected as a parameter because its ownership is still open (LSC Open Question 4, closed by story 040); the raycast is geometric on a 0.5 m grid.

**Why this story**: Courts, sun pools and bushes are the Pillar-1 equivalence predicates (every court looks identical; bushes are balanced), and V-CLUE keeps a stage-card clue from naming a single court. Read first: R3 ObjectiveCourt and BushSite rows, R7 rows V-COURT, V-POOL, V-POOL-40, V-BUSH-1..3, V-CLUE, AC-22.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every V-* predicate exists once against `ILayoutMeasureBackend`; every sight test uses `ViewBlock` (ADR-0014, ADR-0015)
- Forbidden: Never add a field to `FLayoutManifest` beyond section 8.1 (ADR-0014)
- Guardrail: work bounded by `LayoutSliceWorkUnitsLimit` (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-22 (subset), R3 and R7, revised after QL-STORY-READY, scoped to this story:*

- [ ] GIVEN V-COURT and V-POOL fixtures, THEN each fails with exactly its id and the V-POOL sun raycast (`SunAzimuthDeg` injected) is byte-identical over 2 runs (`Validate.PredicateMatrix.<Id>`)
- [ ] GIVEN V-POOL-40 and V-BUSH-3 failures, THEN a Warn is logged and the attempt does not fail; with `bSunPoolBeaconIsReject` = true V-POOL-40 becomes a Reject; the all-pass fixture passes (`Validate.WarnPredicates`)
- [ ] GIVEN V-BUSH-1 and V-BUSH-2 fixtures (spacing, sector cap, Dome Ban, a 2.0 m planter must hold a bush), THEN each fails with exactly its id (`Validate.PredicateMatrix.<Id>`)
- [ ] GIVEN a `ClueTag` computed by one rule from published geometry (rule version part of `GeneratorKitVersion`), THEN a tag shared by more than `ClueMatchMaxCount` (3) courts fails V-CLUE; the tag is identical across 2 runs and a different rule version changes `GeneratorKitVersion`; and `ClueTag` is not a field of `FLayoutManifest` nor of any replicated property (clients derive it by the same pure function if needed; it is exposed server-side in `FLayoutSocketView`) (`Validate.Clue`)

---

## Implementation Notes

- V-COURT: courts meet R3 (6 x 6 to 8 x 8 m, 8 x 8 in Market), at least `CourtSpacingCm` (24 m) apart centre to centre, no bush inside or within `BushCourtExclusionCm` (6 m) of the edge, no `CrowdRoute` edge through them, host-area prop density within 20%; spacing exactly 24.0 m passes.
- V-POOL: every court has a contiguous sun-lit rectangle of at least 6 x 6 m containing its centre (geometric raycast on a 0.5 m grid, at least `SunPoolSamplesMinCount` 144 samples; 143 fails), within `SunPoolParityPct` (20%) of the median court (D9).
- V-POOL-40 (no sun pool visible along a ground line longer than `SunPoolBeaconRangeCm` 40 m) is `Warn` in MVP (`bSunPoolBeaconIsReject` false); V-BUSH-3 backdrop (at least 50%, floor 40%, of dome silhouette rays hit Limewash, Paving or Sky) is always `Warn` (the 25 m greyscale test, story 043, decides).
- V-BUSH-1: count per D5, spacing at least 12 m (exactly 12.0 m passes), at most 2 per 40 m sector, rows of at most 3, 1.0 m open floor on at least 2 sides, no recess, within 4 m of a route node, 6 m from any ExitPoint. V-BUSH-2 Dome Ban: no non-bush mesh is a closed convex form with width:height 0.8-1.4 and height 1.3-2.3 m; every 2.0 x 2.0 planter holds a bush; no decorative planter is 2.0 m square.
- `ClueTag` = area type + nearest distinctive public feature, a pure function of the published geometry computed by the same rule on every court; its rule version is part of `GeneratorKitVersion` so the D7 hash covers it. Reflection test over replicated properties and `FLayoutManifest`.
- Pending confirmation with the Objectives owner (QL note): the "server-side only" exposure wording is from ADR-0014 7.4, not R3.

---

## Out of Scope

- The 25 m greyscale and swatch human tests (story 043); the Dome-Ban art audit; Selection (021)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/Predicates/CourtPoolBushPredicates.cpp`
- `Source/StealthGameplay/Private/Core/Layout/CourtClueTag.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/PredicateMatrixCourtTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Validate.PredicateMatrix.<V-COURT|V-POOL>`
  - Given: one V-COURT and one V-POOL fixture; `SunAzimuthDeg` injected.
  - When: validated; V-POOL raycast run twice.
  - Then: each fails with exactly its id; the V-POOL result is byte-identical over 2 runs.
  - Edge cases: court spacing 24.0 m passes; pool 144 samples passes, 143 fails.
- **AC-2** `Validate.WarnPredicates`
  - Given: V-POOL-40 and V-BUSH-3 failing fixtures; `bSunPoolBeaconIsReject` false then true; the all-pass fixture.
  - When: validated.
  - Then: false gives a Warn and no attempt failure for both; true makes V-POOL-40 fail the attempt; all-pass passes.
  - Edge cases: V-BUSH-3 never becomes a Reject.
- **AC-3** `Validate.PredicateMatrix.<V-BUSH-1|V-BUSH-2>`
  - Given: fixtures for spacing 12 m, sector cap 2, rows of at most 3, Dome Ban, a 2.0 m planter without a bush.
  - When: validated.
  - Then: each fails with exactly its id.
  - Edge cases: spacing 12.0 m passes.
- **AC-4** `Validate.Clue`
  - Given: courts with a ClueTag from one rule; 4 courts sharing a tag; then 3.
  - When: V-CLUE runs; computed twice; the rule version changes.
  - Then: 4 fails, 3 passes; tags identical across runs; a new rule version changes `GeneratorKitVersion`; ClueTag is not on `FLayoutManifest` or any replicated property; it is exposed in `FLayoutSocketView`.
  - Edge cases: reflection over replicated properties.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/PredicateMatrixCourtTest.cpp` — `StealthGame.Layout.Validate.PredicateMatrix.<Id>` for V-COURT, V-POOL, V-BUSH-1, V-BUSH-2, plus `.WarnPredicates` and `.Clue` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-014-socket-records-determinism-required-data.md`
- Cross-epic: `production/epics/objectives-chain/EPIC.md` (`ClueMatchMaxCount` is Objectives-owned, read only)
- Unlocks: `production/epics/level-socket-contract/story-021-selection-combos-and-fairness.md`, `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
