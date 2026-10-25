# Story 059: OnSessionEnding stops processing without release and a host abort before P1 resets at P1 with 0 pawns

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). Also `GDD AC 43`, `R8` (Session end), `R17.8` (host leaves), `R22` (abort in Preparing asks Session to cancel travel), Edge Cases ("If the host issues `AbortRound` during travel"), `States and Transitions` ("any phase, session end").
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 7 (deliberate host quit sends `ClientSessionEnding`, then `EndMatch` and shutdown; `EndMatch` runs on every session exit path; no host migration) and section 2 (each store clears itself on `EndMatch`, so a former host keeps nothing).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. The session end and the travel cancel are fake Session events (`OnSessionEnding`, travel cancel), so spikes S3 (travel), S4 (reconnect) and S5 (listen-host loss and quit-notice bounds) are not touched; the host-loss detection on clients is ADR-0005 section 6/7 and Session's.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Session and travel and Forbidden; the Core Layer Rules hold no Round-specific rule):
- Required: `EndMatch` runs on every session exit path (ADR-0005); Round, on `OnSessionEnding`, only stops processing and flushes its log.
- Forbidden: Never let a `Preparing` abort call `UResolutionQueue::Latch` or stage a `RoundEndStep`; the queue is not open in `Preparing` (ADR-0002). This applies to a host abort before P1.
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by `TravelLoadTimeoutSec` (60 s) (ADR-0005).

**Prototype disposition**: REPLACES the prototype's handling of a session or round abort during start in `AStealthGameMode::TryScheduleRoundStart` (`Source/StealthGame/Private/StealthGameMode.cpp`, line about 235) and `AbortRound` (line about 310), which have no session-end concept and no travel. Retired by this story together with stories 024 and 057.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 43, scoped to this story:*

- [ ] GIVEN `OnSessionEnding` in any phase, THEN processing stops, `SessionEnded` is logged, nothing is released and no phase change occurs; GIVEN a host abort before P1, THEN `Reset` runs at P1 (or on Session's travel cancel) with 0 pawns spawned.

---

## Implementation Notes

*Derived from GDD R8, R17.8, R22, the Edge Cases entry and ADR-0005 sections 2 and 7:*

- Session end is a terminal event, not a phase (R8): on `OnSessionEnding` Round stops processing, flushes the Event Log with `EndReason = SessionEnded`, releases nothing, and makes no phase change. The telemetry section is exported at the `SessionEnded` latch (R19). For a listen host leaving (R17.8) Round does nothing further: the session and the match end and no store survives (ADR-0005 section 2, `EndMatch`).
- Table-driven over the five phases (QA): `Lobby`, `Preparing`, `Active`, `Ended`, `Reset`. In each, fire the fake Session's `OnSessionEnding` and assert (1) processing stops (no further queue item resolves, no timer fires, no phase transition command is accepted; use the fake clock to advance and assert nothing resolves after `SessionEnded`), (2) one `SessionEnded` entry is logged, (3) nothing is released (no Event Log content released through the Visibility Policy; no `ReleaseToCrowd`; no pawn destroyed or freed by Round), (4) the phase value is unchanged.
- Host abort before P1 (R22, Edge Cases): if the host issues `AbortRound` while the round is travelling, Round marks the round aborted; when the new world reports P1 it immediately runs `Reset` and `RequestLobbyTravel`, and no pawn is spawned (0 pawns). If the new world has not reported P1, the abort also asks Session to cancel the travel (R22), so the watchdog is not the only exit; `Reset` then runs on Session's travel cancel. The test covers both triggers: P1 reported after the abort mark, and Session's travel cancel before P1, each with 0 pawns spawned (asserted on the fake pawn factory gateway).
- The `Reset` run here is story 058's cleanup; this story asserts only that it is triggered at P1 or on the cancel with no spawn. The abort in Preparing is a Preparing abort (story 034, reason `PreparingFailed(code)` with `bCalledOff` true only for the host abort); the "before P1" case is its pre-world sub-case. Open: confirm against GDD R6 and the Edge Cases entry (line "If the host issues `AbortRound` during travel") whether the pre-P1 abort archives the log slice as `PreparingFailed` (a Preparing abort) or has no slice because the header (written at P3) does not yet exist.
- Where it lives: the Round Core phase machine (planned `Source/StealthGameplay/Private/Round/Phase/`) on `FRoundPhaseCore`, with the Session event subscriptions bound by the composition root through weak bindings (ADR-0005 section 2); the session-end handler is a thin handler on the store, not a phase.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024 (`production/epics/round-match-framework/story-024-start-ordering.md`): the Start call order and `Preparing` entry on travel acceptance.
- Story 058 (`production/epics/round-match-framework/story-058-reset-clears.md`): the `Reset` cleanup order and the Lobby exit.
- Story 034 (`production/epics/round-match-framework/story-034-preparing-aborts.md`): the 13 Preparing abort codes and the host abort within `Preparing` after P1.
- Story 057 (`production/epics/round-match-framework/story-057-host-abort.md`): the host abort in `Active`.
- Session internals (the quit notice, `ClientSessionEnding`, `ShutdownFlushSec`, host-loss detection on clients, `EndMatch` on every store) and Roster behaviour after `EndMatch` (session-travel and participant-state-roster epics).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/SessionEndAndAbortDuringTravelTest.cpp` (class `FRoundSessionEndAndAbortDuringTravelTest`, test name `StealthGame.Round.SessionEndAndAbortDuringTravel`)
- `Source/StealthGameplay/Private/Round/Phase/` session-end handler and pre-P1 abort mark (new)
- `Source/StealthGame/Private/StealthGameMode.cpp` (`TryScheduleRoundStart`, `AbortRound`; retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: OnSessionEnding stops processing without release, and a host abort before P1 resets at P1 with 0 pawns.
  - Given: each of the five phases.
  - When: `OnSessionEnding` fires; separately, the host aborts before P1.
  - Then: processing stops, `SessionEnded` is logged, nothing is released and no phase change occurs, in every phase.
  - Then: with the host abort before P1, `Reset` runs at P1 (or on Session's travel cancel) with 0 pawns spawned.
  - Edge cases: table-driven over the five phases; no further items resolve after `SessionEnded`.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/SessionEndAndAbortDuringTravelTest.cpp` — must exist and pass (automation test `StealthGame.Round.SessionEndAndAbortDuringTravel`, class `FRoundSessionEndAndAbortDuringTravelTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-024-start-ordering.md`, `production/epics/round-match-framework/story-058-reset-clears.md`
- Unlocks: None

Cross-epic: `production/epics/session-travel` (`OnSessionEnding`, travel cancel, `RequestLobbyTravel`; stories not yet created); `production/epics/participant-state-roster` (`EndMatch` clearing; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
