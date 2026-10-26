# Story 009: Follow-on transitions and subscribers run depth-first in the item's slot; cascade depth 9 logs CascadeOverflow

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-003` (drain once per frame in `TG_PostPhysics`; due-time heap; resolvers per item type; inline follow-ons; Roster events flushed before the next item). GDD AC 44. Rules: R11 (follow-ons resolve inline; Roster events flush first).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source permuted mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies ADR-0002 section 5: follow-ons run synchronously, depth-first, inside the causing item's slot, bounded by `QueueCascadeDepthMax` (8, asserted); a breach logs `CascadeOverflow` and submits the remaining work as items in the order it was raised, each carrying `RootCauseKey`; after each `Resolve` the queue calls `IEventFlushBarrier::FlushPending()` and asserts nothing remains queued before dequeuing the next item.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: None touched. `IEventFlushBarrier` is a `UINTERFACE` at the facade (E7, verified); the plain core takes a raw pointer registered by the facade and the test supplies a spy. ADR-0002 Verification Required items 1 to 3 (spawn inside `TG_PostPhysics`, estimator accuracy, once-per-frame drain) are not touched.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Every interacting state change resolves as an item (or an inline follow-on in an item's slot) of the round's single queue; resolvers run follow-ons inline and never re-submit or re-stamp an outcome (ADR-0002)
- Forbidden: Never order events with a feature-local clock, timestamp, tick-function order or `FTimerManager` (ADR-0002); never route queue Event Log entries through `UE_LOG`/`UE_LOGF`, the output log or trace channels
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms listen-host game thread (ADR-0002)
- Manifest 2026-10-25g re-read at implementation: it changes no Foundation queue or Event Log rule (its changes are the S10 replication rules)

**Why this story**: When a strike kills a participant, a Roster transition, `OnPawnKilled`, its subscribers and Round's elimination check must all happen in the same slot, so no other player's action can interleave between a cause and its consequence. If they were separate items, a second strike could land between "the last Active member was struck" and "the round ends". The bound `QueueCascadeDepthMax` is a safety net: a runaway cascade is a defect that must be logged and must not reorder anything. This story builds the inline execution and flush barrier with spies, before Roster or Assassination exist. Exit A.

**Prototype disposition**: none (new Foundation code). The prototype's `CheckEliminationVictory` runs on its own schedule with no slot concept.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` Acceptance Criteria no. 44, scoped to this story (one criterion, `story_granularity: fine`):*

- [x] GIVEN an item whose resolver triggers a transition, `OnPawnKilled` and two subscribers, THEN all run depth-first inside its slot and every Roster event is dispatched before the next item is dequeued; a cascade of depth 9 logs `CascadeOverflow` and submits the rest in raised order. `StealthGame.Round.FollowOnsInline` [U]

---

## Implementation Notes

*Derived from ADR-0002 section 5 and Implementation Guidelines, and GDD R11:*

