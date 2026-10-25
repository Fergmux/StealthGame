# Story 022: Ledger conservation under the full random event mix

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
**ADR Decision Summary**: The ledger is the authority for cards and the Roster for bindings; paired calls run in one server call stack; an end-of-drain non-Shipping sweep checks every live pawn holds exactly one card, every `BoundLive` card's pawn is the Roster's binding and every `NpcLive` card's pawn is `Unbound`; a mismatch is `LedgerMismatch` (risk R6, ledger/Roster drift).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); plain C++ with a Roster fake
**Engine Notes**: No engine API.

**Why this story**: This is the single most valuable test of the ledger: a randomised 500-event sequence over 100 seeds with the invariants checked after every event (GDD AC-CP-10). Its first of three assertions runs in the smoke set. Read GDD F-6, deck invariants and AC-CP-10.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Paired calls run in the same server call stack; the sweep reports `LedgerMismatch` in every build (ADR-0012)
- Forbidden: Never change `D` or any `c_a` after the deal
- Guardrail: Sweep is non-Shipping only

---

## Acceptance Criteria

*From GDD AC-CP-10 (ledger half; the second-write assertion is story 027) and ADR-0012 R6:*

- [ ] **AC-1** GIVEN a dealt deck and `CharacterPoolTestSeeds::Events` (100 seeds), WHEN a random sequence of 500 events (NPC kill, replacement spawn with draw, participant death, informant kill, informant adoption, strike removal, disconnect, grace expiry and release, returning-informant adoption) is applied per seed, THEN after every event `sum(L_a + V_a) = D`, no `c_a` changes, no card is `NpcLive` without a pawn, and whenever no vacancy exists every `L_a = c_a` (test `StealthGame.CharacterPool.Ledger.ConservationUnderKills`).
- [ ] **AC-2** GIVEN the non-Shipping end-of-drain sweep, THEN a seeded violation (a `BoundLive` card whose pawn is not the Roster's binding, and an `NpcLive` card whose pawn is `Bound`) is detected and reports `LedgerMismatch`, and a clean ledger reports nothing.

---

## Implementation Notes

- A stats line logs the event-kind counts so no kind goes unexercised.
- The Roster is a fake with `BindPawn`, `ReleaseToCrowd` and binding queries; the real Roster is in the participant-state-roster epic.
- This test is one of the first three the GDD runs in the smoke set (AC-CP-01, AC-CP-10 and the allowlist test): register it in `tests/smoke/`.

**Prototype disposition**: none.

---

## Out of Scope

- Story 027: the pawn-side write-once guard; Story 023: disconnect and rejoin integration

---

## Files

- `Source/StealthGameplay/Private/Tests/CharacterPool/ConservationUnderKillsTest.cpp`
- `tests/smoke/` (register in the smoke list)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead). Test `StealthGame.CharacterPool.Ledger.ConservationUnderKills`.*

**Test: conservation under an event mix**
- Given: a dealt deck and `CharacterPoolTestSeeds::Events` (100 seeds).
- When: 500 random events are applied per seed from the nine listed kinds.
- Then: after every event `sum(L_a + V_a) = D` and no `c_a` changes; no card is `NpcLive` without a pawn; whenever no vacancy exists every `L_a = c_a`.
- Edge cases: a stats line logs event-kind counts.

**Test: end-of-drain sweep**
- Given: a seeded violation (BoundLive card whose pawn is not the Roster binding; NpcLive card whose pawn is Bound).
- When: the non-Shipping sweep runs.
- Then: each violation reports `LedgerMismatch`.
- Edge cases: a clean ledger reports nothing.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/ConservationUnderKillsTest.cpp` — automation test `StealthGame.CharacterPool.Ledger.ConservationUnderKills` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-019-draw-replacement-uniform-and-ordering.md`, `production/epics/character-pool-appearance/story-020-take-npc-card-informant-adoption.md`, `production/epics/character-pool-appearance/story-021-release-card-strike-ordering-and-own-card-respawn.md`
- Unlocks: `production/epics/character-pool-appearance/story-023-disconnect-rejoin-no-late-join-host-loss.md`
