# Story 025: A Session refusal at RequestRoundTravel rolls admission, teams and phase back and logs StartRolledBack

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). Also `GDD AC 6b`, `R5` (Rollback, repeat index), `R10` (Reset never calls `ResetAll`), `R19` (`StartRolledBack` entry).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; also ADR-0006: Seeds and deterministic/secret random streams
**ADR Decision Summary**: ADR-0005: four game-instance stores as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, admission modes, host commands on the game instance; `RequestRoundTravel` and `CanRequestRoundTravel()` return `ETravelRefusal` with no side effect on refusal. ADR-0006: a 128-bit `ServerSecret`, in-project SipHash-2-4, and `DrawRoundSeed(RoundIndex)` on `USessionService` (redraw on used seeds), so every start draws a fresh public seed and tests inject entropy through `IEntropySource`.
**ADR Version**: ADR-0005 2026-10-24; ADR-0006 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (governing ADR-0005; ADR-0006 is LOW)
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched; spikes S3, S4, S5 are not touched (fake Session refuses the travel). ADR-0006 depends on no engine API beyond integer arithmetic and one platform entropy call; the test uses the fixed secret and fake clock seam (stories 001 and 002), so the OS entropy source (`BCryptGenRandom`) is not used and the golden-vector verification belongs to the seeds epic.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Seeds and Forbidden; the Core Layer Rules section holds no Round-specific rule):
- Required: Tests inject entropy through `IEntropySource` (ADR-0006); here the fixed `ServerSecret` of story 002 makes the different-seed assertion deterministic.
- Forbidden: Never log, trace, replicate, put in a URL or expose to Blueprint `ServerSecret`, `MatchNonce`, server sub-seeds or stream keys (ADR-0006); the rollback log entry holds no seed material.
- Guardrail: Session: travel load bounded by `TravelLoadTimeoutSec` (60 s) (ADR-0005); the rollback path adds no wait.

**Prototype disposition**: none (new Core code). The prototype has no travel step to refuse and no rollback (`StartRound` in `StealthGameMode.cpp` is replaced by story 024).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 6b, scoped to this story:*

- [ ] GIVEN Session refuses at `RequestRoundTravel`, THEN admission is `Open`, teams are unlocked, phase is `Lobby`, `StartRolledBack` is logged and the host sees `SessionRefused`; GIVEN `CanRequestRoundTravel()` false, THEN no call of 6a happens. Two consecutive starts draw different seeds. `Reset` never calls `ResetAll`.

---

## Implementation Notes

*Derived from GDD R5 and R10 and ADR-0005 section 4:*

- Rollback rule (R5): "Rollback if Session still refuses at step 8: admission `Open`, `SetTeamsLocked(false)`, phase stays `Lobby`, log `StartRolledBack`, report `FStartRefusal(SessionRefused)`. The reset round scope is left as it is: `Active` in Lobby means only 'will compete' (Roster R7), I4 and F4 apply only in phase `Active`, and the next start calls `ResetAll` again." So the rollback restores admission `Open` and unlocks teams, but does not undo `ResetAll`.
- Pre-check (R5 step 5b): `CanRequestRoundTravel()` is a Session query with no side effects; `false` refuses with `SessionRefused` before anything changes, so none of the four calls of story 024 (`SetAdmissionMode`, `SetTeamsLocked`, `ResetAll`, `RequestRoundTravel`) happens. The test uses a fake Session returning a refusal for the query, and a second fake that accepts the query but refuses `RequestRoundTravel` (an `ETravelRefusal` other than `Ok`).
- Log entry: `StartRolledBack` is an Event Log entry type (R19, ADR-0002 section 8.9), appended through `IRoundLogWriter` (story 011) and never through `UE_LOGF`. Open: confirm against GDD R19 whether a Lobby-phase entry is stored in a round log that does not exist yet (the log header is written at P3, in Preparing), and where a Lobby-time `StartRolledBack` is held; the AC requires only that it is logged.
- Seeds: Session draws a fresh seed for every start and an aborted start never reuses one (R5 step 8; ADR-0006 `DrawRoundSeed(RoundIndex)` redraws on used seeds). The test, using the fixed secret (story 002) and fake clock (story 001), starts twice and asserts two different seeds, deterministically. Round never draws or stores the seed itself.
- Repeat index (R5): a refusal leaves `RoundsPlayedCount` unchanged, so the next Start calls `ResetAll(N+1)` again with the same index (assert: `RoundsPlayedCount` unchanged and a second start records `ResetAll` with the same index).
- `Reset` never calls `ResetAll` (R10: "Round does not call `ResetAll` here; it runs at the next `StartRound`"); the test runs a Reset on the fake gateways and asserts zero `ResetAll` calls. The Reset cleanup itself (pawns, timers, travel) is story 058.
- The refusal reaches the host sink as `FStartRefusal(SessionRefused)` only, never a client.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024 (`production/epics/round-match-framework/story-024-start-ordering.md`): the accepted-Start call order.
- Story 022 (`production/epics/round-match-framework/story-022-start-validity.md`): the team-layout refusal codes.
- Story 058 (`production/epics/round-match-framework/story-058-reset-clears.md`) and story 059 (`production/epics/round-match-framework/story-059-session-end-and-abort-during-travel.md`): Reset cleanup and abort during travel.
- Story 011 (`production/epics/round-match-framework/story-011-event-log-content.md`): the Event Log store itself.
- Session internals (seed draw `DrawRoundSeed`, `ETravelRefusal` production, travel) and Roster `ResetAll` repeatability: epics `session-travel` and `participant-state-roster`.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/StartRollbackTest.cpp` (class `FRoundStartRollbackTest`, test name `StealthGame.Round.StartRollback`)
- `Source/StealthGameplay/Private/Round/Start/` Start-orchestration implementation from story 024 (rollback branch; no new header expected)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A Session refusal at `RequestRoundTravel` rolls admission, teams and phase back and logs `StartRolledBack`.
  - Given: a fake Session that refuses `RequestRoundTravel`.
  - When: Start is issued.
  - Then: admission is `Open`, teams are unlocked and the phase is `Lobby`.
  - Then: `StartRolledBack` is logged and the host sink receives `SessionRefused`.
  - Then: with `CanRequestRoundTravel()` false, none of the 6a calls happen.
  - Then: two consecutive starts draw different seeds.
  - Then: `Reset` never calls `ResetAll`.
  - Edge cases: the different-seed assertion uses the fixed secret and fake clock, so it is deterministic; a refusal leaves `RoundsPlayedCount` unchanged. Open: confirm against GDD R19 where a Lobby-time `StartRolledBack` entry is held.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/StartRollbackTest.cpp` — must exist and pass (automation test `StealthGame.Round.StartRollback`, class `FRoundStartRollbackTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-024-start-ordering.md`
- Unlocks: None

Cross-epic: `production/epics/session-travel` (`ETravelRefusal`, `DrawRoundSeed`, `CanRequestRoundTravel`; stories not yet created); `production/epics/participant-state-roster` (`SetTeamsLocked`, `ResetAll`; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
