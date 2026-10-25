# Story 055: In Ended every command except Look is rejected and every pawn gets one stop request carrying RunStopSec

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-004` (Single-assignment latch and the two-step round end). Also `GDD AC 12b` (revised: see `rmf-ac-overrides`, the speed clause moved to story 085), `R9` step (f) and "From the round-end step all commands are rejected by class `RoundEnded` (IIF R7, Look allowed)".
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source permuted mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies section 8.4(f): the whole city holds, because Round calls `ICrowdControl::Halt()` and the Shared Pawn's kind-blind server stop-and-blend for every live pawn in the round-end step, chosen over an `OnRoundEnded` subscriber because R9 requires a fixed order inside the step. The stop path itself is owned by ADR-0010.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0002 uses no post-cutoff API; its three verification items (CMC pawn spawn in `TG_PostPhysics`, client estimator, listen-host frame) are none touched. This story counts stop requests on a Crowd spy and does not run movement, so none of ADR-0010's CMC or possession spikes (S8, S14, H1, P1, traversal) is touched; the speed-below-1-cm/s measurement is story 085's PIE run.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Round phase flow and Forbidden; the Core Layer Rules section holds no Round-specific rule):
- Required: the round-end step calls `ICrowdControl::Halt()` plus the kind-blind stop-and-blend for every live pawn (ADR-0002); one stop request per pawn, AI-driven and human-driven alike.
- Forbidden: Never stop human pawns at round end by input loss (ADR-0002); they stop through the same server-driven stop-and-blend path as the AI pawns.
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (ADR-0002, ADR-0022). The step's 60 pawn stops at no more than 30 microseconds each are budgeted in R9 and verified by story 092.

**Prototype disposition**: REPLACES the prototype's end-of-round behaviour in `AStealthGameMode::EndRound` (`Source/StealthGame/Private/StealthGameMode.cpp`, line about 325), which leaves NPCs and pawns walking and relies on the winner banner and an auto-restart timer (`RoundRestartTimer`). The prototype has no Ended command gate and no crowd halt. Retired by this story and story 052.

---

## Acceptance Criteria

*From the revised GDD AC 12b (`rmf-ac-overrides`, 055), scoped to this story:*

- [ ] GIVEN Ended, THEN every command except Look is rejected, and the Crowd spy received exactly one stop request per NPC, AI-driven, human-driven and in-hand-over pawn in the round-end step, each carrying `RunStopSec` as its bound. (The pawn-speed < 1 cm/s within `RunStopSec` plus one frame clause is asserted in the PIE run of story 085.)

---

## Implementation Notes

*Derived from ADR-0002 section 8.4 and GDD R9 (f), (g), (h):*

- Command gate: from the round-end step, every command is rejected by class `RoundEnded` (IIF R7), Look allowed; the queue is already closed since the latch step (`Rejected_Latched`), so a command arriving in `Ended` is rejected at the IIF's class check and the resolver is never reached. Round's part is to publish the class (phase `Ended`) in the same step; the rejection reply code and class table belong to the IIF epic. Open: confirm against IIF R7 whether the post-round-end rejection is reported as `RoundEnded` or `Rejected_Latched` for items already submitted before the step (R9 latch step (ii) says items submitted before the round-end step return `Rejected_Latched`).
- Stop set (R9 f): the round-end step enumerates, in a fixed order inside the step, (1) NPCs and AI-driven pawns through `ICrowdControl::Halt()` (Crowd stops issuing commands and brings every NPC and AI-driven pawn to a stop by the normal stop-and-blend path), (2) human-driven pawns by the same server-driven stop-and-blend, never by input loss and predicted-velocity decay (which would differ from the AI pawns' stop and mark who was human), (3) pawns in hand-over, namely a pawn whose takeover or hand-over was committed earlier in the drain but is not yet human-driven (neither in Crowd's Halt nor in the human-driven set): Round enumerates the pending hand-overs and applies the same stop-and-blend by raising Halt on the pawn itself, which holds whichever controller drives it. "Live" means strike role not `Dead`; a body awaiting its death write still counts (ADR-0002 8.4(f), ADR-0018).
- "Exactly one stop request per pawn, each carrying `RunStopSec`": the stop request is one call per pawn on the kind-blind stop interface with `RunStopSec` as its bound (a Shared Character key; Crowd R10 bounds the stop by it). The Crowd spy records `(pawn, bound)` and the test asserts the multiset equals {every live pawn, once} with `bound == RunStopSec` for all four pawn kinds. No pawn gets two requests (a pawn that is both AI-driven and listed in the hand-over table is the case to guard) or none.
- Later drains send no further stop requests: the Ended phase has no queue items except the post-latch lane (`EndedMinDwell`, `EndedAutoReset`), and no stop is issued from them.
- The speed clause is not asserted here: whether each pawn is below 1 cm/s within `RunStopSec` plus one frame is measured in the PIE run of story 085 (include NPC, AI-driven and human-driven pawns).
- Where it lives: the round-end step handler in the Round Core (planned `Source/StealthGameplay/Private/Round/Win/`), calling Crowd through the consumer-owned `ICrowdControl` gateway injected by the composition root (story 003 supplies the fake). Open: confirm the name of the kind-blind stop interface and the exact `RunStopSec` parameter against ADR-0010 and the Shared Character GDD (not part of ADR-0002's Key Interfaces).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`): the latch effects and the rest of the round-end step (result, score, `EndRoundAll`, rejections of pending items, `PostRound` viewers).
- Story 053 (`production/epics/round-match-framework/story-053-strike-win-staging.md`): the staging of the round-end step at a strike's death write.
- Story 078 (`production/epics/round-match-framework/story-078-hand-over-pawn-stops-at-end.md`): the same-drain informant takeover case in detail (a pawn released from Crowd, hand-over committing at the next frame start); this story covers the in-hand-over pawn only as one of four kinds counted once.
- Story 085 (`production/epics/round-match-framework/story-085-pie-lifecycle-objective-and-reset.md`): the speed below 1 cm/s measurement in the real PIE run.
- Story 092 (`production/epics/round-match-framework/story-092-round-end-step-budget.md`): the 2.0 ms and 3.1 KB budget (Blocked on spikes).
- Crowd R10 stop behaviour and the stop-and-blend implementation (crowd and shared-character epics); IIF reply codes and the class table (input epic).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/EndedHoldTest.cpp` (class `FRoundEndedHoldTest`, test name `StealthGame.Round.EndedHold`)
- `Source/StealthGameplay/Private/Round/Win/` stop-set enumeration within the round-end step (new; shared with stories 052, 053 and 078)
- `Source/StealthGame/Private/StealthGameMode.cpp` (`EndRound`, `RoundRestartTimer`; retired/replaced by story 052 and this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: In Ended, every command except Look is rejected, and the Crowd spy got one stop request per pawn, each carrying `RunStopSec`.
  - Given: a round in `Ended` with NPC, AI-driven, human-driven and in-hand-over pawns, and a Crowd spy.
  - When: the round-end step runs, then a command of each type is submitted.
  - Then: every command except Look is rejected.
  - Then: the Crowd spy received exactly one stop request per NPC, AI-driven, human-driven and in-hand-over pawn, in the round-end step.
  - Then: each stop request carries `RunStopSec` as its bound.
  - Edge cases: no pawn gets two requests or none; Look is accepted; a later drain sends no further stop requests; the speed clause is not asserted here (see 085).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/EndedHoldTest.cpp` — must exist and pass (automation test `StealthGame.Round.EndedHold`, class `FRoundEndedHoldTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-052-ended-latch-effects.md`
- Unlocks: None

Cross-epic: `production/epics/crowd-npc-behaviour` (`ICrowdControl::Halt()` and Crowd R10 stop; spied here; stories not yet created); `production/epics/shared-character-locomotion` (kind-blind stop-and-blend, `RunStopSec`; stories not yet created); `production/epics/input-interaction-framework` (`RoundEnded` command class, IIF R7; stories not yet created); `production/epics/participant-state-roster` (`EndRoundAll`; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
