# Epic: NPC-Kill Penalty & Informants

> **Layer**: Feature
> **GDD**: design/gdd/npc-kill-penalty-informants.md
> **Architecture Module**: Penalty & Informants in `StealthGameplay/Features` (strike counts, informant respawn timers and takeover commit)
> **Status**: Ready
> **Stories**: 22 stories (see the Stories table below)
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 14 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

NPC-Kill Penalty & Informants gives a wrong read its lasting cost and gives an eliminated player their reduced role. It owns strike accounting (a server-only count per `ParticipantId`, fed only by Assassination's `OnPawnKilled` and counted for `Cause = Strike` and `VictimKind = Npc` alone; on the `NpcKillStrikeLimit`-th kill, default 3 and user-locked, it requests the `Informant` transition inside the same Round queue slot and releases the surviving body to the crowd), the owner-only wrong-target tally and `bExiled` (carried on the C-TeamView owner part together with `T_due`, per the 2026-10-25 decision), and the informant pawn lifecycle: every time a participant needs an informant body (newly eliminated by any reason, killed as an informant, or returning to an `Active` round while awaiting a pawn) Penalty schedules a respawn as a due-time-heap item at `T_due` and performs the single takeover path.

The takeover picks an unseen crowd pawn through LSC `PickRespawnPawn`, takes a card with `TakeNpcCard`, adopts the pawn through `TakeOverCrowdPawn` in one call stack with retries and widening, and keeps the informant-kill invariant (an informant kill is perceptually identical to an NPC kill, Tell row T7). Informant restrictions (walk only, no traversal, no strike) are enforced by their owners, an Absent path covers disconnected victims, and Event Log rows record the lifecycle.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0002: Server resolution queue, server clock and round-end staging | One total-order queue keyed (ServerTickIndex, Seq) drained in TG_PostPhysics, due-time heap, typed staged writes, single-assignment latch, client server-time estimator. | MEDIUM |
| ADR-0019: Informant lifecycle and crowd-pawn takeover | Respawn as a due-time-heap item, takeover of an unseen crowd pawn in one call stack, owner-only tally and T_due on the C-TeamView owner part. | MEDIUM |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

8 of 8 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-pen-001 | Server strike count and owner tally keyed by `ParticipantId`, round-scoped | ADR-0019 ✅ |
| TR-pen-002 | Exile on the Nth NPC kill through the Roster; body released to the crowd | ADR-0019 ✅ |
| TR-pen-003 | Respawn as a due-time-heap item at `T_due`; Absent path for disconnected victims | ADR-0002, ADR-0019 ✅ |
| TR-pen-004 | Takeover of an unseen crowd pawn in one call stack; retries and widening | ADR-0019 ✅ |
| TR-pen-005 | Informant-kill invariant: perception identical to an NPC kill | ADR-0001, ADR-0019 ✅ |
| TR-pen-006 | Owner-only data (tally, `bExiled`, `T_due` on the C-TeamView owner part, user decision 2026-10-25) | ADR-0001, ADR-0019 ✅ |
| TR-pen-007 | Informant restrictions enforced by their owners; Tell row T7 | ADR-0019 ✅ |
| TR-pen-008 | Event Log rows | ADR-0019 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** the penalty and informant code in `StealthGameMode.cpp` (`EliminatePlayer`, `ExilePlayer`, `RespawnInformant`, `MessagePlayer`/`MessageTeam`) and `FStealthRules::ShouldRemoveForNpcKills` (pure rule with unit tests; removal on the limit-th kill is the reference). Prototype networked tests A (third civilian kill exiles the player while a teammate is active) and B (killing an enemy informant respawns them and does not count as an NPC kill) are the behavioural acceptance reference. The prototype respawns informants without a due-time heap, without an unseen-pawn pick and without card conservation.

## Build Order and Gates

- **Depends on (epics)**: assassination-defence-reversal (`OnPawnKilled`), participant-state-roster, round-match-framework (due-time heap), level-socket-contract (`PickRespawnPawn`), crowd-npc-behaviour (eligibility), shared-character-locomotion (`TakeOverCrowdPawn`), character-pool-appearance (`TakeNpcCard`); concealment-bushes (hard, `IsInBushOrTransition` takeover eligibility)
- **Gates and risks**: ADR-0019 is conditional only on amendment requests to owning ADRs (the `T_due` carrier was decided 2026-10-25: C-TeamView owner part). It inherits spikes S1 and S14 for possession parity (MEDIUM risk). An `Unresponsive` informant at commit takes the documented undriven-stop path.
- **Estimate**: about 8 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 4.8 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 48 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Stories

Granularity `balanced` (2-4 acceptance criteria per story; all 48 GDD criteria AC-NKP-01 to AC-NKP-48 are covered exactly once, AC-NKP-04 split into a Logic reflection criterion and a PIE differential criterion; criteria derived from ADR-0019 where the GDD has none are labelled "derived"). Stories embed Control Manifest 2026-10-25f (the epic header says 2026-10-25e: the manifest header now reads f and the Feature-layer Penalty rules are unchanged). All 22 are Ready: spikes S1, S2 and S14 reported 2026-10-25 (`production/qa/spikes/2026-10-25_S1-S2-S14.md`), so the `[Provisional: spikes S1/S14]` criteria (AC-NKP-17, 36, 37, 43) are written now and re-baselined on the production pawn; open conditions there are rejoin, travel, PawnRecovery, a multi-process capture and closing the whole-pawn parity differentiators in shared-character-locomotion. Cross-epic stories not yet written (round-match-framework `IInformantBodyProvider`, hud, assassination-defence-reversal) are named by epic. QL-STORY-READY: 11 ADEQUATE at first pass (001, 002, 003, 006, 007, 010, 014, 015, 017, 020, 021), 11 GAPS revised and re-reviewed (004, 005, 008, 009, 011, 012, 013, 016, 018, 019 ADEQUATE after revision; 022 ADEQUATE after the QA lead's own rewrite, not re-reviewed), 0 INADEQUATE. Open items for the GDD and ADR owners: GDD AC-NKP-30 says two picks where the arithmetic gives three; ADR-0019 section 6 prints a two-argument `SetOwnerRespawnDue`; the `RespawnCancelled` reason values are a story-009 proposal; who writes `RespawnStarved` (LSC, per ADR section 7) versus ADR section 4.3's wording.

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Penalty core store, service shell and round-scoped carry-over | Logic | Ready | ADR-0019 |
| 002 | Strike rule, stale-read release grace and reversal at the limit | Logic | Ready | ADR-0019 |
| 003 | Wrong-target tally, FStrikeResult supply and owner re-push | Logic | Ready | ADR-0019 |
| 004 | Penalty Event Log rows, server-only and never through UE_LOG | Logic | Ready | ADR-0019 |
| 005 | Exile sequence inside the killing Attack's slot | Logic | Ready | ADR-0019 |
| 006 | The exiled body becomes an ordinary NPC and leaves through a scheduled exit slot | Integration | Ready | ADR-0019 |
| 007 | Exile of a last Active player latches the round; two exiles in one drain | Integration | Ready | ADR-0002 |
| 008 | Informant kill retires the binding inline and looks exactly like an NPC kill | Integration | Ready | ADR-0019 |
| 009 | Respawn due time (F2), scheduling from Round's provider call, and cancellation | Logic | Ready | ADR-0019 |
| 010 | Absent path for disconnected victims, readmission and bodies lost without a kill | Logic | Ready | ADR-0019 |
| 011 | T_due carrier adapter writes the countdown end to the owner part only | Integration | Ready | ADR-0019 |
| 012 | Respawn commit sequence: pick, pre-check, release, takeover, card | Logic | Ready | ADR-0019 |
| 013 | Starvation, retries and widening | Logic | Ready | ADR-0019 |
| 014 | One respawn path for every cause, and the takeover is invisible on the wire | Integration | Ready | ADR-0019 |
| 015 | Informant contract, pawn parity and the owner-AC trace | Integration | Ready | ADR-0019 |
| 016 | StrikeCount never leaves the server and removal audiences match an NPC kill | Integration | Ready | ADR-0001 |
| 017 | Owner-side exile, informant-killed and tally timing | Integration | Ready | ADR-0019 |
| 018 | Tally line, returning countdown and exile card | UI | Ready | ADR-0019 |
| 019 | Takeover return, exile body and exiled owner's view stay visually continuous | Visual/Feel | Ready | ADR-0019 |
| 020 | Penalty tuning warnings and the residual and hunt model checks | Config/Data | Ready | ADR-0004 |
| 021 | Retire the prototype penalty code behind the production path | Integration | Ready | ADR-0019 |
| 022 | Downtime and residual telemetry report from internal matches | Integration | Ready | ADR-0019 |

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/npc-kill-penalty-informants.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
