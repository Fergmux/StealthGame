# Story 016: Art-bible T6 allocator invariants simulated over 10,000 matches (OI-11)

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-002 = `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: The allocator deals one balanced deck per round (counts differ by at most 1, at least 4 copies of each active appearance, at most 12 active) from a model-stratified subset drawn from the server-private `Deal` stream.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); plain C++
**Engine Notes**: No engine API.

**Why this story**: Art bible 1.3.7 test **T6** (Allocator invariants, "owned by the Character Pool & Appearance GDD") is the last open sub-item of OI-11, which the art bible's register says must be closed "before the allocator implementation story starts": it is a simulation of the allocator's own output, so it is written with the first allocator stories and closes the item. Read art bible 1.3.7 T6 row and 8.x.17 OI-11 by section.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Statistical tests use the constexpr seed header, chosen once and never reseeded (ADR-0012)
- Forbidden: Never reseed a failing seed (a failing seed is a defect)
- Guardrail: 10,000 deals complete well inside the unit-test time budget; non-gating timing record only

---

## Acceptance Criteria

*From art bible 1.3.7 T6 (a) to (e); (d) and (e) revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN 10,000 simulated deals from `CharacterPoolTestSeeds::T6Seeds` at the default `(P, N)`, THEN (a) max minus min copies is at most 1 in every match, (b) every appearance has at least 4 copies, (c) the active subset has at most 12 appearances, (d) a chi-square test of homogeneity between the NPC-copy-count histograms of player appearances and non-player appearances over the 10,000 matches gives p > 0.05 (art bible T6(d)), (e) the active subset is identical when the same seed is dealt with different player lists of the same size.
- [ ] **AC-2** GIVEN T6 passes, THEN OI-11's remaining sub-item is marked closed in art bible 8.x.17 with the date (not marked if any sub-assertion failed).

---

## Implementation Notes

*Derived from art bible 1.3.7 and GDD F-5 (team-blindness proof):*

- Reuse the statistics header from story 014; a seeded bug (one appearance dealt 3 copies) must make (b) fail, so run that once to watch the check fail.
- (e) is the allocator's seed-only dependence (GDD AC-CP-06) seen from the art side.

**Prototype disposition**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014 and 015: the G-DECK suite and its falsification

---

## Files

- `Source/StealthGameplay/Private/Tests/CharacterPool/T6AllocatorInvariantsTest.cpp`
- `design/art/art-bible.md` (8.x.17 OI-11 row only)

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-1 (d), (e) and seeds revised with the qa-lead's proposed text; not re-run (unattended; decision logged).*

**Test: allocator invariants T6** (`StealthGame.CharacterPool.Deck.T6AllocatorInvariants`)
- Given: 10,000 simulated deals from `T6Seeds` at the default `(P,N)`.
- When: each deal is checked.
- Then: (a) max minus min copies at most 1, (b) every appearance at least 4 copies, (c) subset at most 12 appearances, (d) homogeneity p > 0.05, (e) subset identical across player lists of the same size.
- Edge cases: a seeded bug (one appearance with 3 copies) fails (b).

**Test: OI-11 closure**
- Given: T6 green.
- When: art bible 8.x.17 is updated.
- Then: OI-11's sub-item is marked closed with the date.
- Edge cases: no update if any sub-assertion failed.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/T6AllocatorInvariantsTest.cpp` — automation test `StealthGame.CharacterPool.Deck.T6AllocatorInvariants` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-013-allocator-facade-signature-and-call-order.md`, `production/epics/character-pool-appearance/story-014-gdeck-statistical-suite-on-allocator.md`
- Unlocks: None
