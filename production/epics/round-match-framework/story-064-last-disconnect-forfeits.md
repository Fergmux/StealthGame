# Story 064: The last Active member of a team converting at grace expiry gives EliminationWin to the other team with ForfeitElimination

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void) and `TR-round-005` (Objective and elimination wins; no draw, no clock). Also `GDD AC 24a`, `R15` partition (b), `R17.6`, `F2` (`Expiry(e)`, `EliminationWin(W, e)`, forfeit example), `R20`.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: ADR-0005: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration; section 5 ties Round's grace expiry to the participant's `ConnectionGeneration`. ADR-0002: one total-order queue per round, single-assignment latch storage with Round's rule injected, and the R9 two-step round end; section 8.3 states that a forfeit by grace expiry is a non-strike terminal item, so the round-end step is the latch step and the staged `RoundEndStep` is consumed inline at slot end. This story applies Round's `TryTerminate` partition (b) to the grace-expiry conversion.
**ADR Version**: ADR-0005 2026-10-24; ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched; spikes S3, S4, S5 are not touched (fake Session and Roster gateways). ADR-0002 uses no post-cutoff API and its verification items (CMC spawn in `TG_PostPhysics`, client estimator, listen-host frame) are none touched: the test runs the real latch rule on the plain-C++ queue core with a fake clock.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Round phase flow and Forbidden; the Core Layer Rules hold no Round-specific rule):
- Required: Forfeit and void: when a grace or window expiry (not a kick) converts the last `Active` participant of a team, it is `Win(EliminationWin(W))` logged `ForfeitElimination` if `ConnectedActiveCount(W) >= 1` (ADR-0002); the `TryTerminate` precondition is `ActiveCountAfter[team] = 0`, `RosterCount(team) >= 1`, phase `Active`, not `Terminated`, and batch events never count.
- Forbidden: Never mutate gameplay state outside the drain; the grace expiry is a queue item and the conversion, the evaluation and the latch all run inside its slot (ADR-0002).
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (ADR-0002, ADR-0022).

**Prototype disposition**: none (no prototype equivalent: the prototype has no disconnect policy; its elimination check, `CheckEliminationVictory`, counts active players per team from the PlayerState array, skips inactive PlayerStates, and has no forfeit or void partition). The elimination predicate `FStealthRules::EvaluateEliminationWinner` is promoted as a reference test in story 044, not here.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 24a, scoped to this story:*

- [x] GIVEN the last Active member of team 1 converts at grace expiry and team 0 has >= 1 Connected Active member, THEN `EliminationWin(0)`, log `EndReason = ForfeitElimination`, `MatchScore[0]` +1, banner reason "every rival eliminated for the round".

---

## Implementation Notes

*Derived from GDD R15 (b), R17.6, F2 and R20, and ADR-0002 sections 8.1 and 8.3:*

