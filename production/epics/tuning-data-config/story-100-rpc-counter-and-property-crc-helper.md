# Story 100: Reliable-RPC counter and replicated-property CRC helper (test seams)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap test seams: no registry entry; governed by ADR-0008 section 4; the seams serve GDD AC-12a, verified by story 077, and AC-24, verified by story 093, and trace to the epic's test-seam set from Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; no entry exists for this story)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Test-only helpers are wrapped in `WITH_STEALTH_TEST_SEAMS` (`!UE_BUILD_SHIPPING`, a public definition of StealthFoundation) so Shipping has no seam symbols, and every gate carries a control the suite detects, so a gate that cannot fail cannot pass vacuously.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0008 is MEDIUM. The fake-channel half is plain T1. The real-PIE-connection positive control uses CQTest `FPIENetworkComponent` (`AsListenServer().WithClients(N)`) [V `PIENetworkComponent.h:268, 514-556`], which is compiled only under `ENABLE_PIE_NETWORK_TEST` and whose headless run under `-nullrhi` is [U] (ADR-0008 Verification 1, story 019). PIE always runs in one process [V `PIENetworkComponent.cpp:33-36`]. How the counter hooks the sending side of a real connection (which engine seam counts a reliable RPC) is not specified in the inputs and is open; it is the main technical unknown of this story. Absence from Shipping is checked by story 006.

**Control Manifest Rules (this layer)**:
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008
- Required: T1 tests live in the owning module's `Private/Tests/<System>/`; T2 and T3 code lives in `StealthTestHarness` — source: ADR-0008

---

## Acceptance Criteria

*Bootstrap criterion, from the amendments (story 100 NEW plus AMENDMENTS 2), no GDD AC text:*

- [ ] A test-only `FReliableRpcCounter` counts reliable RPCs sent by a given client (a fake channel proves count 0 and count N, and a known reliable RPC sent on a real PIE connection counts exactly 1, the positive control `StealthGame.Harness.PIE.RpcCounterCountsRealRpc`), and a `FReplicatedPropertyCrc` helper hashes the replicated properties of a Pawn and PlayerState with an explicit exclusion list, giving equal CRC for an unchanged object and a different CRC after a non-excluded property changes. (test `StealthGame.Harness.RpcCounterAndPropertyCrc`)

---

## Implementation Notes

*Derived from ADR-0008 sections 1, 2, 3 and 4, and amendments G1, G4 and G7:*

These helpers measure two claims that other stories assert: story 077 (`HostPath`, AC-12a) needs "without any RPC", and story 093 (`LocalSettingsNotTuning`, AC-24) needs "zero reliable RPCs" plus an unchanged CRC. Without a positive control, a counter that counts nothing would let both pass vacuously (QA review of this story). Per G7, 100 is appended and must be done before stories 077 and 093.

