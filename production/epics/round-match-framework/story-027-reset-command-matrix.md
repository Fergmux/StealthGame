# Story 027: stealth.round.reset/abort and the other host commands behave per phase and exist only on the game instance

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-010` (Host commands on the game instance with a net-mode check); GDD AC 59 plus the `dumplog` half of AC 60 (revised, see Acceptance Criteria); rules R22, R8, R9, R10, R19 (debug commands and `Seq`), States and Transitions.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies ADR-0005 section 8 (Host commands, Round R22): `stealth.round.*` commands are registered with `IConsoleManager` by `URoundMatchStore` on the game instance, each checking net mode (listen server or standalone), a local controller and `ParticipantId == HostId`, and never on a PlayerController, pawn, HUD or cheat manager; debug-only commands are compiled out of Shipping.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk; the residual risk is behavioural, spikes S3 to S5, not API names)
**Engine Notes**: Touches only `IConsoleManager` registration on a game-instance subsystem. No post-cutoff API is used and none of the ADR-0005 verification items (S3 seamless travel, S4 reconnect, S5 listen-host loss) is touched, because this story is a plain-C++ unit/Functional test against fakes. The engine fact it relies on is Round R22's "development builds route a client's `ServerExecRPC` through the PlayerController, pawn, HUD and cheat manager [V]"; the by-console and by-`ServerExecRPC` client attempts are story 026 (PIE).

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, section Session and travel, because the ADR-0005 stores are consumed by Core):
- Required: Host commands are registered on the game instance with the net-mode/host check (ADR-0005). Debug commands such as `stealth.round.dumplog` are `#if !UE_BUILD_SHIPPING` and refuse unless the phase is `Ended`/`Reset` (ADR-0007).
- Forbidden: Never register host commands on a PlayerController, pawn, HUD or cheat manager (ADR-0005).
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms listen-host game thread (ADR-0002); debug commands submit through the real queue and must stay inside it.

**Prototype disposition**: Replaces the prototype dev console commands `StealthStartRound`, `StealthEndRound` and `StealthSwitchTeam` (registered on `AStealthPlayerController`, `Source/StealthGame/Private/StealthPlayerController.cpp`, and calling into `StealthGameMode`). They survive only as host commands on the game instance behind the net-mode check (TR-round-010); the prototype placement on the PlayerController is the pattern ADR-0005 forbids.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 59, amended by the revised ACs (`dumplog` half of AC 60), scoped to this story:*

- [ ] GIVEN each phase, THEN `stealth.round.reset` is a no-op in `Lobby` and `Reset`, aborts in `Preparing`, depends on the flag in `Active` and resets in `Ended` (held until `EndedMinDwellSec`); `abort` behaves as its alias; `swapteam`, `remove` and `preset` return `WrongPhase` outside Lobby; a static scan finds the commands only on the game instance; debug commands consume a `Seq`, carry `Debug` and are excluded from F7; AND `stealth.round.dumplog` is refused in `Lobby`, `Preparing` and `Active` and allowed in `Ended` and `Reset` (the refusal code name is a GDD item to confirm).

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines (item 11) and section 8, and GDD R22:*

