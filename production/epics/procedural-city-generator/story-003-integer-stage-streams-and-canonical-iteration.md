# Story 003: Integer stage streams and canonical iteration

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams
**ADR Decision Summary**: A public seed rebuilds the identical city on every machine: Session's reference `Mix32` derives public sub-seeds and the Generator and client decoration draw from an integer-only PCG32 `FIntStream` with multiply-shift rejection, never `FRandomStream`, `FMath::Rand` or floats. Server-secret draws use a different, keyed stream the Generator may not include.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API beyond integer arithmetic. Golden vectors must reproduce on every CI platform and compiler; cross-compiler stability is claimed only for configurations actually run (MSVC x64 today).

**Control Manifest Rules (this layer, plus Foundation random rules)**:
- Required: Every public geometry draw comes from `FIntStream`; ranged draws use `UniformBelow`/`UniformInRange`; shuffles are Fisher-Yates over canonically sorted arrays; Generator code runs inside `FPublicOnlyScope`; golden vectors are committed — source: ADR-0006
- Forbidden: `FRandomStream`, `FMath::Rand*`, `std::rand`, `<random>`; `%` on a draw, floats in a ranged draw, an unordered container as draw input; any `CityGenerator/` include of `Public/Server/Random/` — source: ADR-0006, ADR-0007
- Guardrail: none (pure integer math)

This story is the Generator's thin layer over the Session primitives: stage seeds, the stage-stream factory and the canonical iteration helpers every later stage uses. The primitives themselves (PCG32 vectors, ranged draws, `Mix32` goldens) belong to the Session epic and are reused, not rewritten.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G3, R-G3.2-3), scoped to this story:*

- [ ] **AC1** (AC-G3, `Rng.IntegerOnlyGolden`): **GIVEN** the Generator's `FIntStream` stage streams **THEN** committed golden vectors reproduce bit-exactly, `RangeInt(7)` over 70,000 draws passes chi-square at p >= 0.01, and the float-draw counter on the build path is 0.
- [ ] **AC2** (R-G3.2, `Rng.StageSeedIndependence`): **GIVEN** `StageSeed(s, j) = Mix32(AttemptSeed, s, j)` **THEN** changing stage s+1's parameters leaves stage s's output byte-identical, and the 576 pinned tuples (attempts 0-5 x stages 1-12 x candidates 0-7) give 576 distinct seeds; j is never folded into the Attempt field.
- [ ] **AC3** (R-G3.3, `Rng.CanonicalIteration`): **GIVEN** cells, blocks, corridors and sockets presented in two different container insertion orders **THEN** row-major and stable-id iteration produces identical draws, Fisher-Yates runs only over canonically sorted arrays, and a seeded unordered-container draw input is caught by the lint (negative control).

---

## Implementation Notes

*Derived from ADR-0006 Decision section 6 and GDD R-G3:*

Add `FCityGenStreams` (plain C++, no `UObject`): `MakeStageStream(AttemptSeed, StageId, CandidateIndex)` returns an `FIntStream` seeded from `Mix32(AttemptSeed, s, j)` using Session's reference mixer as a pure function (Session F5 vectors `Mix32(123456789,1,0) = 2107646860`, `(…,1,1) = 523860287`, `(…,2,0) = 313478722` stay the authority). Stage ids are an `enum class ECityGenStage : uint8` S1..S12 plus a G-PRE id, never renumbered (a bump of `GeneratorKitVersion` is the only way to change one). `j` is the candidate index and is never added to the Attempt field, because `Mix32(Seed, s, k)` for a real attempt k would collide. Canonical iteration helpers: `ForEachCellRowMajor`, `ForEachBlockById`, `SortedById(TArrayView)`; a draw input is always one of these or a `TArray` sorted by an integer key. The float-draw counter is a non-Shipping counter incremented by any float-typed ranged-draw entry point on the build path (there must be none: the counter proves it). The chi-square uses a fixed seed and the critical value for df 6 (16.81 at p = 0.01); a deliberately biased `RangeInt` (modulo) fixture is the negative control.

Prototype replaced: the prototype generator drew from `FRandomStream` seeded by a replicated server seed (`StealthCityGenerator.cpp`); none of that is carried over.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `session-travel/story-006-pcg32-int-stream-reference-vectors.md`, `story-007-ranged-draw-helpers-unbiased.md`, `story-010-seed-derivation-golden-vectors.md`: the primitives and their vectors.
- `story-030-pure-function-determinism-and-stage-hashes.md`: whole-pipeline determinism across processes.
- `story-031-golden-seeds-and-bump-rule.md`: the 20 committed golden seeds.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStreams.h`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStreams.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenCanonical.h`
- `Source/StealthGameplay/Private/Tests/CityGen/IntegerOnlyGoldenTest.cpp` (class `FCityGenRngIntegerOnlyGoldenTest`, name `StealthGame.CityGen.Rng.IntegerOnlyGolden`)
- `Source/StealthGameplay/Private/Tests/CityGen/StageSeedIndependenceTest.cpp` (class `FCityGenRngStageSeedIndependenceTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/CanonicalIterationTest.cpp` (class `FCityGenRngCanonicalIterationTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Rng.IntegerOnlyGolden`
  - Given: committed golden vectors per stage stream.
  - When: the streams are replayed.
  - Then: outputs are bit-exact. `RangeInt(7)` over 70,000 draws from a fixed seed has chi-square (df 6) below 16.81. The float-draw counter over a full build-path run is 0.
  - Edge cases: a deliberately biased `RangeInt` (modulo bias) fails the chi-square (negative control).
- **AC2** `Rng.StageSeedIndependence`
  - Given: one AttemptSeed fixture.
  - When: stage s+1 params change, for each s from 1 to 11.
  - Then: stage s output bytes are identical. The 576 tuples (attempts 0-5, stages 1-12, candidates 0-7) give 576 distinct StageSeeds. The Attempt field of the `Mix32` input always equals AttemptSeed.
  - Edge cases: a variant that folds j into the Attempt field produces a collision and fails (negative control).
- **AC3** `Rng.CanonicalIteration`
  - Given: cells, blocks, corridors and sockets inserted in two fixed shuffled orders.
  - When: the draws run.
  - Then: draw sequences and outputs are equal. Every Fisher-Yates input array is verified sorted by canonical key. The lint flags unordered-container iteration feeding a draw.
  - Edge cases: a seeded `TMap`-iteration draw input is caught (negative control).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-002-provider-shell-registration-and-secret-input-isolation.md`; cross-epic `session-travel/story-006-pcg32-int-stream-reference-vectors.md`, `story-007-ranged-draw-helpers-unbiased.md`, `story-010-seed-derivation-golden-vectors.md`, `story-012-forbidden-rng-ci-grep-and-include-lint.md`
- Unlocks: `story-005-grid-model-s1-frame-and-f1-walkable-target.md`, `story-006-s2-skeleton-lattice-widths-jogs-transpose-and-jitter.md` and every stage story
