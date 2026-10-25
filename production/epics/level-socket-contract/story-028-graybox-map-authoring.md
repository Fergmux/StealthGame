# Story 028: Graybox map authoring and Graybox.Validates

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Config/Data
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`, `TR-lsc-008`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; TR-lsc-008 = Pillar-1 placement constraints (no area type for spawns/objectives))*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The hand-built graybox map is a provider-backed layout that runs the same Validate and Select as a generated one; it carries no saved `ARecastNavMesh`, one `ANavMeshBoundsVolume`, and is the permanent fallback. Its markers hold `ZoneId` only.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: This is a Content step done in the editor (no byte-editing `.umap`); a saved `ARecastNavMesh` in the map overrides the project generation mode [U] and must be removed in the editor.

**Why this story**: The MVP needs a real map that passes every Reject predicate and fits all the counts. Read first: R11 item 5 (authoring checklist), D4, D5, D6, Open Questions 11 and 14, AC-28; `production/first-playable-report.md` known issue 9 (the sample traversal course in `L_Prototype`).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Graybox navmesh built at runtime exactly like a generated one; the service asserts `Dynamic` generation (ADR-0014 4.3)
- Forbidden: Never cook graybox markers as runtime actors; never author to the superseded 60 m final rule (GDD R11.5)
- Guardrail: graybox navmesh <= 1 s (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-28, R11.5 and Open Questions 11/14 (revised after QL-STORY-READY: symmetry review made observable), scoped to this story:*

- [ ] GIVEN the authored graybox, THEN it passes every Reject predicate (`Graybox.Validates`)
- [ ] THEN it holds 4 candidate zones with 6 start cells each within 18 m of the anchor (each used zone with at least 2,000 m2 walkable in its 35 m disc), at least 6 final-eligible courts at least 44 m by path from every start cell each with at least 3 ingresses, 12-13 alleys with one socket each, 8-9 bushes, and 8,000-8,800 m2 walkable
- [ ] THEN the combo enumeration (Open Questions 11 and 14) has run, `ChainLengthMinCm`/`ChainLengthMaxCm` are set from the histogram and recorded, and a recorded level-design review shows every mirrored pair of halves differs in at least N placements (N set by the level designer in the review sheet before the review), so the layout has no exact symmetry
- [ ] THEN the sample's traversal course is removed, the map carries no saved `ARecastNavMesh`, and one `ANavMeshBoundsVolume` covers the grid

---

## Implementation Notes

*From GDD R11.5 (authoring order matters because about 50 features each carry 6 m exclusions):*

1. Courts first (D5; at least 24 m apart; 8 x 8 in Market areas; at least 3 ingresses each if the enumeration still fits, Open Question 14), then zones (4 candidates at the map's sides; no line of sight between start sets; at least 70 m of path; `T_contact` at least 15 s; the tightest constraint, confirm first), then alleys (12-13, one socket each), then bushes (8-9), then traversal (derived from coping-tagged kit pieces).
2. 6 m streets and 4 m alleys (2.8 m clear); staggered T-junctions every 32 m or less; below `NpcCount` 45 each candidate zone needs at least 2,000 m2 walkable in its 35 m disc.
3. Final-eligible courts are those at least `StartToFinalMinPathCm` (44 m) by path from every start cell with parity between zone pairs; do not author to the superseded 60 m rule.
4. Mirrored halves pass V-FAIR easily but create a visible symmetry: follow with an asymmetric edit pass; record the review sheet.
5. Run the combo enumeration on the plan, set the chain band from the histogram (D4 is provisional at 120-240 m); if 16 courts do not fit the first lever is `CourtSpacingCm`, then `FinalCandidatesMinCount` back toward `TeamSize` with a creative-director check of SP-4 (Open Question 11).
6. The map starts from a copy of `/Game/StealthGame/Maps/L_Prototype`; the sample traversal course to the north is removed; saved nav data actors are removed in the editor and the per-map `RuntimeGeneration=Dynamic` override is superseded by the project setting.
7. Evidence is a smoke check (`production/qa/smoke-lsc-graybox-*.md`) plus the commandlet report and the enumeration histogram under `production/qa/evidence/level-socket/`.
- No `.uasset`/`.umap` is written by this story's planning run; the editor session does it. Re-run `Graybox.Validates` after every edit (CI gate, story 027).

---

## Out of Scope

- Story 026/027: markers, bake and commandlet; stories 040-042: lighting and bush value checks on this map; the Procedural City Generator's layouts

---

## Files

- `Content/StealthGame/Maps/L_Graybox.umap` (editor Content step) and its baked `UStealthLayoutSocketData` asset
- `production/qa/smoke-lsc-graybox-validates.md`
- `production/qa/evidence/level-socket/graybox-enumeration-histogram.md`
- `Source/StealthGameplay/Private/Tests/Layout/GrayboxValidatesTest.cpp` (`StealthGame.Layout.Graybox.Validates`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Graybox.Validates`
  - Setup: authored graybox.
  - Verify: the validation commandlet.
  - Pass condition: every Reject predicate passes.
- **AC-2** content counts
  - Setup: measure the map.
  - Verify: 4 candidate zones with 6 start cells each within 18 m of the anchor; each used zone >= 2,000 m2 walkable in its 35 m disc; >= 6 final-eligible courts >= 44 m by path from every start cell, each with >= 3 ingresses; 12-13 alleys with 1 socket each; 8-9 bushes; 8,000-8,800 m2 walkable.
  - Pass condition: all counts in range.
- **AC-3** enumeration and asymmetry
  - Setup: the combo enumeration (OQ 11, 14) and the level-design review sheet.
  - Verify: `ChainLengthMinCm`/`MaxCm` recorded from the histogram; the review sheet records N before review and each mirrored pair differs in >= N placements.
  - Pass condition: all values recorded and met.
- **AC-4** nav and sample cleanup
  - Setup: scan the map.
  - Verify: the sample's traversal course is removed; no saved `ARecastNavMesh`; exactly one `NavMeshBoundsVolume` covers the grid.
  - Pass condition: all three true.

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass in `production/qa/smoke-lsc-graybox-validates.md`, plus the automation test `StealthGame.Layout.Graybox.Validates` passing

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-017-predicates-navigation-area-sight-route-traversal.md`, `production/epics/level-socket-contract/story-018-predicates-zones-separation-reach-respawn.md`, `production/epics/level-socket-contract/story-019-predicates-courts-pool-bushes-clue.md`, `production/epics/level-socket-contract/story-021-selection-combos-and-fairness.md`, `production/epics/level-socket-contract/story-026-graybox-provider-markers-and-bake.md`, `production/epics/level-socket-contract/story-027-graybox-validate-commandlet-and-ci-gate.md`
- Unlocks: `production/epics/level-socket-contract/story-029-start-placement-same-function-zone-fill.md`, `production/epics/level-socket-contract/story-038-batch-audits-pillar1.md`, `production/epics/level-socket-contract/story-039-cook-leak-scan.md`, `production/epics/level-socket-contract/story-040-sun-azimuth-and-cornice-shade-oi4.md`
