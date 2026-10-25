# Story 037: Informant takeover never selects a concealed or transitioning pawn

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: [Dep:Penalty] needs the informant takeover selection (npc-kill-penalty-informants) and Concealment IsInBushOrTransition (concealment-bushes).
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

When a player is eliminated and becomes an informant, the takeover picks an existing crowd NPC pawn and hands it to that player. If the selected pawn were concealed (in a bush or mid-transition), the takeover would either re-open a culled pawn for non-allies at an odd moment or reveal that a concealed pawn can be possessed, both of which break pawn parity. The informant takeover selection therefore must never pick a concealed or transitioning pawn, and must return none if no other candidate exists. This is a pure selection rule over candidate sets with a fixed seed, so it is a Logic story; it protects Pillar 1 by keeping the takeover indistinguishable from any other hand-over.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-003` (UE default overrides of R5 (pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator))
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 4.2, possession is invisible to non-owners); ADR-0003: Replication, relevancy and the concealment cull hybrid (section 5.5 and 5.7)
**ADR Decision Summary**: Takeover, rejoin, `PawnRecovery` and hand-over to crowd AI produce no non-owner-visible bunch: the possession `ForceNetUpdate` is dropped and the next fixed-cadence update carries the change. A concealed pawn is culled for every non-ally connection and must not be a takeover candidate; the cull never re-opens a pawn outside the rules of ADR-0003 section 5.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No engine API is exercised by the selection rule itself (a plain-C++ core with fixture candidates and a fixed seed). The possession-side facts (E8 possession paths call `ForceNetUpdate`, E12 a new owner receives current values) are covered by spike S14 and the possession canary (story-019).

**Control Manifest Rules (this layer)**:
- Required: **Every body is an `AStealthPawn`**: one class, one component set, no per-instance replication or net-config difference — source: ADR-0001
- Required: **Mailbox and tie permutations come from `GetServerSubSeed(SeqOrder|TieRank, ServerTickIndex)`** — source: ADR-0002
- Forbidden: **Never use `FMath::Rand`, `FMath::FRand`, `FMath::RandRange`, `FRandomStream`, `std::rand`, `<random>` engines or time-seeded randomness in gameplay or server code** — source: ADR-0006
- Guardrail: **Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade** — source: ADR-0007

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-31), scoped to this story:*

- [ ] **AC-IVP-31** [Unit][Dep:Penalty] `StealthGame.Visibility.TakeoverSkipsConcealed`: GIVEN candidate sets with concealed, transitioning and visible pawns, WHEN the informant takeover selects with a fixed seed, THEN it never picks a concealed or transitioning pawn, and returns none if no other candidate exists. (R9c)

---

## Implementation Notes

*Derived from ADR-0001 section 4.2, ADR-0003 section 5, ADR-0007 section 5 and ADR-0008 sections 1-4:*

- The takeover selection belongs to npc-kill-penalty-informants (Penalty owns the informant takeover); this story owns the Policy-side rule and its test: the selection must filter candidates through a consumer-owned query (Concealment's `IsInBushOrTransition`, supplied by concealment-bushes through a Foundation-declared interface; the Policy never names a feature type, ADR-0007). A candidate that is concealed, in a bush or mid-transition (enter or exit) is excluded before any random choice.
- Randomness comes from the server sub-seed path (`FSecretStream` via `GetServerSubSeed`, ADR-0006), injected as a stream in the test with a fixed seed; no wall clock, no `FRandomStream`, no `FMath::Rand*`. Determinism: the same candidate set and seed give the same result on every run.
- Cases: a set of only concealed pawns returns none; an empty set returns none; a single visible candidate is chosen; a mixed set with concealed, transitioning and visible pawns never returns a concealed or transitioning one across many seeds (parameterise over fixed seeds from a constant file, not inline magic numbers).
- Negative control: the criterion names none; add a harness-only variant of the selection that ignores the concealed flag and assert the suite detects it (the suite passes only if the control is detected). The control lives in test code under `#if WITH_DEV_AUTOMATION_TESTS` only.
- The selection core is a plain-C++ core with no `UWorld` dependency; test with fixture candidates implementing the consumer-owned query interface.
- Prototype: Replaces: nothing. Promotes: nothing. The prototype has no informant takeover selection.
- Cross-epic: needs npc-kill-penalty-informants' informant takeover selection (the code under test or its seam) and concealment-bushes' `IsInBushOrTransition` query.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-019: the possession canary (takeover, rejoin, `PawnRecovery` invisibility to non-owners)
- story-017: spawned actor instigator and owner on every pawn spawn path including takeover
- story-038: kill-confirmation parity for informant kills
- npc-kill-penalty-informants epic: the takeover mechanism and informant flow; concealment-bushes epic: the concealment and transition state

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthFoundation/Public/Server/StealthVisibilityPolicy.h` (the consumer-owned concealed-or-transitioning query interface used by the takeover selection)
- `Source/StealthFoundation/Private/Visibility/StealthTakeoverCandidateFilter.cpp` (candidate filter if the selection core sits in Foundation; otherwise in the Penalty module, recorded by `/dev-story`)
- `Source/StealthFoundation/Private/Tests/Visibility/TakeoverSkipsConcealedTest.cpp` (T1, `FVisibilityTakeoverSkipsConcealedTest`, `StealthGame.Visibility.TakeoverSkipsConcealed`)

*Note: the `StealthFoundation` module folder is created by tuning-data-config Exit A (it does not exist yet).*

---

## QA Test Cases

- **AC-1**: Takeover skips concealed (AC-IVP-31)
  - Given: candidate sets with concealed, transitioning and visible pawns, and a fixed seed.
  - When: the informant takeover selects.
  - Then: it never picks a concealed or transitioning pawn; it returns none if no other candidate exists.
  - Edge cases: a set of only concealed pawns, an empty set, a single visible candidate.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/TakeoverSkipsConcealedTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: npc-kill-penalty-informants epic (the informant takeover selection); concealment-bushes epic (`IsInBushOrTransition`); tuning-data-config Exit A (module skeleton)
- Unlocks: None