- Placement (G1): `Source/StealthFoundation/Public/Server/Testing/StealthReliableRpcCounter.h` and `Source/StealthFoundation/Public/Server/Testing/StealthReplicatedPropertyCrc.h`, with `StealthServerOnlyGuard.h` included first and all content inside `#if WITH_STEALTH_TEST_SEAMS`.
- `FReliableRpcCounter`: counts reliable RPCs sent by a given client, keyed per client, with a reset and a count read. It is driven through a small channel interface so a fake channel can send 0 and N reliable RPCs in the unit case. The production-side hook that feeds it on a real connection exists only under the seam define. The interface name and the engine seam used for real connections are open (see Engine Notes).
- `FReplicatedPropertyCrc`: hashes the replicated properties of a Pawn and a PlayerState (reflection over the properties flagged as replicated is the natural enumeration; confirm in source) with an explicit exclusion list of property names, each exclusion required to carry a reason string. Equal CRC for an unchanged object; equal after an excluded property changes; different after a non-excluded property changes. The CRC function is not specified; any deterministic CRC is acceptable, and it is a test helper only, unrelated to `TuningHash` or `ClientHash`. Where the fixture type with replicated properties lives (UHT needs a header for a `UCLASS`) is open; keep it test-only, in the harness module or a test-only header, never in runtime production code (ADR-0008 s3).
- Tests: (a) `StealthGame.Harness.RpcCounterAndPropertyCrc`, class `FHarnessRpcCounterAndPropertyCrcTest`, file `Source/StealthFoundation/Private/Tests/Harness/RpcCounterAndPropertyCrcTest.cpp`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`: fake-channel counts 0 and N; CRC unchanged, after an excluded change, after a non-excluded change. (b) The positive control `NETWORK_TEST_CLASS(RpcCounterCountsRealRpc, "StealthGame.Harness.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/RpcCounterCountsRealRpcTest.cpp` (full name `StealthGame.Harness.PIE.RpcCounterCountsRealRpc.<Method>`), inside `#if ENABLE_PIE_NETWORK_TEST`: with a listen server and a client, a known reliable RPC is sent on the real connection and the counter reads exactly 1. Which RPC is used is not named in the inputs and is open; it must be a test-only RPC that does not exist in production classes.
- Negative control (ADR-0008 s3): the CRC case that changes a non-excluded property is itself the detection control, and the counter case with N greater than 0 shows the counter can read non-zero.
- Spike discipline (G4 and story 019): if the PIE positive control cannot run headless, the story closes with the T2 fallback recorded under story 019's ADR-0008 amendment request (run in a non-headless editor on the dev machine) and never by skipping the control.
- Faults: report through the fault sink (story 008) and assert zero unexpected faults at teardown. Use the fake clock (story 007) where time is needed; no wall-clock waits.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-077-host-path.md`: asserts the host path sends no RPC, using this counter (AC-12a).
- `production/epics/tuning-data-config/story-093-local-settings-not-tuning.md`: asserts zero reliable RPCs and an unchanged CRC for local settings changes (AC-24).
- `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`: proves CQTest PIE runs headless and records the fallback.
- `production/epics/tuning-data-config/story-099-selfcheck-and-disconnect-seams.md`: the sibling self-check and disconnect seams.
- Shipping absence check: story 006 (`production/epics/tuning-data-config/story-006-shipping-symbol-check.md`).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthFoundation/Public/Server/Testing/StealthReliableRpcCounter.h
- Source/StealthFoundation/Public/Server/Testing/StealthReplicatedPropertyCrc.h
- Source/StealthFoundation/Private/Tests/Harness/RpcCounterAndPropertyCrcTest.cpp
- Source/StealthTestHarness/Private/Tests/PIE/RpcCounterCountsRealRpcTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: RPC counter and property CRC helper
  - Given: `FReliableRpcCounter` on a fake channel; `FReplicatedPropertyCrc` with an explicit exclusion list over a Pawn and PlayerState; a real PIE connection with a known reliable RPC.
  - When: counts of 0 and N are sent; the CRC is taken, an excluded property is changed, then a non-excluded property is changed; the known reliable RPC is sent on the real PIE connection.
  - Then: the counter reads 0 and N; the CRC is equal for an unchanged object and after an excluded change, and different after a non-excluded change; the counter reads exactly 1 for the real RPC.
  - Edge cases: the PIE-connection control (a known reliable RPC counted as 1) guards stories 077 and 093 against vacuous passes.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Logic
**Required evidence**:
- Logic: the automation test `StealthGame.Harness.RpcCounterAndPropertyCrc` in `Source/StealthFoundation/Private/Tests/Harness/RpcCounterAndPropertyCrcTest.cpp` must exist and pass, together with the PIE positive control `StealthGame.Harness.PIE.RpcCounterCountsRealRpc` in `Source/StealthTestHarness/Private/Tests/PIE/RpcCounterCountsRealRpcTest.cpp` (or its documented fallback run).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`
- Unlocks: `production/epics/tuning-data-config/story-077-host-path.md`, `production/epics/tuning-data-config/story-093-local-settings-not-tuning.md`
- Open verification claims: the engine seam that counts a reliable RPC on a real connection is not specified; the known test RPC and the CRC fixture type location are open; CQTest PIE under `-nullrhi` is [U] (ADR-0008 Verification 1) until story 019 closes.
