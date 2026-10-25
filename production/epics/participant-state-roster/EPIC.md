# Epic: Participant State & Roster

> **Layer**: Foundation
> **GDD**: design/gdd/participant-state-roster.md
> **Architecture Module**: Roster in `StealthFoundation` (game-instance `ParticipantRecord` store, C-Public and C-TeamView carriers)
> **Status**: Ready
> **Stories**: 60 stories (54 Ready, 6 Blocked), see Stories below
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

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [Participant record fields equal the R3 whitelist exactly](story-001-record-schema-whitelist.md) (AC-PSR-01) | Logic | Ready | ADR-0005 |
| 002 | [BeginMatch and registration assign ids, handles and reject a full roster](story-002-begin-match-and-register.md) (AC-PSR-05) | Logic | Ready | ADR-0005 |
| 003 | [Mutation from a worker thread is rejected and faulted](story-003-off-game-thread-mutation-rejected.md) (AC-PSR-03) | Logic | Ready | ADR-0002 |
| 004 | [Every roster call on a client is rejected or answers not-targetable and faults](story-004-client-calls-rejected.md) (AC-PSR-02) | Logic | Ready | ADR-0010 |
| 005 | [A locked roster rejects registration, removal and team changes but reclaims known keys](story-005-locked-roster-is-frozen.md) (AC-PSR-06) | Logic | Ready | ADR-0005 |
| 006 | [A batch swap of two players across full teams applies atomically](story-006-assign-teams-atomic-swap.md) (AC-PSR-08) | Logic | Ready | ADR-0005 |
| 007 | [An invalid team batch changes nothing and fires no event](story-007-assign-teams-all-or-nothing.md) (AC-PSR-09) | Logic | Ready | ADR-0005 |
| 008 | [Registration, team, removal and reclaim each fire exactly one sequenced event](story-008-registration-and-team-events.md) (AC-PSR-55) | Logic | Ready | ADR-0002 |
| 009 | [Session connection states project onto Connection fields only](story-009-connection-projection.md) (AC-PSR-46) | Logic | Ready | ADR-0005 |
| 010 | [BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant](story-010-bind-pawn-guards.md) (AC-PSR-22) | Logic | Ready | ADR-0010 |
| 011 | [Tombstones respect generations and ReleaseToCrowd needs a disconnected participant](story-011-tombstone-generation-and-release.md) (AC-PSR-24) | Logic | Ready | ADR-0010 |
| 012 | [Clearing a live binding during Active faults, but level-teardown clears are silent](story-012-clear-binding-while-live.md) (AC-PSR-25) | Logic | Ready | ADR-0010 |
| 013 | [ResolveVictimKind answers the full pawn-state matrix and never yields Npc for NotTargetable](story-013-resolve-victim-kind-matrix.md) (AC-PSR-26) | Logic | Ready | ADR-0010 |
| 014 | [Active to Informant applies for each entering reason with the correct pawn disposition](story-014-transition-active-to-informant.md) (AC-PSR-14) | Logic | Ready | ADR-0005 |
| 015 | [A Disconnect transition is rejected while the participant is connected](story-015-transition-disconnect-requires-disconnected.md) (AC-PSR-15) | Logic | Ready | ADR-0005 |
| 016 | [The first entering reason is kept and later reasons are no-ops](story-016-transition-first-reason-wins.md) (AC-PSR-16) | Logic | Ready | ADR-0005 |
| 017 | [No revival and no round-ended-to-informant transitions are possible](story-017-transition-no-revival-no-late-events.md) (AC-PSR-17) | Logic | Ready | ADR-0005 |
| 018 | [Unregistered and retired ids are rejected by every id-taking API](story-018-transition-unknown-id-rejected.md) (AC-PSR-18) | Logic | Ready | ADR-0005 |
| 019 | [RoundEnd and Reset reasons are rejected on single transitions](story-019-batch-only-reasons-rejected-singly.md) (AC-PSR-13) | Logic | Ready | ADR-0005 |
| 020 | [Killing an informant retires its binding without a state event](story-020-informant-kill-is-not-a-transition.md) (AC-PSR-21) | Logic | Ready | ADR-0010 |
| 021 | [RetireAtServerTime reaches teammate entries for each retiring reason](story-021-retire-at-server-time-source.md) (AC-PSR-57) | Logic | Ready | ADR-0005 |
| 022 | [ResetAll resets every non-dormant record with exactly one roster-reset event](story-022-reset-all-batch.md) (AC-PSR-11) | Logic | Ready | ADR-0005 |
| 023 | [EndRoundAll ends all records once and is idempotent](story-023-end-round-all-batch-idempotent.md) (AC-PSR-12) | Logic | Ready | ADR-0005 |
| 024 | [Calls carrying an old round epoch or match id are rejected](story-024-stale-epoch-rejected.md) (AC-PSR-19) | Logic | Ready | ADR-0005 |
| 025 | [Undersized rosters count correctly and lock and reset preconditions hold](story-025-undersized-rosters-and-lock.md) (AC-PSR-10) | Logic | Ready | ADR-0005 |
| 026 | [Late binds and transitions after a round boundary are rejected](story-026-round-boundary-late-events.md) (AC-PSR-36) | Logic | Ready | ADR-0005 |
| 027 | [The transition matrix equals the checked-in CSV fixture for every combination](story-027-transition-exhaustive-matrix.md) (AC-PSR-20) | Logic | Ready | ADR-0005 |
| 028 | [Re-entrant listener calls see live state and events dispatch FIFO with strict EventSeq](story-028-reentrant-listener-ordering.md) (AC-PSR-34) | Logic | Ready | ADR-0002 |
| 029 | [Reclaim restores the same record for active and informant participants](story-029-reclaim-core.md) (AC-PSR-47) | Logic | Ready | ADR-0005 |
| 030 | [An informant's released pawn becomes an NPC and a reclaim adopts a different pawn](story-030-informant-disconnect-release-reclaim.md) (AC-PSR-48) | Logic | Ready | ADR-0010 |
| 031 | [GetLegalityClass answers per pawn state and BindPawn adopts an unbound pawn](story-031-legality-class-and-adoption.md) (AC-PSR-51) | Logic | Ready | ADR-0010 |
| 032 | [GetRelation answers the viewer-subject matrix symmetrically](story-032-get-relation-matrix.md) (AC-PSR-27) | Logic | Ready | ADR-0010 |
| 033 | [Hot-path queries allocate nothing and probe at most twice per call](story-033-query-hot-path-budget.md) (AC-PSR-28) | Logic | Ready | ADR-0010 |
| 034 | [A scripted 200-step sequence keeps F1-F3 and I1-I3 equal to a brute-force recount](story-034-counts-and-conservation.md) (AC-PSR-29) | Logic | Ready | ADR-0005 |
| 035 | [The elimination trigger fires only for a strike-out win and never for disconnect or empty sides](story-035-elimination-trigger-core.md) (AC-PSR-50) | Logic | Ready | ADR-0005 |
| 036 | [The dev-only transition log keeps the last N entries and is absent from Shipping](story-036-transition-log-ring.md) (AC-PSR-56) | Logic | Ready | ADR-0002 |
| 037 | [A maximal C-TeamView net-serializes in at most 96 bytes](story-037-team-view-serialized-size.md) (AC-PSR-33) | Logic | Ready | ADR-0001 |
| 038 | [SetOwnerTally and SetOwnerRespawnDue feed only the owner view with the full precedence](story-038-owner-view-feed.md) (AC-PSR-54) | Logic | Ready | ADR-0001 |
| 039 | [No Server/Client/Multicast RPC parameter contains a participant id or identity key](story-039-no-rpc-carries-participant-id.md) (AC-PSR-04) | Integration | Ready | ADR-0001 |
| 040 | [PlayerState replicates only the allowlist and engine defaults are disabled or constant](story-040-player-state-replicated-allowlist.md) (AC-PSR-44) | Integration | Ready | ADR-0001 |
| 041 | [A replaced controller or reclaim re-pushes C-TeamView within one server frame](story-041-team-view-repush-on-controller-swap.md) (AC-PSR-49) | Integration | Ready | ADR-0005 |
| 042 | [PawnRef resolves in either arrival order of pawn and team-view entry](story-042-team-view-pawn-ref-ordering.md) (AC-PSR-40d) | Integration | Ready | ADR-0001 |
| 043 | [Player pawns are bound before BeginPlay and a failed bind destroys the pawn before FinishSpawning](story-043-player-pawn-bound-before-visible.md) (AC-PSR-23) | Integration | Ready | ADR-0010 |
| 044 | [The public player list is identical before and after eliminations and disconnects](story-044-pie-public-list-reveals-nothing.md) (AC-PSR-37) | Integration | Ready | ADR-0001 |
| 045 | [Opponents receive nothing but allowlisted movement updates after a kill of a player, NPC or informant](story-045-pie-elimination-leak-diff.md) (AC-PSR-38) | Integration | Ready | ADR-0001 |
| 046 | [Non-owned pawns carry null PlayerState, Controller and Owner for players and NPCs alike](story-046-pie-pawn-pointer-dump.md) (AC-PSR-39) | Integration | Ready | ADR-0001 |
| 047 | [No opponent count reaches a client before the round ends](story-047-pie-opponent-counts-never-replicated.md) (AC-PSR-41) | Integration | Ready | ADR-0001 |
| 048 | [The listen host's presentation model equals a client's and presentation code never includes the roster query header](story-048-pie-host-view-equals-client-view.md) (AC-PSR-42) | Integration | Ready | ADR-0007 |
| 049 | [Player and NPC pawns replicate identically and bind, transition or possession changes no setting](story-049-pie-pawn-replication-parity.md) (AC-PSR-45) | Integration | Blocked | ADR-0001 |
| 050 | [Killing an informant produces the same attacker-side RPCs, HUD and cues as killing an NPC](story-050-pie-informant-kill-looks-like-npc-kill.md) (AC-PSR-52) | Integration | Ready | ADR-0001 |
| 051 | [A teammate's reconnecting flag and end cause follow the event-only clear rules in all six cases](story-051-pie-teammate-reconnecting-flag.md) (AC-PSR-53) | Integration | Ready | ADR-0001 |
| 052 | [Round declares an elimination win exactly once from the transition payload](story-052-elimination-trigger-from-payload.md) (AC-PSR-30) | Integration | Ready | ADR-0005 |
| 053 | [Two last-active attacks in one tick latch exactly one elimination win](story-053-no-draw-same-tick.md) (AC-PSR-31) | Integration | Ready | ADR-0002 |
| 054 | [Disconnect grace expires on the injected clock and stale-connection timers are rejected](story-054-disconnect-grace-timing.md) (AC-PSR-32) | Integration | Ready | ADR-0005 |
| 055 | [Same-tick assassinations and strike-outs resolve in queue order](story-055-simultaneous-resolution.md) (AC-PSR-35) | Integration | Ready | ADR-0002 |
| 056 | [A disconnect hand-over never decelerates faster than the crowd stop decel and never destroys the pawn](story-056-disconnect-takeover-blend.md) (AC-PSR-43) | Integration | Blocked | ADR-0010 |
| 057 | [Two client processes on one machine with distinct identity keys get distinct records](story-057-mp-same-machine-clients-distinct.md) (AC-PSR-07) | Integration | Blocked | ADR-0005 |
| 058 | [Roster identity and team state survive seamless and hard travel unchanged](story-058-mp-travel-persistence.md) (AC-PSR-40a) | Integration | Blocked | ADR-0005 |
| 059 | [A client rejoining within grace reclaims its record, pawn and informant status](story-059-mp-rejoin-within-grace.md) (AC-PSR-40b) | Integration | Blocked | ADR-0005 |
| 060 | [Host loss empties every client's roster store](story-060-mp-host-loss.md) (AC-PSR-40c) | Integration | Blocked | ADR-0005 |

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/participant-state-roster.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
