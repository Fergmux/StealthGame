# Story 076: A third pawn loss or no eligible socket in time gives Void(PawnLost), and no recovery runs in Ended or Reset

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void; pawn-loss recovery of R17.9); GDD AC 54b; rules R17.9 (cap `PawnLossRecoveryMaxCount`, retry bound `LateJoinTakeoverTimeoutSec`), R8 (void path to `Reset`), R16 (4) (the pawn-loss retry is one of the only Round timers).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: One `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator and possessor, kind-blind possession and hand-over pass, disconnect never destroys a pawn. This story applies the failure side of Round's own-card respawn: a pawn loss that cannot be recovered is a systemic fault, never punished as a decision, so the round ends `Void(PawnLost)` (ADR-0002 section 8.7 (5), "`Void(PawnLost)` follows the R17.9 pawn-loss recovery").
**ADR Version**: ADR-0010 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 knowledge risk)
**Engine Notes**: Same as story 075: the real respawn spawns a CMC pawn inside `TG_PostPhysics`, a [U] spike item (ADR-0010 verification, ADR-0002 E4/E14), and destruction is always deferred through `RequestTeardown`. The unit test uses fake Level Socket and Crowd gateways and a fake clock, so none of it is touched; no post-cutoff API is used.

**Control Manifest Rules (this layer)** (Round policy rules sit in the Foundation Layer Rules; pawn rules in the Core Layer Rules):
- Required: A void is `TryTerminate` plus `Latch` in the same call, stages no `RoundEndStep`, goes `Active -> Reset` with `bCalledOff`, no winner and no score; pawn loss is `Void(PawnLost)` (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never use `FTimerManager`, a feature tick or a latent action for a gameplay timer (ADR-0002); the retry is a `PawnLossRetry` item in the due-time heap.
- Guardrail: Layout (server, `Active`): runtime services at most 0.1 ms average and 0.5 ms in a respawn-pick frame, at most 1 pick per frame (ADR-0014); one retry per second respects it.

**Prototype disposition**: none (new Round Core code).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 54b, scoped to this story:*

- [ ] GIVEN a third pawn loss for the same participant in one round, or no eligible socket within `LateJoinTakeoverTimeoutSec`, THEN `Void(PawnLost)`, no score. GIVEN a pawnless `Disconnected` participant, THEN it is still respawned AI-driven and converts only by grace. No recovery runs in `Ended` or `Reset`.

---

## Implementation Notes

*Derived from GDD R17.9 and ADR-0002 section 8.7 (5) and (6):*

- **Cap.** `PawnLossRecoveryMaxCount` (2) own-card respawns per participant per round. Losses one and two recover (story 075); the third loss in the same round calls `TryTerminate(Void(PawnLost), key)`. The per-participant, per-round count is round-scoped data cleared in `Reset` (R21).
- **Retry and timeout.** If no emergence socket is eligible, Round retries once per second, as `TimerFired(PawnLossRetry)` items in the due-time heap, for up to `LateJoinTakeoverTimeoutSec` (30 by default, Round's only use of that key; R17.9 and ADR-0002 section 8.7 (5)). The participant is pawnless meanwhile and is not put in the engine spectator state. When the bound passes with no socket, `Void(PawnLost)`.
- **Void effects.** A void is `TryTerminate(Void(code), key)` plus `Latch` in the same call, with `code` in {`Abandoned`, `KickedLastActive`, `PawnLost`}: it stages no `RoundEndStep` and runs no round-end step; `Active -> Reset` directly with `EndReason = Void(PawnLost)`, no winner, no score, `bCalledOff` set, the log archived and never released; `RoundsPlayedCount` keeps its Active-entry increment. This story asserts the code and no score; the full void plumbing (phase `Reset`, banner flag) is shared with story 065, so reuse its path rather than re-implementing it.
- **Pawnless `Disconnected` participant.** Such a participant (disconnected before or while without a pawn) is still respawned, AI-driven, by the same own-card path and converts only by grace (R17.4); the pawn-loss recovery does not convert it and does not count it as a void trigger by itself. Open: confirm against GDD R17.9 and R6 whether this respawn counts toward the cap of 2 (the AC says only "still respawned AI-driven and converts only by grace").
- **No recovery in `Ended` or `Reset`.** A `PawnLost` event or a pending retry that is processed after the latch or in `Ended`/`Reset` produces no respawn: items pending at the latch resolve `Rejected_Latched`, and `Reset` cancels every Round timer (R10, R21). The test drives `Ended` and `Reset` explicitly and asserts no gateway call.
- **Kind-blind.** The cap, timeout and void treat every participant alike; Round never reads player-ness (R23 (a)).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 075 (`production/epics/round-match-framework/story-075-active-pawn-lost-recovery.md`): the first and second recovery (this story's dependency).
- Story 065 (`production/epics/round-match-framework/story-065-forfeit-void-cases.md`): `Void(Abandoned)` and `Void(KickedLastActive)` and the shared void path.
- Story 058 (`production/epics/round-match-framework/story-058-reset-clears.md`): `Reset` clearing pawns and the timer heap.
- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): grace conversion of the pawnless Disconnected participant.
- Penalty's informant re-pawn (an `Informant` whose pawn is lost goes to Penalty, R17.9), Crowd's `RequestEmergence` and the Level Socket Contract's eligibility rules (other epics).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Round/Disconnect/PawnLossRecovery.cpp` (created by story 075; extended here with the cap, retry and void)
- `Source/StealthGameplay/Private/Tests/Round/PawnLostVoidTest.cpp` (class `FRoundPawnLostVoidTest`, test name `StealthGame.Round.PawnLostVoid`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A third pawn loss or no eligible socket in time gives Void(PawnLost), and no recovery runs in Ended or Reset.
  - Given: one participant losing pawns repeatedly; a case with no eligible socket; a pawnless Disconnected participant.
  - When: the third loss occurs; `LateJoinTakeoverTimeoutSec` passes with no socket; the phase is `Ended` or `Reset`.
  - Then: the third loss, or the timeout, gives `Void(PawnLost)` and no score.
  - Then: a pawnless `Disconnected` participant is still respawned AI-driven and converts only by grace.
  - Then: no recovery runs in `Ended` or `Reset`.
  - Edge cases: losses one and two recover (story 075). Open: confirm against GDD R17.9 whether the pawnless Disconnected respawn counts toward the cap.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/PawnLostVoidTest.cpp` (automation test `StealthGame.Round.PawnLostVoid`, class `FRoundPawnLostVoidTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-075-active-pawn-lost-recovery.md`
- Unlocks: None
- Cross-epic: `production/epics/crowd-npc-behaviour` (`RequestEmergence(PawnRecovery)`; stories not yet created), `production/epics/level-socket-contract` (emergence socket eligibility; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; `PawnLossRecoveryMaxCount`, `LateJoinTakeoverTimeoutSec` keys; stories not yet created)
