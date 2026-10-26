# Story 053: A strike win latches server-only first and presents the result at the deciding strike's death write

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-004` (Single-assignment latch and the two-step round end: latch step; round-end step at the deciding strike's death write). Also `GDD AC 12c`, `R9` (the two-step round end, latch step i-v, round-end step a-h), `R15` (the terminal latch).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source permuted mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies section 8: the latch step is server-only; Round stages exactly one `RoundEndStep` write whose `CauseKey` is the deciding slot's `RootCauseKey`; at slot end the queue binds it to the latest `DeathWrite` the slot staged and the staging pass runs it in the same server step (section 6, TypeRank `DeathWrite < CounterPose < RoundEndStep`); `ITerminalDecision::RunRoundEndStep` is Round's handler.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0002 uses no post-cutoff API (`TG_NewlySpawned` handling and `TWeakInterfacePtr` re-verified in 5.8.4 source). The three verification items (CMC pawn spawn inside `TG_PostPhysics`, the client clock estimator under net emulation, the drain running once per frame on the listen host) are none touched: this story is a plain-C++ Logic test against a fake clock, stub resolvers and spies, with no spawn, no client and no real frame loop.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Round phase flow and Forbidden; the Core Layer Rules section holds no Round-specific rule):
- Required: The latch step replicates and presents nothing; the round-end step runs in one server step, bound to the deciding slot's latest death write by `CauseKey` (or inline when the slot staged none), and calls `ICrowdControl::Halt()` plus the kind-blind stop-and-blend for every live pawn (ADR-0002).
- Forbidden: Never end the round in one step at the latch, or through an `OnRoundEnded` subscriber chain; it reveals the end before contact and subscriber order is unspecified (ADR-0002).
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (ADR-0002, ADR-0022). The round-end step's own 2.0 ms and 3.1 KB budget is story 092.

**Prototype disposition**: REPLACES `AStealthGameMode::EndRound(uint8 WinningTeam, EStealthWinReason Reason)` (`Source/StealthGame/Private/StealthGameMode.cpp`, line about 325), which ends the round in one step at the decision (winner banner, timers and cleanup together), and `CheckEliminationVictory` (line about 634), which schedules the check for the next tick. Neither stages anything: the prototype reveals the end at the instant of the decision, which R9 forbids for a strike. They are retired by this story and story 052.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 12c, scoped to this story:*

- [x] GIVEN a latch by a strike with `T_v` 0.40 s and `StrikeNetLeadSec` 0.15 s (and a second run with a `BushStrike`, 0.75 s), WHEN the server steps are recorded, THEN in the latch step only `Terminated`, the queue close and `Rejected_Latched` results occur; between the latch and the death write the replicated `FRoundPublicState` is unchanged (phase `Active`, score unchanged), no Roster record is `RoundEnded`, the Crowd spy received no stop or Halt, no `FHoldStatus` cancel and no `NotNow` reply is sent, and pawn velocities are unchanged by Round; a command submitted in that window returns `Rejected_Latched`; AND in the server step of the death write the phase `Ended`, `FRoundResult`, `MatchScore[Winner]` + 1, `EndRoundAll`, Halt, one stop request per pawn, every hold cancel and the held replies all occur, and `EndedMinDwellSec` counts from that step; AND an earlier strike's death write due after that step still writes `FDeathState`.

---

## Implementation Notes

*Derived from ADR-0002 section 8 and Implementation Guidelines, and GDD R9 / R15:*

- Latch step (R9 i-v, ADR-0002 8.2), inside the winning slot: `TryTerminate(outcome, key)` sets `Terminated`, stores `{Winner, EndReason, RoundIndex}` in Round's latch record (Round's record is authoritative; `UResolutionQueue::Latch(FQueueKey)` only closes intake and `IsLatched()` mirrors it), closes the queue (every pending item and every command, AI item or timer item submitted before the round-end step resolves `Rejected_Latched` and is logged), freezes every active hold (its completion item can only return `Rejected_Latched`; it is not cancelled and its owner's ring keeps running), leaves locomotion untouched, and parks a host `ResetRound` or `AbortRound` (a `HostCommand` item is the exception to `Rejected_Latched`; it is handed to Round at the round-end step). The IIF's owed `NotNow` replies go to the latch's held-reply buffer and are not sent. The latch step replicates and presents nothing (control manifest: Round phase flow).
- Staging (ADR-0002 8.3): in the latch step Round stages exactly one `RoundEndStep` write (`FStagedWrite` with `FRoundEndPayload`), `CauseKey` = the deciding slot's `RootCauseKey`. At slot end (after `Resolve` and `FlushPending()`) the queue binds it: if the slot staged one or more `DeathWrite`s (a kill, a reversal or a strike-limit exile, all from one `Attack` slot), the `RoundEndStep`'s `ServerTime` becomes the latest of those death writes' `ServerTime`, and the staging pass runs it immediately after that death write in the same server step. Binding is by `CauseKey`, not time equality, so it does not depend on the order in which the resolver staged its death write and raised the elimination. With the AC's fixtures the gap from slot to death write is 0.25 s (`T_v` 0.40 s minus `StrikeNetLeadSec` 0.15 s) and 0.60 s for the `BushStrike` (0.75 s minus 0.15 s), matching R9's "0.25 s after the slot at defaults, 0.60 s for a `BushStrike`"; freeze both as fixture constants in the test constant file (story 005), never inline.
- Fallback binding (ADR-0002 8.3, second bullet): if no death write matches but the deciding transition has a strike reason (`Assassinated`, `ReversedAgainst`, `StrikeLimit`), which is possible only after a `CascadeOverflow`, the binding falls back in every build (not only an assert) to the latest pending `DeathWrite` whose victim is the deciding participant's pawn, and if none exists defers the step by `StrikeNetLeadSec` from the slot; the case is logged as a defect. The non-strike case (objective win, forfeit, debug win) consumes the staged write inline at slot end and is story 052's. This story's test does not need to assert the fallback (not in the AC); keep it as a code path with its own log entry.
- Round-end step (ADR-0002 8.4, R9 a-h), run by `ITerminalDecision::RunRoundEndStep(const FRoundEndPayload&)` in Round's handler, in this order: (a) phase `Ended` and `FRoundResult {Winner, EndReason (public form), RoundIndex, TeamSummary[2]}` in `FRoundPublicState` with a forced GameState net update; (b) `Roster.EndRoundAll()` with forced net updates on every record it wrote; (c) `MatchScore[Winner]` incremented in the same `FRoundPublicState` write as (a); (d) held `NotNow` replies sent; (e) `OnRoundEnded(Result)`; (f) `ICrowdControl::Halt()` plus the kind-blind stop-and-blend for every live pawn (a body awaiting its death write still counts as live, ADR-0018); (g) every frozen hold cancels through the normal release blend; (h) `EndedMinDwellSec` and `EndedAutoResetSec` start counting from this step. The fixed order exists because delegate subscriber order is unspecified, so Round calls these directly and does not subscribe to its own `OnRoundEnded`.
- An earlier strike's death write falling due after the round-end step still plays: staged writes run even after the latch (ADR-0002 section 6, ASN-R21) and are dropped only on `Reset` by epoch mismatch. In the test, stage a second `DeathWrite` with a later `ServerTime` from an earlier slot and assert `FDeathState` is written (through the stub applier registered for `DeathWrite`).
- Test construction: use the real queue core with the fake clock (story 001), stub resolvers (story 004), an Assassination stub that stages a `DeathWrite` with the fixture times, a stub `DeathWrite` applier, a Roster gateway spy, a Crowd spy, an IIF spy (holds, `NotNow` replies) and a replicated-struct spy; record the order of server steps. Open: confirm which Foundation story delivers the section 6 staging pass and the section 8.3 binding in `FResolutionQueueCore`; none of the Foundation ACs (stories 001 to 017) names them, so this story may need to wait on that code or carry it.
- Open: confirm against GDD R9 and Assassination ASN-R21 the `ContactServerTime - StrikeNetLeadSec` death-write time (the GDD gives 0.25 s and 0.60 s at defaults; the AC gives `T_v` and `StrikeNetLeadSec` instead of the times).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`): the objective-win latch that is also the round-end step (nothing staged), result, score, `EndRoundAll`, rejections and `PostRound` viewers.
- Story 054 (`production/epics/round-match-framework/story-054-team-summary-at-ended.md`): the contents of `TeamSummary`.
- Story 055 (`production/epics/round-match-framework/story-055-ended-hold.md`): the Ended command rejection and the per-pawn stop requests with `RunStopSec`; this story only asserts that the stops occur at the death write and not before.
- Story 056 (`production/epics/round-match-framework/story-056-round-ended-arrival-order.md`): client arrival skew between GameState and Roster records.
- Story 092 (`production/epics/round-match-framework/story-092-round-end-step-budget.md`): the 2.0 ms and 3.1 KB budget of the round-end step (Blocked on spikes).
- Assassination's `DeathWrite` applier and payload meaning (ADR-0018) and Objectives' final completion: stubbed here. Informant respawn scheduling (Penalty) after a latch: Round makes no call after a latch (R17, informant awaiting a body).
- The pawn stop-and-blend implementation (ADR-0010 and Shared Character) and Crowd's Halt behaviour (Crowd R10): spied here.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/StrikeWinStagingTest.cpp` (class `FRoundStrikeWinStagingTest`, test name `StealthGame.Round.StrikeWinStaging`)
- `Source/StealthGameplay/Public/Round/` terminal-decision header (new; `ITerminalDecision` implementation and latch-record types; Open: name set when implemented)
- `Source/StealthGameplay/Private/Round/Win/` latch step and round-end step implementation (new)
- `Source/StealthGame/Private/StealthGameMode.cpp` and `Source/StealthGame/Public/StealthGameMode.h` (`EndRound`, `CheckEliminationVictory`; retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A strike win latches server-only first and presents the result at the deciding strike's death write.
  - Given: a latch by a strike with `T_v` 0.40 s and `StrikeNetLeadSec` 0.15 s, and a second run with a `BushStrike` (0.75 s).
  - When: the server steps are recorded.
  - Then: in the latch step only `Terminated`, the queue close and `Rejected_Latched` results occur.
  - Then: between the latch and the death write `FRoundPublicState` is unchanged (phase `Active`, score unchanged).
  - Then: in that window no Roster record is `RoundEnded`, the Crowd spy received no stop or Halt, no `FHoldStatus` cancel or `NotNow` reply is sent, and pawn velocities are unchanged by Round.
  - Then: a command submitted in that window returns `Rejected_Latched`.
  - Then: in the death-write step the phase is `Ended`, `FRoundResult` is written, `MatchScore[Winner]` rose by 1, `EndRoundAll` ran, Halt, one stop request per pawn, every hold cancel and the held replies all occur, and `EndedMinDwellSec` counts from that step.
  - Then: an earlier strike's death write due after that step still writes `FDeathState`.
  - Edge cases: the gap between latch and death write is derived from the F-formula, not stated in the AC, so freeze it as a fixture; both runs repeat every assertion.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/StrikeWinStagingTest.cpp` — must exist and pass (automation test `StealthGame.Round.StrikeWinStaging`, class `FRoundStrikeWinStagingTest`)

**Status**: [x] Created and passing (`StealthGame.Round.StrikeWinStaging`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-052-ended-latch-effects.md`
- Unlocks: `production/epics/round-match-framework/story-056-round-ended-arrival-order.md`, `production/epics/round-match-framework/story-092-round-end-step-budget.md`

Cross-epic: `production/epics/participant-state-roster` (`EndRoundAll`, `RoundEnded` records; stories not yet created); `production/epics/input-interaction-framework` (held `NotNow` replies, `FHoldStatus` cancel, resolver registration; stories not yet created); `production/epics/shared-character-locomotion` (kind-blind stop-and-blend path; stories not yet created); `production/epics/crowd-npc-behaviour` (`ICrowdControl::Halt()`, spied here; stories not yet created); `production/epics/tuning-data-config` (`EndedMinDwellSec`, module skeleton and StealthTestHarness bootstrap; stories not yet created)

## Completion Notes
**Completed**: 2026-10-26 (RMF round-end and disconnect batch: stories 048-065, two-step round end, TeamSummary, grace/forfeit/void)
**Criteria**: 1/1 passing (T_v 0.40 and BushStrike 0.75: latch step only, nothing observable until the death write; the death write's step runs (a)-(h); dwell from it; an earlier death write still applies after Ended)
**Deviations**: Carried the ADR-0002 s6 staging pass and the s8.3 binding in FResolutionQueueCore (no Foundation story delivered them, the Open item); FRoundEndPayload amended (bStrikeDecided, DecidingVictim, FallbackDeferSec); prototype strikes outside the queue defer the round-end step by AdapterStrikeRoundEndDelaySec (0.25 s)
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/StrikeWinStagingTest.cpp`; full suite: `production/qa/evidence/rmf-round-end-disconnects-2026-10-26/full-suite.txt`
**Run result**: PASS - 3-player PIE strike win: latched at 36.733 s, Ended at 37.066 s (the first ~0.33 s PIE frame at or after the 0.25 s stand-in death write; see evidence)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, RMF round-end batch; findings and fixes in production/qa/evidence/rmf-round-end-disconnects-2026-10-26/evidence.md
