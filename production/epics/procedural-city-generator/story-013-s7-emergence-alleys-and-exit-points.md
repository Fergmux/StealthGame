# Story 013: S7 Emergence alleys and ExitPoints

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
**ADR Decision Summary**: Emergence sockets are LSC R3 records the Generator derives; the Generator ray-casts W(O) (witness cells) on the grid and demands a margin under LSC's 37, and G-PRE pre-checks V-EMERGE. The socket set carries no role data.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++, the W(O) ray cast is a grid DDA)
**Engine Notes**: No engine API. The W(O) measurement is the grid backend's ray cast (story 020); this story builds the alleys it measures. The 2-6 m navmesh re-measure of an ExitPoint is the LSC Validate step on the real navmesh.

**Control Manifest Rules (this layer)**:
- Required: Pure data, integer streams — source: ADR-0014
- Forbidden: Never place a socket by role; ExitPoint is never at the alley mouth — source: GDD S7, ADR-0014
- Guardrail: part of the 0.12 s candidate budget

S7 cuts Z-alleys through blocks of depth 7-8 c and L-alleys into blocks of 9-10 c, derives the 12-14 emergence sockets (LSC D5 `N_E = ceil(A / 700 m^2)`: 12 at 8,000 m^2, 14 at 9,676 m^2) with quotas temple fringe, Market, canal quay and Residential <= 55%, and places each socket's ExitPoint.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G6, R-G4 S7), scoped to this story:*

- [ ] **AC1** (AC-G6 first clause, `Emergence.WitnessAndQuota`): **GIVEN** 50 seeds **THEN** every emergence socket has W(O) <= `CityGenWitnessCellsTargetCount` (30), sockets are >= 6 c apart, the count is LSC D5 N_E = ceil(A / 700 m^2) (12-14), quotas are minimums of >= 2 temple fringe, >= 2 Market and >= 3 canal quay (the canal-quay quota applies only when canals are enabled, tier G1), and Residential is <= 55%.
- [ ] **AC2** (AC-G6 second clause, `Emergence.AlleyGeometry`): **GIVEN** 50 seeds **THEN** every Z-alley sits in a 7-8 c block with legs of 2-3 c clear (middle leg 1-3 c, >= 1 c of building to the block's side streets, no Z in quay-flanked blocks), and every L-alley in a 9-10 c block is registered as an emergence alley exempt from the S11.2 spur prune.
- [ ] **AC3** (AC-G6 third clause, `Emergence.ExitPoint`): **GIVEN** 50 seeds **THEN** every ExitPoint is 5.0 m of grid walk from its origin accepted in [3.0, 6.0] m (the intersection with LSC's 2-6 m), toward the lower-SocketId street, never at the mouth, and >= 6 m from any bush, court or traversal point.

---

## Implementation Notes

*Derived from GDD R-G4 S7 (review 2026-10-24 corrections included):*

Z-alleys through blocks of depth 7-8 c: leg l1 perpendicular to street A, a middle leg parallel, leg l3 to street B, all 2 c wide, with l1 and l3 of 2-3 c clear (LSC legs <= 6 m) and depth `l1 + 2 + l3` in {7, 8} c; the middle leg is 1-3 c clear and the Z keeps >= 1 c of building between it and the block's side streets. Blocks of 9-10 c take an L-alley with one mouth (legs 2-3 c clear) whose origin is the closed end; it is a registered emergence alley, exempt from the S11.2 spur prune (story 016). Origin O is mid-way along the middle leg (Z) or at the closed end (L). ExitPoint is the navmesh point 5.0 m of walk from O inside the leg toward the lower-SocketId street (never at the mouth, which is about 9-10 m from O, outside LSC's 2-6 m), >= 6 m from any bush, court or traversal point. On the 2 m grid the achievable lengths are 4.0 and 6.0 m with target 5.0 (the GDD's "+- one cell" would give 3-7 m, wider than LSC's 2-6 m, so accept [3.0, 6.0] m). Z-alleys are not cut into quay-flanked blocks. Sockets are >= 6 c apart. The Generator ray-casts W(O) and demands <= 30 (a margin under LSC's 37).

Quotas: Temple fringe 2, Market 2, canal quay 3, Residential <= 55%, read as minimums. At tier G0 only Temple and Market are non-Residential and the 55% cap then forces at least 6 sockets outside Residential in a 12-socket layout: confirm feasibility on G0 data and record the outcome (a G0 shortfall is a finding for the level-designer, not a silent waiver). The S11.2 prune and the occurrence of registered alleys are asserted in story 016.

Prototype replaced: none (the prototype had no emergence concept; crowd spawn used `FindCrowdPoint`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-016-s11-repairs-and-occurrence-rule.md`: the prune that must spare registered L-alleys.
- `story-020-g-pre-grid-measure-backend-and-guard-bands.md`, `story-021-g-pre-predicates-a.md`: the W(O) ray-cast backend and V-EMERGE pre-check.
- LSC V-EMERGE on the navmesh and the 2-6 m ExitPoint re-measure: `level-socket-contract/story-016-predicates-emergence-witness-alley.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageEmergence.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/WitnessAndQuotaTest.cpp` (class `FCityGenEmergenceWitnessAndQuotaTest`, name `StealthGame.CityGen.Emergence.WitnessAndQuota`)
- `Source/StealthGameplay/Private/Tests/CityGen/AlleyGeometryTest.cpp` (class `FCityGenEmergenceAlleyGeometryTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/ExitPointTest.cpp` (class `FCityGenEmergenceExitPointTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after the revision of AC1 and AC3).*

- **AC1** `Emergence.WitnessAndQuota`
  - Given: 50 seeds, with A = N_walk x 4 m^2, plus a canal-enabled G1 fixture.
  - When: S7 runs.
  - Then: the socket count is exactly ceil(A / 700), which is 12-14. Every W(O) is at most 30, and sockets are at least 6 c apart. There are at least 2 temple-fringe and at least 2 Market sockets. Residential is at most 55% of sockets. At least 3 canal-quay sockets exist on the G1 fixture only.
  - Edge cases: A = 8,000 gives 12 and A = 9,676 gives 14 as boundary fixtures. W(O) = 31 fails.
- **AC2** `Emergence.AlleyGeometry`
  - Given: 50 seeds.
  - When: S7 and then S11 run.
  - Then: each Z-alley is in a 7-8 c block with l1 + 2 + l3 in {7, 8}, legs l1 and l3 of 2-3 c clear, and a middle leg of 1-3 c. At least 1 c of building separates it from the block's side streets. There are no Z-alleys in quay-flanked blocks. Each L-alley is in a 9-10 c block with legs of 2-3 c clear, is registered, and survives S11.2.
  - Edge cases: a 6 c or 11 c block gets no Z or L. An ordinary dead-end spur is pruned (negative control).
- **AC3** `Emergence.ExitPoint`
  - Given: 50 seeds.
  - When: S7 runs.
  - Then: ExitPoint grid-walk length from the origin is within [3.0, 6.0] m (on a 2 m grid that means 4.0 or 6.0 m, with the target 5.0). It lies toward the lower-SocketId street and is never at the mouth. It is at least 6 m from any bush, court or traversal point.
  - Edge cases: a 7.0 m exit fails. The 2-6 m navmesh re-measure belongs to LSC Validate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-009-s3-area-types.md`, `story-010-s4-courts.md`, `story-011-s5-zones-anchors-and-start-cells.md`
- Unlocks: `story-016-s11-repairs-and-occurrence-rule.md`, `story-017-s12-sockets-socket-ids-slab-cover-and-skeleton-transform.md`, `story-021-g-pre-predicates-a.md`
