# Story 007: IStealthClock and an injectable fake clock

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; TDC test seam: injectable clock; governed by ADR-0008 section 4; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Plain cores take an `IStealthClock` through their constructors and tests never read wall-clock time; T2/T3 pass/fail predicates use server-time windows and frame-count bounds only, with wall time allowed solely as a failure ceiling.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff API in the interface itself. The test uses `EAutomationTestFlags`, an enum class where every test needs an application-context flag (a missing one fails a `static_assert`, `AutomationTest.h:93-137, 4127`). ADR-0008 Verification Required item (7) is open [U]: until it is confirmed that `WITH_DEV_AUTOMATION_TESTS` exists in cooked Development game builds, use only `EditorContext | ProductFilter`.

**Control Manifest Rules (this layer)**:
- Required: Plain cores take an injected clock; tests never read wall-clock time or unseeded randomness (source: ADR-0008)
- Forbidden: Never use wall-clock time as a test pass/fail predicate (only as a failure ceiling); never use wall-clock waits, unseeded randomness or shared mutable state in tests (source: ADR-0008)
- Required: Every `Public/Server/` header includes `StealthServerOnlyGuard.h` first (source: ADR-0007)
- Required: Test names `StealthGame.<System>.<Scenario>`; T1 tests live in the owning module's `Private/Tests/<System>/` (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), from `tdc-plan.md`:*

- [ ] `IStealthClock` (`GetServerTimeSeconds()`, `GetServerTickIndex()`) exists in `FND/Public/Server/`, and a `FFakeStealthClock` test double advances only when told, so a core given the fake clock never reads wall-clock time (test `StealthGame.Harness.FakeClockAdvancesOnlyWhenTold`).

---

## Implementation Notes

*Derived from ADR-0008 sections 4 and Key Interfaces; amendment G1 (test support location):*

- **Interface** `Source/StealthFoundation/Public/Server/StealthClock.h`: `StealthServerOnlyGuard.h` first, then a plain C++ interface `IStealthClock` with `GetServerTimeSeconds()` and `GetServerTickIndex()`. It is a plain interface, not a `UINTERFACE`: plain cores take it by injected reference, and only interfaces held by `TWeakInterfacePtr` must be `UINTERFACE`s (ADR-0007 section 4). The return types are not fixed by the inputs (double seconds and an int64 tick index are the natural reading); confirm them against the queue/server-clock ADR (ADR-0002) before committing the signature, since the real clock implementation arrives with the queue epic.
- **Fake** `Source/StealthFoundation/Public/Server/Testing/StealthFakeClock.h` (amendment G1 replaces the plan's `Private/Tests/Support/StealthFakeClock.h`): `FFakeStealthClock` implementing `IStealthClock`, whole content under `#if WITH_STEALTH_TEST_SEAMS`, guard include first. State is changed only by explicit calls (for example `AdvanceSeconds`, `AdvanceTicks`). The spec requires the tick-index versus time relation to be explicit in the fake's API and forbids an invented tick length: keep seconds and ticks independently advanced, or take the tick duration as a constructor argument supplied by each test. Two fake instances are fully independent (no statics).
- **Sequencing flag**: `WITH_STEALTH_TEST_SEAMS` is defined by story 008, which this story does not list as a dependency. Because the fake sits entirely under that macro, it compiles to nothing until the define exists: implement the one-line define of story 008 first or land both together, or the test will not build. The plan's dependency list is unchanged.
- **Test** `StealthGame.Harness.FakeClockAdvancesOnlyWhenTold`: class `FHarnessFakeClockAdvancesOnlyWhenToldTest` in `Source/StealthFoundation/Private/Tests/Harness/FakeClockTest.cpp`, flags `EditorContext | ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. Arrange a fake and a small stand-in core that reads only the injected clock; act by reading, doing other work (no sleep; the spec says run other work so real time passes without a wall-clock wait), reading again, then advancing explicitly; assert identical reads before the advance and a change by exactly the advanced amount after; assert that two fakes are independent.
- No faults are expected; once story 008 exists, the teardown fault assertion applies to this test as to every T1 test (ADR-0008 section 4).
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`: the module and folder contract this header lives in
- `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`: `IStealthFaultSink` and the `WITH_STEALTH_TEST_SEAMS` define
- `production/epics/tuning-data-config/story-018-replication-system-assertion.md`: harness runner that reads server time
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: Round and Session stubs that use this clock
- `production/epics/tuning-data-config/story-085-hot-reload-pie.md`: the 0.25 s debounce measured on this injected clock

---

## Files

- `Source/StealthFoundation/Public/Server/StealthClock.h`
- `Source/StealthFoundation/Public/Server/Testing/StealthFakeClock.h`
- `Source/StealthFoundation/Private/Tests/Harness/FakeClockTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: `IStealthClock` exists and `FFakeStealthClock` advances only when told
  - Given: `FFakeStealthClock` implementing `IStealthClock`
  - When: the test reads `GetServerTimeSeconds` and `GetServerTickIndex`, lets real time pass (no sleep: run other work), reads again, then advances the clock explicitly
  - Then: reads are identical until advanced; after the advance they change by exactly the advanced amount; a core under test given the fake produces results independent of wall time
  - Edge cases: the tick index versus time relation must be explicit in the fake's API (no invented tick length); two fakes are independent

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Harness/FakeClockTest.cpp` (test `StealthGame.Harness.FakeClockAdvancesOnlyWhenTold`) must exist and pass (qa.level is full, so tests are required)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md` (added after review: the `WITH_STEALTH_TEST_SEAMS` define)
- Unlocks: `production/epics/tuning-data-config/story-018-replication-system-assertion.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`, `production/epics/tuning-data-config/story-085-hot-reload-pie.md`
- Sequencing note (not a plan dependency): needs the `WITH_STEALTH_TEST_SEAMS` define of `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md` to compile the fake.
- Open verification claims from the plan's Note lines: none (no Note line on 007). The return types of the interface members are open (see Implementation Notes).
