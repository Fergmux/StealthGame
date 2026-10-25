# Story 030: MatchScore, RoundsPlayedCount, host id, teams and archived slices persist across travel; RoundEpoch increments

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel); GDD AC 58; rules R21, R4, R5 (repeat index and `RoundEpoch`), R10.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies ADR-0005 section 2: `URoundMatchStore` / `FRoundPhaseCore` owns the match score, `RoundsPlayedCount` and the Event Log archives across travel, and the Roster store owns teams and `RoundEpoch` (incremented by `ResetAll`).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk; residual risk is behavioural, spikes S3 to S5)
**Engine Notes**: Touches ADR-0005 E14 (a `UGameInstanceSubsystem` lives as long as the `UGameInstance` and survives seamless and hard travel, [D] lifetime) and E5/E6 (GameMode, GameState, PlayerController and PlayerState are recreated or replaced). Verification item S3 (seamless and hard travel in multi-process standalone with the game-instance stores intact) is exactly what this test observes; E7 (seamless travel disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`) means a single-process PIE result must be labelled non-representative (ADR-0005 guideline 10). No post-cutoff API is used.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules, section Session and travel):
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005).
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState; never carry match state across travel on them (ADR-0005).
- Guardrail: Travel load is bounded by `TravelLoadTimeoutSec` (60 s) and Session bookkeeping stays inside the 0.5 ms line (ADR-0005).

**Prototype disposition**: Replaces the round counters and per-round fields held on `AStealthGameMode` / `AStealthGameState` in `Source/StealthGame` (the prototype keeps round state on the GameMode and GameState, which seamless travel recreates); the production data lives in the game-instance Round store (GDD R21 table).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 58, scoped to this story:*

- [ ] GIVEN two rounds, THEN `MatchScore`, `RoundsPlayedCount`, the host id, teams and archived slices persist across travel, while phase, `Seq` and `Terminated` reset and `RoundEpoch` increases by one per `ResetAll`.

---

## Implementation Notes

*Derived from ADR-0005 section 2 and GDD R21:*

- **What persists (GDD R21 table, scope "Match").** `ParticipantId`, `Team`, `DisplayName`, `IdentityKey` (Roster and Session, carried); `RoundsPlayedCount` (so `RoundIndex`), `MatchScore[2]` and the host's `ParticipantId` (Round, carried in the game-instance Round object); archived Event Log slices (server only, kept for the session); `RoundEpoch` (incremented by `ResetAll`).
- **What resets (scope "Round").** Phase, `ActivePhaseStart`, `ActiveAtServerTime`, the queue, `Seq`, `Terminated` and the terminal outcome are cleared in `Reset` (`LastResult` is kept until the next `Preparing`); grace timers, the hand-over table, re-admission retries and pawn-loss counts are cancelled in `Reset`.
- **`RoundEpoch` per `ResetAll`.** `ResetAll(RoundIndex)` is called once per accepted Start (R5 step 7) and increments `RoundEpoch`. After an aborted start or a rollback `RoundsPlayedCount` has not advanced, so the next start calls `ResetAll(N+1)` again with the same index and `RoundEpoch` advances again. `Reset` itself never calls `ResetAll`. Only the epoch is used for staleness.
- **Where the data lives.** `FRoundPhaseCore` (behind `URoundMatchStore`) holds the match score, `RoundsPlayedCount`, host id and Event Log archives; the Roster store holds teams and the epoch. Nothing authoritative is held on the GameMode, GameState, PlayerController or PlayerState; the GameState mirror is re-seeded in `InitGameState` (story 029).
- **Test shape.** Multi-client PIE (CQTest `NETWORK_TEST_CLASS`) with two rounds and a travel between them; capture the five persisted values before and after, and read phase, `Seq` and `Terminated` after. Run travel acceptance multi-process standalone where the harness allows; label a single-process PIE result non-representative (ADR-0005 guideline 10).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 029 (`production/epics/round-match-framework/story-029-public-state-replication.md`): `FRoundPublicState` handler, `Unknown` = 0, GameState re-seed and `LastResult` into the lobby (this story's dependency).
- Story 025 (`production/epics/round-match-framework/story-025-start-rollback.md`): rollback behaviour (a rolled-back Start has no `ResetAll`; asserted there, listed here only as an edge case).
- Story 058 (`production/epics/round-match-framework/story-058-reset-clears.md`): what `Reset` destroys and clears.
- Story 084 (`production/epics/round-match-framework/story-084-pie-lifecycle-start.md`): the full lifecycle PIE.
- Roster internals (`ResetAll`, team data) and Session internals (seamless travel, admission): owned by their epics.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Phase/RoundMatchStore.h`
- `Source/StealthGameplay/Private/Round/Phase/RoundMatchStore.cpp`
- `Source/StealthTestHarness/Private/Round/MatchScopeSurvivesTravelTest.cpp` (CQTest `NETWORK_TEST_CLASS(MatchScopeSurvivesTravel, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.MatchScopeSurvivesTravel.<Method>`; the GDD scenario name is `StealthGame.Round.MatchScopeSurvivesTravel`)
- `Source/StealthGame/Private/StealthGameMode.cpp`, `Source/StealthGame/Private/StealthGameState.cpp` (prototype round state on GameMode/GameState: retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: MatchScore, RoundsPlayedCount, host id, teams and archived slices persist across travel, and RoundEpoch increments.
  - Given: a 2-client PIE session with two rounds.
  - When: the session travels between rounds.
  - Then: `MatchScore`, `RoundsPlayedCount`, host id, teams and archived slices are unchanged across the travel.
  - Then: phase, `Seq` and `Terminated` reset.
  - Then: `RoundEpoch` increases by exactly one per `ResetAll`.
  - Edge cases: a rolled-back Start (no `ResetAll`) does not bump the epoch.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/MatchScopeSurvivesTravelTest.cpp` (CQTest `NETWORK_TEST_CLASS(MatchScopeSurvivesTravel, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-029-public-state-replication.md`
- Unlocks: `production/epics/round-match-framework/story-084-pie-lifecycle-start.md`
- Cross-epic: `production/epics/session-travel` (seamless travel and game-instance stores; stories not yet created), `production/epics/participant-state-roster` (teams, `ResetAll`, `RoundEpoch`; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
