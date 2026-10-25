# Epic: Round & Match Framework

> **Layer**: Core
> **GDD**: design/gdd/round-match-framework.md
> **Architecture Module**: Round & Match Rules in `StealthGameplay/Core`, plus the Foundation-layer Resolution Queue & Server Clock and Event Log & fault sink whose contracts it owns, in `StealthFoundation`
> **Status**: Ready
> **Stories**: 92 written 2026-10-25 (see the Stories table below)
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25f
> **Build Order**: 7 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Round & Match Framework turns ten connected people into a round and a round into exactly one result. This epic spans two architectural layers by design (architecture.md Layer rationale, item 3). **Exit A, Foundation:** the Resolution Queue & Server Clock (per-round queue keyed `(ServerTickIndex, Seq)`, per-source mailboxes with permuted intake, the due-time heap, the custom `FTickFunction` drain in `TG_PostPhysics`, typed staged writes applied by registered `IStagedWriteApplier`s, deferred teardown, single-assignment latch storage, the client RTT-compensated `EstimatedServerNow()`) and the Event Log & fault sink (server-memory ring with release classes, protected kill timeline and `TruncatedCount`, telemetry section). **Exit B, Core:** the Round rules (the five-phase machine `Lobby`/`Preparing`/`Active`/`Ended`/`Reset` in a game-instance store, team size and balance rules, the Preparing readiness sequence with spawn gate and watchdog, one shuffled spawn list placed in a single frame, objective and elimination wins with no draw and no clock, the two-step round end and `ITerminalDecision`, the disconnect policy of blend hand-over, guarded grace timer and forfeit/void, host commands with a net-mode check, `FRoundPublicState` with identical bytes for every viewer).

This is the lead epic for MVP item 9 (text-only end-of-round summary, `FRoundResult.TeamSummary` computed at the round-end step from the Event Log; HUD carries the display half), and it carries the Round half of the playtest instrumentation (ADR-0023). Players feel what Round guarantees: a round that begins and ends for all ten at the same instant with one winner and a stated reason, never a clock or a head-count of rivals.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0002: Server resolution queue, server clock and round-end staging | One total-order queue keyed (ServerTickIndex, Seq) drained in TG_PostPhysics, due-time heap, typed staged writes, single-assignment latch, client server-time estimator. | MEDIUM |
| ADR-0003: Replication, relevancy and the concealment cull hybrid | Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus a client-local hide, allowlist audit of every replicated property. | HIGH |
| ADR-0005: Session, seamless travel and match-scoped state stores | Game-instance stores that survive seamless ServerTravel (?Seed=), IdentityKey rejoin, single round acknowledgement, liveness clock, no host migration. | HIGH |
| ADR-0006: Seeds and deterministic/secret random streams | Public sub-seeds for layout vs SipHash-2-4 keyed server-secret streams; integer PCG32 streams so layout is bit-identical across machines. | LOW |
| ADR-0007: Code module layout, composition root and include boundaries | Four runtime modules (StealthFoundation, StealthGameplay, StealthPresentation, StealthGame) plus Editor and StealthTestHarness; compile guard, facade split, consumer-owned interfaces. | MEDIUM |
| ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity | One AStealthPawn class on CMC, factory-only spawn, disconnect never destroys a pawn, stop-and-blend hand-over, possession-invariant net config. | HIGH |
| ADR-0014: Layout service, seeded generation and runtime navmesh | Layout-service world subsystem with provider registry, integer Layout Manifest and LayoutHash, server navmesh build and validation, reseed and graybox fallback. | MEDIUM |
| ADR-0023: Playtest telemetry hooks | Telemetry section of the Event Log store, probes and detectors at Feature call sites, server-only statistics. | LOW |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

