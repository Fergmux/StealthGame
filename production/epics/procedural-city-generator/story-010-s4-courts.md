# Story 010: S4 Courts

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Story 008 report (2026-10-25, verdict CONCERNS)**: `production/qa/spikes/2026-10-25_citygen-oq9-final-court-enumeration.md`. Adopted starting values: anchor-jitter rule MOVING (identical to fixed at jitter 0), `CityGenSkeletonJitterCount` 0 and start-cell radius 16 m at `StartToFinalMinPathCm` 44 m (a_R16+j0: 123/200 skeletons reach 6 under both policies; the defaults 44 m / 18 m / jitter 1 reach only 15-20/200). `FinalCandidatesMinCount` stays 6. The GDD/LSC text change (jitter default 0, radius 16 m) is a level-designer follow-up.
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
**ADR Decision Summary**: S1-S12 are pure integer stages; "final-eligible" is **not placed** as a role: it falls out of the LSC distance predicates (V-SEP), which the grid pre-validator evaluates. The Generator never sees `FSiteAssignment`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. Geometry facts used: courts 3 x 3 to 4 x 4 c, ingress >= 2.8 m clear (more than two capsule diameters, LSC `NavMinClearWidthCm`), fixture at the court centre with >= 1.2 m clear floor on all four faces (four interact sides, Objectives CR-8).

**Control Manifest Rules (this layer)**:
- Required: Pure data, integer streams; court records carry no role field — source: ADR-0014, GDD S4
- Forbidden: Never store "final-eligible" or any role on a court; never lower `FinalCandidatesMinCount` to make a layout pass — source: GDD S4, LSC D4
- Guardrail: part of the 0.12 s candidate budget

S4 places 16-18 courts in strata: 3 axis-corner courts per axis corner (the final-eligible pool, target `CityGenFinalEligibleTargetCount` 6), 3 zone-corner courts per quadrant and 4-6 temple-ring courts, as widened junctions first and block courtyards only in the zone-corner quadrants. Consequence recorded in the GDD: every temple-ring junction is <= about 58 m path from one anchor, so no temple-ring court is ever final-eligible and finals always fall in the two axis corners (Open Question 5 owns that structural read).

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (R-G4 S4, AC-G38 first clauses, AC-G34 pool clause), scoped to this story:*

- [ ] **AC1** (R-G4 S4, `Courts.StrataAndSpacing`): **GIVEN** 50 seeds **THEN** court count is in [`CityGenCourtTargetCount`, target + 2] composed of 6 axis-corner, 6 zone-corner and 4-6 temple-ring courts; block courtyards (2 openings) appear only in zone-corner quadrants and only after every spacing-legal widened junction is used; courts are 3 x 3 to 4 x 4 c (4 x 4 in Market), >= 24 m apart and >= 6 m from any ExitPoint.
- [ ] **AC2** (AC-G38 first clauses, `Courts.ClearWidthsAndIngress`): **GIVEN** 50 seeds **THEN** every court ingress has >= 2.8 m clear, every fixture has >= 1.2 m clear floor on all four faces, and every court that can be final-eligible has >= 3 ingresses.
- [ ] **AC3** (S4 structural rule, `Courts.NoRoleField`): **GIVEN** 50 seeds **THEN** "final-eligible" is not stored as a role (it falls out of the LSC distance predicates), no temple-ring court is final-eligible, and S4 anchor positions follow the anchor-jitter policy chosen by the story 008 report.

---

## Implementation Notes

*Derived from GDD R-G4 S4 (read the full S4 row; it carries the strata, positions and feasibility arithmetic):*

Strata: axis-corner quadrants (the two the x = y sun axis passes through): 3 courts each, symmetric about the axis (an off-axis final adds a fairness skew of about 2 |x - y| m): the T junction where the first interior corridors meet on the axis, about (31, 31) m from the grid corner, and the two T junctions where the second interior corridors meet the perimeter lane, about (9, 50) m and (50, 9) m (grid-corner paths about 62, 59 and 59 m, pairwise >= 29 m apart, all with 3 ingresses). Zone-corner quadrants: 3 courts each at 30-60 m path from the grid corner. Temple ring: 4-6 courts >= 15 m from the pyramid base. Widened junctions first (a T junction gives 3 ingresses for free); a block courtyard (2 openings) only in the zone-corner quadrants; sun-facing host bays drop to one storey when the sun pool needs it (S11.3, story 016). The count is `CityGenCourtTargetCount` (default 16, hard 16-20) up to target + 2; note the composition 6 + 6 + 4-6 tops out at 18, so a target of 19-20 cannot be met by the stated strata: implement the default and record the conflict for the level-designer (the safe range is 16-18).

