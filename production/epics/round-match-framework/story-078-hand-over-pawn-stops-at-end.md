# Story 078: A pawn released from Crowd earlier in the drain still gets one stop request at the round-end step

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-004` (single-assignment latch and the two-step round end); `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 64 (cross-review S-W1, revised in the AC overrides); rules R9 (f) (pawns in hand-over stop too), R17.1 (hand-over committed in the drain, takes effect at the next frame start).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging; ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0002: one total-order queue per round, the latch and the R9 two-step round end with a round-end step that halts the whole city in one server step. ADR-0010: `UStealthPawnFactory` is the only possessor, hand-overs commit in a slot and execute in the hand-over pass at the next frame start, and it exposes `StopAllForRoundEnd()` and `IsHandOverPending()`. This story applies ADR-0002 section 8.4 (f): the round-end step calls `ICrowdControl::Halt()` and the kind-blind stop-and-blend for every live pawn in the same step, including a pawn whose hand-over is committed but not yet executed.
**ADR Version**: ADR-0002 2026-10-24; ADR-0010 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk; ADR-0010, HIGH, is the second ADR)
**Engine Notes**: None touched by the unit test. It uses a Crowd spy and a fake factory with a pending hand-over table; no engine API, post-cutoff API or [U] item (spawn inside `TG_PostPhysics`, client estimator) is exercised. ADR-0010 H1 (AI-to-player hold gap) and S14 (per-tick deceleration limit) concern the real blend and the speed measurement, which moved to the PIE run of story 085.

**Control Manifest Rules (this layer)** (Round and Crowd rules sit in the Foundation and Feature Layer Rules; the hand-over rule in the Core Layer Rules):
- Required: The round-end step runs in one server step and calls `ICrowdControl::Halt()` plus the kind-blind stop-and-blend for every live pawn (ADR-0002).
- Forbidden: Never stop human pawns at round end by input loss (ADR-0002); never execute a hand-over inline in the queue slot (ADR-0010).
- Guardrail: GDD R9 budget: the round-end step adds at most 2.0 ms of listen-host game-thread time, about 60 pawn stops at no more than 30 microseconds each (verified by story 092); the control manifest carries no separate guardrail line for it.

**Prototype disposition**: none (new Round Core code). The prototype's `EndRound` stops nothing.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 64 as revised in the AC overrides, scoped to this story:*

- [ ] GIVEN an informant takeover committed earlier in the same drain as an objective-win latch (the pawn released from Crowd, not yet human-driven), WHEN the round-end step runs, THEN that pawn receives one stop request with the others (carrying `RunStopSec`) and the hand-over commits at the next frame start on an already-stopping pawn. (Speed measurement moved to the PIE run of story 085.)

---

## Implementation Notes

*Derived from GDD R9 (f) and ADR-0002 section 8.4 (f); factory interface from ADR-0010 Key Interfaces:*

- **The gap this closes.** A pawn whose takeover or hand-over was committed earlier in the drain but is not yet human-driven (for example an informant takeover released from Crowd and awaiting the next-frame-start hand-over pass, R17.1) belongs neither to Crowd's Halt (it was released) nor to the human-driven set. Without a rule it would keep walking while the city halts and mark itself as the informant.
- **What Round does.** In the round-end step Round enumerates the pending hand-overs (`UStealthPawnFactory::IsHandOverPending(const AStealthPawn*)`, and the factory's pending table) and applies the same stop-and-blend to those pawns by raising Halt on the pawn itself, which holds whichever controller drives it. Every stop request carries `RunStopSec` as its bound (Shared Character key; Crowd R10 bounds the stop by it). The existing `StopAllForRoundEnd()` is the kind-blind baseline stop for every live pawn; confirm in the story that the pending hand-over pawn is included in that set exactly once and not stopped twice.
- **The commit after the stop.** The pending hand-over still commits at the next frame start (hand-over pass, never inline in the slot), now on an already-stopping pawn. The test advances one frame and asserts the commit happens after the stop request in the call-order log, and that no second stop request is issued for the pawn after the commit.
- **Latch variant.** The objective-win latch is also the round-end step (nothing staged); the strike variant stages the round-end step to the deciding death write (story 053) and is not tested here.
- **Out of the unit test.** The "below 1 cm/s within `RunStopSec` plus one frame" measurement moved to the PIE run of story 085 (include NPC, AI-driven, human-driven and in-hand-over pawns). This test asserts the request, not the physics.
- **Open: confirm against GDD R9 (f) and ADR-0002 section 8.4 (f)** how the Crowd spy records a per-pawn stop request carrying `RunStopSec` (the AC and R9 name the bound but not the spy's record shape); freeze the shape as a fixture shared with story 055.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`): the objective-win latch that is also the round-end step (this story's dependency).
- Story 055 (`production/epics/round-match-framework/story-055-ended-hold.md`): one stop request per NPC, AI-driven and human-driven pawn, and command rejection in `Ended`.
- Story 053 (`production/epics/round-match-framework/story-053-strike-win-staging.md`): the strike-win staging.
- Story 085 (`production/epics/round-match-framework/story-085-pie-lifecycle-objective-and-reset.md`): the speed-below-1-cm/s measurement in PIE.
- Story 092 (`production/epics/round-match-framework/story-092-round-end-step-budget.md`): the 2.0 ms and 3.1 KB budget of the round-end step (Blocked on spikes).
- Crowd's Halt and stop-and-blend internals, the Penalty informant takeover, and the factory's hand-over pass (other epics).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Round/Win/RoundEndStop.cpp` (created by stories 052 and 055; extended here for pending hand-overs)
- `Source/StealthGameplay/Private/Tests/Round/HandOverPawnStopsAtEndTest.cpp` (class `FRoundHandOverPawnStopsAtEndTest`, test name `StealthGame.Round.HandOverPawnStopsAtEnd`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A pawn released from Crowd earlier in the drain gets one stop request at the round-end step, and the hand-over commits at the next frame start.
  - Given: an informant takeover committed earlier in the same drain as an objective-win latch (the pawn released from Crowd, not yet human-driven), and a Crowd spy.
  - When: the round-end step runs, then the next frame starts.
  - Then: that pawn receives one stop request with the others, carrying `RunStopSec`.
  - Then: the hand-over commits at the next frame start, on an already-stopping pawn.
  - Edge cases: the stop request comes before the hand-over commit in the call-order log; no second stop request after the commit; the speed measurement is in story 085.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/HandOverPawnStopsAtEndTest.cpp` (automation test `StealthGame.Round.HandOverPawnStopsAtEnd`, class `FRoundHandOverPawnStopsAtEndTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-052-ended-latch-effects.md`
- Unlocks: None
- Cross-epic: `production/epics/crowd-npc-behaviour` (`ICrowdControl::Halt`, stop-and-blend; stories not yet created), `production/epics/shared-character-locomotion` (`StopAllForRoundEnd`, `IsHandOverPending`, hand-over pass; stories not yet created)
