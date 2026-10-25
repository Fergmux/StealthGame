# Story 011: Valid-subset table and the model-stratified draw (F-4)

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-002 = `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: A valid-subset table is built once per `FPoolView`: for each reachable `K`, enumerate every stratified subset (per-model variant counts differing by at most 1, every model once when `K >= M`), keep those with at least `SwatchFloor = min(5, K, M)` distinct garment swatches, deduplicate, and store a sorted array of `uint32` row-ordinal bitmasks; the subset is one uniform draw over the table.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); plain C++
**Engine Notes**: No engine API. The table is canonical (sorted) so a draw over it is deterministic given the stream. Draws use ADR-0006 `UniformBelow`.

**Why this story**: CP-19 and F-4 make the active subset per-match, shared, uniform over valid subsets with no re-draw loop; this is the most intricate piece of the allocator and the one the pool validator (story 006 AC-4) relies on. Read GDD CP-19, Formulas F-4, AC-CP-02, 02b by section.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Allocator draws use `UniformBelow`/`FisherYates` over canonically sorted inputs (ADR-0012, ADR-0006)
- Forbidden: Never use `%` on a draw, floats in a draw, or an unordered container as draw input (ADR-0006)
- Guardrail: Deal at most 0.2 ms once per round (ADR-0012); table built once per pool

---

## Acceptance Criteria

*From GDD AC-CP-02 and AC-CP-02b and ADR-0012 §2; AC-3 revised per QL-STORY-READY (no wall-clock assertion):*

- [ ] **AC-1** GIVEN 8 models x 3 variants and `K = 12` over the 1,000 seeds `CharacterPoolTestSeeds::StratifiedSet`, THEN every model appears at least once, 4 models contribute 2 appearances and 4 contribute 1, at least 5 distinct garment swatches are active, and each appearance's inclusion frequency matches the exact probability enumerated by an independent oracle in the test (binomial interval 99.9 %, Bonferroni over 24); GIVEN the MVP 6 x 2 pool and `K = 6` THEN each of the 32 valid subsets occurs with frequency 1/32 (chi-square, df 31, alpha 0.001) and no deal fails; GIVEN a stub pool with no valid subset THEN `DeckInvalid` and the validator reports it (test `StealthGame.CharacterPool.Deck.StratifiedSubset`).
- [ ] **AC-2** GIVEN the fixture `FPoolFixtures::UnequalVariants` (3 models with 1, 2 and 3 variants, 6 appearances) and `K` in {3, 4, 5, 6} over `StratifiedSet`, THEN per-model counts differ by more than 1 only where a model has no unchosen variant (test `...Deck.StratifiedUnequalVariants`).
- [ ] **AC-3** GIVEN an `FPoolView`, THEN the valid-subset table is a sorted `TArray<uint32>` of row-ordinal bitmasks (canonical) and is built exactly once per `FPoolView` (build counter is 1 across 1,000 deals); a non-gating performance record logs the median of 100 builds for the 8 x 3 pool (target under 1 ms on the reference PC).

---

## Implementation Notes

*Derived from ADR-0012 §2 (Valid-subset table):*

- The enumeration is at most a few thousand masks for 8 x 3. Deduplicate and sort before storing; the mask is over row ordinals of the `FPoolView`, not `AppearanceId`.
- The oracle in the test must share no code with the allocator: enumerate the valid-subset law independently and compare inclusion frequencies.
- Statistics tests use the fixed seeds (header from story 010); a failing seed is a defect, never reseeded.
- Expose the table through an interface story 006 AC-4 can call (`GetReachableKValidity(PoolView)`).

**Prototype disposition**: the prototype dealt from a flat list of four citizens with no model structure; nothing carries over.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010: sizing counts; Story 012: extras set draw, shuffle and the deal; Story 014: the G-DECK suite

---

## Files

- `Source/StealthGameplay/Public/Server/CharacterPool/AppearanceDeckCore.h` (subset table)
- `Source/StealthGameplay/Private/Server/CharacterPool/ValidSubsetTable.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/StratifiedSubsetTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-1 to AC-3 revised with the qa-lead's proposed text; not re-run (unattended; decision logged).*

**Test: stratified subset** (`StealthGame.CharacterPool.Deck.StratifiedSubset`)
- Given: the 8 x 3 pool at `K = 12`, and the MVP 6 x 2 pool at `K = 6`.
- When: 1,000 deals each from `StratifiedSet`.
- Then: every model appears at least once; 4 models contribute 2 appearances and 4 contribute 1; at least 5 distinct garment swatches are active; per-appearance frequency lies in the 99.9 % binomial interval with Bonferroni over 24; the 6 x 2 pool shows each of 32 subsets at 1/32 (chi-square df 31, alpha 0.001) with no failed deal; a stub with no valid subset gives `DeckInvalid` and a validator report.
- Edge cases: the oracle shares no code with the allocator.

**Test: unequal variants** (`...StratifiedUnequalVariants`)
- Given: `UnequalVariants` at each `K`.
- When: dealing.
- Then: per-model counts differ by more than 1 only where a model has no unchosen variant.
- Edge cases: a 1-variant model at `K = 3`.

**Test: table structure**
- Given: the 8 x 3 pool.
- When: the `FPoolView` is built, then 1,000 deals run.
- Then: the array is sorted, masks are `uint32`, the build counter is 1.
- Edge cases: two views of different pools build independent tables.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/StratifiedSubsetTest.cpp` — automation tests `StealthGame.CharacterPool.Deck.StratifiedSubset`, `...Deck.StratifiedUnequalVariants` must exist and pass (`/story-done` checks they exist; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-010-deck-sizing-and-floor-policy-f1-f3.md`, `production/epics/session-travel/story-007-ranged-draw-helpers-unbiased.md`
- Unlocks: `production/epics/character-pool-appearance/story-012-deal-shuffle-canonical-order-and-streams.md`, `production/epics/character-pool-appearance/story-006-swatch-rule-f7-validator.md` (AC-4 interface)
