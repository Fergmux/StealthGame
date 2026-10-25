# Story 018: Predicates V-SEP, V-REACH, V-RESPAWN, V-ZONE and V-ZONEFILL

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`, `TR-lsc-006`, `TR-lsc-007`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; TR-lsc-006 = Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn; TR-lsc-007 = Round-start placement and spawn gate)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The zone predicates decide which zone pairs are usable: a failing pair is unusable, and a layout fails only when no usable pair remains; reach and contact paths exclude traversal because informants and NPCs cannot use parkour.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Grid fixture backend for the matrix; the real-navmesh repeat is on the graybox (story 028).

**Why this story**: These predicates guarantee that team zones are separated, unseen at t0, reachable, supplied with unseen respawn pawns and parity-balanced. Read first: R7 rows V-SEP, V-REACH, V-RESPAWN, V-ZONE, V-ZONEFILL; D6, D6b; AC-22, AC-23.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every V-* predicate exists once against `ILayoutMeasureBackend` (ADR-0014)
- Forbidden: Never fork predicate logic between backends (ADR-0014)
- Guardrail: work bounded by `LayoutSliceWorkUnitsLimit` (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-22 and AC-23 (subset), scoped to this story:*

- [ ] GIVEN V-SEP and V-REACH fixtures each breaking only that predicate, THEN the attempt fails with exactly that id; a failing zone pair is unusable while no usable pair fails the layout (`Validate.PredicateMatrix.<Id>`)
- [ ] GIVEN a court reachable only by a vault, THEN V-REACH fails while `R_trav` uses the traversal path (`Validate.ReachIgnoresTraversal`)
- [ ] GIVEN V-RESPAWN, V-ZONEFILL and V-ZONE fixtures, THEN each fails with exactly its id (V-ZONE: area-type share parity, two area types per zone, anchor 15 m from tiers above 6.4 m, no unique module variant within 6 m of a socket) (`Validate.PredicateMatrix.<Id>`)

---

## Implementation Notes

- V-SEP per zone pair: no line of sight between the two start-cell sets (dilated 3 m); nearest start cells at least `ZoneSeparationPathCm` (70 m) by path; `T_contact` (D6) at least `ZoneContactMinSec` (15 s) for both zones; every start cell at least `StartToFinalMinPathCm` (44 m) by path from every final-eligible court. Each sub-condition gets its own fixture. A third or fourth candidate zone may fail V-SEP with another as long as each zone belongs to at least one usable pair.
- V-REACH: from every start cell of every candidate zone to every court Selection could use, a complete (non-partial) path within the D2 budget, without traversal; `R_trav` uses traversal for the fairness ratio only (D1).
- V-RESPAWN: for every usable zone pair each zone's cut `RespawnRegion(zone, partner)` has expected unseen pool `E_R` (D6b) at least `RespawnPoolMinExpectedCount` (4); a failing pair is unusable. For V-SEP and V-RESPAWN the failing fixture breaks every pair so that the layout fails.
- V-ZONE: per usable pair area-type shares and sun/shade fractions differ by at most `ZoneParityPct` (15 points); each zone mixes at least 2 area types; each anchor at least 15 m from any tier above 6.4 m; anchors' distances to the nearest temple differ by at most 20%; layout-wide no unique module variant within 6 m of a socket and every variant occurs at least 3 times. Boundary 15.0 m anchor and 6.0 m variant pass.
- V-ZONEFILL: every candidate zone at `P_z = TeamSize` has a D6 window of at least `ZoneNpcWindowMinCount` (3), and the sum of `NpcZoneMin` over a used pair is at most floor(`ZoneNpcShareMaxPct` x `NpcCount`).

---

## Out of Scope

- V-NAV/V-SIGHT/V-ROUTE/V-TRAV (017); V-COURT/V-POOL/V-BUSH/V-CLUE (019); V-FAIR (021)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/Predicates/ZonePredicates.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/PredicateMatrixZoneTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Validate.PredicateMatrix.<V-SEP|V-REACH>`
  - Given: one fixture per predicate that breaks only it.
  - When: validated.
  - Then: the attempt fails with exactly that id; a layout with one failing pair and one usable pair passes (the pair is unusable, not the layout).
  - Edge cases: V-SEP sub-conditions (LOS, 70 m path, T_contact 15 s, 44 m start-to-final) each as a fixture.
- **AC-2** `Validate.ReachIgnoresTraversal`
  - Given: a court reachable only by a vault.
  - When: V-REACH and R_trav are evaluated.
  - Then: V-REACH fails; R_trav uses the traversal path.
  - Edge cases: a partial path fails at any length.
- **AC-3** `Validate.PredicateMatrix.<V-RESPAWN|V-ZONEFILL|V-ZONE>`
  - Given: one fixture per id; V-ZONE variants for parity 15 points, fewer than 2 area types, anchor within 15 m of a tier above 6.4 m, and a unique module variant within 6 m of a socket.
  - When: validated.
  - Then: each fails with exactly its id.
  - Edge cases: 15.0 m anchor and 6.0 m variant boundaries pass.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/PredicateMatrixZoneTest.cpp` — `StealthGame.Layout.Validate.PredicateMatrix.<Id>` for V-SEP, V-REACH, V-RESPAWN, V-ZONEFILL, V-ZONE and `StealthGame.Layout.Validate.ReachIgnoresTraversal` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-007-formulas-d5-d6-d6b-d9.md`, `production/epics/level-socket-contract/story-014-socket-records-determinism-required-data.md`
- Unlocks: `production/epics/level-socket-contract/story-021-selection-combos-and-fairness.md`, `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
