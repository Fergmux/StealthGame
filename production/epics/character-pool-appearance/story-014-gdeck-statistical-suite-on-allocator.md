# Story 014: G-DECK statistical indistinguishability suite on the real allocator

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-007 = Statistical indistinguishability gate G-DECK automated.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Leak and parity tests are headless pure-C++ cores plus a multi-client harness; statistical gates use fixed pre-registered seeds, a stated alpha and a harness that is itself proven to reject known-bad implementations.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0008); plain C++ here
**Engine Notes**: No engine API. Seeds are a `constexpr` array in `CharacterPoolTestSeeds.h` beside the tests: chosen once, never reseeded, no file I/O.

**Why this story**: G-DECK is the gate that makes Pillar 1 true "at the level of statistics" (GDD CP-15, F-8). It evaluates T1 to T7 at the F-8 reference `(P, N) = (10, 40)`, `D = 50`, `r = 2` (deliberately off the shipping default `D = 60` where `r = 0` would make T1 and T7 vacuous). Read GDD Formulas F-8 (the 8-statistic table and null expectations) and AC-CP-04 by section; the qa-lead reviewed this story from the AC text and the AC range, not the F-8 table, so re-check the T1 to T7 numbers against F-8 when implementing.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: G-DECK seeds are a `constexpr` array; tests live in `Source/StealthGameplay/Private/Tests/CharacterPool/` (ADR-0012, ADR-0008)
- Forbidden: Never reseed a failing seed; never disable or skip a failing test to pass CI (testing standards)
- Guardrail: Suite runs headless in the per-commit set (T4 of ADR-0008); nightly calibration is story 015

---

## Acceptance Criteria

*From GDD AC-CP-04 and F-8:*

- [ ] **AC-1** GIVEN `R = 2000` allocator calls on each suite seed at the F-8 reference `(P, N) = (10, 40)`, WHEN T1 to T7 run (T7 scaled by `(K - 1) / (K - r)`, T3b as a per-round permutation test), THEN no statistic rejects at alpha 0.001 and T3a is byte-identical (test `StealthGame.CharacterPool.Deck.IndistinguishabilitySuite`).
- [ ] **AC-2** GIVEN the test source, THEN the seeds are a `constexpr` array `CharacterPoolTestSeeds::SuiteSeeds` in `CharacterPoolTestSeeds.h` chosen once and never reseeded, each suite seed starts one stream that produces all `R` deals in sequence, the test performs no file I/O, and an empty seed array fails the test.

---

## Implementation Notes

*Derived from GDD F-8 and ADR-0012 Implementation Guideline 12:*

- Implement the statistics as a small reusable header (`DeckStatistics.h`): chi-square, hypergeometric z, permutation test; critical values as named constants (chi-square df 11 at alpha 0.001 = 31.264; z = 3.291).
- The suite drives the real allocator through the facade with an injected `IEntropySource`, so it exercises the production code path; the biased allocators of story 015 use the same harness.
- T2-T6 may also run at `D = 60` with null expectations recomputed (F-8); keep that as an optional extra, not the gate.

**Prototype disposition**: none (the prototype had no statistical test).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 015: biased-allocator rejection and nightly calibration; Story 016: art-bible T6; Story 056: the playtest chi-square (AC-CP-25)

---

## Files

- `Source/StealthGameplay/Private/Tests/CharacterPool/DeckStatistics.h`
- `Source/StealthGameplay/Private/Tests/CharacterPool/CharacterPoolTestSeeds.h`
- `Source/StealthGameplay/Private/Tests/CharacterPool/IndistinguishabilitySuiteTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; conditional on re-checking F-8's T1 to T7 numbers).*

**Test: suite at the F-8 reference**
- Given: `(P,N) = (10,40)`, `R = 2000` calls per suite seed from `CharacterPoolTestSeeds::SuiteSeeds`, one stream per seed producing all `R` deals.
- When: T1 to T7 run, T7 scaled by `(K-1)/(K-r)`, T3b a per-round permutation test.
- Then: no statistic rejects at alpha 0.001, and T3a is byte-identical.
- Edge cases: the count and values of `SuiteSeeds` are fixed in the header and never regenerated.

**Test: harness hygiene**
- Given: the test source.
- When: it runs.
- Then: no file I/O occurs and the seeds are `constexpr`.
- Edge cases: the test fails if the seed array is empty.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/IndistinguishabilitySuiteTest.cpp` — automation test `StealthGame.CharacterPool.Deck.IndistinguishabilitySuite` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-013-allocator-facade-signature-and-call-order.md`, `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`
- Unlocks: `production/epics/character-pool-appearance/story-015-gdeck-rejects-biased-allocators-and-calibration.md`
