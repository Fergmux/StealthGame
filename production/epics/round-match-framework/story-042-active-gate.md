# Story 042: Active entry stamps state, increments RoundsPlayedCount, fires OnRoundActive once and rejects Preparing commands with NotNow

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-006` (Preparing readiness sequence, spawn gate and watchdog); GDD AC 10; rules R6 ("All commands are rejected until `Active`"), R7 (Active entry), R11 (queue opens at Active entry).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores (also ADR-0002: Server resolution queue, server clock and round-end staging)
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies ADR-0005's `URoundMatchStore` (the phase core and `RoundsPlayedCount` live in the game-instance store) and ADR-0002 section 8.8: Active entry is one server step at the first drain whose `ServerNow() >= ActiveAtServerTime`; the queue opens at `Seq` 1 while `ServerTickIndex` continues.
**ADR Version**: ADR-0005 2026-10-24; ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk; ADR-0002 is MEDIUM)
**Engine Notes**: None touched. The test runs the plain C++ phase core and queue core against the fake clock and fake Crowd gateway (stories 001 and 003). The ADR-0002 verification items (spawn inside `TG_PostPhysics`, the client estimator, the drain running once per server frame) and ADR-0005 spikes S3 to S5 are not exercised; the real drain tick function (`FResolutionDrainTickFunction`, `TG_PostPhysics`) is covered by the Foundation queue stories and the PIE lifecycle stories 084 and 085.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Round phase flow and Forbidden Approaches):
- Required: `FRoundPublicState` is replicated unconditionally with `Phase` 0 = `Unknown`, `PhaseEnteredServerTime` 0 during `Active` and `ActiveAtServerTime` non-zero only in `Preparing` after P7 (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never mutate gameplay state outside the drain; never resolve inline in an RPC body or AI source (ADR-0002).
- Guardrail: queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0022); the Active-entry step runs once per round and adds nothing per frame.

**Prototype disposition**: Replaces the round-activation part of `AStealthGameMode::StartRound` and `TryScheduleRoundStart` in `Source/StealthGame/Private/StealthGameMode.cpp` (and `StealthGameMode.h`) and the replicated round fields in `StealthGameState.*`. The prototype activates a round by first-come order with no `ActivePhaseStart`, no `RoundsPlayedCount` and no queue to open; the production step is one stamped server step in the phase core.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 10, scoped to this story:*

- [ ] GIVEN Active entry, THEN in the same drain `ActiveAtServerTime` is 0, `ActivePhaseStart` is stamped, `RoundsPlayedCount` rose by 1, `OnRoundActive` fired once and Crowd received one start; a command submitted in Preparing returns `NotNow`.

---

## Implementation Notes

*Derived from ADR-0002 section 8.8 and GDD R6, R7, R11:*

- **One server step (R7).** At the first drain whose server time is at or after `ActiveAtServerTime` (set at P7 as `ServerTime + StartLeadSec`), Round performs in one step: sets `ActiveAtServerTime = 0` in `FRoundPublicState` (so no Active start time stays public, AC 27); stamps `ActivePhaseStart` (server time, a `double`); increments `RoundsPlayedCount` (so `RoundIndex` is now this round); opens the resolution queue (`Seq` starts at 1 and restarts at 1 each round, `ServerTickIndex` continues); raises `OnRoundActive(RoundEpoch, ActivePhaseStart)`; accepts commands; tells Crowd to start every NPC and every AI-driven pawn; and releases the loading screen (Session rule 11.8) in the same step. The AC asserts the first five; the loading-screen release and the client drop rule are stories 084 and 091.
- **Fires once.** `OnRoundActive` and the Crowd start are raised exactly once per round, and `RoundsPlayedCount` is incremented exactly once. A later drain re-fires nothing. The phase core guards the step with its phase (`Preparing` to `Active` is legal once; `Active` to `Active` is not a transition, see story 028).
- **Preparing commands return `NotNow` (R6).** All commands are rejected until `Active` (human and AI, `NotNow`) and the Crowd AI does not start; pawns exist in place but nothing moves. The queue is not open in `Preparing`, so the rejection is Round's phase gate answering the command's ingress, not a queue item. Open: confirm against GDD R6 and R13 (the `PawnCommand` row, IIF R5) which seam carries the `NotNow` reply in [U] (the story 004 stub resolver or a Round command-gate fake); the AC asks only that the command submitted in Preparing returns `NotNow`.
- **Stamping rule.** `ActivePhaseStart` is the value F4 reads for `GraceExpiry = max(DisconnectedAt, ActivePhaseStart) + G_eff`; it is supplied by the fake clock in tests and by `ServerNow()` in production, never by a client value or `GetServerWorldTimeSeconds()`.
- **Order inside the step.** State changes first (stamps, count, queue open), then `OnRoundActive`, then the Crowd start, so any subscriber that reads `ActivePhaseStart` or `RoundsPlayedCount` sees the new values. Story 041's `GetUnboundActiveParticipants()` guard runs before this step and prevents it entirely when it fails.
- **Where the code goes.** The step is a method of `FRoundPhaseCore` (plain C++, no `UObject`) behind `URoundMatchStore`, with the queue core, the Crowd gateway and the clock injected (ADR-0007, ADR-0005). Replication of the zeroed `ActiveAtServerTime` and the forced GameState net update belong to story 029 and the allowlist test of story 080; here assert the value on the server struct only.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 041 (`production/epics/round-match-framework/story-041-active-entry-unbound-aborts.md`): the `GetUnboundActiveParticipants()` guard and the `BindFailed` abort.
- Story 029 (`production/epics/round-match-framework/story-029-public-state-replication.md`) and story 080 (`production/epics/round-match-framework/story-080-no-leak-before-ended.md`): `FRoundPublicState` replication, the zeroed times as seen on three clients, and the allowlist.
- Stories 084 and 091: the PIE lifecycle (one bound pawn per participant, `Seq` 1 on a real server) and the loading-screen drop within 50 ms of `ActiveAtServerTime`.
- Story 051 (`production/epics/round-match-framework/story-051-no-timer-ends-active-round.md`): that no timer ends an Active round.
- Story 060: the grace timer created at Active entry for a participant disconnected before Active (story 040 covers the record's expiry value).
- Crowd internals (how NPCs start) and IIF internals (the `NotNow` reply flush) belong to the crowd-npc-behaviour and input-interaction-framework epics.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Phase/RoundPhaseCore.h` (Active-entry method; the file is created by story 028)
- `Source/StealthGameplay/Private/Round/Phase/RoundPhaseCore.cpp`
- `Source/StealthGameplay/Private/Tests/Round/ActiveGateTest.cpp` (class `FRoundActiveGateTest`, test name `StealthGame.Round.ActiveGate`)
- `Source/StealthGame/Private/StealthGameMode.cpp`, `Source/StealthGame/Public/StealthGameMode.h` (prototype round activation: retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Active entry stamps state, increments `RoundsPlayedCount`, fires `OnRoundActive` once and rejects Preparing commands with `NotNow`.
  - Given: Preparing with the barrier released, and a command queued while in Preparing.
  - When: the drain that enters Active runs.
  - Then: in that same drain `ActiveAtServerTime` is 0 and `ActivePhaseStart` is stamped.
  - Then: `RoundsPlayedCount` rose by 1, `OnRoundActive` fired once, and Crowd received one start.
  - Then: the command submitted in Preparing returns `NotNow`.
  - Edge cases: a later drain does not re-fire anything; the `RoundsPlayedCount` increment happens exactly once.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/ActiveGateTest.cpp` (automation test `StealthGame.Round.ActiveGate`, class `FRoundActiveGateTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.ActiveGate`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-041-active-entry-unbound-aborts.md`
- Unlocks: `production/epics/round-match-framework/story-051-no-timer-ends-active-round.md`, `production/epics/round-match-framework/story-052-ended-latch-effects.md`, `production/epics/round-match-framework/story-057-host-abort.md`, `production/epics/round-match-framework/story-058-reset-clears.md`, `production/epics/round-match-framework/story-060-grace-timeline.md`, `production/epics/round-match-framework/story-067-bound-ai-move-look-only.md`, `production/epics/round-match-framework/story-075-active-pawn-lost-recovery.md`, `production/epics/round-match-framework/story-080-no-leak-before-ended.md`, `production/epics/round-match-framework/story-083-no-clock-no-count.md`, `production/epics/round-match-framework/story-084-pie-lifecycle-start.md`
- Cross-epic: `production/epics/crowd-npc-behaviour` (the Crowd start call, via the fake Crowd gateway; stories not yet created), `production/epics/input-interaction-framework` (the `NotNow` reply for a command in Preparing; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (same drain: ActiveAtServerTime 0, ActivePhaseStart stamped, RoundsPlayedCount +1, OnRoundActive once, one Crowd start, queue Seq restarts at 1; Preparing commands NotNow; nothing re-fires)
**Deviations**: ICrowdControl::StartRound added by amendment (ADR-0016 named no start call). The NotNow gate is FRoundMatchCore::AdmitCommand; in L_Prototype the prototype command adapter consults Round's live phase (IsRoundLive) instead of sending a NotNow reply, until the IIF stage-2 resolver lands.
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/ActiveGateTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: OBSERVED: production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
