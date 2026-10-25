# Story 007: Formulas D5, D6, D6b and D9 pass their test vectors

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`, `TR-lsc-007`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; TR-lsc-007 = Round-start placement and spawn gate)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: V-* predicates are written once against `ILayoutMeasureBackend`; thresholds, guard bands and comparisons are integer cm so exact thresholds pass.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Plain integer arithmetic; no engine API.

**Why this story**: D5 (socket counts), D6 (zone NPC fill and contact time), D6b (respawn pool) and D9 (small checks) are pure functions the predicates, placement and respawn stories call. Read first: Formulas D5, D6, D6b, D9 and the AC-30 vector table (`md-section.sh extract ... "Formulas"`, then the Acceptance Criteria table).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every V-* predicate exists once, written against `ILayoutMeasureBackend` (ADR-0014)
- Forbidden: Never fork predicate logic between the grid and world backends (ADR-0014)
- Guardrail: Validate + Select within about 1.5 s in <= 4 ms GT slices (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-30 (D5, D6, D6b, D9 vectors), scoped to this story. Comparisons are integer-scaled so exact thresholds pass.*

- [ ] D5: A 8,800 -> 13/9/11; 8,000 -> 12/8/10; 13,000 -> 19/13/16; 8,500 -> N_B 9 (half-up); N_B > 2 x N_sec fails; N_C 16 with overlap 2 passes, 15 fails (`Formula.D5`)
- [ ] D6: N 50, A 8,800, A_z 1,800, P 5 -> min 9, max 13, W 5, peak 1.47x pass; A_z 1,400 -> W 2 fail; contact path 68 m -> 15.1 s pass, 60 m -> 13.3 s fail; NpcCount 35, A 8,000, A_z 1,800 -> min 8, max 10, W 3 pass; NpcCount 35, A 8,100, A_z 1,750 -> min 8, max 9, W 2 fail; 34 -> V-AREA band empty, `NpcCountTooLow` (`Formula.D6`)
- [ ] D6b: rho A_R 13.636, phi 0.2, k 2 -> 8.73 pass; phi 0.5 -> 3.41 fail (`Formula.D6b`)
- [ ] D9: pool 47 vs median 40 -> 0.175 pass, 49 -> 0.225 fail; bushes 4/3 pass, 5/2 fail; N_E 13 -> lambda floor 0.108/s, cancel bound 1.625 per minute (`Formula.D9`)

---

## Implementation Notes

- One header-only or small-cpp module of integer-scaled functions in `StealthGameplay/Private/Core/Layout/LayoutFormulas.h`; every threshold arrives as a parameter read from the frozen tuning copy (`FLayoutParams`), never a literal.
- Use half-up rounding for D5 (8,500 -> N_B 9). Name the D5 tuple order (N_E, N_B, N_T) in the test.
- D6 defaults: r 0.8, s_max 0.40, d_max 1.5, q 0.60, `ZoneContactMinSec` 15, v_run 4.5; contact exactly 15.0 s passes (path 67.5 m); W exactly 3 passes.
- No `float` in a comparison that decides pass/fail; scale to integers.
- Keys live in Tuning Data & Config (TDC); read them through `FLayoutParams`, not a tuning service call (ADR-0014 `FLayoutAttemptParams`).

---

## Out of Scope

- Story 006: D1-D4. Story 008: D7. Story 021: D8
- The predicates that call these functions (stories 016-019) and placement (029)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/LayoutFormulas.h`
- `Source/StealthGameplay/Private/Core/Layout/LayoutFormulas.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/FormulaD5D6D6bD9Test.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Formula.D5`
  - Given: defaults (700 / 1,000 / 800 m2 per socket/bush/point; N_E clamp 12-24).
  - When: A = 8,800; 8,000; 13,000; 8,500.
  - Then: (N_E, N_B, N_T) = 13/9/11; 12/8/10; 19/13/16; N_B(8,500) = 9 (half-up). N_B > 2 x N_sec fails; N_C 16 (overlap 2) passes, 15 fails.
  - Edge cases: A 8,499 gives N_B 8; overlap 3 invalid.
- **AC-2** `Formula.D6`
  - Given: TeamSize 5, defaults r 0.8, s_max 0.40, d_max 1.5, q 0.60, ZoneContactMinSec 15, v_run 4.5.
  - When: N 50 / A 8,800 / A_z 1,800 / P 5; A_z 1,400; contact path 68 m and 60 m; N 35 / A 8,000 / A_z 1,800; N 35 / A 8,100 / A_z 1,750; N 34.
  - Then: min 9, max 13, W 5, peak 1.47x pass; A_z 1,400 gives W 2 fail; 68 m gives 15.1 s pass, 60 m gives 13.3 s fail; 8/10/W 3 pass; 8/9/W 2 fail; N 34 gives empty V-AREA band and `NpcCountTooLow`.
  - Edge cases: contact exactly 15.0 s passes (path 67.5 m); W exactly 3 passes.
- **AC-3** `Formula.D6b`
  - Given: rho x A_R = 13.636, k 2, RespawnPoolMinExpectedCount 4.
  - When: phi 0.2 and 0.5.
  - Then: E_R 8.73 pass; 3.41 fail.
  - Edge cases: E_R exactly 4.0 passes.
- **AC-4** `Formula.D9`
  - Given: SunPoolParityPct 0.20, BushChainDiffMaxCount 2, AmbientEmergencePerSocketMinPerMin 0.5, EmergenceExitCancelMaxPct 25%.
  - When: pool 47 and 49 vs median 40; bushes 4/3 and 5/2; N_E 13.
  - Then: 0.175 pass, 0.225 fail; diff 1 pass, diff 3 fail; lambda floor 0.108/s (6.5 per minute), cancel bound 1.625 per minute.
  - Edge cases: pool exactly 48 (0.200) passes; bush diff exactly 2 passes.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/FormulaD5D6D6bD9Test.cpp` — automation tests `StealthGame.Layout.Formula.D5`, `.D6`, `.D6b`, `.D9` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None within this epic
- Cross-epic: `production/epics/tuning-data-config/EPIC.md` (keys and module skeleton; see story 001 note)
- Unlocks: `production/epics/level-socket-contract/story-018-predicates-zones-separation-reach-respawn.md`, `production/epics/level-socket-contract/story-029-start-placement-same-function-zone-fill.md`
