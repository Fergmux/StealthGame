# Story 026: A non-host client cannot run any of the eight stealth.round commands by console or ServerExecRPC

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-010` (Host commands on the game instance with a net-mode check). Also `GDD AC 5`, `R22` (Start and reset: host-only commands on the game instance), `R2` (no client-to-server RPC added by Round), `R5` check 1 (`NotHost`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 8: host commands (`stealth.round.*`, `stealth.tuning.preset`) are registered with `IConsoleManager` by `URoundMatchStore` / `UStealthTuningStore` on the game instance, each checking net mode (listen server or standalone), a local controller and `ParticipantId == HostId`; never on the PlayerController, pawn, HUD or cheat manager, because the development `ServerExecRPC` routes a client's exec through those. Debug-only commands are compiled out of Shipping.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. This is a multi-client PIE test of console and exec routing (`IConsoleManager` registration, the development `ServerExecRPC` path, a verified fact per R22 "V"); the multi-process seamless-travel spikes S3, S4, S5 are not touched. Per ADR-0005, a PIE result is labelled non-representative for travel; this test does not travel.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Session and travel and Forbidden; the Core Layer Rules section holds no Round-specific rule):
- Required: Host commands are registered on the game instance with the net-mode/host check (ADR-0005).
- Forbidden: Never register host commands on a PlayerController, pawn, HUD or cheat manager (ADR-0005).
- Guardrail: Tests: per-commit pipeline at most 10 min; nightly cook + T3 estimated 1-2 h [U] (ADR-0008); T2 tests are bound by frames or server time and record the active replication system.

**Prototype disposition**: REPLACES the prototype's client-routed dev commands `StealthStartRound`, `StealthEndRound` and `StealthSwitchTeam` on `AStealthPlayerController` (`Source/StealthGame/Private/StealthPlayerController.cpp`, each calling a `Server*` RPC on the controller), which is exactly the surface ADR-0005 section 8 forbids. They survive only as the host commands behind the net-mode check (TR-round-010).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 5, scoped to this story:*

- [ ] GIVEN a non-host client, WHEN it invokes each of the eight `stealth.round.*` commands by client console and by a development `ServerExecRPC`, THEN each returns `NotHost` or is unreachable, and a hash of (phase, teams, `RoundsPlayedCount`, log entry count, `Seq`) is unchanged.

---

## Implementation Notes

*Derived from GDD R22 and R2 and ADR-0005 section 8:*

- Command set (R22): `stealth.round.start` (R5), `stealth.round.reset`, `stealth.round.abort` (alias of the abort forms), `stealth.round.swapteam <participant>`, `stealth.round.remove <participant>`, `stealth.round.preset <name>`; and, in development builds, `stealth.round.debugwin <team>` and `stealth.round.debugeliminate <participant>`. That is eight commands. `stealth.round.dumplog` (R19, `#if !UE_BUILD_SHIPPING`) is a ninth and AC 59 names it with these. Open: confirm against GDD R22 and AC 59 whether `dumplog` belongs to "the eight"; freeze the list as a fixture constant (story 005) and include `dumplog` if it is host-gated by the same check.
- Registration: each command is registered with `IConsoleManager` by `URoundMatchStore` on the game instance and calls one entry point (the same one the optional host panel uses) that checks net mode (listen server or standalone), a local controller and `ParticipantId == HostId`. A non-host caller gets `NotHost` and no effect. Round adds no client-to-server RPC (R2). Development builds route a client's `ServerExecRPC` through the PlayerController, pawn, HUD or cheat manager, so no command may live on those objects: a client exec then finds no such command (unreachable) or reaches the game-instance command and gets `NotHost`.
- Test shape (T2, multi-client PIE in `StealthTestHarness`): a 2-client session in a known phase (listen host plus one client), record the state hash of (phase, teams, `RoundsPlayedCount`, log entry count, `Seq`); the non-host client invokes each command by client console and by a development `ServerExecRPC`; assert each returns `NotHost` or is unreachable and that the hash is unchanged after every call. Repeat in each phase (the AC does not pin one phase; Open: confirm against GDD R22 and the phase table which phases are reachable in the PIE fixture, since Preparing and Ended need the Core stories 031 to 055 to be built for the PIE run to reach them; until then the Lobby phase and an Active-with-stubs phase suffice).
- Test class declaration follows the CQTest exemption in `.claude/rules/test-standards.md`: `NETWORK_TEST_CLASS(HostOnlyCommands, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.HostOnlyCommands.<Method>`; the scenario name stays `HostOnlyCommands` and the GDD test name `StealthGame.Round.HostOnlyCommands` maps to it. The test sets `WithGameMode(AStealthGameMode)` and loads its fixture map (ADR-0008 T2), is bounded by frames or server time and records the active replication system.
- The behaviour of each command per phase (reset in Preparing/Active/Ended, dumplog matrix) is story 027; this story proves only who may call.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 022 (`production/epics/round-match-framework/story-022-start-validity.md`) and story 024 (`production/epics/round-match-framework/story-024-start-ordering.md`): what Start does for the host (validity, ordering).
- Story 027 (`production/epics/round-match-framework/story-027-reset-command-matrix.md`): reset/abort/swapteam/remove/preset behaviour per phase, the `dumplog` refusal matrix, and the static scan that commands exist only on the game instance.
- `stealth.tuning.preset` registration (epic `tuning-data-config`) and the host panel (HUD epic).
- Session internals (host identity, `HostId` derivation): epic `session-travel`.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- Source/StealthGameplay/Public/Server/Core/Round/StealthRoundMatchStore.h, Private/Core/Round/StealthRoundMatchStore.cpp (new: URoundMatchStore, console registration, host check, FRoundRemoteExecScope)
- Source/StealthGame/Public/StealthPlayerController.h, Private/StealthPlayerController.cpp (StealthStartRound/StealthEndRound/StealthSwitchTeam and Server RPCs retired; ConsoleCommand marks remote ServerExec)
- Source/StealthTestHarness/Private/Tests/PIE/Round/HostOnlyCommandsTest.cpp (new, CQTest PIE)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A non-host client cannot run any of the eight `stealth.round.*` commands, by console or by `ServerExecRPC`.
  - Given: a 2-client PIE session in a known phase, and the state hash (phase, teams, `RoundsPlayedCount`, log entry count, `Seq`).
  - When: the non-host client invokes each command by client console and by a development `ServerExecRPC`.
  - Then: each returns `NotHost` or is unreachable.
  - Then: the state hash is unchanged after every call.
  - Edge cases: the AC says "eight" but does not list them (AC 59 names start, reset, abort, swapteam, remove, preset, debugwin, debugeliminate and dumplog, which is nine), so freeze the list as a fixture constant (Open: confirm against GDD R22 and AC 59); repeat in each phase (the AC does not pin one).

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/HostOnlyCommandsTest.cpp` — must exist and pass (CQTest network test `StealthGame.Round.PIE.HostOnlyCommands.<Method>`; GDD scenario name `StealthGame.Round.HostOnlyCommands`)

**Status**: [x] Created and passing (`StealthGame.Round.PIE.HostOnlyCommands.NonHostCommandsAreNotHostInLobbyAndActive`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-022-start-validity.md`, `production/epics/round-match-framework/story-024-start-ordering.md`
- Unlocks: None

Cross-epic: `production/epics/session-travel` (host identity, game-instance stores; stories not yet created); `production/epics/participant-state-roster` (participant ids and teams in the state hash; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 1/1 passing (Lobby and Active)
**Deviations**: The nine-name list (dumplog included) is frozen in RoundHostCommandNames. Hash fields: the composition root has no Event Log or queue yet, so 'log entry count' and 'Seq' are the Roster EventSeq and the Round host-message count. Preparing and Ended are not dwelt in by the PIE run (Core stories 031-055).
**Test Evidence**: `Source/StealthTestHarness/Private/Tests/PIE/Round/HostOnlyCommandsTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: N/A: refusals are invisible by design; the PIE test is the observation
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
