# Story 065: Both teams fully disconnected gives Void(Abandoned) and a host kick as last conversion gives Void(KickedLastActive)

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void). Also `GDD AC 24b`, `R15` partitions (b) and (c), `R17.6`, `R8` (void in Active), `F2` (`Void(e)`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 5 (a kick that closes an `Expired` connection is a legal `Disconnect`: the Roster accepts it for a participant whose Session connection is `Expired`; closeout 2026-10-25 D9) together with Round's void path, which is `TryTerminate` plus `Latch` in the same call and goes `Active -> Reset` with no `RoundEndStep` staged (ADR-0002 section 8.1).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched; spikes S3, S4, S5 are not touched (fake Session and Roster gateways; the kick is a fake Session `Disconnected(Kicked)` event, so how a real kick closes an `Expired` connection is Session's and S4's concern, not this story's). The queue and latch behaviour it relies on (ADR-0002) uses no post-cutoff API.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Round phase flow and Forbidden; the Core Layer Rules hold no Round-specific rule):
- Required: Forfeit and void: the same conversion caused by a host kick (`Kicked = true`) is `Void(KickedLastActive)`; a void is `TryTerminate` plus `Latch` in the same call, stages no `RoundEndStep`, goes `Active -> Reset` with `bCalledOff`, no winner and no score (ADR-0002).
- Forbidden: Never write the Round Event Log except through `IRoundLogWriter::Append`; the void's `WinLatched` entry is appended there and carries no secret (ADR-0002).
- Guardrail: Round Event Log: entry 64-96 B, about 25 KB typical, about 400 KB at the default 4,096 entries (ADR-0002).

**Prototype disposition**: none (no prototype equivalent: the prototype has no void outcome and no kick).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 24b, scoped to this story:*

- [x] GIVEN the same with every Active member of team 0 disconnected, THEN `Void(Abandoned)`: phase `Reset`, no score, `bCalledOff` true. GIVEN the last conversion was a host kick, THEN `Void(KickedLastActive)`.

---

## Implementation Notes

*Derived from GDD R15 (b), (c), R17.2, R17.6, R8 and F2:*