- Slot end means: after `Resolve` and after `IEventFlushBarrier::FlushPending()` have both returned. The queue calls the barrier after each `Resolve`, and asserts nothing remains queued before dequeuing the next item. The Roster implements the barrier in production (FIFO dispatch of events queued during the outermost call, Roster R12); here a spy records the order of events and the point at which `FlushPending()` is called relative to the next dequeue.
- Follow-ons (a Roster transition, `OnPawnKilled` and its subscribers, a strike-limit transition, an elimination check) execute synchronously, depth-first, inside the causing slot. Only genuinely separate events (a timer, a deferred outcome) become new items, taking the current `ServerTickIndex` and the next `Seq`, processed later in the same drain. Subscriber order is registration order.
- `QueueCascadeDepthMax` = 8 (GDD constants, "a safety bound, asserted in tests, not a balance value"). A cascade deeper than the bound is a defect: log `CascadeOverflow` and submit the remaining follow-ons as items without changing the order in which they were raised, each carrying `RootCauseKey` (the originating slot's key). Make the overflow branch a plain `if` that works in every build, with an assert only in non-Shipping, since an `ensure`/`check` alone does nothing useful in Shipping.
- The depth counter is per slot and resets between slots. A depth-8 cascade must not overflow and a depth-9 cascade must; the QA spec carries the depth-9 case from the AC and adds depth 8 only if the cap is 8. Open: confirm against GDD R11 and the Constants line (`QueueCascadeDepthMax = 8`): the cap is stated there as 8, so the depth-8 no-overflow case is expected; confirm that "depth" counts nested follow-on calls from the item's own resolver (resolver at depth 0 or 1) before writing the boundary assertion.
- Log entry `CascadeOverflow` is appended through `IRoundLogWriter` (introduced in story 008). Never pass the entry content to `UE_LOG`/`UE_LOGF`, `ensureMsgf` text or trace channels.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004 (`production/epics/round-match-framework/story-004-test-seam-stub-resolvers.md`) and story 008 (`production/epics/round-match-framework/story-008-resolver-contract.md`): the stub resolvers and the outcome contract this story builds on.
- Roster internals (the real `FRosterCore::RequestTransition`, the real event queue and `IEventFlushBarrier` implementation): participant-state-roster epic. The spy stands in here.
- The real elimination evaluation inside the flush (`TryTerminate`): Core stories 044 onward.
- The post-latch round-end step and staging pass, which also call the barrier (ADR-0002 section 8): Core stories 052 and 053.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Queue/StealthResolutionQueueCore.h` (inline depth-first follow-ons, depth bound 8, `CascadeOverflow`, `RootCauseKey`, flush barrier after every slot)
- `Source/StealthFoundation/Private/Queue/StealthResolutionQueueCore.cpp`
- `Source/StealthFoundation/Public/Server/Queue/StealthEventFlushBarrier.h` (`IEventFlushBarrier` with `HasPending()`)
- `Source/StealthFoundation/Private/Tests/Round/FollowOnsInlineTest.cpp` (test `StealthGame.Round.FollowOnsInline`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN an item whose resolver triggers a transition, `OnPawnKilled` and two subscribers, THEN all run depth-first inside its slot and every Roster event is dispatched before the next item is dequeued; a cascade of depth 9 logs `CascadeOverflow` and submits the rest in raised order.
  - Given: a resolver that triggers a transition, `OnPawnKilled` and two subscribers, with a Roster event spy.
  - When: the item is drained, then a cascade of depth 9 is raised.
  - Then: transition, `OnPawnKilled` and both subscribers all run depth-first inside the item's slot.
  - Then: every Roster event is dispatched before the next item is dequeued.
  - Then: the depth-9 cascade logs `CascadeOverflow` and submits the remaining follow-ons in raised order.
  - Edge cases: add a depth-8 case if the GDD cap is 8 (Open: confirm against GDD R11; the AC states only depth 9); subscriber order is registration order.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Round/FollowOnsInlineTest.cpp` — automation test `StealthGame.Round.FollowOnsInline` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundFollowOnsInlineTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [x] Created and passing (`StealthGame.Round.FollowOnsInline`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-004-test-seam-stub-resolvers.md`, `production/epics/round-match-framework/story-008-resolver-contract.md`
- Cross-epic: `production/epics/participant-state-roster` (contract only: the Roster implements `IEventFlushBarrier` and the FIFO event dispatch; this test uses a spy, so the Roster stories are not a build prerequisite)
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: None

---

## Completion Notes
**Completed**: 2026-10-26 (round-match-framework Exit A batch 001-017)
**Criteria**: 1/1 passing (`StealthGame.Round.FollowOnsInline`)
**Notes**: The trace is Resolve, Transition, OnPawnKilled (depth 1), Subscriber1, Subscriber2 (depth 2, registration order), the Roster event at slot end, then the next item, all under the first key. Depth 8 runs inline; depth 9 logs one `CascadeOverflow`, faults, and moves the two leaves out in raised order with the originating `RootCauseKey`, after the already-stamped next item. Depth = nested `ExecuteFollowOn` calls with the item's resolver at depth 0 (the story's Open item).
**Build/test**: see `production/qa/evidence/story-009-follow-ons-inline/full-suite.txt`
