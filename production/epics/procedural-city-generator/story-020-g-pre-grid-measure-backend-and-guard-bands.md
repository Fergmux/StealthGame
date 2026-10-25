# Story 020: G-PRE grid measure backend and guard bands

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
**ADR Decision Summary**: Every V-* predicate exists once, written against `ILayoutMeasureBackend`; the Generator supplies a grid backend (integer BFS, integer path lengths, DDA sightlines, W(O) ray casts) and the service supplies the world backend. The grid and world backends must not fork predicate logic. G-PRE adds a guard band because grid and navmesh path lengths differ by 5-8%.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++, floats only in measurement)
**Engine Notes**: No engine API. The fixed-sun raycast, W(O) rays and sightlines are measurements; any float is quantised to cm (round half away from zero) before comparison so the verdict is identical across CPUs and compilers (R-G3.4).

**Control Manifest Rules (this layer)**:
- Required: Every V-* predicate exists once, written against `ILayoutMeasureBackend` — source: ADR-0014
- Forbidden: Never fork predicate logic between the grid and world backends; never decide a G-PRE verdict from a float comparison — source: ADR-0014, ADR-0006
- Guardrail: G-PRE is part of the 0.12 s candidate budget; the pre-validator discards most bad cities in about 1 s total

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (R-G5.2, R-G3.4, AC-G39 last clause), scoped to this story:*

- [ ] **AC1** (R-G5.2 / ADR guideline 8, `GPre.SinglePredicateSource`): **GIVEN** the LSC V-* predicate code **THEN** the grid backend implements `ILayoutMeasureBackend` (integer BFS, integer grid path lengths, DDA sightlines in every direction, W(O) ray casts, fixed-sun raycast) and no predicate is forked: a static check finds one definition per V-* id.
- [ ] **AC2** (R-G5.2, `GPre.GuardBands`): **GIVEN** default params **THEN** the guarded thresholds are the committed constants StartToFinal 47.5 m, ZoneSeparation 76 m, legs 33/43/43 m, enemy-zone-to-prelim 54 m, A_z x 1.04 and chain band 130-222 m, and ratio predicates use the 4-point band (fairness 1.10 checked at 1.06).
- [ ] **AC3** (AC-G39 last clause, `GPre.ZonePredicatesAndPathGuard`): **GIVEN** an injected candidate whose grid StartToFinal path is 46 m **THEN** G-PRE rejects it (inside the 8% guard of the 44 m floor), while a 48 m candidate passes this predicate.
- [ ] **AC4** (R-G3.4, `GPre.IntegerOnly`): **GIVEN** a candidate **THEN** floats are quantised to cm by round-half-away-from-zero before any comparison, the verdict is identical on two runs and the float-op counter in the decision path is 0.

---

## Implementation Notes

*Derived from GDD R-G5.2 and ADR-0014 guideline 8:*

The LSC predicates (stories 016-019 of the LSC epic) are written once against `ILayoutMeasureBackend` (LSC story 015 defines the world backend). This story writes `FGridMeasureBackend`: integer BFS connectivity, integer grid path lengths, DDA sightlines in every direction (diagonals across plazas and junctions, LSC change request 1b), W(O) ray casts, the sun-pool raycast at the fixed sun. Guard bands: `CityGenGPreGuardBandPct` (4 points) on ratio predicates (fairness 1.10 is pre-checked at 1.06) and `CityGenGPrePathGuardPct` (8%) on absolute floors and band edges: floors raised 8% (StartToFinal 44 -> 47.5 m since decision D4 2026-10-25, ZoneSeparation 70 -> 76 m, legs 30/40/40 -> 33/43/43 m, enemy zone to prelim 50 -> 54 m, A_z x 1.04 on area) and ceilings lowered 8% (chain band 120-240 -> 130-222 m). No single rounding rule reproduces those committed values (30 x 1.08 = 32.4 -> 33, 40 x 1.08 = 43.2 -> 43, 240 x 0.92 = 220.8 -> 222), so they are committed constants at the default guard keys; state in the dev-story notes how a non-default `CityGenGPrePathGuardPct` (hard range 0-15) derives its values (proposal: multiply and round to the nearest 0.5 m, outward for floors) and ask the GDD owner to ratify. The first 50-seed batch measures the sign of the grid-versus-navmesh difference; a side that never binds may then be set to 0 (story 039). Quantisation: round half away from zero to cm (47.495 m -> 4750 cm).

