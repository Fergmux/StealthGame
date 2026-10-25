# Story 092: The round-end step adds at most 2.0 ms host time and about 3.1 KB in a 10-player, 60-pawn round

> **Epic**: Round & Match Framework
> **Status**: Blocked — BLOCKED: the ADR-0002 round-end spike and the ADR-0022 R1-R3 performance spikes (RTX 3060-class) have not reported. Unblocks when those spikes report on RTX 3060-class hardware (listen host, 9 clients, 60 pawns); then re-check the story with `/story-readiness`.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-004` (Single-assignment latch and the two-step round end: latch step; round-end step at the deciding strike's death write); GDD AC 65 ([PIE-10] [SPIKE]; a miss is a defect against R9, cross-review S-W3); rules R9 (budget of the round-end step).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round, drained once per frame in `TG_PostPhysics`, with typed staged writes, a single-assignment latch and the R9 two-step round end. This story applies section 8.4: the round-end step is one server step that publishes the result, runs `EndRoundAll`, sends held replies, fires `OnRoundEnded`, calls `ICrowdControl::Halt()` and the kind-blind stop-and-blend (`UStealthPawnFactory::StopAllForRoundEnd()`) for every live pawn, and cancels frozen holds; GDD R9 caps it at 2.0 ms of listen-host game-thread time and about 3.1 KB of non-movement bytes.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: Touches ADR-0002 Verification items (1) (spawning a Shared Pawn inside `TG_PostPhysics`, and that destruction is always deferred) and (3) (the drain tick function runs exactly once per server frame after CMC and AI ticks on the listen host, whose server frame is its render frame; `NetServerMaxTickRate` has no effect on a listen server unless `bClampListenServerTickRate` is set, so the condition is host frame time <= 1/60 s, measured by the harness frame-time capture). The byte budget excludes pawn movement update bytes and is counted by network channel; the exact per-channel counter is a harness facility (ADR-0008). ADR-0022 owns the CPU/performance spikes R1-R3. No post-cutoff API is called directly.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules):
- Required: The latch step replicates and presents nothing; the round-end step runs in one server step, bound to the deciding slot's latest death write by `CauseKey` (or inline when the slot staged none), and calls `ICrowdControl::Halt()` plus the kind-blind stop-and-blend for every live pawn (ADR-0002).
- Forbidden: Never end the round in one step at the latch, or through an `OnRoundEnded` subscriber chain - it reveals the end before contact; subscriber order is unspecified (ADR-0002).
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread (ADR-0002, ADR-0022). This story's 2.0 ms and about 3.1 KB are the separate design limits of GDD R9 for the round-end step, not tuning keys.

**Prototype disposition**: None (new Core performance test; no prototype equivalent).

**Where it sits**: Exit B (Core), the budget gate for the R9 round-end step (story 052 builds the latch effects, story 053 the strike staging). It runs in the largest session the harness supports (listen host plus 9 clients, 60 pawns) for both ending types. A miss is a defect against R9, not a tuning change.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 65 (revised in `rmf-ac-overrides.md`), scoped to this story:*

- [ ] In a listen-host session with 9 clients and 60 pawns at the shipped config, on RTX 3060-class hardware, over 3 runs of 300 drain frames each, the added host game-thread time of the round-end step is at most 2.0 ms over the median drain frame and the host's bytes sent in that frame to all clients, excluding pawn movement update bytes (counted by channel), are at most 3,150 bytes (9 x 350 B), for both an objective win and a strike.

---

## Implementation Notes

*Derived from ADR-0002 section 8.4 and Performance Implications, and GDD R9 (Budget of the round-end step):*

- **What the step does (R9 a-h).** Publishes `FRoundResult` and `FRoundPublicState` (about 64 B) with a forced net update; `EndRoundAll()` with forced net updates on every Roster record written (at most `2 x TeamSize` = 10, each at most 96 B for the coalesced C-TeamView update, plus about 16 B of PlayerState delta); clears at most 10 team ping parts (about 24 B each, an estimate); sends the held `NotNow` replies; fires `OnRoundEnded`; Halt and one stop request for each of about 60 pawns (`D = P + NpcCount` = 60 at defaults, plus pawns in hand-over); cancels the frozen holds.
- **CPU limit.** At most 2.0 ms of listen-host game-thread time over a normal drain frame (about 12 % of the 16.7 ms frame at 60 Hz): 60 pawn stops at no more than 30 microseconds each is 1.8 ms plus 0.2 ms for the records, the GameState and the ping clear. Measure the added time as the round-end drain frame minus the median of the 300 drain frames before the end in that run.
- **Byte limit.** Each client receives its own C-TeamView (at most 96 B) and ping part (about 24 B), the 10 PlayerState deltas (at most about 160 B) and the GameState update (about 64 B): about 0.35 KB per client, so at most 3,150 bytes (9 x 350 B) across 9 clients in that frame, counted before the pawn movement updates the stops cause (those stay inside SCL F9's pawn line). The test must name the per-channel byte counter it uses (excluding the movement channel) and read it for the frame the step runs in.
- **Runs.** QA specifies 3 runs of 300 drain frames each, for an objective win and for a strike; the AC does not state how the 3 runs aggregate. QA's default is the strictest reading: assert the limits in every run, giving 3 runs per ending type and 6 in total. Open: confirm the aggregation against GDD AC 65. For a strike, the round-end step is bound to the deciding slot's death write (ADR-0002 section 8.3), so measure the frame in which the death write and the step run.
- **Hardware.** RTX 3060-class, at the shipped config, with the harness frame-time capture confirming the listen host holds <= 1/60 s outside the round-end frame. Open: confirm against ADR-0008 and ADR-0022 which tier runs a 10-process session (the GDD tag is [PIE-10]: T2 in-editor PIE with many clients versus a T3 Gauntlet multi-process node `StealthGame.Round.MP.RoundEndStepBudget`).
- **Failure meaning.** A miss is a defect against R9, to be fixed in the step (for example by batching the stop requests), never by raising the limit.
- **Gate.** Do not start until the ADR-0002 round-end spike and ADR-0022 R1-R3 report; `/story-readiness` re-confirms.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`): the objective-win latch and its effects (this story's dependency).
- Story 053 (`production/epics/round-match-framework/story-053-strike-win-staging.md`): the strike-win staging and the death-write binding (this story's dependency).
- Story 055 (`production/epics/round-match-framework/story-055-ended-hold.md`): the stop requests and rejected commands in `Ended`, at unit level.
- Story 085 (`production/epics/round-match-framework/story-085-pie-lifecycle-objective-and-reset.md`): the two-client speed measurement after an objective win.
- The drain's own 0.5 ms line (queue, staging, teardown and Roster flush; Foundation queue work) and the ADR-0022 CPU spikes themselves; pawn movement bandwidth (Shared Character F9).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthTestHarness/Private/Round/RoundEndStepBudgetTest.cpp` (CQTest `NETWORK_TEST_CLASS(RoundEndStepBudget, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.RoundEndStepBudget.<Method>`; the GDD scenario name is `StealthGame.Round.RoundEndStepBudget`; tier to be confirmed per the Open item above)
- `Source/StealthFoundation/Private/Tests/Fixtures/StealthRoundTestConstants.h` (fixture constants file from story 005; the 2.0 ms and 3,150 B limits and the run counts)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: The round-end step adds at most 2.0 ms host time and at most 3,150 bytes in a 9-client, 60-pawn round.
  - Given: a listen-host session with 9 clients and 60 pawns at the shipped config, on RTX 3060-class hardware.
  - When: 3 runs of 300 drain frames each end the round, for an objective win and for a strike.
  - Then: the added host game-thread time of the round-end step is at most 2.0 ms over the median drain frame.
  - Then: the host's bytes sent in that frame to all clients, excluding pawn movement update bytes (counted by channel), are at most 3,150 bytes (9 x 350 B).
  - Then: both hold for the objective win and for the strike.
  - Edge cases: the AC does not say how the 3 runs aggregate, so assert in every run and treat the strictest reading as the default (Open: confirm against GDD AC 65); that gives 3 runs per ending type, 6 in total; take the median over the 300 drain frames before the end in each run; the per-channel byte counter must be named in the test; a miss is a defect against R9; the story stays blocked on the ADR-0002 round-end spike and the ADR-0022 R1-R3 spikes.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/RoundEndStepBudgetTest.cpp` (CQTest `NETWORK_TEST_CLASS(RoundEndStepBudget, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`; performance thresholds are the AC's, and the test fails if exceeded) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-053-strike-win-staging.md`, `production/epics/round-match-framework/story-052-ended-latch-effects.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (`EndRoundAll` and the forced record updates counted in the budget; stories not yet created), `production/epics/shared-character-locomotion` (`StopAllForRoundEnd()` and the per-pawn stop cost; stories not yet created), `production/epics/crowd-npc-behaviour` (`Halt()`; stories not yet created), `production/epics/input-interaction-framework` (held `NotNow` replies and hold cancels in the step; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
