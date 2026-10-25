# Story 006: Formulas D1, D2, D3 and D4 pass their test vectors

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; <= 33 ms slices)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Predicate thresholds, guard bands and comparisons are integer cm (LSC AC-30), written once against `ILayoutMeasureBackend`; D3's detour statistic is bracketed by a grid bound and refined exactly only where the bracket straddles the threshold.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Plain integer arithmetic; no engine API.

**Why this story**: D1 (fairness ratios), D2 (reachability budget), D3 (detour statistic) and D4 (chain band, walk share) feed V-NAV-3, V-REACH and Selection. Read first: Formulas D1-D4 and the AC-30 vector table. **Known GDD defect:** the AC-30 table row "D4: 170 m -> S = 0.169" uses the superseded T = 12; the Formulas section (restated 2026-10-25) uses T = 9, giving 0.225. This story implements the Formulas section and flags the AC-30 row for the GDD owner.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every V-* predicate exists once against `ILayoutMeasureBackend` (ADR-0014)
- Forbidden: Never fork predicate logic between backends (ADR-0014)
- Guardrail: Validate + Select within about 1.5 s in <= 4 ms GT slices (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-30 (D1-D4 vectors; D4 per the Formulas section), scoped to this story:*

- [ ] D1: 230/215 -> 1.070 pass; 220/200 -> 1.100 pass (exact); 250/220 -> 1.136 fail; final legs 70/62 -> 1.129 pass (`Formula.D1`)
- [ ] D2: Euclid 90 -> B 180 and a 152 m path passes; Euclid 140 -> 200 (cap); Euclid 10 -> 40; a partial path fails at any length (`Formula.D2`)
- [ ] D3: n = 100,000 -> p95 at rank 95,000; pairs under 40 m excluded; 1.60 and 2.00 pass, 1.61 and 2.01 fail; the 4 m lattice samples a 2-cell alley fixture (`Formula.D3`)
- [ ] D4 (T = 9): 170 m -> S = 170 / (1.4 x 9 x 60) = 0.225; L 120 -> 0.159; L 240 -> 0.317; 119 m and 241 m rejected; 120 m and 240 m accepted (`Formula.D4`)

---

## Implementation Notes

- Same file as story 007 (`LayoutFormulas.h/.cpp`); integer-scaled comparisons so exact thresholds pass (220/200 = 1.100 passes at `ChainFairnessMaxMult` 1.10; 1.101 fails).
- D2: budget B = clamp(2 x Euclid, 20 m, 200 m) with defaults `ReachDetourMult` 2.0; path exactly B passes, B + 1 cm fails; a partial path (no complete navmesh path) fails at any length.
- D3: p95 is the value at rank ceil(0.95 n) (n = 100,001 gives rank 95,001); pairs under 40 m are excluded; max and p95 thresholds `DetourMaxMult` 2.00 and `DetourP95MaxMult` 1.60; the 4 m lattice places at least one sample in a 2-cell alley.
- D4: the band is `ChainLengthMinCm`..`ChainLengthMaxCm` (provisional 120-240 m, set from the graybox enumeration in story 028); S uses v = 1.4 m/s and T = 9 min.
- Thresholds arrive as parameters from `FLayoutParams`.

---

## Out of Scope

- Story 007: D5, D6, D6b, D9; story 008: D7; story 021: D8 and Selection that uses D1/D4

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/LayoutFormulas.h`
- `Source/StealthGameplay/Private/Core/Layout/LayoutFormulas.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/FormulaD1ToD4Test.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Formula.D1`
  - Given: ChainFairnessMaxMult 1.10, FinalLegFairnessMaxMult 1.15, integer-scaled ratios.
  - When: chains 230/215, 220/200, 250/220; final legs 70/62.
  - Then: 1.070 pass; 1.100 pass (exact); 1.136 fail; 1.129 pass.
  - Edge cases: 1.101 fails; min = 0 is excluded by the leg floors.
- **AC-2** `Formula.D2`
  - Given: ReachDetourMult 2.0, floor 20 m, cap 200 m.
  - When: Euclid 90 (path 152), 140, 10; a partial path.
  - Then: B 180 and 152 passes; B 200; B 40; the partial path fails at any length.
  - Edge cases: path exactly B passes, B+1 cm fails.
- **AC-3** `Formula.D3`
  - Given: 100,000 ratios; pairs under 40 m; thresholds 1.60 (p95) and 2.00 (max); a 2-cell alley fixture.
  - When: p95 and max are computed on the 4 m lattice.
  - Then: p95 is the value at rank 95,000 (ceil(0.95n)); under-40 m pairs excluded; 1.60/2.00 pass, 1.61/2.01 fail; the lattice places >= 1 sample in the alley.
  - Edge cases: n = 100,001 rank 95,001.
- **AC-4** `Formula.D4`
  - Given: v 1.4, T 9, band 120-240 m.
  - When: L = 170, 120, 240, 119, 241.
  - Then: S 0.225, 0.159, 0.317; 119 and 241 rejected; 120 and 240 accepted.
  - Edge cases: the AC-30 table row (0.169, T = 12) is stale; flag for the GDD owner.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/FormulaD1ToD4Test.cpp` — `StealthGame.Layout.Formula.D1`, `.D2`, `.D3`, `.D4` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None within this epic
- Cross-epic: `production/epics/tuning-data-config/EPIC.md` (keys and module skeleton)
- Unlocks: `production/epics/level-socket-contract/story-017-predicates-navigation-area-sight-route-traversal.md`, `production/epics/level-socket-contract/story-021-selection-combos-and-fairness.md`