Prototype replaced: the prototype logged a reachability check on the navmesh after build; there is no grid pre-validator.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-021-g-pre-predicates-a.md`, `story-022-g-pre-predicates-b-and-role-counts.md`: the per-predicate fixtures and the role-count pre-check.
- `story-039-g-pre-false-pass-rate-and-guard-band-sign.md`: p_v measurement and guard sign.
- The predicate code itself and the world backend: `level-socket-contract/story-015-layout-measure-backend-world.md` to `story-019-predicates-courts-pool-bushes-clue.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenGridMeasureBackend.h`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenGridMeasureBackend.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenGPre.cpp` (guard bands)
- `Source/StealthGameplay/Private/Tests/CityGen/SinglePredicateSourceTest.cpp` (class `FCityGenGPreSinglePredicateSourceTest`, name `StealthGame.CityGen.GPre.SinglePredicateSource`)
- `Source/StealthGameplay/Private/Tests/CityGen/GuardBandsTest.cpp` (class `FCityGenGPreGuardBandsTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/ZonePredicatesAndPathGuardTest.cpp` (class `FCityGenGPreZonePredicatesAndPathGuardTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/IntegerOnlyTest.cpp` (class `FCityGenGPreIntegerOnlyTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC3 ADEQUATE; AC2 and AC4 revised and ADEQUATE).*

- **AC1** `GPre.SinglePredicateSource`
  - Given: the LSC V-* predicate source.
  - When: the static check runs.
  - Then: the grid backend implements `ILayoutMeasureBackend` (integer BFS, integer path lengths, DDA in every direction, W(O) rays, fixed-sun raycast). Each V-* id has exactly 1 definition.
  - Edge cases: a seeded duplicate V-SIGHT in the Generator fails (negative control). Ids are enumerated from the LSC table.
- **AC2** `GPre.GuardBands`
  - Given: default params.
  - When: the guarded thresholds are read.
  - Then: they equal the committed constants (StartToFinal 47.5 m, ZoneSeparation 76 m, legs 33/43/43 m, enemy-zone-to-prelim 54 m, A_z x 1.04, chain band 130-222 m). Ratio predicates use the 4-point band (fairness 1.10 checked at 1.06).
  - Edge cases: the constants hold at the default guard keys only; the derivation for non-default `CityGenGPrePathGuardPct` is stated in the notes. A fairness ratio of 1.061 fails and 1.059 passes.
- **AC3** `GPre.ZonePredicatesAndPathGuard`
  - Given: injected candidates with grid StartToFinal of 46 m and 48 m, all other predicates clean.
  - When: G-PRE runs.
  - Then: 46 m is rejected with the path-floor failure id. 48 m passes this predicate.
  - Edge cases: 47.5 passes and 47.4 fails. At guard 0% the 44 m floor lets 46 pass. At guard 15% the floor is 50.6 m.
- **AC4** `GPre.IntegerOnly`
  - Given: candidates with float measurements, such as 47.495 m and -x mirrors.
  - When: G-PRE runs twice.
  - Then: floats are quantised to cm by round-half-away-from-zero (47.495 -> 4750, -47.495 -> -4750) before any comparison. The float-op counter in the decision path is 0 and the verdicts are identical.
  - Edge cases: a threshold-straddling value, 4749.5 cm against 4750, passes or fails identically on both runs.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the four test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-005-grid-model-s1-frame-and-f1-walkable-target.md`, `story-006-s2-skeleton-lattice-widths-jogs-transpose-and-jitter.md`; cross-epic `level-socket-contract/story-015-layout-measure-backend-world.md`, `story-016-predicates-emergence-witness-alley.md`, `story-017-predicates-navigation-area-sight-route-traversal.md`, `story-018-predicates-zones-separation-reach-respawn.md`, `story-019-predicates-courts-pool-bushes-clue.md`
- Unlocks: `story-016-s11-repairs-and-occurrence-rule.md` (sun raycast), `story-021-g-pre-predicates-a.md`, `story-022-g-pre-predicates-b-and-role-counts.md`, `story-023-candidate-loop-work-unit-cap-and-metrics.md`
