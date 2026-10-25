# Story 052: An objective-win latch is also the round-end step: result, score, EndRoundAll, rejections and PostRound viewers

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-004` (Single-assignment latch and the two-step round end: latch step; round-end step at the deciding strike's death write); GDD AC 12a; rules R9 (latch step and round-end step (a), (b), (c), (e)), R15 (the terminal latch), R16 (1) (no draw state in the result or the score).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies sections 8.2 to 8.4 for a non-strike terminal item: the staged `RoundEndStep` is consumed inline at slot end, so the round-end step is the latch step, run through `ITerminalDecision::RunRoundEndStep`.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched in production code (plain C++ phase core, queue core and fake gateways; no `UWorld`, tick function or post-cutoff API). The test uses reflection (`TFieldIterator<FProperty>` over `FRoundResult`, a pre-cutoff API) for the no-duration-field check. ADR-0002 verification items (1) to (3) and the round-end step budget (story 092, ADR-0002 round-end spike) are not exercised here.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Resolution queue and Forbidden Approaches):
- Required: the latch step replicates and presents nothing; the round-end step runs in one server step, bound to the deciding slot's latest death write by `CauseKey`, or inline when the slot staged none (ADR-0002).
- Forbidden: Never end the round in one step at the latch, or through an `OnRoundEnded` subscriber chain; it reveals the end before contact and subscriber order is unspecified (ADR-0002). For an objective win there is no strike to wait for, so the round-end step is the latch step by design (ADR-0002 section 8.3, third bullet); the prohibition constrains strike wins (story 053) and forbids using a subscriber chain for the round-end sequence here.
- Guardrail: queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0022); the 2.0 ms and 3.1 KB round-end step budget is measured in story 092, not here.

**Prototype disposition**: Replaces `AStealthGameMode::EndRound(WinningTeam, Reason)` in `Source/StealthGame/Private/StealthGameMode.cpp` (win reasons `EStealthWinReason`, round fields in `StealthGameState.*`, immediate end with no latch, no staging and no ordered steps). No part is promoted wholesale.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 12a, scoped to this story:*

- [ ] GIVEN a latch by an objective win, THEN in the latch step (which is also the round-end step) `FRoundResult` has no duration field, `MatchScore[Winner]` rose by exactly 1, `EndRoundAll` ran once and produced no evaluation, every pending item is `Rejected_Latched`, `OnRoundEnded` fired once and every viewer resolves as `PostRound`.

---

## Implementation Notes

*Derived from ADR-0002 sections 8.1 to 8.4 and GDD R9, R15, F2:*

- **Objective win: one step.** For every terminal item that is not a strike (objective win, forfeit, debug win) the round-end step is the latch step and nothing is staged. In the winning slot, `TryTerminate` (story 043) sets `Terminated`, holds `{Winner, EndReason, RoundIndex}` in Round's latch record, closes the queue, and the staged `RoundEndStep` is consumed inline at slot end (after `Resolve` and `FlushPending()`), calling `ITerminalDecision::RunRoundEndStep(const FRoundEndPayload&)`.
- **What this story implements of R9 (a) to (h).** (a) the phase becomes `Ended` and `FRoundResult {Winner, EndReason (public form), RoundIndex, TeamSummary[2]}` is published in `FRoundPublicState` with a forced GameState net update (the duration is kept in the server log only: `FRoundResult` has no duration field); (b) `EndRoundAll()` is called once, and its batch event is never evaluated as an elimination (F2 proof item 5); (c) `MatchScore[Winner]` is incremented in the same `FRoundPublicState` write as (a); (e) `OnRoundEnded(Result)` fires once, after which every viewer is class `PostRound`. The queue closing is the latch step's effect: every pending item, and any command, AI item or timer item submitted before the round-end step, resolves `Rejected_Latched` (logged).
- **Left to neighbouring stories.** `TeamSummary` computation and its all-zero state before `Ended` is story 054; the held `NotNow` replies (d), the hold cancels (g) and strike staging are stories 053 and 056; the crowd Halt and one stop request per pawn (f) are stories 055 and 078; `EndedMinDwellSec` and `EndedAutoResetSec` (h) and the held host `ResetRound` belong to the reset stories (058, 057). Open: confirm slot ownership of steps (d), (g) and (h) at /dev-story; this story builds the handler skeleton with (a), (b), (c) and (e) and leaves clearly named hooks for the rest.
- **Assertions.** (1) Reflect over `FRoundResult` and compare its property list with the R9 (a) tuple `{Winner, EndReason, RoundIndex, TeamSummary[2]}`: no field named or typed as a duration. Open: confirm against ADR-0002 section 8.10 and ADR-0003 where `FRoundResult` is declared (Foundation `Public/View/`) and that it is a reflected `USTRUCT`; if it is plain C++, replace the reflection check with a `static_assert` on `sizeof`/member list in the same test file. (2) `MatchScore[Winner]` is exactly one higher and the loser's score unchanged. (3) A spy on the fake Roster gateway sees `EndRoundAll` once, and the elimination evaluation spy is not entered for the batch event. (4) Pending stub items, including one submitted after the latch within the same drain, return `Rejected_Latched` and are logged through the `IRoundLogWriter::Append` spy. (5) `OnRoundEnded` fires once. (6) Every viewer resolves as `PostRound`; the source is the Visibility Policy viewer class. Open: confirm against GDD R9 (e) and ADR-0003 which seam supplies the viewer class in [U] (the story 003 fakes do not list a Visibility Policy gateway); an `OnRoundEnded`-driven fake viewer-class provider is the assumed shape.
- **Second latch attempt.** A second `TryTerminate` returns false, logs `LateWinSuppressed`, and re-runs none of the above (score unchanged, `EndRoundAll` not called again, no second `OnRoundEnded`).
- **Where the code goes.** `Source/StealthGameplay/Public/Round/Win/RoundEndStep.h` and `Private/Round/Win/RoundEndStep.cpp` (the `ITerminalDecision` implementation, plain C++), wired by the composition root; `FRoundResult` and `FRoundPublicState` are Foundation `Public/View/` types from ADR-0002 section 8.10. Planned names; `/dev-story` replaces them. `MatchScore` lives in `URoundMatchStore` (ADR-0005).
- Manifest 2026-10-25f: `EndRoundAll()` is batch-only so its conversions never count toward a forfeit, and `FRoundResult.TeamSummary[2]` published in this step is the only carrier of opponent counts.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 043 (`production/epics/round-match-framework/story-043-no-draw-property.md`): the latch itself and outcome uniqueness.
- Story 053 (`production/epics/round-match-framework/story-053-strike-win-staging.md`): a strike win, latched server-only first and presented at the death write.
- Story 054 (`production/epics/round-match-framework/story-054-team-summary-at-ended.md`): `FRoundResult.TeamSummary`, computed at the round-end step (data half of MVP item 9; the HUD epic carries the display half).
- Story 055 (`production/epics/round-match-framework/story-055-ended-hold.md`) and story 078: every command except Look rejected in Ended, one stop request per pawn including pawns in hand-over.
- Story 056: arrival order of Roster and GameState records on clients.
- Story 092: the 2.0 ms and about 3.1 KB round-end step budget.
- Stories 057 and 058: host abort and Reset (including the `EndedMinDwellSec` hold).
- Roster internals (`EndRoundAll`, `RoundEnded` records) and Visibility Policy internals (viewer classes) belong to other epics.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Win/RoundEndStep.h`
- `Source/StealthGameplay/Private/Round/Win/RoundEndStep.cpp`
- `Source/StealthGameplay/Private/Tests/Round/EndedLatchEffectsTest.cpp` (class `FRoundEndedLatchEffectsTest`, test name `StealthGame.Round.EndedLatchEffects`)
- `Source/StealthGame/Private/StealthGameMode.cpp`, `Source/StealthGame/Public/StealthGameMode.h` (`EndRound(WinningTeam, Reason)` and `EStealthWinReason`: retired/replaced by this story), `Source/StealthGame/Public/StealthGameState.h` (prototype replicated round fields: retired/replaced)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: An objective-win latch is also the round-end step, with the specified effects.
  - Given: an `Active` round with pending items and a latch by an objective win.
  - When: the latch step runs.
  - Then: `FRoundResult` has no duration field (a reflection check).
  - Then: `MatchScore[Winner]` rose by exactly 1.
  - Then: `EndRoundAll` ran once and produced no evaluation.
  - Then: every pending item is `Rejected_Latched`.
  - Then: `OnRoundEnded` fired once, and every viewer resolves as `PostRound`.
  - Edge cases: a second latch attempt does not re-run any of these. Open: confirm the reflection mechanism and the viewer-class seam against ADR-0002 section 8.10 and ADR-0003 (see Implementation Notes).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/EndedLatchEffectsTest.cpp` (automation test `StealthGame.Round.EndedLatchEffects`, class `FRoundEndedLatchEffectsTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-042-active-gate.md`, `production/epics/round-match-framework/story-043-no-draw-property.md`
- Unlocks: `production/epics/round-match-framework/story-053-strike-win-staging.md`, `production/epics/round-match-framework/story-054-team-summary-at-ended.md`, `production/epics/round-match-framework/story-055-ended-hold.md`, `production/epics/round-match-framework/story-078-hand-over-pawn-stops-at-end.md`, `production/epics/round-match-framework/story-085-pie-lifecycle-objective-and-reset.md`, `production/epics/round-match-framework/story-092-round-end-step-budget.md`
- Cross-epic: `production/epics/participant-state-roster` (`EndRoundAll()` and the `RoundEnded` batch event, via the fake Roster gateway; stories not yet created), `production/epics/crowd-npc-behaviour` (`ICrowdControl::Halt()` hook, via the fake Crowd gateway; stories not yet created), `production/epics/input-interaction-framework` (the held `NotNow` replies and the command-rejection class `RoundEnded`; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