- Path: the `TimerFired(GraceExpiry)` item for team 1's last `Active` member (story 060) resolves; `RequestTransition(P, Informant, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)` is `Applied`; `ReleaseToCrowd(pawn)` runs in the same call stack; Round then evaluates R15 on the resulting event `e` in `EventSeq` order. `Zero(e) = e.Applied AND e.Old = Active AND e.New = Informant AND Phase = Active AND NOT L AND e.ActiveCountAfter[e.Team] = 0 AND RosterCount(e.Team) >= 1`, `W = 1 - e.Team`. With `Expiry(e) = e.Reason = Disconnect AND NOT e.Kicked`, `EliminationWin(W, e)` holds when `RosterCount(W) >= 1` and `ConnectedActiveCount(W) >= 1` (F2 partition (b); a Decision-reason elimination is story 044's and does not apply here). Round calls `TryTerminate(Win(EliminationWin(0)), key)`.
- Outcome (R15 b): `EliminationWin(0)` with the log-only `EndReason = ForfeitElimination` (the score counts); `MatchScore[0]` rises by exactly 1 in the round-end step. The deciding item is a grace expiry, which is not a strike, so the round-end step is the latch step and the staged `RoundEndStep` is consumed inline at slot end (ADR-0002 8.3); the full effects (result, `EndRoundAll`, rejections, `PostRound` viewers) are story 052's and are not re-asserted here beyond the score and the result.
- Public form (R15 last bullet): there are exactly two public win types, so `FRoundResult` shows `EliminationWin` for (a) and (b) alike and the banner keeps "every rival eliminated for the round" (literally true: a converted player is removed from active play). A separate forfeit phrase would tell opponents that someone disconnected (R20; creative-director ruling), so the forfeit/elimination split lives only in the Event Log's `EndReason = ForfeitElimination`, and F7 (story 016) can exclude forfeits. QA: assert the banner through the reason code and its text-table entry, not through a widget: the public `EndReason` form equals the one `EliminationWin` code and the text table maps it to "every rival eliminated for the round". Open: confirm against GDD R20 and the HUD epic the name of the public reason code and the text-table entry that carries that string.
- Fixture (F2 forfeit example): team 1's last Active member's grace expires at tick 5000 while team 0 has 2 Active, 1 of them Connected: `EliminationWin(0)`, `EndReason = ForfeitElimination`, `MatchScore[0] += 1`. The AC only requires team 0 to have at least 1 Connected Active member; keep the fixture constants in the test constant file (story 005).
- Edge cases to keep out: a team 0 with no Connected Active member gives `Void(Abandoned)` (story 065); a host kick as the last conversion gives `Void(KickedLastActive)` (story 065); a forfeit and a final completion in one tick (story 066).
- Where it lives: the R15 evaluation and `TryTerminate` partition in the Round Core win logic (planned `Source/StealthGameplay/Private/Round/Win/`), called from the disconnect policy's conversion handler (planned `Source/StealthGameplay/Private/Round/Disconnect/`, story 060).
- Manifest 2026-10-25f: the `TryTerminate` precondition (`ActiveCountAfter[team] = 0`, `RosterCount(team) >= 1`, phase `Active`, not `Terminated`) is checked on the conversion's own event, and `EventSeq` rises by exactly 1 per emitted event.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 044 (`production/epics/round-match-framework/story-044-elimination-truth-table.md`): the Decision-reason zeroing truth table and the F2 clauses.
- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline and conversion machinery.
- Story 065 (`production/epics/round-match-framework/story-065-forfeit-void-cases.md`): `Void(Abandoned)` and `Void(KickedLastActive)`.
- Story 066 (`production/epics/round-match-framework/story-066-forfeit-vs-completion.md`): the same-tick forfeit and final completion.
- Story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`) and story 054 (`production/epics/round-match-framework/story-054-team-summary-at-ended.md`): the round-end step effects and the forfeiting member's `EliminatedCount`.
- Story 016 (`production/epics/round-match-framework/story-016-pacing-metrics.md`): the F7 exclusion of forfeits.
- The on-screen banner text rendering (HUD epic); Roster `RequestTransition` internals (participant-state-roster epic).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/LastDisconnectForfeitsTest.cpp` (class `FRoundLastDisconnectForfeitsTest`, test name `StealthGame.Round.LastDisconnectForfeits`)
- `Source/StealthGameplay/Private/Round/Win/` R15 evaluation and `TryTerminate` partition (b) (new, shared with stories 044 to 046 and 065)
- `Source/StealthGameplay/Private/Round/Disconnect/` conversion-to-evaluation hook (new, extends story 060)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: The last Active member of a team converting at grace expiry gives EliminationWin to the other team with ForfeitElimination.
  - Given: team 1's last Active member in grace, and team 0 with at least 1 Connected Active member.
  - When: grace expires and the member converts.
  - Then: the outcome is `EliminationWin(0)`.
  - Then: the log `EndReason` = `ForfeitElimination`.
  - Then: `MatchScore[0]` rose by 1.
  - Then: the banner reason is "every rival eliminated for the round".
  - Edge cases: assert the banner through the reason code and its text table entry, not through a widget.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/LastDisconnectForfeitsTest.cpp` — must exist and pass (automation test `StealthGame.Round.LastDisconnectForfeits`, class `FRoundLastDisconnectForfeitsTest`)

**Status**: [x] Created and passing (`StealthGame.Round.LastDisconnectForfeits`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-044-elimination-truth-table.md`, `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: `production/epics/round-match-framework/story-065-forfeit-void-cases.md`, `production/epics/round-match-framework/story-066-forfeit-vs-completion.md`

Cross-epic: `production/epics/session-travel` (disconnect events and `ConnectionGeneration`; stories not yet created); `production/epics/participant-state-roster` (`RequestTransition`, `ConnectedActiveCount` inputs, `ActiveCountAfter`; stories not yet created); `production/epics/crowd-npc-behaviour` (`ReleaseToCrowd`, faked here; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

## Completion Notes
**Completed**: 2026-10-26 (RMF round-end and disconnect batch: stories 048-065, two-step round end, TeamSummary, grace/forfeit/void)
**Criteria**: 1/1 passing (EliminationWin(0), log ForfeitElimination, MatchScore[0] +1 at the expiry, public reason elimination, banner text 'every rival eliminated for the round')
**Deviations**: The banner text table is GetRoundEndReasonText (Foundation View) until the HUD epic
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/LastDisconnectForfeitsTest.cpp`; full suite: `production/qa/evidence/rmf-round-end-disconnects-2026-10-26/full-suite.txt`
**Run result**: N/A - Logic, covered headless
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, RMF round-end batch; findings and fixes in production/qa/evidence/rmf-round-end-disconnects-2026-10-26/evidence.md
