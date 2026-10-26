# Story 057: The host abort in Active resets as Aborted with no score when bHostMayAbortActive, else WrongPhase

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). Also `GDD AC 11`, `R8` (abort in Active), `R22` (`stealth.round.abort` and the `reset` abort form), `States and Transitions` (Active to Reset on host `AbortRound`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 8 (host commands registered on the game instance with a net-mode and host check) and the store rule that `URoundMatchStore` / `FRoundPhaseCore` owns the phase machine; an abort in `Active` is an `Active -> Reset` move that never passes through `Ended`.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. The abort is decided in Round's plain-C++ phase core against fake gateways, so none of spikes S3 (seamless and hard travel), S4 (reconnect, retained PlayerState) or S5 (listen-host loss) is touched; the lobby travel that follows an abort is spied, not executed.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Session and travel and Forbidden; the Core Layer Rules section holds no Round-specific rule):
- Required: Host commands are registered on the game instance with the net-mode/host check (ADR-0005); `AbortRound` reaches the queue as a `HostCommand` item (ADR-0002).
- Forbidden: Never register host commands on a PlayerController, pawn, HUD or cheat manager (ADR-0005).
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (ADR-0002, ADR-0022).

**Prototype disposition**: REPLACES `AStealthGameMode::AbortRound` (`Source/StealthGame/Private/StealthGameMode.cpp`, line about 310) and the dev command that reaches it. The prototype abort restarts the round in the same world and has no `bHostMayAbortActive` gate, no `Reset` phase and no log entry. Retired by this story; the command survives as `stealth.round.abort` behind the net-mode check (TR-round-010, story 027).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 11, scoped to this story:*

- [x] GIVEN `bHostMayAbortActive` true, WHEN the host aborts in Active, THEN `Reset` with `EndReason = Aborted`, `bCalledOff` true, `MatchScore` unchanged, the host's id in the log, nothing released, `RoundsPlayedCount` keeps its Active-entry increment; GIVEN false, THEN `WrongPhase` and the phase stays `Active`.

---

## Implementation Notes

*Derived from GDD R8 (abort in Active), R22 and ADR-0005 section 8:*

- Flag true (R8): the abort goes `Active -> Reset` directly (no `Ended`), `EndReason = Aborted`, no winner and no `FRoundResult`, `MatchScore` untouched, `bCalledOff` true in the replicated `FRoundPublicState` (every client may show "The round was called off", naming no one), the Event Log slice archived server-side and never released (release classes: nothing is released after an abort), and the host's `ParticipantId` recorded in the log (R22). After an Active abort `RoundsPlayedCount` keeps the increment it received at Active entry (R8, "the round was played; each Active round has a unique `RoundIndex`"); the abort does not decrement it.
- Flag false (R8): refused with `WrongPhase` and the phase stays `Active`; no state changes. Without the flag the only ways out of `Active` are a win or a void (R15), which is why the flag exists. `bHostMayAbortActive` is a Round key in the tuning data (TDC, the `tuning-data-config` epic); in the test it is injected through the fake TDC gateway (story 003), never read from a file.
- Entry path: the host command (`stealth.round.abort` or the abort form of `stealth.round.reset`, R22) calls the same entry point as the host panel after the net-mode, local controller and `ParticipantId == HostId` check (story 026 owns that check; this story's test calls the entry point as the host). In `Active` the command is a `HostCommand` item (`FHostCommandPayload { Command = AbortRound }`, ADR-0002 Key Interfaces) that resolves in the queue; after a latch it is parked unresolved (ADR-0002 8.1) and is not this story's case. Open: confirm against GDD R22 and ADR-0002 section 5 whether the Active abort resolves in a queue slot (and therefore consumes a `Seq`) or is executed directly on the host entry point; the AC does not depend on it.
- The abort then runs the `Reset` cleanup (R10, story 058) with reason `Aborted` and `RequestLobbyTravel`; this story asserts the transition and its data (phase `Reset`, `EndReason`, `bCalledOff`, score, `RoundsPlayedCount`, log, nothing released) and not the cleanup order. "Nothing released" means no Event Log content is released through the Visibility Policy and no `ReleaseToCrowd` is called (a `Reset` destroys player pawns, R10 step 4).
- Open: confirm against GDD R8, R19 and the QA note whether the refused abort (flag false) also writes a log entry; the AC states the host's id in the log only for the accepted abort.
- Open: confirm against ADR-0002 section 8.1 and R8 whether an Active abort also calls `UResolutionQueue::Latch` to close intake before the `Reset` cleanup (R8 and ADR-0002 only say a void is `TryTerminate` plus `Latch` in the same call, and stages no `RoundEndStep`).
- Where it lives: the abort handling in the Round Core phase machine (planned `Source/StealthGameplay/Private/Round/Phase/`) on `FRoundPhaseCore` inside `URoundMatchStore`, with the host entry in `Source/StealthGameplay/Private/Round/Commands/` (story 027 and 026 own the command wiring).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 028 (`production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`): the phase and admission table and illegal transitions.
- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): Active entry and the `RoundsPlayedCount` increment.
- Story 058 (`production/epics/round-match-framework/story-058-reset-clears.md`): the `Reset` cleanup order, pawn destruction and the Lobby exit.
- Story 034 (`production/epics/round-match-framework/story-034-preparing-aborts.md`): the abort in `Preparing`, including the host abort there.
- Story 027 (`production/epics/round-match-framework/story-027-reset-command-matrix.md`) and story 026 (`production/epics/round-match-framework/story-026-host-only-commands.md`): the per-phase command matrix and the `NotHost` check.
- Story 059 (`production/epics/round-match-framework/story-059-session-end-and-abort-during-travel.md`): a host abort before P1.
- Void outcomes (`Abandoned`, `KickedLastActive`, `PawnLost`): stories 065 and 076.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/HostAbortTest.cpp` (class `FRoundHostAbortTest`, test name `StealthGame.Round.HostAbort`)
- `Source/StealthGameplay/Private/Round/Phase/` Active-abort transition (new)
- `Source/StealthGame/Private/StealthGameMode.cpp` and `Source/StealthGame/Public/StealthGameMode.h` (`AbortRound`; retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: The host abort in Active resets as Aborted with no score when bHostMayAbortActive, else WrongPhase.
  - Given: an `Active` round, with `bHostMayAbortActive` true, then false.
  - When: the host aborts.
  - Then: with the flag true: `Reset` with `EndReason = Aborted`, `bCalledOff` true, `MatchScore` unchanged, the host's id in the log, nothing released, and `RoundsPlayedCount` keeps its Active-entry increment.
  - Then: with the flag false: `WrongPhase` and the phase stays `Active`.
  - Edge cases: the abort writes a log entry in both cases only if the contract says so (Open: confirm against GDD R8 and R19).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/HostAbortTest.cpp` — must exist and pass (automation test `StealthGame.Round.HostAbort`, class `FRoundHostAbortTest`)

**Status**: [x] Created and passing (`StealthGame.Round.HostAbort`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`, `production/epics/round-match-framework/story-042-active-gate.md`
- Unlocks: None

Cross-epic: `production/epics/session-travel` (`RequestLobbyTravel`, admission mode; stories not yet created); `production/epics/participant-state-roster` (`RoundsPlayedCount` scope, `ResetAll` at the next start; stories not yet created); `production/epics/tuning-data-config` (`bHostMayAbortActive`, module skeleton and StealthTestHarness bootstrap; stories not yet created)

## Completion Notes
**Completed**: 2026-10-26 (RMF round-end and disconnect batch: stories 048-065, two-step round end, TeamSummary, grace/forfeit/void)
**Criteria**: 1/1 passing (flag: Reset, Aborted with the host id, bCalledOff, score unchanged, nothing released, RoundsPlayedCount kept; no flag: WrongPhase, nothing changes)
**Deviations**: The Active abort runs on the host entry point, not as a queue item; the refused abort writes no log entry (Open items, decision log); a reset between the latch and the round-end step is held regardless of the flag (R9 v)
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/HostAbortTest.cpp`; full suite: `production/qa/evidence/rmf-round-end-disconnects-2026-10-26/full-suite.txt`
**Run result**: N/A - Logic, covered headless
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, RMF round-end batch; findings and fixes in production/qa/evidence/rmf-round-end-disconnects-2026-10-26/evidence.md
