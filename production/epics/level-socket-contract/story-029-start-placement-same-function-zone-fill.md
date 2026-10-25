# Story 029: Round-start placement — one function, zone fill and density cap

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-007`
*(At write time: TR-lsc-007 = Round-start placement and spawn gate)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: `RequestStartPlacement` (served only in `Published`) places players and NPCs with the same placement function and facing rule; player cells come from `FSecretStream(GetServerSubSeed(PlayerPlacement, LayoutAttempt), "LSC_PLCE")` and zone NPC counts from the same sub-seed with a distinct tag; it returns transforms only and never spawns a pawn (Round owns spawn order and the factory).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Pure server logic; statistical tests use pinned sub-seeds and state their critical values. Grid fixture until the graybox exists (story 028).

**Why this story**: Same-function placement is how the game avoids any t0 tell between players and NPCs (Pillar 1), and the density window prevents a cluster that gives the zone away. Read first: R6 (all), D6, V-ZONEFILL, AC-19, AC-20.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Draw sites use ADR-0006 shapes and stream tags `LSC_PLCE`, `LSC_ZONE` (ADR-0014); every pawn is created by `UStealthPawnFactory::SpawnSharedPawn` (ADR-0010)
- Forbidden: Crowd's density function may not read `FSiteAssignment`, zone roles or player positions (server assert) (GDD R6.1, ADR-0014)
- Guardrail: draws well under 0.05 ms/frame (ADR-0006)

---

## Acceptance Criteria

*From GDD AC-19 and AC-20, scoped to this story:*

- [ ] GIVEN `RequestStartPlacement(5v5, 50)` on the graybox, THEN one placement function and one facing rule serve players and NPCs; each player gets a distinct start cell of its zone; NPC cells are at least 3 m apart (`Placement.SameFunction`)
- [ ] THEN each used zone's NPC count lies in its D6 window, is uniform over the window across 600 pinned rounds (chi-square at the window's df, p = 0.01), and the two zones total at most 60% of `NpcCount` (`Placement.ZoneFillWindow`)
- [ ] GIVEN 20 pinned graybox round starts, THEN at t0 the pawn density within `TeamZoneRadiusCm` of each used anchor is at most 1.5 x the map mean (with the real Crowd, also at `T_contact` and 30 s after `Active`: ADVISORY) (`Placement.ZoneDensityCap`)

---

## Implementation Notes

*Derived from GDD R6:*

- Player slots: each player draws one distinct start cell of its team's zone, uniform from the `PlayerPlacement` stream. NPC pool: every walkable cell within 3 m of a `CrowdRoute` node and at least 3 m from any other chosen cell (start cells included); NPCs are placed from it by Crowd's density function.
- Zone fill (D6): NPCs per used zone drawn uniformly from `[NpcZoneMin, NpcZoneMax]`; Crowd treats the disc as full; `NpcZoneMin` enforces `ZoneNpcDensityMinMult` (0.8) and `ZonePlayerShareMaxPct` (40%); `NpcZoneMax` enforces `ZoneDensityMaxMult` (1.5). Zone NPCs across both zones at most `ZoneNpcShareMaxPct` (60%) of `NpcCount` (30 of 50).
- A pawn on a cell next to an alley mouth faces out of the alley (same rule for NPCs and players). Placement happens while clients are in the briefing state; the first world frame is drawn only after `Active` (story 025).
- Chi-square: window W at df = W - 1 (W 5: 13.277; W 3: 9.210) with at least 5 expected per cell; exactly 1.5 x passes.
- **Prototype replaced, not promoted:** `AStealthGameMode::FindCrowdPoint` / crowd spawn near a hand-placed crowd centre is superseded by this placement; Round owns the single spawn-order shuffle and spawns all pawns in one frame.

---

## Out of Scope

- Story 025: spawn gate; the real Crowd density function (`production/epics/crowd-npc-behaviour/EPIC.md`); Round's spawn order (`production/epics/round-match-framework/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/StartPlacement.cpp`, `StartPlacement.h`
- `Source/StealthGameplay/Private/Tests/Layout/StartPlacementTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Placement.SameFunction`
  - Given: `RequestStartPlacement(5v5, 50)` on the graybox (or a grid fixture until story 028).
  - When: placement runs.
  - Then: one placement function and one facing rule are called for players and NPCs (spy counter); each player has a distinct start cell in its own zone; NPC cells are >= 3 m apart and >= 3 m from start cells.
  - Edge cases: alley-adjacent cells face out of the alley.
- **AC-2** `Placement.ZoneFillWindow`
  - Given: 600 pinned rounds.
  - When: zone NPC counts are drawn.
  - Then: each count lies in its [NpcZoneMin, NpcZoneMax]; chi-square vs uniform below the p = 0.01 critical value for df = W-1 (W 5: 13.277); two zones total <= 30 (60% of 50).
  - Edge cases: W = 3 (df 2, 9.210); expected count per cell >= 5.
- **AC-3** `Placement.ZoneDensityCap`
  - Given: 20 pinned graybox round starts.
  - When: density is measured at t0 within 35 m of each used anchor.
  - Then: <= 1.5 x map mean in all 20 (later checkpoints ADVISORY with the real Crowd).
  - Edge cases: exactly 1.5 passes.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/StartPlacementTest.cpp` — `StealthGame.Layout.Placement.SameFunction`, `.ZoneFillWindow`, `.ZoneDensityCap` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-007-formulas-d5-d6-d6b-d9.md`, `production/epics/level-socket-contract/story-022-selection-secret-draw-streams.md`, `production/epics/level-socket-contract/story-025-spawn-gate-and-no-view-before-active.md`, `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
- Cross-epic: `production/epics/round-match-framework/EPIC.md`, `production/epics/crowd-npc-behaviour/EPIC.md` (stubs until they exist)
- Unlocks: `production/epics/level-socket-contract/story-045-graybox-fairness-playtest.md`
