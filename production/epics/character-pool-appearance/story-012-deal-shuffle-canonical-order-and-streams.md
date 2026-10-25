# Story 012: Deal, shuffle, canonical order and the three domain-separated streams

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-002`, `TR-cpa-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-002 = `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates. TR-cpa-003 = Server-private streams from `GetServerSubSeed(AppearanceDeal)` (`FSecretStream`, SipHash counter mode) for Replace; `Take` reserved (no draw).)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation (streams per ADR-0006)
**ADR Decision Summary**: `Root = GetServerSubSeed(AppearanceDeal, 0)`; three `FSecretStream`s (`Deal`, `Replace`, `Take` reserved and never consumed) derived by keyed hash with a stream tag; the deal draws subset, extras, then Fisher-Yates over a canonical-order deck; participants (sorted by `ParticipantId`) take cards `0..P-1`, NPC cards are `P..D-1`; only `DealDigest` is exposed.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); plain C++
**Engine Notes**: No engine API. The entropy-injection hook (`IEntropySource`) is compiled out of Shipping (story 042 audits it).

**Why this story**: This is the allocator's random heart: team-blindness and unreachable seed derivation (CP-8, CP-9, F-5). Read GDD CP-8, CP-9, Formulas F-5 ("Team-blindness proof"), AC-CP-06, AC-CP-09, and ADR-0006 §5 and §7 (keyed streams, `DealDigest`).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every allocator draw comes from `FSecretStream(GetServerSubSeed(AppearanceDeal, 0), Tag("CPA_DEAL"|"CPA_REPL"))` with `UniformBelow`/`FisherYates` over sorted inputs (ADR-0012)
- Forbidden: Never consume `CPA_TAKE`; never log, replicate or reflect the root sub-seed, stream state or participant-to-card map; no engine RNG (ADR-0006, ADR-0012)
- Guardrail: Deal at most 0.2 ms once per round

---

## Acceptance Criteria

*From GDD AC-CP-06 and AC-CP-09 and ADR-0012 §2, scoped to this story:*

- [ ] **AC-1** GIVEN a stub stream, THEN the rejection-sampled generator yields equal frequencies for a range of 3 over a 2-bit source, the shuffle equals a reference Fisher-Yates driven by the same stub stream (golden), participants take the first `|Players|` cards in canonical `ParticipantId` order, and a code check (`LINT-CPA-NoResort`) proves no step after the deal sorts or groups cards by appearance (test `StealthGame.CharacterPool.Deck.ShuffleUnbiasedNoResort`).
- [ ] **AC-2** GIVEN a fixed injected entropy, WHEN the layout seed, round id, participant ids and server tick are each varied, THEN the deal is byte-identical; GIVEN two different entropies THEN the deals differ (test `...Deck.SeedNotDerivable`).
- [ ] **AC-3** GIVEN the `Deal`, `Replace` and `Take` streams for a fixed root, THEN their outputs differ (domain separation, golden vectors), the `CPA_TAKE` tag is never consumed (`LINT-CPA-NoTakeTag`), and `FDealSummary` exposes `DealDigest` and no root or entropy property.

---

## Implementation Notes

*Derived from ADR-0012 §2 (Deal, Streams) and ADR-0006:*

- Use the session-travel stories' `FSecretStream`, `UniformBelow` and `FisherYates` (stories 005, 007, 008); do not reimplement them.
- Order of draws from `Deal` is fixed: (a) subset index `UniformBelow(Table[K].Num())`, (b) extras via partial Fisher-Yates over the sorted active ids taking the first `r`, (c) build the deck in canonical order (active ids ascending, `q` or `q+1` copies) and run `FisherYates` over all `D` cards.
- Golden vectors for the three streams are committed constants produced by an independent reference (ADR-0006 vectors), not by the code under test.
- Register `LINT-CPA-NoResort` and `LINT-CPA-NoTakeTag` with story 042's lint harness and watch each fail against a seeded violation.

**Prototype disposition**: replaces `FMath::RandRange` dealing in `StealthGameMode.cpp` (invertible, engine-versioned, forbidden by ADR-0006).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 013: facade, signature and call-order; Story 018 and 019: ledger operations and the `Replace` draw itself
- Story 014: the G-DECK statistical suite

---

## Files

- `Source/StealthGameplay/Private/Server/CharacterPool/AppearanceDeckCore.cpp` (Deal)
- `Source/StealthGameplay/Private/Tests/CharacterPool/ShuffleAndSeedTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead). Tests `...Deck.ShuffleUnbiasedNoResort`, `...Deck.SeedNotDerivable`.*

**Test: unbiased generator and Fisher-Yates**
- Given: a stub stream cycling a 2-bit source 0,1,2,3, and a committed golden shuffle.
- When: generating integers in [0,3) over one full stub cycle, and shuffling a 12-card deck.
- Then: values 0, 1 and 2 occur with exactly equal counts (exact), 3 is rejected and resampled; the shuffle equals the reference Fisher-Yates driven by the same stub; participants take the first `|Players|` cards in canonical id order; the no-resort lint is green.
- Edge cases: range 1; a range that is a power of 2.

**Test: entropy-only determinism**
- Given: fixed injected entropy.
- When: the layout seed, round id, participant ids and server tick are each varied, then entropy is changed.
- Then: the deal is byte-identical across the first four variations; two entropies give different deals.
- Edge cases: the participant id set varies in order only.

**Test: stream separation**
- Given: a fixed root.
- When: the Deal, Replace and Take streams each produce 8 values.
- Then: outputs equal committed golden vectors and are pairwise different; `FDealSummary` reflection exposes `DealDigest` and no root or entropy property; the NoTakeTag lint is green.
- Edge cases: a Replace draw count does not move the Deal stream.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/ShuffleAndSeedTest.cpp` — automation tests `StealthGame.CharacterPool.Deck.ShuffleUnbiasedNoResort`, `...Deck.SeedNotDerivable` must exist and pass (`/story-done` checks they exist; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-011-valid-subset-table-stratified-draw-f4.md`, `production/epics/session-travel/story-005-siphash24-reference-vectors.md`, `production/epics/session-travel/story-008-secret-stream-counter-independence.md`, `production/epics/session-travel/story-011-server-sub-seed-server-only.md`
- Unlocks: `production/epics/character-pool-appearance/story-013-allocator-facade-signature-and-call-order.md`, `production/epics/character-pool-appearance/story-014-gdeck-statistical-suite-on-allocator.md`
