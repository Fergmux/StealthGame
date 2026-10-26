# Story 061: URosterStore wraps FRosterCore and BeginMatch/EndMatch run on every session exit path

> **Epic**: Participant State & Roster
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-001`, `TR-roster-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0006 (seed domains 15 `PublicHandle`, 17 `MatchId`), ADR-0007 (facade split, composition root, weak bindings)
**ADR Decision Summary**: ADR-0005 section 2 puts the Roster in a `UGameInstanceSubsystem` facade, `URosterStore`, over the plain-C++ `FRosterCore`; it survives seamless and hard travel, `BeginMatch` runs from `Session.OpenSession` and `EndMatch` runs on **every** session exit path (host quit, return to main menu, `OnSessionEnding`, `UGameInstance::Shutdown`, `FWorldDelegates::OnWorldCleanup` of the last networked world), so a former host keeps nothing. On a client process the store exists but stays empty and every query answers the client value and faults. Decision D43 (2026-10-25) adds the Round-phase input as an injected dependency of the core.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `UGameInstanceSubsystem` lifetime across seamless travel is what spike S3 verifies (not yet reported); this story's automation runs in one process and does not replace S3. `UE_LOGF` is the 5.8 logging form; the core itself logs nothing.

**Context paragraph**: Stories psr-001..014 built `FRosterCore` with seven injected dependencies plus `IRosterRoundPhase` (D43). Nothing in the engine owns a core yet. This story adds the facade that owns exactly one core per game instance, adapts engine objects to the core's interfaces, and wires the match lifetime so no record, binding or `MatchId` can leak into a new session. It was added by the main session under decision D43 ("Roster unowned work: add stories for the URosterStore facade + BeginMatch/EndMatch wiring on every session exit path").

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId` (ADR-0005)
- Required: Roster server queries are O(1) and allocation-free through the pre-sized `PawnBindingTable` reserved at `BeginMatch`; on a client every query returns 0/false (binding queries `NotTargetable(Stale)`) and reports to the fault sink (ADR-0005, Technical Setup 2026-10-25b)
- Forbidden: Never return a real value or the permissive `Npc` from a Roster query on a client (ADR-0005)
- Required: `EndMatch` must run on every session exit path; a store that survives into a new session is a defect (ADR-0005 Implementation Guideline 12)

---

## Acceptance Criteria

- [ ] **AC-PSR-61a** [Integration] `StealthGame.Roster.StoreFacadeAdapters`: GIVEN a game instance with `URosterStore`, WHEN it initialises, THEN it owns one `FRosterCore` whose dependencies are the engine adapters: authority from the game instance's net mode (client process = no authority), game-thread check `IsInGameThread()`, pawn liveness from the pawn object (`IsValid`, destroyed, pending kill), the pawn handle from `FObjectKey` plus a per-pawn spawn generation, the server clock, the Foundation fault sink, `GetServerSubSeed(ESeedDomain::PublicHandle | MatchId, Counter)` for `IRosterRandom` (ADR-0006 domains 15 and 17, golden vectors for both), and Round's phase through `IRosterRoundPhase` (registered weakly by the composition root; no Round registered = phase not Active).
- [ ] **AC-PSR-61b** [Integration] `StealthGame.Roster.StoreMatchLifetime`: GIVEN a populated store, WHEN each session exit path fires (host quit, return to main menu, `OnSessionEnding`, `UGameInstance::Shutdown`, `OnWorldCleanup` of the last networked world), THEN `EndMatch` has run and the store has no record, binding or `MatchId`; AND `Session.OpenSession` runs `BeginMatch(TeamSize)` with the latched `TeamSize`.
- [ ] **AC-PSR-61c** [Integration] `StealthGame.Roster.StoreClientEmpty`: GIVEN a client-process game instance, WHEN any facade query runs, THEN it returns the client answer and reports one fault, and the store holds no record.

---

## Implementation Notes

- The facade owns the core by value (or `TUniquePtr`) and exposes typed server calls that forward to it; it adds no logic of its own beyond adapting types (`TWeakObjectPtr<APawn>` -> `FRosterPawnHandle`).
- The per-pawn spawn generation comes from the pawn factory (ADR-0010 `UStealthPawnFactory`); until that exists, use a generation counter kept by the facade keyed by `FObjectKey` and bumped at each `BeginPlay`.
- Listener bridging: the facade implements `IRosterEventListener` once and re-broadcasts to server-only multicast delegates (`OnParticipantStateChanged`, ...), bound weakly by consumers (ADR-0007 section 4).
- Exit-path wiring may need the Session epic's `USessionService` (session-travel epic); if it is not built yet, wire `UGameInstance::Shutdown` and `OnWorldCleanup` here and leave `OnSessionEnding` to the Session story that raises it (record the hand-off).

---

## Out of Scope

- Story 062 (C-TeamView component and push step)
- Story 041 (`RebindView` and controller-swap re-push), Story 043 (spawn site)
- Session admission and travel (session-travel epic)

---

## Files


- Source/StealthFoundation/Public/Server/Roster/StealthRosterStore.h (new: URosterStore)
- Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp (new: engine adapters, exit paths, tick)
- Source/StealthFoundation/Public/Server/StealthLogFaultSink.h, Private/Faults/StealthLogFaultSink.cpp (new: production fault sink)
- Source/StealthFoundation/Public/Server/Roster/StealthRosterCore.h, StealthRosterDeps.h (moved from Private/Roster so cross-module adapters can name the core vocabulary)
- Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp (ResetAll/IsRoundScopeReset (psr-022 function, early), GetParticipantIds, FindLatestRetiredPawn, bKicked on ConnectionChanged)
- Source/StealthFoundation/Private/Tests/Roster/StealthRosterStoreTest.cpp (new, 3 tests)
- Source/StealthFoundation/Private/Tests/Roster/StealthClientCallsRejectedTest.cpp (ResetAll client row)

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterStoreTest.cpp` — must exist and pass (`qa.level` is full).

**Status**: [x] Created and passing (`StealthGame.Roster.StoreFacadeAdapters`, `StoreMatchLifetime`, `StoreClientEmpty`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md` (core complete through the transition); cross-epic: ADR-0006 `GetServerSubSeed` implementation (session-travel epic seeds story)
- Unlocks: `production/epics/participant-state-roster/story-043-player-pawn-bound-before-visible.md`, `production/epics/participant-state-roster/story-058-mp-travel-persistence.md`, `production/epics/participant-state-roster/story-060-mp-host-loss.md`

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 3/3 passing (61a StoreFacadeAdapters, 61b StoreMatchLifetime, 61c StoreClientEmpty)
**Deviations**: Golden vectors for domains 15/17 are asserted as equality with FStealthServerSeeds::GetServerSubSeed on the RoundTestSeeds S1 secret (the existing SipHash reference-vector test covers the primitive). The client-process authority branch is driven through FRosterStoreInitOverrides.bAuthority; the default (no world) branch is asserted. OnSessionEnding has a NotifySessionEnding entry point; Session (session-travel epic) must call it, and SetTravelInProgress, when it lands (hand-off). Session secret entropy is a FGuid development source until story st-009. FRosterCore::ResetAll implemented here for the Round Start ordering; psr-022 keeps its own AC test.
**Test Evidence**: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterStoreTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: OBSERVED: production/qa/evidence/integration-spine-2026-10-26/01-host-active.png (host dev line: Roster 3, T0 2 / T1 1, epoch 1)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
