# Story 020: TakeNpcCard: informant adoption of an unobserved crowd pawn

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
**ADR Decision Summary**: `TakeNpcCard(ParticipantId, Pawn)` moves exactly that pawn's `NpcLive` card to `BoundLive` with no random draw (the `Take` stream is reserved, Penalty CR-6); the pawn is picked by Level Socket Contract's `PickRespawnPawn` (the one uniform pick among unobserved `Unbound` pawns inside the team's respawn region) and `TakeNpcCard` runs only after `TakeOverCrowdPawn` returned `Applied`, in the same call stack.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); plain C++ with injected predicates and a `PickRespawnPawn` test fake
**Engine Notes**: No engine API. The eligibility predicates `IsObserved` and `InRespawnRegion` take no `AppearanceId` (a `static_assert` on the injected predicate types).

**Why this story**: A participant's look never survives its death as informant and the adopted look is distributed like the eligible crowd looks (CP-12b, F-6). Read GDD CP-12(b), CP-13(c), AC-CP-12, AC-CP-14, OQ-14 (pawn-level review is Penalty's).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `TakeNpcCard` runs in the same server call stack as its Roster counterpart, only after `TakeOverCrowdPawn` returned `Applied` (ADR-0012, ADR-0010)
- Forbidden: Never consume the reserved `CPA_TAKE` stream tag; never retire-and-spawn a participant pawn (ADR-0012)
- Guardrail: Pick and move O(|pawns|); negligible

---

## Acceptance Criteria

*From GDD AC-CP-12 and AC-CP-14:*

- [ ] **AC-1** GIVEN the fixed ledger snapshot `FLedgerFixtures::InformantSnapshotA` and injected `IsObserved` and `InRespawnRegion` predicates (neither takes an `AppearanceId`; `static_assert`), WHEN a participant's pawn dies over 10,000 trials, THEN its card becomes `NpcVacant`, the informant's pawn is an existing eligible `Unbound` pawn (no actor created or destroyed, `AppearanceId` unchanged), adopted looks fit the multiset of eligible `NpcLive` pawns at bind time (chi-square, df = distinct looks - 1, alpha 0.001), every eligible pawn has probability `1 / |E|` (exact, tolerance 1e-9) and every ineligible pawn 0, and `D` and every `c_a` are unchanged (test `...Ledger.InformantAdoptionMatchesUnobservedLiveMultiset`).
- [ ] **AC-2** GIVEN 40 NPC pawns of which 25 are eligible with a known multiset, WHEN `PickRespawnPawn` (a test fake of LSC's) then `TakeNpcCard(ParticipantId, Pawn)` run 10,000 times for a returning informant, THEN only eligible pawns are chosen, chosen appearances fit the eligible multiset (chi-square; bins merged to keep expected counts at 5 or more), `TakeNpcCard` moves exactly the picked pawn's card, and `D` and every `c_a` are unchanged; GIVEN no eligible NPC THEN no card moves and the request waits (test `...Ledger.InformantAdoptsUnobservedNpc`).

---

## Implementation Notes

- `PickRespawnPawn(Team, RequestSeq, ExcludePawn)` is LSC's (level-socket-contract epic, R5b); here a fake with the same signature and the injected predicates. The retry on no candidate is Penalty's (LSC R5b.5), not this story's.
- The multiset is of pawns' `AppearanceId`; the test must compute the expected distribution from the fixture, never from the allocator.
- The look-level claims only (OQ-14): do not assert anything about pawn-level location effects.

**Prototype disposition**: replaces the prototype's informant respawn (`AStealthGameMode` spawning a pawn at the team spawn), which is a location tell; the production path adopts an existing pawn (SCL story 028 owns the takeover).

---

## Out of Scope

- Story 021: strike-removal ordering and own-card respawn; LSC `PickRespawnPawn` and Penalty's retry (their epics)

---

## Files

- `Source/StealthGameplay/Private/Server/CharacterPool/CardLedger.cpp` (`TakeNpcCard`)
- `Source/StealthGameplay/Private/Tests/CharacterPool/LedgerFixtures.h`
- `Source/StealthGameplay/Private/Tests/CharacterPool/InformantAdoptionTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead).*

**Test: adoption matches the unobserved live multiset**
- Given: `InformantSnapshotA` and injected predicates.
- When: a participant pawn dies over 10,000 trials.
- Then: its card is `NpcVacant`; the informant's pawn is an existing eligible `Unbound` pawn with no actor created or destroyed and `AppearanceId` unchanged; adopted looks fit the eligible multiset (chi-square); every eligible pawn has probability `1/|E|` (1e-9) and every ineligible pawn 0; `D` and `c_a` unchanged.
- Edge cases: `|E| = 1`; no eligible pawn.

**Test: returning informant**
- Given: 40 NPC pawns, 25 eligible, known multiset.
- When: `PickRespawnPawn` then `TakeNpcCard` run 10,000 times.
- Then: only eligible pawns chosen; appearances fit the eligible multiset; `TakeNpcCard` moves exactly the picked pawn's card; `D` and `c_a` unchanged; with no eligible NPC no card moves.
- Edge cases: the multiset has a count-1 look, so merge bins.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/InformantAdoptionTest.cpp` — automation tests `StealthGame.CharacterPool.Ledger.InformantAdoptionMatchesUnobservedLiveMultiset`, `...Ledger.InformantAdoptsUnobservedNpc` must exist and pass (`/story-done` checks they exist; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-018-ledger-core-commit-vacate-invariants.md`, `production/epics/character-pool-appearance/story-019-draw-replacement-uniform-and-ordering.md`
- Unlocks: `production/epics/character-pool-appearance/story-021-release-card-strike-ordering-and-own-card-respawn.md`, `production/epics/character-pool-appearance/story-022-ledger-conservation-under-event-mix.md`
