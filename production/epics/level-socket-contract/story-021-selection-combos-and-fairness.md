# Story 021: Selection — combos, V-FAIR and the single final (with D8)

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-009`
*(At write time: TR-lsc-009 = Server-only query API for site assignment and team spawns)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Selection enumerates passing combos (zone pair, zone map, final, P1/P2 per team) against a precomputed path matrix in canonical SocketId order, checks V-FAIR (minimum combos, role share, prefix ambiguity, final candidates), then makes a uniform two-stage draw; `FSiteAssignment` lives only in server memory and the Event Log header.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Pure integer logic on the game thread outside `FPublicOnlyScope`; the path matrix comes from the measure backend (fixtures use the grid backend).

**Why this story**: The combo rules and V-FAIR are what make the secret assignment fair and non-deducible. Read first: R9 items 2-6, D1, D4, D8, V-FAIR, AC-25, AC-27, AC-30 (D8 row).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Draw sites use ADR-0006 shapes and stream tags (`LSC_SITE`) (ADR-0014)
- Forbidden: Never log `FSiteAssignment` or zone -> team outside the Event Log header (ADR-0014)
- Guardrail: Validate + Select about 1.5 s in <= 4 ms GT slices (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-25, AC-27 and AC-30 (D8), scoped to this story:*

- [ ] GIVEN 1,000 pinned secrets, THEN every drawn combo uses distinct courts and meets the leg floors, enemy-zone distance, D4 band, D1 ratios and bush balance (`Selection.ComboPasses`)
- [ ] GIVEN a fixture where one court is the final in 7 of 12 passing combos, or where one team prefix leaves only 2 distinct finals, THEN V-FAIR fails; at 6 of 12 and at 3 finals it passes (`Selection.RoleShareAndAmbiguity`)
- [ ] GIVEN any published layout, THEN exactly one `ObjectiveFinalSite` is active and every court has the same fixture class and sun-pool presence (`Selection.SingleFinal`)
- [ ] D8: 16 courts, 6 final -> N_raw 393,120; four usable pairs -> 1,572,480 (formula-only boundary: 14 courts, 4 final -> 137,280); p 0.5, N 5 -> P_fallback 1.5625%, E_attempts 1.97 (`Formula.D8`)

---

## Implementation Notes

*Derived from GDD R9 and ADR-0014 5.5:*

- A combo assigns together: the 2 used zones from usable pairs and zone -> team (2 options per pair); one final from final-eligible courts; per team P1(T) and P2(T) from distinct prelim-eligible courts; no court holds two roles.
- A combo passes when: leg floors (zone -> P1 at least 30 m, P1 -> P2 at least 40 m, P2 -> Final at least 40 m); each team's prelims at least 50 m by path from the other zone's anchor; chain length in the D4 band; D1 ratios within 1.10 / 1.15 / 1.20 (with traversal); bush balance within 20 m of each chain polyline differs by at most 2; and for the Generated provider only, no mirror intercept (inactive for the graybox).
- V-FAIR: at least `MinFairCombosCount` (12) passing combos; at least `PrelimCandidatesPerStageMinCount` (3) per `ObjectivePrelimSite(Team, Stage)` and `FinalCandidatesMinCount` (6, hard minimum 3) final-eligible courts; role share at most `RoleShareMaxPct` (50%) per court per role; prefix ambiguity: for each team and passing combo the combos sharing the team's prefix hold at least `FinalAmbiguityMinCount` (3) distinct finals, none above 50%. Eleven combos fails the minimum; 7/12 (58%) fails, 6/12 passes.
- Exactly one final site is active; every court has the same fixture class and sun-pool presence (a court without a pool fails parity).
- D8 pure function (`Formula.D8`): E_attempts is 1.96875, compare to 2 dp.
- The two-stage draw itself and the secret streams are story 022. `FSiteAssignment` = {UsedZones[2], ZoneTeam[2], Final, P1[2], P2[2]}, ServerOnly.

---

## Out of Scope

- Story 022: draw and streams; story 023: server query API and leak tests; Generator mirror-intercept transform (Generator epic)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/Selection.cpp`, `Selection.h`
- `Source/StealthGameplay/Private/Core/Layout/LayoutFormulas.cpp` (D8)
- `Source/StealthGameplay/Private/Tests/Layout/SelectionComboTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Selection.ComboPasses`
  - Given: 1,000 pinned secrets and a synthetic layout with >= 12 passing combos.
  - When: a combo is drawn per secret.
  - Then: all 1,000 combos use distinct courts and meet leg floors, enemy-zone distance, D4 band (120-240 m), D1 ratios (1.10 / 1.15) and bush balance (diff <= 2).
  - Edge cases: a secret that yields a combo at an exact threshold.
- **AC-2** `Selection.RoleShareAndAmbiguity`
  - Given: 12 passing combos with one court final in 7 (and in 6); a team prefix leaving 2 (and 3) distinct finals.
  - When: V-FAIR runs.
  - Then: 7 of 12 (58%) fails, 6 of 12 (50%) passes; 2 finals fail, 3 pass.
  - Edge cases: 11 combos fails `MinFairCombosCount`.
- **AC-3** `Selection.SingleFinal`
  - Given: any published layout.
  - When: Selection completes.
  - Then: exactly 1 `ObjectiveFinalSite` active; every court has the same fixture class and sun-pool presence.
  - Edge cases: a court missing a pool fails parity.
- **AC-4** `Formula.D8`
  - Given: D8 inputs.
  - When: computed.
  - Then: 16 courts / 6 final gives N_raw 393,120; four usable pairs give 1,572,480; 14 courts / 4 final gives 137,280; p 0.5, N 5 gives 1.5625% and 1.97.
  - Edge cases: E_attempts is 1.96875, compare to 2 dp.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/SelectionComboTest.cpp` — `StealthGame.Layout.Selection.ComboPasses`, `.RoleShareAndAmbiguity`, `.SingleFinal`, `StealthGame.Layout.Formula.D8` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-006-formulas-d1-to-d4.md`, `production/epics/level-socket-contract/story-014-socket-records-determinism-required-data.md`, `production/epics/level-socket-contract/story-018-predicates-zones-separation-reach-respawn.md`, `production/epics/level-socket-contract/story-019-predicates-courts-pool-bushes-clue.md`
- Unlocks: `production/epics/level-socket-contract/story-022-selection-secret-draw-streams.md`, `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
