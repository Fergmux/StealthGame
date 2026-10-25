# Story 010: Deck sizing (D, K, q, r) and the floor policy

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-002 = `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: `D = P + N`; `K = max(1, min(A, ActiveMax, floor(D / CopiesFloor)))`; copies `q = floor(D / K)` with `r = D mod K` extras; floor check `K >= ActiveMin`; `Enforce` returns `DeckInvalid`, the non-Shipping `Warn` policy proceeds with the largest feasible `K` and flags `DeckBelowFloor`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); this story is plain C++ integer arithmetic
**Engine Notes**: No engine API. `FAppearanceDeckCore` is plain C++ (no `UObject`, no `UWorld`; ADR-0007).

**Why this story**: This is the first slice of `FAppearanceDeckCore::Deal`: the counts. Read GDD CP-6, CP-7, Formulas F-1 to F-3, Edge Cases "Deal and validation" and AC-CP-01, 01b, 08b.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The deal input is an opaque set of `FParticipantId`, sorted by the facade; `Enforce` is the only floor policy compiled into Shipping (ADR-0012)
- Forbidden: Never ship the `Warn` floor policy; never change `D` or any `c_a` after the deal (ADR-0012)
- Guardrail: Deal at most 0.2 ms once per round (ADR-0012)

---

## Acceptance Criteria

*From GDD AC-CP-01, AC-CP-01b and AC-CP-08b, scoped to this story:*

- [ ] **AC-1** GIVEN an MVP pool of 12 appearances, WHEN `AllocateAppearances` runs for each of 1,000 seeds at `(P, N) = (10, 50)`, THEN `D = 60`, `K = 12`, every appearance has exactly 5 copies (`r = 0`, empty extras set), sum = 60; also `(10, 40)` gives `K, q, r = 12, 4, 2`; `(10, 36)` gives `11, 4, 2`; `(10, 32)` gives `10, 4, 2`; `(10, 14)` gives `6, 4, 0` with no `DeckInvalid` on any seed (test `StealthGame.CharacterPool.Deck.CountsBalanced`).
- [ ] **AC-2** GIVEN the `Warn` policy (non-selectable development preset), THEN `(2, 10)` gives `K, q, r = 3, 4, 0` with `DeckBelowFloor` and `SwatchFloor = 3`, and an 8-appearance pool at `D = 50` gives `K = 8`, `q = 6`, `r = 2` with `PoolBelowMvpFloor`; GIVEN `Enforce` and the same inputs THEN `DeckInvalid` (test `...Deck.FloorPolicyBranches`).
- [ ] **AC-3** GIVEN `A = 14`, `D = 110` THEN `K = 12` (the cap binds); GIVEN `A = 8`, `D = 50` (development) THEN `K = 8` (test `...Deck.ValidationLadder.CapAndPoolBound`).

---

## Implementation Notes

*Derived from ADR-0012 §2 (Deal step 1) and the GDD formulas:*

- Seeds are `CharacterPoolTestSeeds::Counts` (1,000 values) in `CharacterPoolTestSeeds.h`, chosen once, never reseeded; no file I/O.
- This story computes the sizing and the extras count; drawing the subset and the extras set is story 011/012. Until they land, expose the sizing as a pure function `ComputeDeckSizing(D, A, Params, Policy)` the tests call directly, and let `Deal` call it.
- `EDealError::DeckTooSmall` (`D < CopiesFloor`) is a defensive refusal, unreachable under TDC hard ranges; cover it with one direct call.
- `EDeckFloorPolicy::Warn` exists only under `#if !UE_BUILD_SHIPPING`; story 042 audits that it is absent from Shipping.

**Prototype disposition**: the prototype's balanced-deck idea (in `StealthGameMode.cpp`) is the origin of this rule; it is rewritten, not promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 011: valid-subset table and the stratified draw; Story 012: shuffle, deal and streams
- Story 013: the facade, call-order guard and misuse branches

---

## Files

- `Source/StealthGameplay/Public/Server/CharacterPool/AppearanceDeckCore.h`
- `Source/StealthGameplay/Private/Server/CharacterPool/AppearanceDeckCore.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/CharacterPoolTestSeeds.h`
- `Source/StealthGameplay/Private/Tests/CharacterPool/DeckSizingTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead).*

**Test: F-1 to F-3 counts**
- Given: the MVP 12-appearance pool and seed array `CharacterPoolTestSeeds::Counts` (1,000 values).
- When: `AllocateAppearances` runs for `(P,N)` = (10,50), (10,40), (10,36), (10,32), (10,14).
- Then: `K,q,r` are 12,5,0 with `D = 60`, then 12,4,2, 11,4,2, 10,4,2 and 6,4,0; for (10,50) every appearance has 5 copies and the extras set is empty; copy counts sum to `D`; no `DeckInvalid` on any seed.
- Edge cases: the extras set differs per seed but the counts do not.

**Test: floor policy branches**
- Given: the Warn and Enforce policies.
- When: (2,10) and an 8-appearance pool at `D = 50` are allocated.
- Then: Warn gives `K,q,r` = 3,4,0 with `DeckBelowFloor` and SwatchFloor 3, and 8,6,2 with `PoolBelowMvpFloor`; Enforce gives `DeckInvalid` on the same inputs.
- Edge cases: the test is excluded from the Shipping build.

**Test: cap and pool bound**
- Given: `A = 14` with `D = 110`, and `A = 8` with `D = 50` under the development policy.
- When: allocating.
- Then: `K = 12` (the cap binds), then `K = 8`.
- Edge cases: `A = 12` with `D = 110`, so `K = 12` by pool exhaustion rather than the cap.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/DeckSizingTest.cpp` — automation tests `StealthGame.CharacterPool.Deck.CountsBalanced`, `...Deck.FloorPolicyBranches`, `...Deck.ValidationLadder.CapAndPoolBound` must exist and pass (`/story-done` checks they exist; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-009-deck-tuning-keys-and-invariant-i3.md`
- Unlocks: `production/epics/character-pool-appearance/story-011-valid-subset-table-stratified-draw-f4.md`, `production/epics/character-pool-appearance/story-012-deal-shuffle-canonical-order-and-streams.md`