The anchor-jitter policy (do the junctions move with S2's +-1 c cut jitter?) comes from the story 008 report; until then, parametrise the placement over {fixed, moving}. Use `CityGenFinalPoolProbe` from story 008 as the starting point for anchor placement. No single-pawn chokepoint (Penalty CR-13, Objectives CR-8): ingress >= `NavMinClearWidthCm` (2.8 m), fixture clearance as in AC2; the disc-removal connectivity check is a G-PRE predicate (story 021). `ClueTag` is Objectives-owned and read only: S4 emits no tag.

Prototype replaced: none (the prototype had objective sites moved to seeded cells, not courts).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-s5-zones-anchors-and-start-cells.md`: zone anchors and start cells.
- `story-021-g-pre-predicates-a.md`, `story-022-g-pre-predicates-b-and-role-counts.md`: G-PRE checks, the single-disc chokepoint test and the role-count pre-check.
- `story-016-s11-repairs-and-occurrence-rule.md`: sun-pool host regeneration round a failing court.
- LSC V-SEP, V-FAIR and the Selection draw: `level-socket-contract` stories 018 and 021.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageCourts.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenFinalPoolProbe.cpp` (promoted from story 008)
- `Source/StealthGameplay/Private/Tests/CityGen/StrataAndSpacingTest.cpp` (class `FCityGenCourtsStrataAndSpacingTest`, name `StealthGame.CityGen.Courts.StrataAndSpacing`)
- `Source/StealthGameplay/Private/Tests/CityGen/ClearWidthsAndIngressTest.cpp` (class `FCityGenCourtsClearWidthsAndIngressTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/NoRoleFieldTest.cpp` (class `FCityGenCourtsNoRoleFieldTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after the revision of AC1; the AC3 policy assertion is DEFERRED until the story 008 report).*

- **AC1** `Courts.StrataAndSpacing`
  - Given: 50 seeds.
  - When: S4 runs.
  - Then: court count is in [`CityGenCourtTargetCount`, target + 2], composed of 6 axis-corner, 6 zone-corner and 4-6 temple-ring courts. Courtyards are only in zone-corner quadrants; a stage trace shows each was placed only when no spacing-legal widened junction remained. Widened courts are 3x3 to 4x4 c (4x4 in Market). Courts are at least 24 m apart and at least 6 m from any ExitPoint.
  - Edge cases: composition tops out at 18, so target 19 or 20 is unsatisfiable; run at the default. A courtyard placed while a legal junction site exists fails (negative control).
- **AC2** `Courts.ClearWidthsAndIngress`
  - Given: 50 seeds.
  - When: S4 runs.
  - Then: every court ingress is at least 2.8 m clear. Every fixture has at least 1.2 m clear floor on all 4 faces. Every axis-corner pool court has at least 3 ingresses.
  - Edge cases: fixtures with a 2.6 m ingress, a face blocked at 1.0 m, and a 2-ingress court in the pool each fail. A 2-ingress block courtyard is excluded from the pool.
- **AC3** `Courts.NoRoleField` (parametrised over policy {fixed, moving})
  - Given: 50 seeds at jitter 0, 1 and 2.
  - When: S4 runs.
  - Then: under fixed, the anchor positions (31,31), (9,50), (50,9) don't move with jitter; under moving, they shift with the junction offset. The court record has no role field. 0 temple-ring courts are final-eligible by the LSC distance predicates.
  - Edge cases: the "chosen policy" assertion is DEFERRED until the story 008 report.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-008-oq9-final-court-enumeration-script.md` (report), `story-009-s3-area-types.md`
- Unlocks: `story-011-s5-zones-anchors-and-start-cells.md`, `story-013-s7-emergence-alleys-and-exit-points.md`, `story-022-g-pre-predicates-b-and-role-counts.md`
