# Story 018: Card ledger core: CommitCard, VacateCard and the conservation invariants

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-004 = Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: The ledger is a `TArray<FCard>` of length `D` (`{AppearanceId, State, FPawnKey Holder, FParticipantId}`), per-appearance `c_a` fixed at the deal, and a vacant index set sorted by card index; `CommitCard` and `VacateCard` (one call for every victim kind, no kind parameter) are the first two operations; every operation re-asserts `sum(L_a + V_a) = D` in non-Shipping builds and reports ledger faults to the fault sink in every build.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); plain C++
**Engine Notes**: No engine API. `FPawnKey` is an opaque `uint32` the facade issues per pawn; the core never sees a pointer (ADR-0007: no `UObject` in the core).

**Why this story**: States, transitions and invariants I-C1 to I-C4 are the conservation backbone (CP-11). Read GDD CP-11, "States and Transitions" (card states and deck invariants) and ADR-0012 §2 (Ledger table).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `VacateCard` is called for every pawn death or removal with no kind parameter (ADR-0012)
- Forbidden: Never change `D` or any `c_a` after the deal; never log or reflect the participant-to-card map (ADR-0012)
- Guardrail: Ledger operations O(1) or O(|V|); well inside the deal budget

---

## Acceptance Criteria

*From GDD CP-11 and deck invariants I-C1 to I-C4, scoped to this story:*

- [ ] **AC-1** GIVEN a dealt ledger, WHEN `CommitCard(ticket, pawnKey)` runs for every card, THEN each card is `BoundLive` (participant ticket) or `NpcLive`, `sum(c_a) = D` with every `c_a` fixed, and `CheckInvariants` (I-C1 to I-C4) is true.
- [ ] **AC-2** GIVEN `VacateCard(pawnKey)` with no kind parameter (checked by `decltype`), THEN a `NpcLive` or `BoundLive` card becomes `NpcVacant` and the participant mapping is cleared; an unknown pawn, a double vacate and a stale-`DealEpoch` ticket return `UnknownPawn`, `WrongCardState` and `StaleTicket` with the ledger unchanged.
- [ ] **AC-3** GIVEN a non-Shipping build, THEN every operation re-asserts `sum(L_a + V_a) = D`, and an invariant failure is reported to an injected fault sink in every build (the sink call is not under `#if !UE_BUILD_SHIPPING`).

---

## Implementation Notes

*Derived from ADR-0012 §2 (Ledger, Key Interfaces):*

- `FCardTicket { uint16 CardIndex; uint32 DealEpoch; FAppearanceId Appearance; }`; error enum `ELedgerError { NoVacancy, UnknownPawn, WrongCardState, StaleTicket, CallOrder }`; operations return `TOptional<ELedgerError>`.
- The facade (story 013) maps `TWeakObjectPtr<AStealthPawn>` to `FPawnKey`; fill that map here in the facade methods `CommitCard(const FCardTicket&, AStealthPawn*)` and `VacateCard(const AStealthPawn*)`.
- Production callers: Round commits at spawn (`SpawnSharedPawn` then `CommitCard`); the death-write handler vacates in the kill slot while the body is still present (ADR-0018 §6.4).

**Prototype disposition**: none (the prototype had no ledger).

---

## Out of Scope

- Story 019: `DrawReplacement`; Story 020: `TakeNpcCard`; Story 021: `ReleaseCard`, `RecoverOwnCard`; Story 022: the randomised event-mix test

---

## Files

- `Source/StealthGameplay/Public/Server/CharacterPool/AppearanceDeckCore.h` (ledger types)
- `Source/StealthGameplay/Private/Server/CharacterPool/CardLedger.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/LedgerCommitVacateTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead). Test `StealthGame.CharacterPool.Ledger.CommitVacateInvariants`.*

**Test: commit invariants**
- Given: a dealt ledger.
- When: `CommitCard(ticket, pawnKey)` runs for every card.
- Then: each card is BoundLive or NpcLive, `sum(c_a) = D`, every `c_a` unchanged, `CheckInvariants` true.
- Edge cases: a card committed twice returns `WrongCardState`.

**Test: vacate**
- Given: BoundLive and NpcLive cards.
- When: `VacateCard(pawnKey)` runs, then with an unknown pawn, a repeated pawn and a stale-`DealEpoch` ticket.
- Then: cards become NpcVacant, mapping cleared; the error cases return `UnknownPawn`, `WrongCardState`, `StaleTicket` with the ledger byte-identical; `decltype` shows one parameter.
- Edge cases: vacate during the same tick as a commit.

**Test: assertions and fault sink**
- Given: a Development build and an injected fault sink.
- When: a corrupted ledger (sum off by one) is operated on.
- Then: the operation re-asserts the invariant and the sink receives the report; the sink call is not inside a non-Shipping guard (review item).
- Edge cases: none.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/LedgerCommitVacateTest.cpp` — automation test `StealthGame.CharacterPool.Ledger.CommitVacateInvariants` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-013-allocator-facade-signature-and-call-order.md`
- Unlocks: `production/epics/character-pool-appearance/story-019-draw-replacement-uniform-and-ordering.md`, `production/epics/character-pool-appearance/story-020-take-npc-card-informant-adoption.md`, `production/epics/character-pool-appearance/story-021-release-card-strike-ordering-and-own-card-respawn.md`
