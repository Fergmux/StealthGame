# Story 072: Snapshot replication hold-back and arrival-order test hook

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md` (Acceptance Criteria preamble, "Test seams": a test hook that holds back snapshot replication or forces arrival order)
**Requirement**: `TR-tdc-???` (TDC test seam: hold-back hook; bootstrap-style seam story, no registry entry; governed by ADR-0008 section 4; traced to the epic's Exit B test seams)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (section 4, Test seams)
**ADR Decision Summary**: Test-only hooks are wrapped in `WITH_STEALTH_TEST_SEAMS`, which `StealthFoundation` defines as `!UE_BUILD_SHIPPING` through a public definition, so a Shipping build has no seam symbols; the Shipping-symbol check (story 006) proves it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff engine API is needed for the hook itself; it is driven against a fake snapshot target, not a live `UNetConnection`. The ADR-0008 [U] items that matter downstream are Verification 1 (CQTest PIE headless, story 019) and ADR-0004 Verification Required (1) (spike 071), which uses this hook. `WITH_STEALTH_TEST_SEAMS` is defined `!UE_BUILD_SHIPPING`, so the hook also exists in cooked Development processes (needed by the multi-process stories).

**Control Manifest Rules (this layer)**:
- Forbidden: never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Required: every `Public/Server/` header includes `StealthServerOnlyGuard.h` first; every Logic story ships T1 tests — source: ADR-0007, ADR-0008
- Required: T1 tests live in the owning module's `Private/Tests/<System>/`; never use wall-clock waits or shared mutable state in tests — source: ADR-0008

---

## Acceptance Criteria

*From the amended story plan (no GDD AC; the test seam is named in the GDD Acceptance Criteria preamble), scoped to this story:*

- [ ] A test-only hook behind `WITH_STEALTH_TEST_SEAMS` driven against a fake snapshot target can hold back delivery to a chosen client, release it on demand, force snapshot-first versus controller-first arrival order, and corrupt one received field; absent from Shipping (story 006) (test `StealthGame.Harness.SnapshotReplicationHook`).

---

## Implementation Notes

*Derived from ADR-0008 section 4 and Key Interfaces, and global amendment G1:*

- **Placement (G1).** The hook header is `Source/StealthFoundation/Public/Server/Testing/StealthSnapshotReplicationHook.h`: a server-only header (`StealthServerOnlyGuard.h` first) whose whole content sits under `#if WITH_STEALTH_TEST_SEAMS`. It is not in `Private/Tests/Support/` (the plan's original path) because a harness module cannot include another module's Private folder. The class name is open; `FStealthSnapshotReplicationHook` is proposed.
- **Operations required by the AC.** (1) hold back delivery to one chosen client (other clients unaffected); (2) release held delivery on demand; (3) force arrival order, snapshot-first versus controller-first; (4) corrupt exactly one received field (it changes one value of the received struct, not the server's). Signatures are not given in the inputs; design them for use by stories 071 (spike), 074 and 076 (stories that call hold-back, release, order and corrupt).
- **Fake snapshot target.** The unit test drives the hook against a plain C++ fake target that records, per client, what was delivered and in which order. No `UNetConnection`, no PIE, no wall clock: delivery is advanced by explicit calls, so the test is deterministic (ADR-0008 section 4, injected time).
- **Test.** `Source/StealthFoundation/Private/Tests/Harness/SnapshotReplicationHookTest.cpp` (folder follows the `Private/Tests/Harness/` convention of the other seam self-tests; the plan names no path), class `FHarnessSnapshotReplicationHookTest`, name `StealthGame.Harness.SnapshotReplicationHook`, flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`. Assertions: nothing delivered while held; delivered on release; arrival order as forced (both orders); exactly one field differs after corruption (compare the received struct with the original field by field); hold-back for one client leaves the others delivering; release without a prior hold is a no-op.
- **Shipping.** Absence from Shipping is checked by story 006 (the `WITH_STEALTH_TEST_SEAMS` code carries a unique sentinel and the scan must not find it in a Shipping build); this story does not duplicate that check. Whether this hook needs a sentinel of its own beyond story 006's mechanism is open.
- **Plan file to flag.** The plan also lists `Source/StealthFoundation/Public/Server/StealthTuningService.h` ("seam entry under WITH_STEALTH_TEST_SEAMS"). That header is created by story 066, which is not in this story's dependency list (070 only), and the QA spec drives the hook against a fake target, not the service. Open: add the seam entry only if story 066 is Done; otherwise the first consumer (story 074) adds it. Record the choice here.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`: the snapshot struct and `NetSerialize` that the hook delivers
- `production/epics/tuning-data-config/story-071-snapshot-atomic-spike.md`: spike T-1, which uses the hold-back to release mid-update
- `production/epics/tuning-data-config/story-074-client-hash-handoff.md` and `production/epics/tuning-data-config/story-076-no-pawn-before-ready.md`: the PIE tests that drive the hook on live connections
- `production/epics/tuning-data-config/story-099-selfcheck-and-disconnect-seams.md`: the self-check forcing hook and the disconnect recorder (a different seam)
- `production/epics/tuning-data-config/story-006-shipping-symbol-check.md`: proof that the seam is absent from Shipping

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Testing/StealthSnapshotReplicationHook.h` (G1 path; the plan's `Private/Tests/Support/SnapshotReplicationHook.h` is superseded)
- `Source/StealthFoundation/Private/Tests/Harness/SnapshotReplicationHookTest.cpp`
- `Source/StealthFoundation/Public/Server/StealthTuningService.h` (seam entry; conditional on story 066, see Implementation Notes)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the hook holds back, releases, forces arrival order and corrupts one field against a fake snapshot target
  - Given: the hook under `WITH_STEALTH_TEST_SEAMS` driven against a fake snapshot target (test `StealthGame.Harness.SnapshotReplicationHook`)
  - When: hold back one client; release; force snapshot-first then controller-first order; corrupt one received field
  - Then: nothing delivered while held; delivered on release; arrival order as forced; exactly one field differs after corruption
  - Edge cases: hold-back for one client only; absence from Shipping is story 006

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Harness/SnapshotReplicationHookTest.cpp` (`StealthGame.Harness.SnapshotReplicationHook`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`
- Unlocks: `production/epics/tuning-data-config/story-071-snapshot-atomic-spike.md`, `production/epics/tuning-data-config/story-074-client-hash-handoff.md`
- Open verification claims: none from the plan Note lines (the plan has none for 072). Open: whether the `StealthTuningService.h` seam entry belongs here given story 066 is not a dependency; whether the hook needs its own sentinel for story 006.
