# Story 099: Self-check forcing hook and disconnect recorder (test seams)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap test seam: no registry entry; governed by ADR-0008 section 4; the seam serves GDD AC-12c, verified by story 079, and traces to the epic's test-seam set from Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; no entry exists for this story)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Test-only hooks are wrapped in `WITH_STEALTH_TEST_SEAMS` (defined as `!UE_BUILD_SHIPPING` by StealthFoundation as a public definition), so a Shipping build has no seam symbols. Plain cores take injected interfaces so tests control time, faults and connection events.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0008 is MEDIUM (CQTest PIE and `EAutomationTestFlags` as an enum class are post-cutoff). This story is a plain T1 test and uses neither CQTest nor PIE. `EAutomationTestFlags` is an enum class and every test needs an application-context flag or it fails a `static_assert` [V `AutomationTest.h:93-137, 4127`]. The self-check the hook forces (`FNetBitWriter(nullptr)` round trip, story 079) is not exercised against a real `NetSerialize` here. Absence from Shipping is checked by story 006, and until that runs it is an expectation, not an observation.

**Control Manifest Rules (this layer)**:
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Required: Every Logic story ships T1 tests; T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Required: Every `Public/Server/` header includes `StealthServerOnlyGuard.h` first — source: ADR-0007

---

## Acceptance Criteria

*Bootstrap criterion, from the amendments (story 099 NEW), no GDD AC text:*

- [ ] A test-only hook behind `WITH_STEALTH_TEST_SEAMS` makes the server self-check see a differing serialised struct at freeze, and a `FConnectionEventRecorder` records every disconnect and kick on a fake connection set, so a test can assert zero disconnects; both absent from Shipping. (test `StealthGame.Harness.SelfCheckSeams`)

---

## Implementation Notes

*Derived from ADR-0008 sections 4, 3 and 2, ADR-0007 section 2, and amendments G1 and G7:*

These seams exist so story 079 (`SelfCheckFault`, GDD AC-12c) can force the self-check to fail at freeze and assert that no client is disconnected. The GDD AC-12c requires: `TuningSnapshotFault` logged, the pipeline `Failed`, the Round stub receiving `Failed`, and no client disconnected. Per G7, 099 is appended and must be done before story 079.

- Placement (G1): shared test support lives in `Source/StealthFoundation/Public/Server/Testing/` as server-only headers with `StealthServerOnlyGuard.h` included first and the whole content inside `#if WITH_STEALTH_TEST_SEAMS`. Headers: `StealthSelfCheckHook.h` and `StealthConnectionEventRecorder.h` (unique Stealth-prefixed names; do not use `Private/Tests/Support/`, which a harness module cannot include).
- Self-check hook: a small test-only object the production self-check consults only under `#if WITH_STEALTH_TEST_SEAMS`. When armed, it makes the self-check's re-serialised struct differ from the frozen one, so the self-check reports a mismatch. The production consult point is added by story 079; here the hook is driven against a minimal fake self-check consumer in the test that reads it the way the production code will. The exact hook API (arm and disarm calls, whether one-shot) is not specified in the inputs and is open; keep it minimal and document it in the header.
- `FConnectionEventRecorder`: records every disconnect and every kick as an event (connection id and kind) on a fake connection set, and exposes the event count. The spec edge case says production code must disconnect through the injected interface the recorder implements; that interface is not named in the inputs and is open. Define the smallest consumer-owned interface (ADR-0007 s4) needed, and the recorder implements it. Without it a real disconnect path could bypass the recorder and the zero-disconnects assertion would pass vacuously, which is why the control case below is required.
- Test: `StealthGame.Harness.SelfCheckSeams`, class `FHarnessSelfCheckSeamsTest`, file `Source/StealthFoundation/Private/Tests/Harness/SelfCheckSeamsTest.cpp`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS` (system name `Harness` per ADR-0008 s2).
- Cases: (1) unarmed hook, the fake self-check passes; (2) armed hook, the self-check reports the difference; (3) recorder holds zero events when nothing disconnects; (4) control case: a disconnect and a kick are issued on the fake connection set and the recorder records each (negative control, ADR-0008 s3). Wrap (4) with `FStealthNegativeControl` (story 010) if that helper fits.
- Faults: report through the fault sink (story 008) and assert zero unexpected faults at teardown.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-079-self-check-fault.md`: the production self-check, the `TuningSnapshotFault` log, the Failed pipeline state and the Round stub assertion (AC-12c).
- `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`: the snapshot struct and its `NetSerialize`.
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: the Round and Session stubs.
- `production/epics/tuning-data-config/story-100-rpc-counter-and-property-crc-helper.md`: the sibling counter and CRC seams.
- Shipping absence check: story 006 (`production/epics/tuning-data-config/story-006-shipping-symbol-check.md`).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthFoundation/Public/Server/Testing/StealthSelfCheckHook.h
- Source/StealthFoundation/Public/Server/Testing/StealthConnectionEventRecorder.h
- Source/StealthFoundation/Private/Tests/Harness/SelfCheckSeamsTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: self-check forcing hook and connection event recorder
  - Given: the self-check hook and `FConnectionEventRecorder` on a fake connection set, both behind `WITH_STEALTH_TEST_SEAMS`.
  - When: the hook forces a differing serialised struct at freeze; a disconnect and a kick are recorded in a control case.
  - Then: the self-check reports the difference; the recorder holds zero events when none occur and records each disconnect and kick in the control; both are absent from Shipping.
  - Edge cases: production code must disconnect through the injected interface the recorder implements.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Logic
**Required evidence**:
- Logic: the automation test `StealthGame.Harness.SelfCheckSeams` in `Source/StealthFoundation/Private/Tests/Harness/SelfCheckSeamsTest.cpp` must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`
- Unlocks: `production/epics/tuning-data-config/story-079-self-check-fault.md`
- Open verification claims: absence from Shipping is checked by story 006 (not yet observed); the hook API and the injected disconnect interface are not specified in the inputs.
