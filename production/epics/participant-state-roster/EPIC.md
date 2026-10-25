# Epic: Participant State & Roster

> **Layer**: Foundation
> **GDD**: design/gdd/participant-state-roster.md
> **Architecture Module**: Roster in `StealthFoundation` (game-instance `ParticipantRecord` store, C-Public and C-TeamView carriers)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories participant-state-roster`
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 2 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

The Roster is the single server-authoritative record of every human participant: a match-stable `ParticipantId`, the rejoin `IdentityKey`, team, lifecycle state (`Active`, `Informant`, `RoundEnded`) and the server-only binding between a participant and the Shared Pawn they drive. It is the only writer of lifecycle state, through `RequestTransition(Participant, NewState, Reason)` with a fixed legal-transition table and result-code precedence, and it answers the question no other system may answer for itself: is this pawn a player, an informant or an NPC (`ResolveVictimKind`, `IsPawnBound`). It owns data only; strike counts, defence cooldowns and concealment live with their feature owners.

The epic delivers the plain-C++ core (no `UWorld` dependency, headless property tests), the thin game-instance facade that survives seamless and hard travel (`BeginMatch`/`EndMatch`, batch-only `ResetAll`/`EndRoundAll`), pawn binding with bind-before-visible spawn, crowd adoption and tombstones, FIFO event dispatch after the outermost call with `EventSeq`, the three carriers (C-Public on `APlayerState` with the engine defaults disabled or constant, C-TeamView as an owner-only `APlayerController` sub-object, C-Server never replicated), the MVP additions from the 2026-10-25 scope decisions (teammate `bReconnecting` and `ReconnectEndCause`, owner tally and `T_due` fields, `RetireAtServerTime`, `OnViewRebound`), and the `PossessionEpoch` owner-only token. Everything presentation reads (including the listen host) comes from C-TeamView and public structs; opponent counts never leave the server before `Ended`. This epic is the lead epic for MVP item 10 (teammate-reconnecting indicator): the Roster fields and event-only clear semantics (ruling D23) are its part.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0002: Server resolution queue, server clock and round-end staging | One total-order queue keyed (ServerTickIndex, Seq) drained in TG_PostPhysics, due-time heap, typed staged writes, single-assignment latch, client server-time estimator. | MEDIUM |
| ADR-0005: Session, seamless travel and match-scoped state stores | Game-instance stores that survive seamless ServerTravel (?Seed=), IdentityKey rejoin, single round acknowledgement, liveness clock, no host migration. | HIGH |
| ADR-0007: Code module layout, composition root and include boundaries | Four runtime modules (StealthFoundation, StealthGameplay, StealthPresentation, StealthGame) plus Editor and StealthTestHarness; compile guard, facade split, consumer-owned interfaces. | MEDIUM |
| ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity | One AStealthPawn class on CMC, factory-only spawn, disconnect never destroys a pawn, stop-and-blend hand-over, possession-invariant net config. | HIGH |
| ADR-0020: Objectives data and team-view sub-structs | Chain data asset of FObjectiveDef rows, court binding from FSiteAssignment, FObjectiveTeamView on C-TeamView. | LOW |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

12 of 12 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-roster-001 | Game-instance store of `ParticipantRecord`s surviving seamless and hard travel; `BeginMatch`/`EndMatch` | ADR-0005 ✅ |
| TR-roster-002 | Single writer of lifecycle state through `RequestTransition` with the legal-transition table and result precedence; batch-only reset/end | ADR-0005 ✅ |
| TR-roster-003 | Game-thread-only mutation; events queued and dispatched FIFO after the outermost call; `EventSeq` +1 per event | ADR-0002 ✅ |
| TR-roster-004 | Pawn binding states with bind-before-visible spawn and crowd-adoption paths; tombstones | ADR-0010 ✅ |
| TR-roster-005 | O(1) allocation-free server queries; on clients return 0/false and report to the fault sink | ADR-0010 ✅ |
| TR-roster-006 | Carriers C-Public (PlayerState allowlist), C-TeamView (owner-only PlayerController), C-Server (never replicated) | ADR-0001 ✅ |
| TR-roster-007 | Engine PlayerState defaults disabled or constant; frozen in-round `PlayerArray`; no PlayerState byte change on a transition | ADR-0001 ✅ |
| TR-roster-008 | Opponent counts never leave the server before `Ended` | ADR-0001 ✅ |
| TR-roster-009 | All presentation, incl. the listen host, reads only C-TeamView and public structs | ADR-0001, ADR-0007 ✅ |
| TR-roster-010 | Disconnect/reclaim keyed by IdentityKey; epoch token and connection generation guard every timer | ADR-0005 ✅ |
| TR-roster-011 | Pawn replication configuration identical for NPC and player and invariant under possession | ADR-0001 ✅ |
| TR-roster-012 | Server-only `OnViewRebound` after `RebindView`; feature team views rebuild from it | ADR-0005, ADR-0020 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** `Source/StealthGame/Public/StealthPlayerState.h` / `Private/StealthPlayerState.cpp` (team and lifecycle state replicated on `APlayerState` to every client, which is the network-level identity leak recorded as known issue 2 in the first-playable report) and the participant/team bookkeeping inside `StealthGameMode.cpp` (`SwitchTeam`, player counts per team). `FStealthRules::ChooseTeamForNewPlayer` and `EvaluateEliminationWinner` in `StealthRules.cpp` are reference logic to be re-expressed in the pure core and re-tested.
**Reference only, never promoted:** `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*` (throwaway S1/S2/S14 spike code for ADR-0001; compiled out of Shipping) is the starting point for the spike evidence this epic is gated on. `StealthTypes.h` enums (team, state) are superseded by the Foundation `View/` wire types.

## Build Order and Gates

- **Depends on (epics)**: tuning-data-config (module skeleton, schema); session-travel registers participants through the Roster API (peer epic built in the same window)
- **Gates and risks**: Conditional gate from ADR-0001: spikes S1, S2, S14 (owner-only resets of `APawn::PlayerState/Controller/Owner` without breaking possession) must report before bind/clear and carrier work is declared done. Roster is built first as a pure core with property tests (largest effort risk: eleven result codes, epoch tokens, tombstones, re-entrant FIFO event flush).
- **Estimate**: about 8 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 4.8 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 61 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/participant-state-roster.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
