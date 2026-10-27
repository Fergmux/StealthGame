# Epic: Crowd & NPC Behaviour

> **Layer**: Feature
> **GDD**: design/gdd/crowd-npc-behaviour.md
> **Architecture Module**: Crowd in `StealthGameplay/Features` (Crowd Director subsystem, StateTree per NPC, follower; implements `ICrowdControl` and `ITakeoverEligibilitySource`)
> **Status**: Ready
> **Stories**: 39 stories (see the Stories table at the end of this file)
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 11 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Crowd & NPC Behaviour owns the fifty-odd citizens who are not players: how many there are (`NpcCount`, default 50), where they stand (the density model), what they do minute to minute (the baseline behaviour catalogue and the gaze catalogue), which player-like things they deliberately also do (red herrings), how they react to a nearby strike, how the city refills after a death (replacement through the shared emergence sockets and `DrawReplacement`), and how a pawn passes between crowd control and a participant (disconnect takeover, `ReleaseToCrowd`, informant takeover eligibility). It drives every NPC through the same pawn command interface a human uses on the one Shared Pawn class, so an NPC can do nothing a player could not.

The epic delivers the server-only Crowd Director subsystem (ledger, ambient schedule, density, stimulus fan-out, Halt in the round-end step), a StateTree per NPC at about 4 Hz plus a follower emitting `Move`/`Look` through the IIF (an AI controller whose path-following does not tick and whose `UpdateControlRotation` is overridden), a 2 m spatial hash, at most 2 async path starts per frame on a navmesh frozen at `OnLayoutReady`, counter-based draws from the crowd server sub-seed, identity blindness (no crowd code reads player, binding, team or state), the Baseline and Tell Matrix rates as data with calibration counters (ADR-0023), dormancy until `OnRoundActive`, participant-bound AI driving, and the budget of at most 1.5 ms AI + nav with global-only cost cuts that never change replication. Without it the game is a deathmatch among mannequins; with it Pillar 2 has a stable, measurable baseline.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0006: Seeds and deterministic/secret random streams | Public sub-seeds for layout vs SipHash-2-4 keyed server-secret streams; integer PCG32 streams so layout is bit-identical across machines. | LOW |
| ADR-0014: Layout service, seeded generation and runtime navmesh | Layout-service world subsystem with provider registry, integer Layout Manifest and LayoutHash, server navmesh build and validation, reseed and graybox fallback. | MEDIUM |
| ADR-0016: Crowd AI architecture | Server Crowd Director, StateTree per NPC at about 4 Hz emitting IIF commands, async path queries, global-only cost cuts, ICrowdControl implementation. | MEDIUM |
| ADR-0023: Playtest telemetry hooks | Telemetry section of the Event Log store, probes and detectors at Feature call sites, server-only statistics. | LOW |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

10 of 10 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-crowd-001 | Server-only Crowd Director subsystem | ADR-0016 ✅ |
| TR-crowd-002 | StateTree per NPC (~4 Hz) + follower emitting `Move`/`Look` through the IIF; no path-following driving | ADR-0016 ✅ |
| TR-crowd-003 | Async path queries ≤ 2 starts/frame; navmesh frozen at `OnLayoutReady`; 2 m spatial hash | ADR-0014, ADR-0016 ✅ |
| TR-crowd-004 | Identity blindness: no crowd code reads player/binding/team/state | ADR-0001 ✅ |
| TR-crowd-005 | Counter-based draws from the crowd server sub-seed | ADR-0006 ✅ |
| TR-crowd-006 | ≤ 1.5 ms AI + nav; global-only cuts; never changes replication | ADR-0016 ✅ |
| TR-crowd-007 | Participant-bound AI driving and takeover eligibility supply | ADR-0016 ✅ |
| TR-crowd-008 | Ambient emergence and replacement via shared sockets and `DrawReplacement` | ADR-0016 ✅ |
| TR-crowd-009 | Dormant until `OnRoundActive`; Halt in the round-end step | ADR-0016 ✅ |
| TR-crowd-010 | Baseline and Tell Matrix rates as data with calibration counters | ADR-0016, ADR-0023 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** `Source/StealthGame/Private/StealthNpcController.cpp` (66 lines; wandering on the nav mesh with acceleration-based paths) and the crowd spawning in `StealthGameMode.cpp` (`SpawnCrowd`, `SpawnCrowdWhenNavReady`, `ScheduleNpcReplacement`, `FindCrowdPoint`), plus `BP_CrowdCharacter` as the Blueprint wrapper. The prototype crowd is 40 NPCs by default (50 measured at 89.6 FPS, 11.2 ms game thread, on an RTX 5090). The Baseline and Tell Matrix, gaze catalogue, replacement rules and Halt are all new. The prototype `StealthBot` console driver is a test aid, not crowd code.

## Build Order and Gates

