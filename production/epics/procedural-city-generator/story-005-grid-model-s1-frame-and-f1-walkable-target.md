# Story 005: Grid model, S1 Frame and F1 walkable target

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: S1-S12 and G-PRE are pure functions over plain C++ data (an integer cell grid, kit tables copied into plain structs at `Build` entry, no `UObject`) run as one worker task per attempt; stage streams are `Mix32(AttemptSeed, s, j)` integer PCG32.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; this story is plain integer C++)
**Engine Notes**: No engine API. The grid, its row-major order and the per-stage grid hash are the foundation of every later stage and of the drift localisation in story 030.

**Control Manifest Rules (this layer)**:
- Required: The Generator runs as a single cancellable worker task over pure data, inside `FPublicOnlyScope` — source: ADR-0014, ADR-0006
- Forbidden: Never use floats in a draw or an unordered container as draw input; never fold wall-clock into a result — source: ADR-0006, ADR-0014
- Guardrail: Generator worker <= 1 s per attempt off the game thread (J <= 8) — source: ADR-0014

This story creates the cell grid type and stage S1: a square district of N x N cells (2.0 m each), a 3 c sealed perimeter facing an outer water skirt, and the perimeter lane's sawtooth. It also implements F1, the walkable-target formula, as a pure function the candidate loop (story 023) and batch audit (story 037) use.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G26 first clause, R-G4 S1, R-G3.3), scoped to this story:*

- [ ] **AC1** (AC-G26, `Formula.WalkableBand`): **GIVEN** N = 62, 64, 66 at defaults **THEN** T = 2,007, 2,153 and 2,304 and the N = 64 acceptance band is [2,046, 2,260].
- [ ] **AC2** (R-G4 S1, `Frame.PerimeterAndSawtooth`): **GIVEN** 50 candidates **THEN** N is drawn per candidate in 62-66, a 3 c perimeter of sealed 6.4 m buildings faces a non-walkable water skirt, and no unbroken straight stretch of the perimeter lane exceeds 12 c (a 2 c notch every 12 c).
- [ ] **AC3** (R-G3.3, `Frame.GridStateHash`): **GIVEN** one seed run twice **THEN** the integer cell grid is row-major canonical and the per-stage grid hash after S1 is identical.

---

## Implementation Notes

*Derived from GDD R-G4 S1 and F1:*

F1: `N_int = N - 2 * P`; `T = clamp(round(f_walk * N_int^2), A_min, A_max)`; acceptance band `[max(A_min, ceil((1 - tol) * T)), min(A_max, floor((1 + tol) * T))]`, with `A_min = LayoutWalkableCellsMinCount` (2,000), `A_max = min(LayoutWalkableCellsMaxCount, WalkableCellsPerPawnMaxCount * (NpcCount + 2 * TeamSize))` (2,700 at `NpcCount` 50, `TeamSize` 5), `f_walk` `CityGenWalkableTargetPct` 0.64, `tol` `CityGenWalkableTolPct` 0.05, `P` `CityGenPerimeterRowCount` 3. Keep it integer-friendly: compute `round` and `ceil`/`floor` from fixed-point or double values quantised once, and assert the three vectors. Derived bands for reference: [2,000, 2,107] at N = 62 and [2,189, 2,419] at N = 66 (the second clause of AC-G26, "every accepted batch layout inside its band", is asserted by the batch audit, story 037).

S1: N is drawn per candidate from `CityGenGridSideMinCount`..`MaxCount` (62-66, 64 = 128 m) with the S1 stage stream; the 3 c perimeter is sealed 6.4 m building cells facing an outer non-walkable water skirt; the perimeter lane inside it has a 2 c notch every 12 c so it never becomes a 120 m sightline. The grid is `TArray<uint8>` per layer in row-major order from the origin; the stage hash (FNV-1a over the layers) is recorded after S1 and every later stage for story 030.

Prototype replaced: `StealthCityGenerator` built a fixed grid of cells with houses, halls, a temple and stalls from a replicated seed; its grid constants are not carried over.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-006-s2-skeleton-lattice-widths-jogs-transpose-and-jitter.md`: the lattice, corridors and junctions.
- `story-037-batch-harness-and-f4-f6-audits.md`: the batch-level band check and acceptance rate.
- `story-030-pure-function-determinism-and-stage-hashes.md`: cross-process stage-hash comparison.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenGrid.h`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageFrame.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenFormulas.h` (F1)
- `Source/StealthGameplay/Private/Tests/CityGen/WalkableBandTest.cpp` (class `FCityGenFormulaWalkableBandTest`, name `StealthGame.CityGen.Formula.WalkableBand`)
- `Source/StealthGameplay/Private/Tests/CityGen/PerimeterAndSawtoothTest.cpp` (class `FCityGenFramePerimeterAndSawtoothTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/GridStateHashTest.cpp` (class `FCityGenFrameGridStateHashTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Formula.WalkableBand`
  - Given: defaults f_walk 0.64, P 3, tolerance 5%, A_min 2,000, A_max 2,700.
  - When: N is 62, 64 and 66.
  - Then: T is 2,007, 2,153 and 2,304. The N = 64 band is [2,046, 2,260].
  - Edge cases: derived bands are [2,000, 2,107] for N = 62 and [2,189, 2,419] for N = 66. The batch-level clause of AC-G26 lands in story 037.
- **AC2** `Frame.PerimeterAndSawtooth`
  - Given: 50 candidates from a seed list covering N = 62 to 66.
  - When: S1 runs.
  - Then: N is in 62-66. The 3 c perimeter is all sealed 6.4 m building cells facing non-walkable water skirt. The longest straight perimeter-lane stretch is at most 12 c, with a 2 c notch every 12 c.
  - Edge cases: a 13 c fixture fails. Corners don't reset a run.
- **AC3** `Frame.GridStateHash`
  - Given: one seed.
  - When: it is run twice.
  - Then: the grid is row-major canonical and the post-S1 stage hash is equal.
  - Edge cases: flipping one cell changes the hash (negative control).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-001-citygen-tuning-keys-invariants-and-params.md`, `story-003-integer-stage-streams-and-canonical-iteration.md`
- Unlocks: `story-006-s2-skeleton-lattice-widths-jogs-transpose-and-jitter.md`, `story-008-oq9-final-court-enumeration-script.md`
