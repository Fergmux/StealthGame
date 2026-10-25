# Story 052: Round and Session stubs for TDC tests

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 4 (TDC AC test seams); traced to the epic's Exit A bootstrap; the TDC GDD names this seam in its Acceptance Criteria preamble as "a Round/Session stub for criteria whose owner GDD is unwritten")
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Tests run plain C++ cores against injected seams (fault sink, clock, hooks compiled only under `WITH_STEALTH_TEST_SEAMS`), so no pass/fail predicate depends on wall-clock time or on gameplay epics that do not exist yet.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff engine API is used; these are plain C++ test doubles. The stubs sit in `StealthFoundation/Public/Server/Testing/` (G1) so the harness module can include them; a module cannot include another module's `Private/` folder. `WITH_STEALTH_TEST_SEAMS` is defined by story 008, not here. ADR-0008 Verification 7 (whether `WITH_DEV_AUTOMATION_TESTS` exists in cooked Development game builds) is [U] and not touched by this story.

**Control Manifest Rules (this layer)**:
- Required: every `Public/Server/` header includes `StealthServerOnlyGuard.h` first — source: ADR-0007
- Required: T1 tests live in the owning module's `Private/Tests/<System>/`; every Logic story ships T1 tests — source: ADR-0008
- Forbidden: never compile negative-control types or test seams into Shipping (`WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Forbidden: never use wall-clock time as a test pass/fail predicate — source: ADR-0008

---

## Acceptance Criteria

*From the story plan and amendments (bootstrap story, no GDD AC), scoped to this story:*

- [ ] Test-only stubs in `FND/Public/Server/Testing/` exist and a self-test (`StealthGame.Harness.TuningStubsSelfTest`) shows each records exactly the calls made on it: `FStubRound` (calls BeginResolve, receives OnTuningReady/OnTuningFailed, holds a Round Event Log header buffer, releases pawns at a common tick), `FStubSession` (records the single acknowledgement per participant, reports accepted/rejected, deadline path, exposes `RejoinWindowSec` and `AcceptCommand(participant)` returning false until that participant's tuning is valid), and `FStubActionConsumer` (captures `DefenceDurationSec` into an action record at start and ends the action at start plus the captured value, never re-reading it).

---

## Implementation Notes

*Derived from ADR-0008 section 4 (Test seams), Implementation Guidelines and amendment G1:*

- **Why stubs.** The Round and Session & Travel GDDs have no code yet, but TDC acceptance criteria (AC-6c, AC-9, AC-10, AC-12c, AC-13, AC-14a, AC-16, AC-20, AC-32) are phrased against them. These stubs stand in until the real epics land and replace them with their own integration tests. Nothing here is a production API.
- **Placement (G1).** Headers under `Source/StealthFoundation/Public/Server/Testing/` with unique Stealth-prefixed names: `StealthStubRound.h`, `StealthStubSession.h` and `StealthStubActionConsumer.h` (the plan's `Private/Tests/Support/StubRound.h` and `StubSession.h` are replaced by G1; the third header name is derived by the same rule, since the plan's file list predates `FStubActionConsumer`). Each header starts with `#include "StealthServerOnlyGuard.h"` and has its whole content inside `#if WITH_STEALTH_TEST_SEAMS`.
- **`FStubRound`.** Records, in order, every call made on it. Surface: a `BeginResolve` call that forwards to an injected callable (the real `UStealthTuningService::BeginResolve` arrives in story 066, which is not a dependency, so the target must be injected, not named); receivers `OnTuningReady(uint64 TuningHash)` and `OnTuningFailed(reason)` (reasons per ADR-0004 Key Interfaces: LoadFailed, SnapshotFault); a Round Event Log header buffer the test can read back; and a pawn-release step that releases at a common tick read from an injected `IStealthClock` (story 007, `GetServerTickIndex()`). The header buffer's field list is defined by story 084 (TuningHash, PresetId, SchemaVersion, FallbackLevel, Revision, validation warnings, HotReloaded flag); here it is an opaque buffer that the test writes and reads. Whether it is a typed struct or raw fields is open until 084.
- **`FStubSession`.** Records one acknowledgement per participant, in order; a second acknowledgement from the same participant is recorded as a second one (spec edge case), never silently merged. Reports accepted or rejected per participant, has a deadline path (a rejected acknowledgement on deadline), a settable `RejoinWindowSec` (the real value belongs to the Session GDD and is not in the inputs: the test supplies it), and `AcceptCommand(participant)` that returns false until that participant's tuning is valid and true afterwards. Stories 076 and 080 assert through this gate.
- **`FStubActionConsumer`.** At action start it captures `DefenceDurationSec` into its own action record and computes the end as start plus the captured value; it never re-reads the live value afterwards. This is the Action-latch rule of ADR-0004 section 8 (manifest: Action-latched values are captured into the action record at start). Story 085 uses it as "the defence started at t=0".
- **Test.** `StealthGame.Harness.TuningStubsSelfTest`, class `FHarnessTuningStubsSelfTest`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file inside `#if WITH_DEV_AUTOMATION_TESTS` (ADR-0008 section 2). Drive scripted call sequences on each stub and assert the recorded sequence equals exactly the calls made. Use `FFakeStealthClock` (story 007) for the tick; no wall-clock reads.
- **Folder inconsistency (flag).** The plan places the test at `Private/Tests/Tuning/StubsSelfTest.cpp` although its test name uses the `Harness` system; sibling harness tests (stories 007-010) sit in `Private/Tests/Harness/`. The plan path is kept in Files; `/dev-story` may move it to `Tests/Harness/` to follow ADR-0008 section 2.
- **Fault sink.** Where story 008's helper exists, the self-test asserts zero unexpected faults at teardown; story 008 is not a declared dependency, so this is "use if present".
- **Prototype.** None: no prototype code is replaced or promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-007-stealth-clock-interface.md`: `IStealthClock` and `FFakeStealthClock` themselves
- `production/epics/tuning-data-config/story-053-fallback-failed.md`: the first consumer (AC-6c), which wires a resolve result into `FStubRound`
- `production/epics/tuning-data-config/story-084-round-log-header.md`: the Round Event Log header field list and the header assertions
- `production/epics/tuning-data-config/story-074-client-hash-handoff.md`: the client hash hand-off checked against `FStubSession`
- `production/epics/tuning-data-config/story-085-hot-reload-pie.md`: the hot-reload scenario that drives `FStubActionConsumer`
- `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`: the real `UStealthTuningService::BeginResolve`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Testing/StealthStubRound.h`
- `Source/StealthFoundation/Public/Server/Testing/StealthStubSession.h`
- `Source/StealthFoundation/Public/Server/Testing/StealthStubActionConsumer.h` (name derived by G1)
- `Source/StealthFoundation/Private/Tests/Tuning/StubsSelfTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the stubs exist in `FND/Public/Server/Testing/` and each records exactly the calls made on it (test `StealthGame.Harness.TuningStubsSelfTest`)
  - Given: `FStubRound`, `FStubSession` (with `RejoinWindowSec`, `AcceptCommand(participant)`), `FStubActionConsumer` in `FND/Public/Server/Testing/`
  - When: scripted calls are made on each
  - Then: each records exactly the calls made; `AcceptCommand` is false until that participant's tuning is valid; the action consumer captures `DefenceDurationSec` at start, ends at start plus the captured value and never re-reads it
  - Edge cases: a second acknowledgement is recorded as a second; the stubs are reachable from the harness module

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/StubsSelfTest.cpp` must exist and pass (`StealthGame.Harness.TuningStubsSelfTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `production/epics/tuning-data-config/story-007-stealth-clock-interface.md`
- Unlocks: `production/epics/tuning-data-config/story-053-fallback-failed.md`, `production/epics/tuning-data-config/story-054-validated-tuning-frozen-immutable.md`, `production/epics/tuning-data-config/story-074-client-hash-handoff.md`, `production/epics/tuning-data-config/story-076-no-pawn-before-ready.md`, `production/epics/tuning-data-config/story-079-self-check-fault.md`, `production/epics/tuning-data-config/story-080-rejoin-snapshot.md`, `production/epics/tuning-data-config/story-081-travel-epoch.md`, `production/epics/tuning-data-config/story-083-full-hash-not-replicated.md`, `production/epics/tuning-data-config/story-084-round-log-header.md`, `production/epics/tuning-data-config/story-085-hot-reload-pie.md`, `production/epics/tuning-data-config/story-099-selfcheck-and-disconnect-seams.md`
- Open verification claims: the plan Note says the stubs are replaced by the real epics' integration tests (a lifecycle note, not an engine claim). Open items found while writing: the Round header buffer's field list (story 084), the value of `RejoinWindowSec` (Session GDD), and the test-file folder (`Tests/Tuning` per plan versus `Tests/Harness`).
