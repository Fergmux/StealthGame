# Story 086: AC-14b HotReloadRejected

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-010`; GDD AC-14b (CR-12)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: In a WITH_EDITOR build the store subscribes to `FCoreUObjectDelegates::OnObjectPropertyChanged` and accepts only events on the preset in use whose `ChangeType` is `EPropertyChangeType::ValueSet` with a non-null property. It debounces 0.25 s, re-validates through the CR-6 ladder, and a failing set is rejected so the previous set stays live.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: ADR-0004 Engine Compatibility lists no post-cutoff API for this story. `OnObjectPropertyChanged`, `EPropertyChangeType::Interactive` and `ValueSet` are tagged [V] (`UObjectGlobals.h:3293`, `UnrealType.h:6957-6961`). That undo arrives through `PostEditUndo` as an `Unspecified` event with a null property is [V] (`Obj.cpp:885-889`). ADR-0004 Verification Required (3) (the delegate fires with `ValueSet` once per committed edit on a data asset open in a details panel during PIE) is still [U]. This story's spec drives synthetic `FPropertyChangedEvent` values, so it does not observe the real details panel; whether story 085 records that observation is open. The T2 PIE test is subject to ADR-0008 Verification 1 [U] (story 019).

**Control Manifest Rules (this layer)**:
- Required: Hot-reload, `?Tuning=` and console-command code sits inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` blocks (also `!UE_BUILD_TEST` for `stealth.tuning.preset`) — source: ADR-0004
- Required: Invariant `Check` functions are pure and take `const` input; only F-1 changes a value; invariants reject or warn, never repair — source: ADR-0004
- Forbidden: Never use wall-clock time as a test pass/fail predicate (only as a failure ceiling); never disable, skip or reseed a failing test to pass a gate — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-14b (CR-12)**: GIVEN PIE, WHEN a committed edit violates I-1, THEN it is rejected, and the previous frozen set and `Revision` stay live. AND 20 interactive (non-committed) slider ticks raise `Revision` by 0. (`StealthGame.Tuning.HotReloadRejected`)

---

## Implementation Notes

*Derived from ADR-0004 section 9 and Implementation Guidelines 5 and 10:*

This story hardens the WITH_EDITOR hot-reload block that story 085 creates in `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp`. Story 085 proves the accept path (a committed edit re-validates and publishes `Revision + 1`). This story proves the reject paths: an invariant failure on a committed edit, and every event kind the filter must ignore.

- Filter rule (ADR-0004 s9, CR-12): accept an event only when the changed object is the preset in use, `ChangeType == EPropertyChangeType::ValueSet`, and `GetProperty()` is non-null. `Interactive` events (slider drags, which the engine follows with one `ValueSet`) and the null-property `Unspecified` event raised by `PostEditUndo` are ignored without touching `Revision`. Rejecting only `Interactive` would still let undo through.
- Reject path: a committed edit that breaks I-1 (for example `DefenceDurationSec` 5 with `DefenceCooldownSec` 35, the AC-6a fixture) fails the CR-6 ladder on the live set. The previous `TSharedPtr<const FValidatedTuning>` stays published, `Revision` is unchanged, and the failure is logged on `LogStealthFoundation` through `UE_LOGF`. The invariant rejects; nothing clamps or repairs the dependent key.
- Test (`Source/StealthTestHarness/Private/Tests/PIE/HotReloadRejectedTest.cpp`): `NETWORK_TEST_CLASS(HotReloadRejected, "StealthGame.Tuning.PIE")` with `FPIENetworkComponent` (`AsListenServer().WithClients(N).WithGameMode(AStealthGameMode::StaticClass())`, fixture map loaded in the first step), inside `#if ENABLE_PIE_NETWORK_TEST`. The full name is `StealthGame.Tuning.PIE.HotReloadRejected.<Method>`; the GDD's shorter name `StealthGame.Tuning.HotReloadRejected` is the criterion label only. The synthetic `FPropertyChangedEvent` values are built in the test and fed to the same entry point the delegate calls. Time comes from the injected clock (story 007) and the injectable debounce timer introduced by story 085, never wall time. Wall clock is only a failure ceiling.
- Expected-fault discipline: the rejected commit is an expected `LogStealthFoundation` warning or error. Declare it through the fault sink and log-capture seams (stories 008 and 009) so the zero-unexpected-faults teardown assertion still holds.
- Per G4 and story 019's fallback: if T2 cannot run headless, this test runs in a non-headless editor on the dev machine under the ADR-0008 amendment request that story 019 records. It is never skipped or disabled.
- Prototype: none. The prototype `UStealthSettings` has no hot-reload path; this code is new.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-085-hot-reload-pie.md`: the accept path, the live-set rule, the 0.25 s debounce, routing to the PIE server world, `Revision + 1` and the `HotReloaded` flag (AC-14a).
- `production/epics/tuning-data-config/story-087-hot-reload-absent-packaged.md`: proof that the hot-reload code compiles only under `WITH_EDITOR` (AC-14c).
- `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`: the sentinel literal on the hot-reload hook.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp
- Source/StealthTestHarness/Private/Tests/PIE/HotReloadRejectedTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-14b (CR-12) rejection and interactive-tick behaviour
  - Given: a PIE session (listen server plus clients) with a preset in use and the frozen set live; synthetic `FPropertyChangedEvent` sources.
  - When: (a) a committed edit (`ValueSet`, non-null property) violating I-1 is delivered; (b) 20 interactive (non-committed) slider ticks are delivered; (c) a null-property event and an undo event are delivered.
  - Then: the I-1 edit is rejected and the previous frozen set and `Revision` stay live; the 20 interactive ticks raise `Revision` by 0; the null-property and undo events are ignored.
  - Edge cases: a following valid commit still works (re-validates and publishes normally after a rejection).

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest `NETWORK_TEST_CLASS(HotReloadRejected, "StealthGame.Tuning.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/HotReloadRejectedTest.cpp` must exist and pass (or a documented playtest under the story 019 fallback).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-085-hot-reload-pie.md`
- Unlocks: None
- Open verification claims: ADR-0004 Verification Required (3), real details-panel `ValueSet` behaviour, is [U] and is not observed by this story's synthetic events; the PIE headless run is [U] until story 019 closes.