13 of 13 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-round-001 | Five-phase machine in a game-instance store surviving travel | ADR-0005 ✅ |
| TR-round-002 | One queue per round, `Key = (ServerTickIndex, Seq)`, per-source mailboxes, permuted intake | ADR-0002 ✅ |
| TR-round-003 | Drain once per frame in `TG_PostPhysics`; due-time heap; resolvers per item type; inline follow-ons; Roster events flushed before the next item | ADR-0002 ✅ |
| TR-round-004 | Single-assignment latch and the two-step round end (latch step; round-end step at the deciding strike's death write) | ADR-0002 ✅ |
| TR-round-005 | Objective and elimination wins; no draw, no clock | ADR-0002 ✅ |
| TR-round-006 | Preparing readiness sequence, spawn gate and watchdog | ADR-0005, ADR-0014 ✅ |
| TR-round-007 | Disconnect policy: blend hand-over, guarded grace timer, forfeit/void | ADR-0005, ADR-0010 ✅ |
| TR-round-008 | Server-only Event Log ring with release classes; never through logs or traces | ADR-0007 ✅ |
| TR-round-009 | `FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal | ADR-0003 ✅ |
| TR-round-010 | Host commands on the game instance with a net-mode check | ADR-0005 ✅ |
| TR-round-011 | Single shuffled spawn list in one frame, server-secret order | ADR-0006 ✅ |
| TR-round-012 | Playtest instrumentation (brace timing, run share, final camping, team-size bucket) in the Event Log | ADR-0023 ✅ |
| TR-round-013 | Event Log ring bound; kill timeline protected from eviction; `TruncatedCount`; telemetry section | ADR-0023, ADR-0007 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** the round logic in `StealthGameMode.cpp` (1,019 lines) and `StealthGameMode.h`: `StartRound`, `AbortRound`, `EndRound(WinningTeam, Reason)`, `CheckEliminationVictory`, `TryScheduleRoundStart`, `CleanupRoundActors`, the auto-restart, win reasons (`EStealthWinReason`), `OnObjectiveCompleted` and the dev start/reset console commands (`StealthStartRound`, `StealthEndRound`, `StealthSwitchTeam`), plus the replicated round fields in `StealthGameState.*`. Prototype resolution is first-come order with no total-order key, which the GDD's simultaneous-event rules replace.
**Promoted as reference tests:** `FStealthRules::EvaluateEliminationWinner` and the elimination tests in `StealthRulesTests.cpp` are re-expressed against the new queue-driven latch. The dev console commands survive as host commands behind the net-mode check (TR-round-010).

## Build Order and Gates

- **Depends on (epics)**: participant-state-roster, session-travel, tuning-data-config, level-socket-contract, character-pool-appearance, input-interaction-framework (resolver registration), shared-character-locomotion; mutual peer with crowd-npc-behaviour through `ICrowdControl`
- **Gates and risks**: ADR-0002, ADR-0005, ADR-0006, ADR-0007, ADR-0014 and ADR-0023 are unconditional. Depends transitively on the ADR-0010 spikes (possession parity). The architecture splits Round: the queue and clock contract is Foundation and is built before its consumers (IIF, Assassination, Objectives, Penalty, Crowd); phases, win latch policy, team rules, disconnect policy and host commands are Core. The producer review judged this epic overscoped (86 ACs, about 14 d, spanning two layers) and recommended extracting a Foundation epic. Decision: keep one epic as the user mandated, and define **two exit points**: **Exit A (Foundation)**, Resolution Queue & Server Clock plus Event Log and fault sink, built first, which unblocks every queue consumer; **Exit B (Core)**, Round rules, round-end staging, disconnect policy and host commands. Create stories queue-first.
- **Estimate**: about 14 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 8.4 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 86 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Stories

92 stories at `story_granularity: fine` (one GDD acceptance criterion each; GDD AC 2 `PIELifecycleReal` is [DEFERRED] to the Objectives and Assassination epics and has no story here). Exit A, Foundation (Resolution Queue and Server Clock, Event Log and fault sink, test seams): 001-017, built first. Exit B, Core (Round rules): 018-092. Blocked stories 087-092 wait on spikes named in each story (ADR-0010 S8/S14/H1/P1 and the OQ1(d)/OQ11(a) spikes; the OQ11(b) client clock-sync spike; the ADR-0002 round-end and ADR-0022 R1-R3 spikes). Stories 011 and 054 carry the data half of MVP item 9 (text-only end-of-round summary); the HUD epic carries the display half.

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [Tests inject a fake IClock supplying server time and ServerTickIndex](story-001-test-seam-fake-clock.md) | Logic | Ready | ADR-0002 |
| 002 | [Tests inject a fixed ServerSecret and therefore fixed sub-seeds](story-002-test-seam-fixed-server-secret.md) | Logic | Ready | ADR-0006 |
| 003 | [Tests inject fake Session, Roster, TDC, Character Pool, Level Socket and Crowd gateways](story-003-test-seam-fake-gateways.md) | Logic | Ready | ADR-0007 |
| 004 | [Tests register stub resolvers per queue item type](story-004-test-seam-stub-resolvers.md) | Logic | Ready | ADR-0002 |
| 005 | [Fixture constants live in test constant files, not inline](story-005-test-fixture-constants.md) | Logic | Ready | ADR-0007 |
| 006 | [Intake permutation is fair per source, deterministic per (secret, tick, mailbox set), keys strictly increasing](story-006-intake-fairness.md) | Logic | Ready | ADR-0002 |
| 007 | [Per-source order, DueEpsilonMs stamping, TieRank ties, stale-connection and per-drain cap rules hold](story-007-intake-ordering-rules.md) | Logic | Ready | ADR-0002 |
| 008 | [Resolvers return Voided/in-slot/deferred outcomes with the specified keys and Seq consumption](story-008-resolver-contract.md) | Logic | Ready | ADR-0002 |
| 009 | [Follow-on transitions and subscribers run depth-first in the item's slot; cascade depth 9 logs CascadeOverflow](story-009-follow-ons-inline.md) | Logic | Ready | ADR-0002 |
| 010 | [An item stamped with the previous RoundEpoch is Rejected_StaleEpoch when drained](story-010-stale-epoch-item.md) | Logic | Ready | ADR-0002 |
| 011 | [Event Log header and WinLatched hold the specified fields for every terminal outcome and are unreadable before Ended](story-011-event-log-content.md) | Logic | Ready | ADR-0007 |
| 012 | [The 4096-entry ring evicts oldest ordinary entries first, protects header and WinLatched, and counts TruncatedCount](story-012-log-truncation.md) | Logic | Ready | ADR-0023 |
| 013 | [Mean entry size is reported against F8 and the ring never exceeds 16,384 entries](story-013-log-size-bound.md) | Logic | Ready | ADR-0023 |
| 014 | [No log device, ClientMessage or trace channel carries a sentinel; dumplog is refused before Ended](story-014-log-never-leaks.md) | Logic | Ready | ADR-0007 |
| 015 | [At most one Voided entry per second per source and the kill timeline stays intact](story-015-voided-rate-limit.md) | Logic | Ready | ADR-0007 |
| 016 | [PacingRatio, ObjectiveEndShare, AbortedActiveShare and stall/abort flags compute per F7 from fixture logs](story-016-pacing-metrics.md) | Logic | Ready | ADR-0023 |
| 017 | [A second final Complete after the latch returns Rejected_Latched and is logged TendCompleted with Latched = true](story-017-latched-final-complete-logged.md) | Logic | Ready | ADR-0002 |
| 018 | [A new key joins the smaller team; ties go to team RoundsPlayedCount parity](story-018-team-assignment.md) | Logic | Ready | ADR-0005 |
| 019 | [A reclaimed Dormant key keeps its team and triggers no AssignTeams call](story-019-team-reclaim-keeps-team.md) | Logic | Ready | ADR-0005 |
| 020 | [SwapTeam is refused only when the target team is full; pair exchange is atomic; swaps outside Lobby return WrongPhase](story-020-swap-team-rules.md) | Logic | Ready | ADR-0005 |
| 021 | [A preset with a different TeamSize calls SetTeamSize once; refusals keep the previous preset; locked teams never call it](story-021-team-size-injection.md) | Logic | Ready | ADR-0005 |
| 022 | [Start refuses each invalid team layout with the specified code in check order and stays in Lobby](story-022-start-validity.md) | Logic | Ready | ADR-0005 |
| 023 | [Start refuses with ParticipantNotConnected naming exactly the Dormant, Unresponsive or Connecting set](story-023-start-refuses-not-connected.md) | Logic | Ready | ADR-0005 |
| 024 | [An accepted Start calls Session and Roster in the specified order and enters Preparing only after travel acceptance](story-024-start-ordering.md) | Logic | Ready | ADR-0005 |
| 025 | [A Session refusal at RequestRoundTravel rolls admission, teams and phase back and logs StartRolledBack](story-025-start-rollback.md) | Logic | Ready | ADR-0005 |
| 026 | [A non-host client cannot run any of the eight stealth.round commands by console or ServerExecRPC](story-026-host-only-commands.md) | Integration | Ready | ADR-0005 |
| 027 | [stealth.round.reset/abort and the other host commands behave per phase and exist only on the game instance](story-027-reset-command-matrix.md) | Logic | Ready | ADR-0005 |
| 028 | [Each phase pushes the specified admission mode and every illegal transition is refused](story-028-phase-admission-and-illegal-transitions.md) | Logic | Ready | ADR-0005 |
| 029 | [FRoundPublicState replicates with Unknown = 0, survives seamless travel and LastResult survives into Lobby](story-029-public-state-replication.md) | Integration | Ready | ADR-0003 |
| 030 | [MatchScore, RoundsPlayedCount, host id, teams and archived slices persist across travel; RoundEpoch increments](story-030-match-scope-survives-travel.md) | Integration | Ready | ADR-0005 |
| 031 | [AllocateAppearances is called 0 times with tuning withheld and exactly once after OnTuningReady](story-031-allocate-once.md) | Logic | Ready | ADR-0005 |
| 032 | [Layout, a client acknowledgement or world-ready each alone hold Preparing; releasing reaches Active](story-032-preparing-barrier.md) | Logic | Ready | ADR-0005 |
| 033 | [An acknowledgement with an old nonce or replaced controller is dropped and does not satisfy the barrier](story-033-stale-ack-dropped.md) | Logic | Ready | ADR-0005 |
| 034 | [Each of the 13 abort codes returns to Lobby with no result, no pawns, PreparingFailed archived and nothing released](story-034-preparing-aborts.md) | Logic | Ready | ADR-0005 |
| 035 | [F6 T_needed is computed at freeze with the specified values and warning boundaries](story-035-watchdog-bound.md) | Logic | Ready | ADR-0005 |
| 036 | [Per-slot spawn rank is uniform over 1..D across 1,000 fixed-secret round starts](story-036-spawn-order-uniform.md) | Logic | Ready | ADR-0006 |
| 037 | [Spawn order sigma is identical when player flags are flipped and differs with a different secret](story-037-spawn-order-ignores-playerness.md) | Logic | Ready | ADR-0006 |
| 038 | [The placement share invariant passes at 5 players/5 NPCs and aborts PlacementInvariant at 5/4; GlobalShare matches F5](story-038-placement-invariant.md) | Logic | Ready | ADR-0006 |
| 039 | [All D pawns spawn in one server frame in sigma order and a Connecting participant's pawn is gated](story-039-spawn-one-frame-and-gate.md) | Integration | Ready | ADR-0006 |
| 040 | [A participant Disconnected at P6 gets a bound, unpossessed, AI-driven pawn and grace measured from ActivePhaseStart](story-040-disconnected-at-start-spawned.md) | Logic | Ready | ADR-0010 |
| 041 | [A non-empty GetUnboundActiveParticipants() at Active entry aborts BindFailed and never enters Active](story-041-active-entry-unbound-aborts.md) | Logic | Ready | ADR-0005 |
| 042 | [Active entry stamps state, increments RoundsPlayedCount, fires OnRoundActive once and rejects Preparing commands with NotNow](story-042-active-gate.md) | Logic | Ready | ADR-0005 |
| 043 | [At most one terminal outcome per round and equal to the F2 reference over 100,000 interleavings; the enums have no draw](story-043-no-draw-property.md) | Logic | Ready | ADR-0002 |
| 044 | [The F2 zeroing event yields an outcome only when all clauses hold; flipping any one yields none](story-044-elimination-truth-table.md) | Logic | Ready | ADR-0002 |
| 045 | [A final completion and a last-Active elimination in one tick resolve by key order with one WinLatched](story-045-same-tick-final-vs-elimination.md) | Logic | Ready | ADR-0002 |
| 046 | [Two last-Active strikes in one tick are won by the lower-key attacker in both permutations](story-046-mutual-last-strike.md) | Logic | Ready | ADR-0002 |
| 047 | [A 1v0 solo player never triggers an elimination or timer end; the final objective gives ObjectiveWin](story-047-empty-team-no-elimination.md) | Logic | Ready | ADR-0002 |
| 048 | [Killing or re-informing Informants triggers no evaluation and leaves the Active count unchanged](story-048-informants-do-not-count.md) | Logic | Ready | ADR-0002 |
| 049 | [A final completion counts only from a human, Connected, Active completer; other cases are Voided with distinct codes](story-049-completion-needs-human.md) | Logic | Ready | ADR-0002 |
| 050 | [A defence-window expiry and a strike in one tick resolve by key order (strike lands or reversal)](story-050-brace-expiry-ordered.md) | Logic | Ready | ADR-0002 |
| 051 | [A 24 h fake-clock advance in Active ends nothing and leaves only grace and pawn-loss timers](story-051-no-timer-ends-active-round.md) | Logic | Ready | ADR-0002 |
| 052 | [An objective-win latch is also the round-end step: result, score, EndRoundAll, rejections and PostRound viewers](story-052-ended-latch-effects.md) | Logic | Ready | ADR-0002 |
| 053 | [A strike win latches server-only first and presents the result at the deciding strike's death write](story-053-strike-win-staging.md) | Logic | Ready | ADR-0002 |
| 054 | [FRoundResult.TeamSummary is computed at the round-end step from the Event Log and is all-zero before Ended (MVP item 9 data)](story-054-team-summary-at-ended.md) | Logic | Ready | ADR-0002 |
| 055 | [In Ended every command except Look is rejected and every pawn gets one stop request carrying RunStopSec](story-055-ended-hold.md) | Logic | Ready | ADR-0002 |
| 056 | [With Roster/GameState arrival skew a strike win changes no ally marker or HUD state before Phase = Ended](story-056-round-ended-arrival-order.md) | Integration | Ready | ADR-0002 |
| 057 | [The host abort in Active resets as Aborted with no score when bHostMayAbortActive, else WrongPhase](story-057-host-abort.md) | Logic | Ready | ADR-0005 |
| 058 | [Reset destroys player pawns before lobby travel, empties the timer heap and reaches Lobby only when all participants are terminal or timed out](story-058-reset-clears.md) | Logic | Ready | ADR-0005 |
| 059 | [OnSessionEnding stops processing without release and a host abort before P1 resets at P1 with 0 pawns](story-059-session-end-and-abort-during-travel.md) | Logic | Ready | ADR-0005 |
| 060 | [Silent, quit, informant and debug-kill timelines resolve at the specified times on a fake clock (plus one PIE smoke)](story-060-grace-timeline.md) | Integration | Ready | ADR-0005 |
| 061 | [A rejoin 1 s before grace expiry keeps the participant Active and repossesses the same pawn](story-061-rejoin-inside-grace.md) | Logic | Ready | ADR-0005 |
| 062 | [A rejoin after grace expiry makes the participant an Informant re-pawned by the C1 takeover](story-062-rejoin-after-grace.md) | Logic | Ready | ADR-0005 |
| 063 | [A rejoin during Ended is admitted with no pawn, no transition and bIsSpectator false](story-063-rejoin-in-ended.md) | Logic | Ready | ADR-0005 |
| 064 | [The last Active member of a team converting at grace expiry gives EliminationWin to the other team with ForfeitElimination](story-064-last-disconnect-forfeits.md) | Logic | Ready | ADR-0005 |
| 065 | [Both teams fully disconnected gives Void(Abandoned) and a host kick as last conversion gives Void(KickedLastActive)](story-065-forfeit-void-cases.md) | Logic | Ready | ADR-0005 |
| 066 | [A forfeit item and a final completion in one tick give exactly one outcome by key order](story-066-forfeit-vs-completion.md) | Logic | Ready | ADR-0002 |
| 067 | [A bound AI-driven pawn's Interact, Prepare, Attack and Hide are rejected with BoundAiForbidden and nothing is enqueued](story-067-bound-ai-move-look-only.md) | Logic | Ready | ADR-0010 |
| 068 | [DisconnectGraceSec is clamped to RejoinWindowSec with a warning above it and refused by the schema below 3 s](story-068-grace-clamp.md) | Logic | Ready | ADR-0005 |
| 069 | [A rejoin-window expiry for a still-Active participant converts in the same slot with a WindowBeforeGrace defect, and none in Preparing](story-069-window-expiry-while-active.md) | Logic | Ready | ADR-0005 |
| 070 | [A kicked Active participant's window expiry is ignored and conversion happens at DisconnectedAt + G_eff](story-070-kick-window-expiry-ignored.md) | Logic | Ready | ADR-0005 |
| 071 | [A stale-generation grace timer is Rejected_StaleConnection and logged Voided, and the matching one converts](story-071-grace-generation-guard.md) | Logic | Ready | ADR-0005 |
| 072 | [An Informant disconnect calls ReleaseToCrowd once with no evaluation, and later kills resolve Npc](story-072-informant-disconnect-releases.md) | Logic | Ready | ADR-0005 |
| 073 | [ShouldAdmitExpired is true exactly for InRound records in Preparing, Active and Ended](story-073-should-admit-expired-table.md) | Logic | Ready | ADR-0005 |
| 074 | [With no eligible NPC the returner is admitted with a neutral view and the re-pawn is handed to Penalty with no timer of its own](story-074-readmit-retry.md) | Logic | Ready | ADR-0005 |
| 075 | [A pawn destroyed without a kill is replaced once with the same AppearanceId through RequestEmergence(PawnRecovery)](story-075-active-pawn-lost-recovery.md) | Logic | Ready | ADR-0010 |
| 076 | [A third pawn loss or no eligible socket in time gives Void(PawnLost), and no recovery runs in Ended or Reset](story-076-pawn-lost-void.md) | Logic | Ready | ADR-0010 |
| 077 | [A superseded Connected participant is AI-driven at t_s, hands back at rejoin, or converts at t_s + G_eff](story-077-supersede-grace-basis.md) | Logic | Ready | ADR-0005 |
| 078 | [A pawn released from Crowd earlier in the drain still gets one stop request at the round-end step](story-078-hand-over-pawn-stops-at-end.md) | Logic | Ready | ADR-0002 |
| 079 | [A kicked Active participant clears teammates' reconnecting flag, cannot reconnect and never enters R18](story-079-kicked-cannot-return.md) | Logic | Ready | ADR-0005 |
| 080 | [Replicated-property sets match the allowlist and FRoundPublicState bytes are identical on 3 clients with zeroed times](story-080-no-leak-before-ended.md) | Integration | Ready | ADR-0003 |
| 081 | [A client missing pawns drops its loading screen at ActiveAtServerTime + RevealPawnWaitMaxSec; own-pawn-killed and Ended-first cases release at once](story-081-late-loader-reveal-bound.md) | Integration | Ready | ADR-0003 |
| 082 | [The host HUD module compiles with includes of FRoundPublicState and C-TeamView only](story-082-host-parity-includes.md) | Logic | Ready | ADR-0007 |
| 083 | [No widget binds elapsed time, a countdown or a count in a full Active round; screenshots retained](story-083-no-clock-no-count.md) | UI | Ready | ADR-0002 |
| 084 | [Host Start through travel reaches Active with one bound pawn per participant (one drain stamps ActivePhaseStart, RoundsPlayedCount 1, queue at Seq 1)](story-084-pie-lifecycle-start.md) | Integration | Ready | ADR-0005 |
| 085 | [debugwin 0 ends the round with ObjectiveWin, every pawn stops within RunStopSec plus one frame, and reset returns to Lobby with teams unchanged](story-085-pie-lifecycle-objective-and-reset.md) | Integration | Ready | ADR-0005 |
| 086 | [A second round ended by debugeliminate gives EliminationWin, MatchScore [2,0] and a queue restarted at Seq 1](story-086-pie-lifecycle-elimination.md) | Integration | Ready | ADR-0005 |
| 087 | [Takeover, hand-over, hand-back, ReleaseToCrowd, re-pawn and pawn-loss respawn show no identity-bearing pawn property delta](story-087-takeover-no-pawn-property-delta.md) | Integration | Blocked | ADR-0010 |
| 088 | [A silent-then-rejoining player's pawn shows no zero-velocity run or speed drop beyond the crowd baseline on a non-team client](story-088-takeover-kinematics.md) | Integration | Blocked | ADR-0010 |
| 089 | [Blind observers cannot identify the taken-over pawn better than chance (exact binomial, alpha 0.05, power 0.8)](story-089-takeover-blind-observer.md) | Integration | Blocked | ADR-0010 |
| 090 | [Unresponsive at 3 s hands over once without a Roster transition, cancels holds correctly and hands back on Responsive](story-090-hand-over-and-back.md) | Logic | Blocked | ADR-0010 |
| 091 | [Each client's loading screen drops within 50 ms after ActiveAtServerTime and never before it holds ExpectedPawnCount pawns](story-091-scheduled-reveal.md) | Integration | Blocked | ADR-0002 |
| 092 | [The round-end step adds at most 2.0 ms host time and about 3.1 KB in a 10-player, 60-pawn round](story-092-round-end-step-budget.md) | Integration | Blocked | ADR-0002 |

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/round-match-framework.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25f and every governing ADR's conditional gate has reported before dependent work is marked Done
