# Story 019: DrawReplacement is uniform over vacant cards and ordering-safe

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-004 = Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: `DrawReplacement()` chooses one `NpcVacant` card uniformly at random with `UniformBelow(|V|)` on the `Replace` stream over the card-index-sorted vacant set, returns a pending ticket that must be committed in the same call stack as its `SpawnSharedPawn`, and rolls back to `NpcVacant` (ensure in non-Shipping, `LedgerMismatch` in every build) if not.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); plain C++
**Engine Notes**: No engine API. Draws use `FSecretStream` `Replace`, never an invertible PRNG (ADR-0006).

**Why this story**: The look is always the uniform draw over all vacant cards, never the oldest's (CP-12a, F-6); Crowd's schedule governs timing only. Read GDD CP-12(a), F-6, Edge Cases "Replacement, kills and respawn", AC-CP-11a, 11b.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: A `DrawReplacement` ticket commits in the same stack as its `SpawnSharedPawn` (ADR-0012)
- Forbidden: Never draw a replacement uniformly from the active set (breaks conservation, F-6); never consume the `Take` tag
- Guardrail: Draw O(|V|); negligible

---

## Acceptance Criteria

*From GDD AC-CP-11a, AC-CP-11b and ADR-0012 §2:*

- [ ] **AC-1** GIVEN two vacant cards `a` and `b`, WHEN the first replacement is drawn over 10,000 trials from `CharacterPoolTestSeeds::Replace`, THEN `a` and `b` each occur 50 % within a 99.9 % binomial interval (4,835 to 5,165) and a bound card is never returned; GIVEN a backlog of 4 vacancies of different ages THEN the drawn look is uniform over all 4 (chi-square df 3, critical 16.27) (test `...Ledger.ReplacementRules.UniformDraw`).
- [ ] **AC-2** GIVEN an NPC kill and an informant adoption in the same Round tick, THEN the informant's card is taken only from `NpcLive` cards of existing pawns and the vacated NPC card is refilled by `DrawReplacement`; GIVEN `V` empty THEN `Error_NoVacancy` and `LedgerMismatch`; GIVEN a pawn destroyed by a non-kill THEN its card becomes `NpcVacant` (test `...Ledger.ReplacementRules.Ordering`).
- [ ] **AC-3** GIVEN a `DrawReplacement` ticket not committed before the caller's call stack returns, THEN it rolls back to `NpcVacant`, raises `ensure` in non-Shipping (caught with the expected-error hook) and reports `LedgerMismatch` in every build.

---

## Implementation Notes

- The pending ticket is an RAII object; a ticket committed inside the scope does not roll back.
- `TakeNpcCard` is story 020; here the same-tick test uses a minimal adoption stub that only moves an `NpcLive` card.
- Crowd (crowd-npc-behaviour epic) calls `DrawReplacement` in the tick it spawns the replacement; this story is the allocator side.

**Prototype disposition**: replaces the prototype's per-spawn uniform draw over the active set (the pattern F-6 forbids); nothing promoted.

---

## Out of Scope

- Story 018: commit and vacate; Story 020 and 021: adoption, release; Crowd's scheduling (crowd-npc-behaviour epic)

---

## Files

- `Source/StealthGameplay/Private/Server/CharacterPool/CardLedger.cpp` (`DrawReplacement`)
- `Source/StealthGameplay/Private/Tests/CharacterPool/ReplacementRulesTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead). Tests `...Ledger.ReplacementRules.UniformDraw`, `...Ordering`.*

**Test: uniform replacement draw**
- Given: two vacant cards a and b, and a bound card.
- When: 10,000 first replacements are drawn.
- Then: a and b each occur in [4835, 5165]; a bound card is never returned; with a backlog of 4 the look is uniform (chi-square df 3).
- Edge cases: the oldest vacancy is not favoured.

**Test: ordering in one tick**
- Given: an NPC kill and an informant adoption in the same tick, then an empty V, then a non-kill destruction.
- When: processing the Round tick.
- Then: the informant's card comes only from `NpcLive` cards of existing pawns and the vacated NPC card is refilled by `DrawReplacement`; empty V gives `Error_NoVacancy` and `LedgerMismatch`; a non-kill destruction becomes `NpcVacant`.
- Edge cases: two adoptions and one kill in one tick.

**Test: uncommitted ticket rollback**
- Given: a ticket from `DrawReplacement`.
- When: it goes out of scope uncommitted.
- Then: the card returns to `NpcVacant`, `ensure` fires in non-Shipping (expected-error hook) and `LedgerMismatch` is reported in every build.
- Edge cases: a ticket committed inside the scope does not roll back.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/ReplacementRulesTest.cpp` — automation tests `StealthGame.CharacterPool.Ledger.ReplacementRules.UniformDraw`, `...Ordering` must exist and pass (`/story-done` checks they exist; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-018-ledger-core-commit-vacate-invariants.md`
- Unlocks: `production/epics/character-pool-appearance/story-022-ledger-conservation-under-event-mix.md`