- Setup "the same" is story 064's: team 1's last Active member converts at grace expiry. With every Active member of team 0 also disconnected (none `Connected`), `ConnectedActiveCount(W) = 0`, so `Void(e) = Zero(e) AND RosterCount(W) >= 1 AND Expiry(e) AND ConnectedActiveCount(W) = 0 -> Abandoned` (F2): no team has anyone playing. Round calls `TryTerminate(Void(Abandoned), key)`.
- Kick (R15 c, F2 `Kick(e) = e.Reason = Disconnect AND e.Kicked`): a conversion caused by a host kick (Session flags `Disconnected(Kicked)`; the grace record carries `Kicked = true`, story 070) gives `Void(KickedLastActive)`, so the host cannot win by kicking the last rival. This holds whether or not team 0 has a Connected Active member (the partition is by Reason, not by team 0's connection). Where a kick and a grace expiry for the same participant fall in the same frame, the kick is processed first and the later expiry is the Roster's idempotent answer, so the outcome cannot flip on frame order (decision D32, R15 c). The window-expiry and conversion-timing details of a kick are story 070's.
- Void effects (R8, ADR-0002 8.1): the void leaves `Active` straight to `Reset` (no `Ended`), with `EndReason = Void(code)`, no winner, no `FRoundResult`, no score (`MatchScore` unchanged), `bCalledOff` true so every client sees "The round was called off", the Event Log archived and never released, and `WinLatched(outcome, EndReason, key)` written for the void as well. A void also calls `UResolutionQueue::Latch` in the same call and stages no `RoundEndStep`. After a void `RoundsPlayedCount` keeps its Active-entry increment. The `Reset` cleanup then runs (story 058, not asserted here beyond the phase being `Reset`).
- Assertions: for each of the two cases, (1) the phase is `Reset`, (2) `EndReason` is `Void(Abandoned)` or `Void(KickedLastActive)` respectively, (3) `MatchScore` unchanged and no `FRoundResult` in the public struct, (4) `bCalledOff` is true, and (QA edge case) no score in either case. A single test method per case in one test class.
- Open: confirm against GDD R15 (c) and F2 that, when the kicked participant's conversion zeroes team 1 and team 0 also has no Connected Active member, the kick partition wins over the `Abandoned` partition (R15 (c) and F2 say `Kick(e) -> KickedLastActive` independent of `ConnectedActiveCount`); the AC lists the kick case separately and does not state the combined case.
- Where it lives: the R15 partition and the void path in the Round Core win logic (planned `Source/StealthGameplay/Private/Round/Win/`), extending story 064.
- Manifest 2026-10-25f: the explicit forfeit/void precondition (`ActiveCountAfter[team] = 0`, `RosterCount(team) >= 1`, phase `Active`, not `Terminated`; batch events never count) gates every outcome here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 064 (`production/epics/round-match-framework/story-064-last-disconnect-forfeits.md`): the forfeit win.
- Story 066 (`production/epics/round-match-framework/story-066-forfeit-vs-completion.md`): the same-tick forfeit and final completion.
- Story 070 (`production/epics/round-match-framework/story-070-kick-window-expiry-ignored.md`) and story 079 (`production/epics/round-match-framework/story-079-kicked-cannot-return.md`): the kick's window expiry, grace timing and the kicked participant's cannot-return rule.
- Story 076 (`production/epics/round-match-framework/story-076-pawn-lost-void.md`): `Void(PawnLost)`.
- Story 058 (`production/epics/round-match-framework/story-058-reset-clears.md`): the `Reset` cleanup after the void.
- Session's kick and `Expired` handling and the Roster's acceptance of `Disconnect` for an `Expired` connection (session-travel and participant-state-roster epics).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/ForfeitVoidCasesTest.cpp` (class `FRoundForfeitVoidCasesTest`, test name `StealthGame.Round.ForfeitVoidCases`)
- `Source/StealthGameplay/Private/Round/Win/` void partition and `Void` latch path (new, shared with stories 064 and 076)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Both teams fully disconnected gives Void(Abandoned), and a host kick as last conversion gives Void(KickedLastActive).
  - Given: the setup of 064 with every Active member of team 0 also disconnected; separately, a last conversion that was a host kick.
  - When: the last conversion happens.
  - Then: the first gives `Void(Abandoned)`, phase `Reset`, no score, and `bCalledOff` true.
  - Then: the second gives `Void(KickedLastActive)`.
  - Edge cases: no score in either case.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/ForfeitVoidCasesTest.cpp` — must exist and pass (automation test `StealthGame.Round.ForfeitVoidCases`, class `FRoundForfeitVoidCasesTest`)

**Status**: [x] Created and passing (`StealthGame.Round.ForfeitVoidCases`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-064-last-disconnect-forfeits.md`
- Unlocks: None

Cross-epic: `production/epics/session-travel` (`Disconnected(Kicked)`, `Expired` connection; stories not yet created); `production/epics/participant-state-roster` (`RequestTransition` for an `Expired` connection, `ConnectedActiveCount` inputs; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

## Completion Notes
**Completed**: 2026-10-26 (RMF round-end and disconnect batch: stories 048-065, two-step round end, TeamSummary, grace/forfeit/void)
**Criteria**: 1/1 passing (Void(Abandoned) and Void(KickedLastActive): Reset, no score, no result, bCalledOff; D32 kick in the expiry frame -> KickedLastActive)
**Deviations**: None
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/ForfeitVoidCasesTest.cpp`; full suite: `production/qa/evidence/rmf-round-end-disconnects-2026-10-26/full-suite.txt`
**Run result**: N/A - Logic, covered headless
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, RMF round-end batch; findings and fixes in production/qa/evidence/rmf-round-end-disconnects-2026-10-26/evidence.md
