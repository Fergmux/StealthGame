# Story 066: A forfeit item and a final completion in one tick give exactly one outcome by key order

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); `TR-round-005` (objective and elimination wins; no draw, no clock); GDD AC 24c; rules R15 (terminal latch), R16 (no ties), R17.4 and R17.6 (grace expiry and a team whose every player disconnects), formula F2.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), drained once per frame by a custom `TG_PostPhysics` tick function, single-assignment latch storage with Round's rule injected, and the R9 two-step round end. This story applies section 8.7 (disconnect policy, forfeit and void conditions) together with the latch rules of section 8.1: a forfeit is a win whose latch step is its round-end step, and whichever of two same-tick terminal items holds the lower key latches, so the other returns `Rejected_Latched`.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched. The test drives the plain-C++ queue core and Round's latch with stub resolvers and a fake clock; no engine API, post-cutoff API or [U] item (spawn inside `TG_PostPhysics`, client clock estimator, listen-host tick) is exercised.

**Control Manifest Rules (this layer)** (the Round policy rules sit in the Foundation Layer Rules, section Round phase flow, Event Log and public round struct):
- Required: A grace or window expiry (not a kick) that converts the last `Active` participant of a team is `Win(EliminationWin(W))` logged `ForfeitElimination` if `ConnectedActiveCount(W) >= 1`, otherwise `Void(Abandoned)`; the latch is single-assignment (ADR-0002).
- Forbidden: Never re-submit or re-stamp an outcome (ADR-0002).
- Guardrail: Tests: per-commit pipeline at most 10 min (ADR-0008); this story adds one small headless test.

**Prototype disposition**: none (new Round Core code). The prototype resolves first-come with no total-order key (`CheckEliminationVictory`, `EndRound` in `StealthGameMode.cpp`), which the key-order rule replaces; nothing is promoted.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 24c, scoped to this story:*

- [ ] GIVEN a forfeit item and a final completion in one tick, THEN exactly one outcome, by key order.

---

## Implementation Notes

*Derived from ADR-0002 section 8.1 (latch) and section 8.7 (disconnect policy), GDD R15, R16 and R17.6:*

- **What the two items are.** The forfeit item is the `TimerFired(GraceExpiry)` item whose resolver converts the last `Active` member of a team (story 064: `RequestTransition(P, Informant, Disconnect, ...)` applied, `ReleaseToCrowd`, R15 partition (b), `TryTerminate(Win(EliminationWin(W)))` with the log-only `EndReason = ForfeitElimination`). The final completion is the Objectives resolver's call `Round.DeclareObjectiveWin(team, Key)` after re-validation (story 049). In this story both are stub items with chosen keys; no real Objectives code exists yet (GDD AC 2 is deferred).
- **One latch decides.** `TryTerminate(outcome, key)` is single-assignment: it succeeds only in phase `Active` with `Terminated` false and then performs the latch step (and the round-end step too, because neither a forfeit nor an objective win is a strike). The second attempt returns false, is logged `LateWinSuppressed(outcome, key)`, and the item itself returns `Rejected_Latched` because `Latch(key)` closed intake (a grace item still pending at the latch has no post-latch exemption, ADR-0002 section 8.7 (4)).
- **Both orders.** The test builds the same tick twice: forfeit key lower, then completion key lower, with stub keys from the fake clock and stub resolvers (story 004). Lower key wins; the outcome is exactly one `Win` with exactly one `WinLatched` entry; the other item is `Rejected_Latched`.
- **No state leaks from the loser.** When the completion wins, the forfeit item's `RequestTransition` never runs, so the participant is not converted and the roster is unchanged by the loser. When the forfeit wins, the completion changes no objective state (story 017 covers its `TendCompleted ... Latched = true` log row).
- **Where stamping order comes from (reference only).** GDD R12 stamps due-time items before that frame's intake items and orders them by `(DueTime, TieRank)`; a client-submitted completion is stamped after due items. This story does not test stamping (story 007 does); it asserts the latch result for given keys.
- **Open: confirm against GDD F2 / R15** which team the stub completer is on in each order; the AC only requires exactly one outcome set by the lower key, so the fixture should name both teams explicitly and the test asserts the winner team for each order from the F2 reference.
- Manifest 2026-10-25f: forfeit versus void is decided by `ConnectedActiveCount(W) >= 1` only after the explicit `TryTerminate` precondition holds.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 064 (`production/epics/round-match-framework/story-064-last-disconnect-forfeits.md`): the forfeit conversion itself (`EliminationWin` with `ForfeitElimination`, score, banner reason).
- Story 049 (`production/epics/round-match-framework/story-049-completion-needs-human.md`): which completers count (human, Connected, Active) and the distinct `Voided` codes.
- Story 065 (`production/epics/round-match-framework/story-065-forfeit-void-cases.md`): `Void(Abandoned)` and `Void(KickedLastActive)`.
- Story 045 (`production/epics/round-match-framework/story-045-same-tick-final-vs-elimination.md`): the same-tick race between a final completion and a strike elimination.
- Story 017 (`production/epics/round-match-framework/story-017-latched-final-complete-logged.md`): the `TendCompleted` log row for a latched final completion.
- Real objectives and assassination (GDD AC 2 is [DEFERRED], no story here) and the Roster's transition internals (participant-state-roster epic).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/ForfeitVsCompletionTest.cpp` (class `FRoundForfeitVsCompletionTest`, test name `StealthGame.Round.ForfeitVsCompletion`)
- No new production file is planned: the latch and the grace-expiry resolver are created by stories 043 and 060/064 (their names are fixed there); this story edits them only if the test exposes a defect.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A forfeit item and a final completion in one tick give exactly one outcome by key order.
  - Given: a forfeit item and a final completion in one tick, with stub keys.
  - When: the tick drains in both key orders.
  - Then: exactly one outcome results, set by the lower key.
  - Then: the other item is `Rejected_Latched`.
  - Edge cases: both orders are asserted (forfeit key lower, then completion key lower).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/ForfeitVsCompletionTest.cpp` (automation test `StealthGame.Round.ForfeitVsCompletion`, class `FRoundForfeitVsCompletionTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-064-last-disconnect-forfeits.md`, `production/epics/round-match-framework/story-049-completion-needs-human.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (`RequestTransition` and its events; stories not yet created)
