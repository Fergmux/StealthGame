# Story 004: Tests register stub resolvers per queue item type

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-003` (drain once per frame in `TG_PostPhysics`; due-time heap; resolvers per item type; inline follow-ons; Roster events flushed before the next item). GDD AC: Test seams (binding on implementation), item 4. Rules: R13.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source permuted mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies ADR-0002 section 5: resolvers register per item type (and per `TimerFired` subkind) through `RegisterResolver(EQueueItemType, TWeakInterfacePtr<IQueueResolver>)`, one resolver per type, weak registrations cleared in `Deinitialize`; `FResolutionQueueCore` takes raw pointers registered by the facade so headless specs inject stubs without UObjects.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Touches `TWeakInterfacePtr` and `UINTERFACE` only at the facade (E7: `TWeakInterfacePtr<T>` requires a UHT `UINTERFACE`, so `IQueueResolver` is a `UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))`; verified in 5.8.4 source). The plain core used by this story takes raw pointers, so the test needs no UObject. No post-cutoff API is called. ADR-0002 Verification Required items 1 to 3 (spawn in `TG_PostPhysics`, estimator accuracy, once-per-frame drain) are not touched.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Every interacting state change resolves as an item (or an inline follow-on in an item's slot) of the round's single queue; nothing outside the drain mutates gameplay state (ADR-0002)
- Forbidden: Never declare a type that `FQueueItem` or `FStagedWrite` names in a feature module; every queue item payload lives in `StealthFoundation/Public/Server/` (ADR-0002, ADR-0007)
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms listen-host game thread (ADR-0002); a stub resolver adds one virtual call and one vector append per item

**Why this story**: The queue is Round's heart, but Round does not know what a command means (R13): each owning system registers a resolver. Before those systems exist, every queue and Round test needs a stub that can be registered for any item type, resolve with a scripted result, and record which keys it saw. This story builds the stub and the registration seam against the real item-type enum, so the queue stories (006-010) and the Core stories reuse one stub. Exit A seam story; it consumes story 001's fake clock.

**Prototype disposition**: none (new Foundation code). The prototype resolves in first-come order with no resolver registry.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md`, "Test seams (binding on implementation)" item 4, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN a [U] test, THEN stub resolvers can be registered for any queue item type and record the keys they resolve. `StealthGame.Round.StubResolversSeam` [U]

---

## Implementation Notes

*Derived from ADR-0002 section 1 (closed item-type set), section 5 (resolver registration) and Key Interfaces:*

- The item types are the closed set `EQueueItemType { PawnCommand, HoldTimer, TimerFired, PawnLost, ConnectionTransition, HostCommand, DebugItem }` (`DebugItem` is `!UE_BUILD_SHIPPING` only), with `TimerFired` subkinds in `ETimerKind` registered through `RegisterTimerResolver(ETimerKind, ...)`. Types and payloads are declared in `StealthFoundation/Public/Server/StealthQueuePayloads.h` (ADR-0002 section 2a) behind the server-only guard. A test stub must be registrable for every value of both enums.
- `IQueueResolver::Resolve(const FQueueItem&, FQueueKey) -> EResolveResult { Executed, Voided }`. The stub records `FQueueKey` per call in resolve order and returns a scripted result. It never changes game state.
- Only the resolver registration surface and the minimum of `FResolutionQueueCore` needed to drain a submitted item to its resolver are built here; intake permutation (006), due-time stamping and caps (007), the resolver outcome contract (008), inline follow-ons (009) and epoch rejection (010) are separate stories. If the core does not exist yet, create it with the smallest drain loop that dispatches by type and no more.
- An item whose type has no registered resolver is reported (fault sink call plus a returned status), never silently dropped. Open: confirm against ADR-0002 section 5 and the QA spec: the ADR says "one resolver per type", so registering twice for one type must either be rejected or replace the earlier registration; the rule is not stated as one or the other. Confirm which before writing the double-registration assertion.
- Registrations are weak and cleared in `Deinitialize` (ADR-0002 section 5, ADR-0007); registered only from the composition root in production, directly in tests.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 001 (`production/epics/round-match-framework/story-001-test-seam-fake-clock.md`): the fake clock (this story depends on it).
- Story 006 (`production/epics/round-match-framework/story-006-intake-fairness.md`), story 007 (`story-007-intake-ordering-rules.md`): intake permutation, due-time stamping, per-drain cap.
- Story 008 (`production/epics/round-match-framework/story-008-resolver-contract.md`): the `Voided` / in-slot / deferred outcome contract and key accessors.
- Story 009 (`story-009-follow-ons-inline.md`) and story 010 (`story-010-stale-epoch-item.md`): inline follow-ons and stale-epoch rejection.
- The real resolvers: IIF, Objectives, Assassination, Penalty, Crowd and Concealment register their own in their own epics.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/StealthQueuePayloads.h` (new or extended; `EQueueItemType`, `ETimerKind`, `FQueueKey`, `FQueueItem`, `EResolveResult`)
- `Source/StealthFoundation/Public/Server/Queue/StealthQueueResolver.h` (new; `IQueueResolver`)
- `Source/StealthFoundation/Public/Server/Queue/StealthResolutionQueueCore.h` and `Source/StealthFoundation/Private/Queue/StealthResolutionQueueCore.cpp` (new or extended; registration and the minimal drain)
- `Source/StealthFoundation/Private/Tests/Queue/StealthStubResolver.h` (new; test-only recording stub)
- `Source/StealthFoundation/Private/Tests/Queue/StubResolversSeamTest.cpp` (new; class `FRoundStubResolversSeamTest`)
- Tests live under the engine test roots (`tests/README.md`), never under `tests/`. `/dev-story` replaces this planned list with the real one. `Source/StealthFoundation` does not exist yet (tuning-data-config epic creates the skeleton); write under `Source/StealthGame/` until it lands.

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN a [U] test, THEN stub resolvers can be registered for any queue item type and record the keys they resolve.
  - Given: the fake clock (story 001) and a stub resolver registered per item type.
  - When: items of each type are submitted and drained.
  - Then: each stub records exactly the keys of its own type, in resolve order.
  - Edge cases: an item type with no registered resolver is reported, not silently dropped; registering twice for one type is rejected or replaces, per the registry contract (Open: confirm against ADR-0002 section 5); multiple types in one drain.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Queue/StubResolversSeamTest.cpp` — automation test `StealthGame.Round.StubResolversSeam` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundStubResolversSeamTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-001-test-seam-fake-clock.md`
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: `production/epics/round-match-framework/story-006-intake-fairness.md`, `production/epics/round-match-framework/story-007-intake-ordering-rules.md`, `production/epics/round-match-framework/story-008-resolver-contract.md`, `production/epics/round-match-framework/story-009-follow-ons-inline.md`, `production/epics/round-match-framework/story-010-stale-epoch-item.md`
