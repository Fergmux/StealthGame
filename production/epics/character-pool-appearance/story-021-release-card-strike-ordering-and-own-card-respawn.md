# Story 021: ReleaseCard (strike removal ordering) and RecoverOwnCard (own-card respawn)

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
**ADR Decision Summary**: `ReleaseCard(pawn)` moves `BoundLive` to `NpcLive` and is paired in one call stack with the Roster's `ReleaseToCrowd` (strike exile at the exile, disconnect grace expiry); the released body is passed as `ExcludePawn` to the later adoption; `RecoverOwnCard(Participant, NewPawn)` re-points a `BoundLive` card to Round's own-card respawn pawn with no state change.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); plain C++
**Engine Notes**: No engine API.

**Why this story**: Strike removal must never let a struck-out participant re-adopt its own released pawn, and an own-card respawn (Round R17.9) must change no card (CP-11, CP-12b ordering). Read GDD CP-11, CP-12b "Ordering for strike removal", AC-CP-12b, Edge Cases (strike, own-card respawn).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `TakeNpcCard`, `ReleaseCard` and `RecoverOwnCard` run in the same server call stack as their Roster counterpart (ADR-0012)
- Forbidden: Never destroy-and-respawn on disconnect or takeover (ADR-0010); never consume the `Take` tag
- Guardrail: negligible

---

## Acceptance Criteria

*From GDD AC-CP-12b and CP-11 (own-card respawn):*

- [ ] **AC-1** GIVEN a struck-out participant whose pawn survives, THEN `ReleaseToCrowd` runs at the exile (card `BoundLive` to `NpcLive`), the later adoption passes the released pawn as `ExcludePawn`, the adopted pawn is never the released pawn (0 of 10,000 trials), `TakeNpcCard` leaves the stream counter unchanged (a test accessor reads it) and the released pawn keeps its `AppearanceId`; GIVEN two adoptions in one tick THEN they never return the same pawn (test `...Ledger.StrikeRemovalOrdering`).
- [ ] **AC-2** GIVEN Round's own-card respawn (R17.9), WHEN `RecoverOwnCard(Participant, NewPawn)` runs, THEN the card stays `BoundLive` re-pointed to the new pawn, no other card changes state and `D` and every `c_a` are unchanged.

---

## Implementation Notes

- `NewPawn` equal to the old pawn is rejected. A request whose only eligible NPC is the released pawn moves no card.
- The new pawn is spawned with the participant's current `AppearanceId` through Crowd's replacement path (`RequestEmergence(Kind = PawnRecovery)`); this story is the ledger side only.
- Facade methods: `ReleaseCard(const AStealthPawn*)`, `RecoverOwnCard(FParticipantId, AStealthPawn*)`.

**Prototype disposition**: none.

---

## Out of Scope

- Story 020: adoption itself; Round's R17.9 flow and Crowd's emergence (their epics)

---

## Files

- `Source/StealthGameplay/Private/Server/CharacterPool/CardLedger.cpp` (`ReleaseCard`, `RecoverOwnCard`)
- `Source/StealthGameplay/Private/Tests/CharacterPool/StrikeRemovalOrderingTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead).*

**Test: strike removal ordering**
- Given: a struck-out participant whose pawn survives.
- When: `ReleaseToCrowd` runs at the exile, then adoption with `ExcludePawn` set, over 10,000 trials.
- Then: the card goes `BoundLive` to `NpcLive`, the released pawn keeps its `AppearanceId`, the adopted pawn is never the released pawn (0 of 10,000), `TakeNpcCard` leaves the stream counter unchanged, and two adoptions in one tick never return the same pawn.
- Edge cases: the released pawn is the only eligible NPC (no card moves).

**Test: own-card respawn**
- Given: a `BoundLive` card.
- When: `RecoverOwnCard(Participant, NewPawn)` runs.
- Then: the card stays `BoundLive` re-pointed to the new pawn; no other card changes state; `D` and every `c_a` unchanged.
- Edge cases: `NewPawn` equals the old pawn (rejected).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/StrikeRemovalOrderingTest.cpp` — automation test `StealthGame.CharacterPool.Ledger.StrikeRemovalOrdering` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-018-ledger-core-commit-vacate-invariants.md`, `production/epics/character-pool-appearance/story-020-take-npc-card-informant-adoption.md`
- Unlocks: `production/epics/character-pool-appearance/story-022-ledger-conservation-under-event-mix.md`, `production/epics/character-pool-appearance/story-023-disconnect-rejoin-no-late-join-host-loss.md`
