# Story 013: UAppearanceAllocator facade: opaque input, one-shot, call-order guard

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-002`, `TR-cpa-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-002 = `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates. TR-cpa-004 = Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: `UAppearanceAllocator : UWorldSubsystem` (server nets only, never Blueprint-exposed) is the facade over `FAppearanceDeckCore`: it sorts the participant set before the core sees it, enforces the call-order guard (`Preparing`, after `OnTuningReady`, before any pawn spawn, one deal per round) and takes the misuse branch for any other caller, thread or net mode.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); world subsystem lifecycle only
**Engine Notes**: Each round is its own world after seamless travel, so a world subsystem makes the per-round re-deal structural (CP-14). `ShouldCreateSubsystem` true only for `NM_ListenServer`, `NM_DedicatedServer`, `NM_Standalone`. The phase provider is an injected interface (Round owns the phases; a test fake stands in until Round is coded).

**Why this story**: CP-6 makes team-blindness a property of the signature and the call a one-shot. Read GDD CP-6, AC-CP-03, AC-CP-08a, AC-CP-28 and ADR-0012 §3.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The deal input is an opaque set of `FParticipantId`, sorted by the facade; only GameMode-side server code calls the allocator (ADR-0012)
- Forbidden: Never Blueprint-expose the allocator; never accept team, role, join order or state in the deal input
- Guardrail: Deal at most 0.2 ms once per round

---

## Acceptance Criteria

*From GDD AC-CP-03, AC-CP-08a and AC-CP-28:*

- [ ] **AC-1** GIVEN the allocator signature, THEN a `static_assert` proves `FParticipantId` is opaque (4 bytes, no team, role, join-order or state member); GIVEN two rosters that differ only in team assignment and fixed entropy, WHEN the Round adapter interface (a test fake) builds the call, THEN the `AppearanceDeal` is byte-identical (test `StealthGame.CharacterPool.Deck.TeamBlindMetamorphic`).
- [ ] **AC-2** GIVEN a second `AllocateAppearances` call in one round THEN `Error_AlreadyDealt` and an unchanged ledger (test `...Deck.ValidationLadder.SecondCall`).
- [ ] **AC-3** GIVEN a call outside `Preparing`, before `OnTuningReady`, or after any pawn of the round has spawned THEN `Error_CallOrder` and nothing changes; two `TSet`s with the same elements in different insertion order give identical deals (the facade sorts) (test `...Deck.CallOrderGuard`).

---

## Implementation Notes

*Derived from ADR-0012 §3:*

- Key Interfaces: `TValueOrError<FDealResult, EDealError> AllocateAppearances(const TSet<FParticipantId>& Players, int32 NpcCount)`; `EDealError { AlreadyDealt, CallOrder, DeckTooSmall, DeckInvalid }`.
- The facade owns the `FAppearanceDeckCore`, the `TWeakObjectPtr<AStealthPawn>` to `FPawnKey` map (filled in story 018) and the guard. Misuse (wrong thread, net mode or caller) returns an error and reports to the fault sink in every build.
- `FParticipantId` is Roster's type (participant-state-roster epic); until it lands use a 4-byte opaque struct fixture with the same `static_assert`.
- Log only `DealDigest`, active set, counts, extras, flags (server-only Event Log header, Round's); never the root or the participant-to-card map.

**Prototype disposition**: replaces the deal inside `AStealthGameMode` (`StealthGameMode.cpp`); removal is story 052.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 012: the core deal; Story 018 onward: ledger operations on the facade
- Round's adapter and phase provider (round-match-framework epic)

---

## Files

- `Source/StealthGameplay/Public/Server/CharacterPool/AppearanceAllocator.h`
- `Source/StealthGameplay/Private/Server/CharacterPool/AppearanceAllocator.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/AllocatorFacadeTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead).*

**Test: opaque participant id and team-blind deal**
- Given: `FParticipantId` and two rosters differing only in team, with fixed entropy.
- When: the test-fake Round adapter builds the call.
- Then: `static_assert(sizeof == 4)` and no team, role, join-order or state member; the two `AppearanceDeal` byte arrays are identical.
- Edge cases: the team swap covers every participant.

**Test: second call**
- Given: a completed deal.
- When: `AllocateAppearances` is called again in the same round.
- Then: `Error_AlreadyDealt` and the ledger is byte-identical to before.
- Edge cases: a second call after a failed first call is allowed.

**Test: call-order guard**
- Given: states Lobby, Preparing before `OnTuningReady`, Preparing after, and Active after a pawn spawn.
- When: calling.
- Then: `Error_CallOrder` outside the legal window with no state change; two `TSet`s with the same elements in different insertion order give identical deals.
- Edge cases: the call after `OnTuningReady` but before any spawn succeeds.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/AllocatorFacadeTest.cpp` — automation tests `StealthGame.CharacterPool.Deck.TeamBlindMetamorphic`, `...Deck.ValidationLadder.SecondCall`, `...Deck.CallOrderGuard` must exist and pass (`/story-done` checks they exist; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-012-deal-shuffle-canonical-order-and-streams.md`
- Unlocks: `production/epics/character-pool-appearance/story-018-ledger-core-commit-vacate-invariants.md`, `production/epics/character-pool-appearance/story-052-retire-prototype-pool-and-dealer.md`
