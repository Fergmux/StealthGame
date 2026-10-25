# Story 001: Tests inject a fake IClock supplying server time and ServerTickIndex

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-002` (one queue per round, `Key = (ServerTickIndex, Seq)`, per-source mailboxes, permuted intake), `TR-round-003` (drain once per frame in `TG_PostPhysics`; due-time heap; resolvers per item type). GDD AC: Test seams (binding on implementation), item 1. Rules: R11, R12.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source permuted mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies ADR-0002 section 1 and section 9: `ServerTickIndex` increments once per server frame at drain start, and `ServerNow()` is the server world's `TimeSeconds` as a double, read on the game thread; no gameplay rule uses wall-clock time.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: None touched. The seam is a plain C++ interface used by the plain `FResolutionQueueCore` (ADR-0002 section 1; ADR-0007 plain-core rule), so it needs no `UWorld`. ADR-0002 Verification Required items (spawn inside `TG_PostPhysics`, client estimator accuracy, once-per-frame drain on the listen host) are not touched: they concern the real drain tick function and `EstimatedServerNow()`, which this story does not build. E6 (`UWorld::TimeSeconds` is a double) is the fact the production adapter will rely on later.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F...Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Forbidden: Never use wall-clock time as a test pass/fail predicate (only as a failure ceiling); never use wall-clock waits, unseeded randomness or shared mutable state in tests (ADR-0008)
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms listen-host game thread (ADR-0002); the seam adds one virtual call per read and no allocation

**Why this story**: Every [U] test in this epic needs to say "it is now tick N at server time T" without sleeping. The GDD makes the fake clock the first of four binding test seams. This story builds that seam before any queue code so the queue core is written against an injected clock from its first line. Exit A (Foundation) is built first and unblocks every queue consumer; this is the first Exit A story.

**Prototype disposition**: none (new Foundation code). The prototype has no queue, no server clock abstraction and no total-order key (first-come resolution in `StealthGameMode.cpp`).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md`, "Test seams (binding on implementation)" item 1, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN a [U] test, WHEN it needs server time or ServerTickIndex, THEN a fake IClock supplies both and only the test advances them (no wall-clock read). `StealthGame.Round.FakeClockSeam` [U]

---

## Implementation Notes

*Derived from ADR-0002 Implementation Guidelines and Decision sections 1 and 9, and GDD R11/R12:*

- Declare a small plain-C++ interface `IClock` (proposed shape: a server-time read returning `double` seconds and a `ServerTickIndex` read returning `uint64`) in `StealthFoundation/Public/Server/Clock/`. Open: confirm against ADR-0002 section 1 and section 9: the ADR names `ServerNow()` on `UResolutionQueue` and the `FQueueKey` type but declares no `IClock`; the GDD test-seams paragraph names it. Confirm the final interface name and signatures before creating the header, and keep them consistent with `UResolutionQueue::ServerNow()` (double, game thread, server only).
- `FResolutionQueueCore` (plain C++, no `UWorld`, built by stories 006-010) receives the clock by constructor injection. It must never read `FPlatformTime`, `UWorld::TimeSeconds` or any wall clock itself. The production adapter (reads `UWorld::TimeSeconds`; the drain tick function increments `ServerTickIndex` once per frame at drain start) is wired by the composition root and is out of scope here.
- The fake is test-only (`WITH_DEV_AUTOMATION_TESTS`), lives under `Private/Tests/`, and is never compiled into Shipping (ADR-0008: test seams only under `StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS`). It exposes `Advance(dt, ticks)` plus independent setters so a test can advance time without a tick and a tick without time.
- Headers carry the `Stealth` prefix and file names are unique across modules (ADR-0007). Server-only headers include `StealthServerOnlyGuard.h` first and live under `Public/Server/`. `IClock` carries no server secret or gameplay content, but it is server-side by use, so it goes under `Public/Server/` unless the Open item above says otherwise.
- Never route anything about the clock through `UE_LOG`/`UE_LOGF`, `ensureMsgf` text or trace channels (ADR-0007 logging policy).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004 (`production/epics/round-match-framework/story-004-test-seam-stub-resolvers.md`): stub resolvers that record keys; it consumes this fake clock.
- Story 006 (`production/epics/round-match-framework/story-006-intake-fairness.md`) and story 007 (`production/epics/round-match-framework/story-007-intake-ordering-rules.md`): the queue intake and due-time stamping that read the clock.
- The production clock adapter, the real `FResolutionDrainTickFunction` and the client `EstimatedServerNow()` estimator (ADR-0002 sections 5 and 9): later Exit A work, not part of the seam; the estimator's net-emulation acceptance test is a spike item.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Clock/StealthClock.h` (new; `IClock`)
- `Source/StealthFoundation/Private/Tests/Clock/StealthFakeClock.h` (new; test-only fake with `Advance`)
- `Source/StealthFoundation/Private/Tests/Clock/FakeClockSeamTest.cpp` (new; class `FRoundFakeClockSeamTest`)
- Tests live under the engine test roots (`tests/README.md`), never under `tests/`. `Source/StealthFoundation` does not exist yet: the tuning-data-config epic creates the module skeleton; until it lands, write under `Source/StealthGame/` and move when the skeleton arrives. Module placement follows ADR-0007 section 2.

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN a [U] test, WHEN it needs server time or ServerTickIndex, THEN a fake IClock supplies both and only the test advances them (no wall-clock read).
  - Given: a fake `IClock` injected into the queue/clock consumer.
  - When: the test reads time and tick twice with no advance, then calls `Advance(dt, ticks)`.
  - Then: both reads are identical before the advance (no wall-clock read); after the advance, time and `ServerTickIndex` equal the advanced values exactly.
  - Edge cases: the test never sleeps; tick advance without time advance and the reverse are both settable; a production-clock type is never constructed in [U] code.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Clock/FakeClockSeamTest.cpp` — automation test `StealthGame.Round.FakeClockSeam` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundFakeClockSeamTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (within this epic)
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: `production/epics/round-match-framework/story-004-test-seam-stub-resolvers.md`, `production/epics/round-match-framework/story-006-intake-fairness.md`, `production/epics/round-match-framework/story-007-intake-ordering-rules.md`, `production/epics/round-match-framework/story-008-resolver-contract.md`, `production/epics/round-match-framework/story-011-event-log-content.md`, `production/epics/round-match-framework/story-040-disconnected-at-start-spawned.md`, `production/epics/round-match-framework/story-043-no-draw-property.md`, `production/epics/round-match-framework/story-060-grace-timeline.md`
