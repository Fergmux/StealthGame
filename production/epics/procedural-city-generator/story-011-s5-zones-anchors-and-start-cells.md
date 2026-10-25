# Story 011: S5 Zones, anchors and start cells

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: the OQ9 / C-W24 final-court enumeration (story 008) has not reported; its start-cell radius recommendation (18 m or 16 m) and anchor-jitter rule fix the S5 placement. Story 008 is Ready and releases this one.
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
**ADR Decision Summary**: The Generator emits candidate zones, start cells (as CrowdRoute nodes) and the socket set; it never sees which team uses which zone (`FSiteAssignment` is the service's). The LSC predicates V-SEP, V-ZONEFILL, V-RESPAWN and V-ZONE decide validity; G-PRE pre-checks them on the grid.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++ plus a fixed-sun raycast measurement quantised to cm)
**Engine Notes**: No engine API. The sun-fraction measurement is the only float use and is quantised to cm before comparison (R-G3.4); the fixed sun is on the x = y axis.

**Control Manifest Rules (this layer)**:
- Required: Pure data, integer streams; start cells are CrowdRoute nodes (S12) — source: ADR-0014
- Forbidden: Never read a team, role or assignment in S5; the default 2 zones are mirror images so V-ZONE parity is exact — source: ADR-0014, GDD S5
- Guardrail: part of the 0.12 s candidate budget

S5 places candidate zone anchors at the anti-diagonal corners (mirror images across x = y) with >= `TeamSize` + 1 start cells each. 4-zone mode (`CityGenZoneCandidatesCount` 4) adds the axis corners, used by Selection only when their pair passes V-SEP and V-ZONE (and then the axis-corner courts are no longer final-eligible for that pair).

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (R-G4 S5, AC-G4 second clause, AC-G39 first clause, Edge Cases "temple tier within 15 m"), scoped to this story:*

- [ ] **AC1** (AC-G39 first clause, `Zones.AnchorRule`): **GIVEN** 50 seeds **THEN** every anchor is a plain street cell (never a junction) on the corner-facing arm 1-2 c from the T junction where the second lattice corridors cross.
- [ ] **AC2** (R-G4 S5, `Zones.StartCells`): **GIVEN** each candidate zone **THEN** there are >= `TeamSize` + 1 start cells, each a CrowdRoute node, >= 4 c apart within the start-cell radius of the anchor (18 m, or the radius story 008 recommends), with >= 2 area types within 35 m and >= 15 m from any tier above 6.4 m.
- [ ] **AC3** (AC-G4 second clause, `Zones.SunShadeParity`): **GIVEN** 50 seeds **THEN** the two anti-diagonal zones' sun/shade fractions differ by <= 1 point before fill, the default 2 zones are mirror images across x = y, and 4-zone mode (`CityGenZoneCandidatesCount` 4) adds the axis corners.
- [ ] **AC4** (Edge Cases, `Zones.TempleSlide`): **GIVEN** a temple tier within 15 m of an anchor **THEN** the anchor and its mirror slide one block along the perimeter lane together, and if no slide passes the candidate fails.

---

## Implementation Notes

*Derived from GDD R-G4 S5 and Edge Cases:*

Anchor: a plain street cell (never a junction) on the corner-facing arm, 1-2 c from the T junction where the second lattice corridors from the grid edge cross (the first interior street ring, about 15-16 c from the grid corner, +-33 m from the centre at N = 64; wording adopted from the lean re-review decision D5 2026-10-25). Start cells: >= `TeamSize` + 1 (6 at defaults), each a CrowdRoute node (S12, story 018), >= 4 c apart within 9 c (18 m) of the anchor. At this inset the 35 m density disc is about 3,050 m^2 gross, about 1,950 m^2 walkable, a 33% margin over V-ZONEFILL's floor of about 1,467 m^2 at A = 8,800 m^2 (the floor scales with the candidate's own A). The "1 point" in AC3 is one percentage point of zone cells measured with the fixed-sun raycast; 4-zone mode keeps the original pair's parity. If the temple tier lies within 15 m of an anchor, the anchor and its mirror slide one block along the perimeter lane together (symmetry kept); if no slide passes, the candidate fails with a named failure id (name it in the dev-story notes; the GDD does not).

