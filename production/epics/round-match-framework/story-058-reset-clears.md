# Story 058: Reset destroys player pawns before lobby travel, empties the timer heap and reaches Lobby only when all participants are terminal or timed out

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel). Also `GDD AC 13a`, `R10` (Reset, steps 1-7), `R21` (what survives and what resets), `States and Transitions` (Reset to Lobby).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies the Round store's lifecycle (`URoundMatchStore` / `FRoundPhaseCore` keeps the phase machine, match score, `RoundsPlayedCount` and archived log slices across travel), the admission modes (`Closed` during `Reset`, `Open` in `Lobby`) and the bounded waits (`TravelLoadTimeoutSec`, rule "every wait is bounded by a server-measured clock"). `Reset` never calls `ResetAll`; that runs at the next start.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. The lobby travel is a spied Session call, so spikes S3 (seamless and hard travel, including the destination-map arrival of a lobby travel) and S4 and S5 are not touched; the PIE reset-to-lobby result is story 085's. ADR-0010's engine-gate override of `APawn::Reset` (never destroys a Shared Pawn) is relevant to the Forbidden rule below but is not exercised: the test uses fake pawns and a fake pawn-destroy gateway.

**Control Manifest Rules (this layer)** (the Round-relevant rule sits in the manifest's Foundation Layer Rules, Session and travel; the Forbidden rule is from the Core Layer Rules, Shared Pawn; the Core Layer Rules hold no other Round-specific rule):
- Required: Every wait is bounded by a server-measured clock (ADR-0005); `Reset` waits for terminal participants only up to `TravelLoadTimeoutSec`.
- Forbidden: Never destroy a pawn inside a resolver except via `RequestTeardown`; never destroy a Shared Pawn in `Reset` (ADR-0010). Open: confirm against ADR-0010 section "Other engine destroy paths" that this line means the engine's `APawn::Reset`/`ResetLevel` path (ADR-0010: "`Reset` does nothing to a Shared Pawn (rounds reset by travel, never by `ResetLevel`)") and not Round's R10 step 4, which AC 13a requires to destroy the player pawns; the story must use the factory/teardown path, never `APawn::Reset`.
- Guardrail: Session: travel load bounded by `TravelLoadTimeoutSec` (60 s) (ADR-0005).

**Prototype disposition**: REPLACES `AStealthGameMode::CleanupRoundActors` (`Source/StealthGame/Private/StealthGameMode.cpp`, line about 352) and the auto-restart in `EndRound` (`RoundRestartTimer`, line about 349), which clean up and restart in the same world with no phase, no admission mode and no lobby travel. Retired by this story and story 052.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 13a, scoped to this story:*

- [ ] GIVEN `Reset`, THEN all player pawns are destroyed before `RequestLobbyTravel`, the Round timer heap is empty, admission went `Closed`, and the phase becomes `Lobby` (admission `Open`) only when every participant is terminal or `TravelLoadTimeoutSec` has passed.

---

## Implementation Notes

*Derived from GDD R10 and R21, and ADR-0005 sections 2 and 4:*

- R10 order, each step asserted by a call-order spy: (1) admission `Closed` (`SetAdmissionMode(Closed)`); (2) cancel every Round timer and invalidate `RoundEpoch`-stamped work (the `ResetAll` of the next start increments the epoch; stale timer items return `Rejected_StaleEpoch`, story 010); (3) archive the Event Log slice server-side (match-scoped, `RoundLogRetainCount`) and clear the round log buffer; (4) destroy or release every player pawn of the round (Roster R9 precondition for the next `ResetAll`): destroy by default, and `ReleaseToCrowd` is not used because the world is about to be torn down; (5) clear round-scoped data (R21: phase fields, `ActivePhaseStart`, `ActiveAtServerTime`, the queue, `Seq`, `Terminated`, the terminal outcome; grace timers, the hand-over table, informant re-admission retries, pawn-loss counts and retries); (6) `RequestLobbyTravel()`; (7) when Session reports every participant terminal (`TravelComplete` or `Disconnected`, bounded by `TravelLoadTimeoutSec`), set phase `Lobby`, `SetTeamsLocked(false)` and admission `Open`.
- The AC's assertions map to: pawn destruction (4) strictly before `RequestLobbyTravel` (6) in the spy order; the Round timer heap holds no items after step (2) (assert the heap's count is 0, including grace and pawn-loss retry items from a seeded Active state); admission `Closed` pushed at (1); and the Lobby exit (7) gated on terminal participants or the timeout.
- Two exit paths, one test each: (a) participants become terminal one by one on the fake Session (`TravelComplete` or `Disconnected`), and the phase turns `Lobby` only at the last one; (b) with at least one participant never terminal, the fake clock passes `TravelLoadTimeoutSec` and the phase turns `Lobby` at that time. In both, one tick before the exit the phase is still `Reset` (QA edge case). `Reset` therefore never waits on anything unbounded (R10).
- Pawn destruction path: R10 says destroy; ADR-0002 section 7 says resolvers never destroy actors inline (`RequestTeardown`, run after the staging pass). The `Reset` cleanup is not inside a drain slot (the queue is closed or already empty), so it needs a destroy path that is safe outside the drain and consistent with ADR-0010's rule that the pawn factory and teardown own pawn lifetime. Open: confirm against ADR-0002 section 7 and ADR-0010 which gateway the Round core calls to destroy a bound pawn at `Reset` (a factory destroy method or `RequestTeardown` with a post-drain pass); the fake gateway records the call either way.
- Round does not call `ResetAll` here: it runs at the next `StartRound` (R5 step 7, story 024); the test asserts the spy sees no `ResetAll` during `Reset` (also asserted by story 025).
- The Event Log slice archived at (3) is never released (R8, R19). `LastResult` is kept in `FRoundPublicState` through `Reset` and re-seeded on the lobby GameState (R9, story 029).
- Where it lives: the `Reset` cleanup in the Round Core phase machine (planned `Source/StealthGameplay/Private/Round/Phase/`) on `FRoundPhaseCore` in `URoundMatchStore`, using the injected Session, Roster, pawn-destroy and timer-heap gateways (story 003).
- Manifest 2026-10-25f: `ResetAll(RoundIndex)` is batch-only, and ping parts and the per-connection spawn gate are cleared at `OnRosterReset` and round-travel start (ping parts through `ITeamPingReset`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010 (`production/epics/round-match-framework/story-010-stale-epoch-item.md`): an item stamped with the previous `RoundEpoch` returns `Rejected_StaleEpoch` when drained.
- Story 057 (`production/epics/round-match-framework/story-057-host-abort.md`): the Active abort that enters `Reset`; story 034: the Preparing abort codes.
- Story 059 (`production/epics/round-match-framework/story-059-session-end-and-abort-during-travel.md`): `OnSessionEnding` and a host abort before P1.
- Story 085 (`production/epics/round-match-framework/story-085-pie-lifecycle-objective-and-reset.md`): the real PIE reset returning both clients to `Lobby` with teams unchanged.
- Story 027 (`production/epics/round-match-framework/story-027-reset-command-matrix.md`): the `stealth.round.reset` command per phase and the `EndedMinDwellSec` hold.
- Session's lobby travel, seamless arrival and terminal-state reporting (session-travel epic); Roster `ResetAll` and `EndRoundAll` internals (participant-state-roster epic); feature-state resets on `OnRosterReset` (owner epics); Character Pool ledger discard and TDC `Idle` (their epics).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/ResetClearsTest.cpp` (class `FRoundResetClearsTest`, test name `StealthGame.Round.ResetClears`)
- `Source/StealthGameplay/Private/Round/Phase/` `Reset` cleanup sequence (new)
- `Source/StealthGame/Private/StealthGameMode.cpp` and `Source/StealthGame/Public/StealthGameMode.h` (`CleanupRoundActors`, auto-restart timers; retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Reset destroys player pawns before lobby travel, empties the timer heap, and reaches Lobby only when all participants are terminal or timed out.
  - Given: a `Reset` with a call-order spy and the fake clock.
  - When: the reset runs, then participants become terminal one by one, then separately `TravelLoadTimeoutSec` passes.
  - Then: all player pawns are destroyed before `RequestLobbyTravel`.
  - Then: the Round timer heap is empty and admission went `Closed`.
  - Then: the phase becomes `Lobby` (admission `Open`) only when every participant is terminal or `TravelLoadTimeoutSec` has passed.
  - Edge cases: both exit paths; the phase is not `Lobby` one tick before either.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/ResetClearsTest.cpp` — must exist and pass (automation test `StealthGame.Round.ResetClears`, class `FRoundResetClearsTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-010-stale-epoch-item.md`, `production/epics/round-match-framework/story-042-active-gate.md`
- Unlocks: `production/epics/round-match-framework/story-059-session-end-and-abort-during-travel.md`, `production/epics/round-match-framework/story-085-pie-lifecycle-objective-and-reset.md`

Cross-epic: `production/epics/session-travel` (`RequestLobbyTravel`, admission mode, participant terminal reporting, `TravelLoadTimeoutSec`; stories not yet created); `production/epics/participant-state-roster` (`SetTeamsLocked`, pawn bindings, `ResetAll` precondition; stories not yet created); `production/epics/tuning-data-config` (`TravelLoadTimeoutSec`, module skeleton and StealthTestHarness bootstrap; stories not yet created)
