# Epic: Round & Match Framework

> **Layer**: Core
> **GDD**: design/gdd/round-match-framework.md
> **Architecture Module**: Round & Match Rules in `StealthGameplay/Core`, plus the Foundation-layer Resolution Queue & Server Clock and Event Log & fault sink whose contracts it owns, in `StealthFoundation`
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories round-match-framework`
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
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

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/round-match-framework.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
