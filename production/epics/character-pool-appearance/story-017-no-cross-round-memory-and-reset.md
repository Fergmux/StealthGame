# Story 017: No cross-round memory; Reset discards everything

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
**ADR Decision Summary**: Each round is its own world with its own allocator subsystem and ledger, so CP-14's re-deal is structural; `Reset` drops the deck, active set, ledger and streams; the allocator core is a free function with no static mutable state and no history input.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); plain C++
**Engine Notes**: No engine API.

**Why this story**: A persistent look would let opponents remember who is who (CP-14); the Roster's `AppearanceHistory` is not an allocator input (GDD Interactions). Read GDD CP-14, AC-CP-07.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One deal per round world; the deal input carries no history (ADR-0012)
- Forbidden: Never add a no-repeat-last-round rule (it makes the deal depend on the participant, a leak)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-07 and CP-14:*

- [ ] **AC-1** GIVEN 2,000 independent round pairs for one participant with `K = 12` (independent entropy per pair from the constexpr seeds), THEN the count of "same look as the previous round" lies in [135, 199] (99 % binomial interval around 167; a sanity bound, not a leak gate).
- [ ] **AC-2** GIVEN a `Reset`, THEN the ledger is empty and the streams are dropped, and a code-structure check (`LINT-CPA-FreeFunctionNoStatic`) proves the allocator core is a free function with no mutable static state.

---

## Implementation Notes

- `Reset()` on an empty ledger is a no-op. Cards move to `Discarded` at Reset (Visibility rows, story 041).
- Register `LINT-CPA-FreeFunctionNoStatic` with story 042's lint harness (watch it fail on a seeded static).

**Prototype disposition**: none.

---

## Out of Scope

- Story 018 onward: ledger operations; Story 041: the Event Log content at `Reset`

---

## Files

- `Source/StealthGameplay/Private/Server/CharacterPool/AppearanceDeckCore.cpp` (Reset)
- `Source/StealthGameplay/Private/Tests/CharacterPool/NoCrossRoundMemoryTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead). Test `StealthGame.CharacterPool.Deck.NoCrossRoundMemory`.*

**Test: no memory between rounds**
- Given: 2,000 round pairs with `K = 12` and independent entropy per pair from the seed array.
- When: counting a participant's "same look as previous round".
- Then: the count lies in [135, 199].
- Edge cases: a sanity bound, deterministic through fixed seeds.

**Test: reset**
- Given: a dealt ledger with live streams.
- When: `Reset` runs.
- Then: the ledger is empty and the streams are dropped; the lint finds no mutable static in the allocator core.
- Edge cases: `Reset` on an empty ledger is a no-op.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/NoCrossRoundMemoryTest.cpp` — automation test `StealthGame.CharacterPool.Deck.NoCrossRoundMemory` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-013-allocator-facade-signature-and-call-order.md`
- Unlocks: None