- **One entry point per command, shared by console and host panel.** `URoundMatchStore` (the `UGameInstanceSubsystem` facade over `FRoundPhaseCore`, ADR-0005 section 2) registers `stealth.round.start`, `reset`, `abort`, `swapteam <participant>`, `remove <participant>`, `preset <name>`, and (development builds only) `debugwin <team>`, `debugeliminate <participant>`, `dumplog`. Each console handler and the optional host panel call the same host-checked function, which verifies net mode (listen server or standalone), a local controller, and `ParticipantId == HostId`. A non-host caller gets `NotHost` and no effect (the client-side attempts are story 026).
- **Phase matrix for `reset` (GDD R22, R8, R9, States and Transitions).** `Lobby` and `Reset`: no-op. `Preparing`: abort (R8; the abort cleanup itself is story 034). `Active`: only if `bHostMayAbortActive`, else `WrongPhase` (the abort effects are story 057). `Ended`: legal, but held until `EndedMinDwellSec` (3 s) after the round-end step and then executed as R10 (the Reset cleanup is story 058). `abort` is an alias of the abort forms. This story asserts the entry point's return value and the phase action it requests, using fakes; it does not re-test the cleanup owned by those stories.
- **Lobby-only commands.** `swapteam`, `remove` and `preset` return `WrongPhase` in every phase other than `Lobby` (Preparing, Active, Ended, Reset). The Lobby behaviour of swap, remove and preset is stories 020 and 021.
- **Held reset in `Ended`.** The held `reset` is parked (R9 (v) and R9 last paragraph: an earlier `ResetRound` is held and executed at the dwell's end). Drive it on the fake `IClock` (story 001) so the timeline shows hold-then-execute.
- **Debug commands go through the real queue.** `debugwin <team>` submits a `DebugObjectiveCompletion` and `debugeliminate <participant>` a `DebugRequestTransition(Informant, Assassinated)` as `DebugItem` queue items (`FDebugPayload { uint8 Command; FSubjectRef Subject; uint32 Arg; }`, ADR-0002 payloads in `Public/Server/`), so each consumes a `Seq`, is flagged `Debug` in the Event Log, and is excluded from the F7 pacing metrics. Producers and resolver are compiled out of Shipping (`#if !UE_BUILD_SHIPPING`); a Shipping `DebugItem` is dropped with a fault.
- **`dumplog`.** Compiled out of Shipping and refuses unless the phase is `Ended` or `Reset` (ADR-0007). The refusal produces no log output. Open: confirm against GDD R19 and the Edge Cases section the name of the `dumplog` refusal code.
- **Static scan.** The [STATIC] check scans `Source/` for `IConsoleManager` registrations of `stealth.round.*` names and confirms each sits only in the game-instance store's registration code, never in a PlayerController, pawn, HUD or cheat-manager file.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024 (`production/epics/round-match-framework/story-024-start-ordering.md`): `stealth.round.start` effects and ordering.
- Story 026 (`production/epics/round-match-framework/story-026-host-only-commands.md`): a non-host client running the commands by console and `ServerExecRPC` (PIE).
- Story 034 (`production/epics/round-match-framework/story-034-preparing-aborts.md`): the Preparing abort cleanup and the 13 abort codes.
- Story 057 (`production/epics/round-match-framework/story-057-host-abort.md`): host abort in Active (`Reset` with `EndReason = Aborted`).
- Story 058 (`production/epics/round-match-framework/story-058-reset-clears.md`): what `Reset` destroys and clears.
- Story 020/021: the Lobby behaviour of `swapteam`, `remove`, `preset`.
- Story 011 and 014: Event Log content and log leakage (the `dumplog` read API is the Foundation half).
- Objectives and Assassination (AC 2 [DEFERRED]): the real objective and assassination items that `debugwin` and `debugeliminate` stand in for.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Commands/RoundHostCommands.h`
- `Source/StealthGameplay/Private/Round/Commands/RoundHostCommands.cpp`
- `Source/StealthGameplay/Private/Tests/Round/ResetCommandMatrixTest.cpp` (class `FRoundResetCommandMatrixTest`, test name `StealthGame.Round.ResetCommandMatrix`)
- `Source/StealthGame/Private/StealthPlayerController.cpp` and `Source/StealthGame/Public/StealthPlayerController.h` (prototype `StealthStartRound`, `StealthEndRound`, `StealthSwitchTeam`: retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: The host commands behave per phase and exist only on the game instance, including the `dumplog` phase rule.
  - Given: the host in each phase, with `bHostMayAbortActive` set per case.
  - When: `reset`, `abort`, `swapteam`, `remove`, `preset`, the debug commands and `dumplog` are run.
  - Then: `reset` is a no-op in `Lobby` and `Reset`, aborts in `Preparing`, depends on the flag in `Active`, and resets in `Ended` held until `EndedMinDwellSec`.
  - Then: `abort` behaves as its alias.
  - Then: `swapteam`, `remove` and `preset` return `WrongPhase` outside Lobby.
  - Then: `dumplog` is refused in `Lobby`, `Preparing` and `Active`, and allowed in `Ended` and `Reset`.
  - Then: a [STATIC] scan finds the commands registered only on the game instance.
  - Then: debug commands consume a `Seq`, carry `Debug` and are excluded from F7.
  - Edge cases: Open: confirm against GDD R19 and Edge Cases the `dumplog` refusal code name (the AC does not name it); no log output is produced on a refusal; the Ended-dwell `reset` is held and then executed, which the fake clock timeline shows.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/ResetCommandMatrixTest.cpp` (automation test `StealthGame.Round.ResetCommandMatrix`, class `FRoundResetCommandMatrixTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-024-start-ordering.md`, `production/epics/round-match-framework/story-034-preparing-aborts.md`
- Unlocks: None
- Cross-epic: `production/epics/session-travel` (`RequestLobbyTravel`, admission and travel cancel via the fake Session gateway; stories not yet created), `production/epics/participant-state-roster` (`SwapTeam`, `RemoveParticipant` via the fake Roster gateway; stories not yet created), `production/epics/tuning-data-config` (module skeleton, StealthTestHarness bootstrap and `stealth.tuning.preset`; stories not yet created)