The start-cell radius and the anchor-jitter policy come from the story 008 report (lever (a) is 16 m instead of 18 m). Adopt the minimal anchor/start-cell probe of story 008 and replace it with the real stage. G-PRE checks per candidate zone (V-SEP, V-ZONEFILL, V-RESPAWN, V-ZONE) are story 022; this story only places.

Prototype replaced: the prototype's team start points were hand-placed in `L_Prototype` and read by `StealthGameMode::FindCrowdPoint`; none of that is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-010-s4-courts.md`: courts (the axis-corner pool).
- `story-018-s12-crowd-route.md`: route nodes (start cells are registered there).
- `story-022-g-pre-predicates-b-and-role-counts.md`: V-SEP, V-ZONEFILL, V-RESPAWN, V-ZONE pre-checks.
- LSC Selection of the zone pair: `level-socket-contract/story-021-selection-combos-and-fairness.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageZones.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/AnchorRuleTest.cpp` (class `FCityGenZonesAnchorRuleTest`, name `StealthGame.CityGen.Zones.AnchorRule`)
- `Source/StealthGameplay/Private/Tests/CityGen/StartCellsTest.cpp` (class `FCityGenZonesStartCellsTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/SunShadeParityTest.cpp` (class `FCityGenZonesSunShadeParityTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/TempleSlideTest.cpp` (class `FCityGenZonesTempleSlideTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Zones.AnchorRule`
  - Given: 50 seeds.
  - When: S5 runs.
  - Then: each anchor is a plain street cell (not an alley, court or junction) on the corner-facing arm, 1-2 c (2-4 m) from the T junction where the second lattice corridors cross.
  - Edge cases: anchors at 0 c or 3 c fail. 4-zone mode is checked as well.
- **AC2** `Zones.StartCells`
  - Given: each candidate zone, with the pipeline run through S12 so route nodes exist.
  - When: S5 completes.
  - Then: there are at least `TeamSize` + 1 start cells, each a CrowdRoute node and at least 4 c (8 m) apart. All are within the radius key of the anchor (18 m, or the radius story 008 recommends). At least 2 area types lie within 35 m, and each start cell is at least 15 m from any tier above 6.4 m.
  - Edge cases: each rule gets a negative-control fixture. The radius boundary is 18.0 versus 18.01.
- **AC3** `Zones.SunShadeParity`
  - Given: 50 seeds.
  - When: S5 completes, before fill.
  - Then: the two anti-diagonal zones' sun/shade fractions differ by at most 1 percentage point (fixed-sun raycast). The default 2 zones are exact transposes. `CityGenZoneCandidatesCount` 4 adds the axis-corner zones.
  - Edge cases: "1 point" is a percentage point of zone cells. The 4-zone mode keeps the original pair's parity.
- **AC4** `Zones.TempleSlide`
  - Given: a fixture with a temple tier 14.9 m from an anchor, and another at 15.0 m.
  - When: S5 runs.
  - Then: at 14.9 m the anchor and its mirror both slide one block along the perimeter lane and stay mirror images. At 15.0 m neither slides (negative control). If no slide passes, the candidate fails with a named failure id.
  - Edge cases: the id is named in the dev-story notes.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the four test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-008-oq9-final-court-enumeration-script.md` (report), `story-009-s3-area-types.md`, `story-010-s4-courts.md`
- Unlocks: `story-013-s7-emergence-alleys-and-exit-points.md`, `story-018-s12-crowd-route.md`, `story-022-g-pre-predicates-b-and-role-counts.md`