- **Depends on (epics)**: character-pool-appearance, shared-character-locomotion, input-interaction-framework, level-socket-contract, information-visibility-policy, round-match-framework; soft peers concealment-bushes, assassination-defence-reversal, objectives-chain. First in the Feature build order (the architecture moved it from Core to Feature).
- **Gates and risks**: Conditional gate from ADR-0016: the 60-character spike (`NpcCount` 50 plus 10 players, AC-CNB-26 capture on the reference host); the 1.5 ms AI + nav cost rule is provisional until it reports. StateTree has no page in docs/engine-reference/unreal/, so claims were read from 5.8.4 source. Systems-index High-Risk system (design plus technical: readability tuning and AI cost). The 3060-class budget is unmeasured (the prototype's 50-character run was on an RTX 5090). This epic owns the cross-cutting **50-character performance pass** (about 6 d bottom-up) as an exit criterion; it also carries the Crowd half of MVP item 11 (the interim full-pace rule is lifted, Crowd R3.1).
- **Estimate**: about 14 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 8.4 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 38 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/crowd-npc-behaviour.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done

## Stories

Created 2026-10-25 by `/create-stories` at `story_granularity: balanced`. Every GDD criterion AC-CNB-01 to AC-CNB-38 is covered by at least one story (AC-CNB-26 by the early spike 032 and the exit pass 038; AC-CNB-30 advisory in 037). TR-crowd-011 exists in `docs/architecture/tr-registry.yaml` but is missing from this epic's requirements table above; stories 010, 034 and 035 carry it (raise at the next `/architecture-review`).

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Spike: StateTree, controller and path-latency engine verifications | Integration | Complete | ADR-0016 |
| 002 | Crowd draws: counter-based streams, continuous mappings and kill independence | Logic | Complete | ADR-0016 |
| 003 | Crowd tuning keys, Baseline and Tell Matrix rates, and asset references as data | Config/Data | Complete | ADR-0016 |
| 004 | Crowd Director core: server-only subsystem, ledger and phase gating | Logic | Complete | ADR-0016 |
| 005 | Command sinks: motion/command split, control-epoch check and Halt filter | Logic | Complete | ADR-0016 |
| 006 | AStealthCrowdController: no path following, Look pitch path and tick order | Integration | Complete | ADR-0016 |
| 007 | Path source and start budget: pre-baked edges, async queries and leg ids | Logic | Complete | ADR-0016 |
| 008 | Spatial hash, ray budget and the concealment-blind perception filter | Logic | Complete | ADR-0016 |
| 009 | Follower: path shaping, pace set, corners and the shared stop floor | Logic | Complete | ADR-0016 |
| 010 | Virtual input layer: device profiles, virtual camera and recordable stream | Logic | Complete | ADR-0016 |
| 011 | Avoidance as Move offsets: yield rate, obstacles and the blocked ladder | Logic | Complete | ADR-0016 |
| 012 | StateTree schema, assets, decision grid and the per-frame due-time list | Integration | Complete | ADR-0016 |
| 013 | Identity blindness and command-path gate | Integration | Complete | ADR-0001 |
| 014 | Pauses, stall-cover regulator and the stationary-stretch bound | Logic | Complete | ADR-0016 |
| 015 | Conversation (C3) and turn-in-place (C6), partner-blind | Integration | Complete | ADR-0016 |
| 016 | Gaze catalogue (G1-G7): glances, holds, goods and salient motion | Logic | Complete | ADR-0016 |
| 017 | Density model (F1) and assignment-blind start placement | Logic | Complete | ADR-0016 |
| 018 | Tell sampler: compensated clocks, chain-gap guard and calibration counters | Logic | Complete | ADR-0016 |
| 019 | Red herrings: late-porter runs, public traversals and reversals | Integration | Complete | ADR-0016 |
| 020 | Decoy tending: uniform court choice, early releases and the T3 posterior | Integration | Complete | ADR-0016 |
| 021 | Brace and startle red herring: due clock, flinch triggers and Prepare cap | Integration | Complete | ADR-0016 |
| 022 | Bush red herring: occupancy-blind shade-seeking with uncapped stays | Integration | Complete | ADR-0016 |
| 023 | Tell Matrix calibration gate: 600-minute rates and evidence-compounds test | Logic | Complete | ADR-0016 |
| 024 | Strike stimulus fan-out and outcome-blind reactions (R8) | Logic | Complete | ADR-0016 |
| 025 | Strike reaction look: the graybox market reads calm, not a stampede | Visual/Feel | In Review (lead sign-off pending) | ADR-0016 |
| 026 | Ambient schedule as queue items and witness-free emergence | Integration | Complete (seams; LSC/CPA wiring pending) | ADR-0016 |
| 027 | Kill vacancy, victim-blind replacement delay and exit cancellation | Logic | Complete (seams; OnPawnKilled wiring pending) | ADR-0016 |
| 028 | Participant-bound pawn driving: takeover blend, restricted driver and hand-back | Integration | Complete | ADR-0016 |
| 029 | ReleaseToCrowd: invisible adoption, strike lockout and the exile exit slot | Integration | Complete (MP half deferred) | ADR-0016 |
| 030 | Takeover eligibility supply and atomic NPC release for informants | Integration | Complete (AC-CNB-23c deferred) | ADR-0016 |
| 031 | Halt at the round-end step and handover epoch discipline | Integration | Complete | ADR-0016 |
| 032 | Spike: 60-character server cost on the reference host (50 NPCs + 10 players) | Integration | Complete (NOT ASSESSED: no reference host) | ADR-0016 |
| 033 | Global cost-cut order with behaviour invariance | Logic | Blocked (no reference capture; story 034 blocked) | ADR-0016 |
| 034 | Pipeline parity: AIController versus bot-client input path, nightly | Integration | Blocked (self-hosted CI runner, bot-client replay harness) | ADR-0016 |
| 035 | Classifier gate: NPC motion features cannot identify NPCs (playtest) | Integration | Blocked (evidence pending playtest) | ADR-0016 |
| 036 | No crowd UI, debug draw or indicator in Shipping | UI | Blocked (packaged Shipping build; scan done) | ADR-0016 |
| 037 | Tell Matrix row citations across the dependent GDDs (advisory check) | Config/Data | Complete (advisory: 12 citations missing) | N/A |
| 038 | Performance pass: 50/60-character server cost on the full catalogue (exit criterion) | Integration | Blocked (no reference capture; story 033 blocked) | ADR-0016 |
| 039 | Retire the prototype crowd: swap to the Director and one pawn class | Integration | Blocked (LSC emergence sockets) | ADR-0016 |
