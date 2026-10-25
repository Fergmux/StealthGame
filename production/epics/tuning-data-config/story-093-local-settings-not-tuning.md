# Story 093: AC-24 LocalSettingsNotTuning

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-009`; GDD AC-24 (CR-14)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: Tuning values reach clients only as the server's replicated snapshot; clients never supply a value. Client-local presentation preferences (sensitivity, FOV, volume, UI scale) are not tuning keys and never reach the server (CR-14).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API from ADR-0004 for this story. The test runs on the ADR-0008 T2 harness, which is MEDIUM risk: CQTest `FPIENetworkComponent` under `-nullrhi` is [U] (ADR-0008 Verification 1, story 019). PIE always runs in one process [V `PIENetworkComponent.cpp:33-36`], so a reliable-RPC count read from a counter on the client's connection is the measurement seam (story 100). Counting a real reliable RPC on a real PIE connection is proven by story 100's positive control, not here.

**Control Manifest Rules (this layer)**:
- Required: Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct — source: ADR-0004
- Forbidden: Never mark tuning types Config/DefaultConfig/GlobalUserConfig/PerObjectConfig; never read a gameplay value from `UDeveloperSettings`, ini, `Saved/Config`, command line (Shipping) or a console variable — source: ADR-0004
- Required: T2/T3 tests bound by frames or server time and assert and record the active replication system — source: ADR-0008
- Forbidden: Never use wall-clock time as a test pass/fail predicate (only as a failure ceiling) — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (AC-24), as amended (amendment 093 REVISED):*

- [ ] **AC-24 (CR-14)**: when the player changes sensitivity, FOV, volume and UI scale (client-local settings stub), the reliable-RPC counter of story 100 reads zero for the client, and a CRC of that player's replicated Pawn and PlayerState properties, taken on a second client with the player stationary and with an explicit exclusion list of naturally changing properties (ping, movement; each with its reason in the test), is identical before and after. (`StealthGame.Tuning.PIE.LocalSettingsNotTuning`)

---

## Implementation Notes

*Derived from ADR-0004 Implementation Guidelines 2 and 3, ADR-0008 sections 1 and 4, and amendment G1:*

The Settings and Accessibility owner is not in the MVP set, so there is no real settings system to drive. The test drives a minimal client-local settings stub and stays provisional on the real owner (plan note); when that epic lands, its integration test replaces or extends the stub.

- Test: `NETWORK_TEST_CLASS(LocalSettingsNotTuning, "StealthGame.Tuning.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/LocalSettingsNotTuningTest.cpp`, inside `#if ENABLE_PIE_NETWORK_TEST`. Full name `StealthGame.Tuning.PIE.LocalSettingsNotTuning.<Method>`. It uses `FPIENetworkComponent` with `AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass())` and loads its fixture map in the first step (T2 rule, ADR-0008). Flags come from the CQTest macro.
- Stub: a test-only client-local settings object (in the test file, under `#if WITH_DEV_AUTOMATION_TESTS` or the harness module) that holds sensitivity, FOV, volume and UI scale and applies them locally. It must not send any RPC or write any replicated property; if it did, the test would detect it.
- Measurement seams from story 100, in `Source/StealthFoundation/Public/Server/Testing/` per G1: `FReliableRpcCounter` (`StealthReliableRpcCounter.h`) reads the reliable RPCs sent by the changing client; `FReplicatedPropertyCrc` (`StealthReplicatedPropertyCrc.h`) hashes the replicated properties of that player's Pawn and PlayerState as seen on the second client. Both are test-only under `WITH_STEALTH_TEST_SEAMS`.
- Exclusion list: the player is stationary, and the CRC excludes naturally changing replicated properties (for example ping on the PlayerState and movement-driven properties on the Pawn). Each exclusion carries a reason string in the test. Which exact properties exist on `AStealthPawn` and `AStealthPlayerState` is not stated in the inputs and is settled by reflection at implementation; the exclusion list must stay minimal and justified, never widened to make the test pass.
- Positive control (spec edge case, ADR-0008 s3): changing a non-excluded replicated property must alter the CRC, and the test passes only if the control is detected (`FStealthNegativeControl`, story 010). Without it the zero-RPC and equal-CRC results could pass vacuously.
- Timing: advance by frames or server time (injected clock, story 007); no wall-clock waits. Record and assert the replication system as legacy (story 018).
- Per G4 and story 019's fallback: if T2 cannot run headless, the test runs in a non-headless editor on the dev machine under the ADR-0008 amendment request that story 019 records; it is never skipped.
- Prototype: none; the stub is test-only.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-100-rpc-counter-and-property-crc-helper.md`: builds and proves the RPC counter and CRC helper (including the real-PIE-connection positive control).
- `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`: the headless T2 proof and fallback record.
- `production/epics/tuning-data-config/story-082-local-tamper-ignored.md`: client-side tampering of tuning assets, ini and console (AC-8), a different check.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthTestHarness/Private/Tests/PIE/LocalSettingsNotTuningTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-24 local settings are not tuning
  - Given: a client-local settings stub; a stationary player; a second client computing `FReplicatedPropertyCrc` with an exclusion list (ping, movement; a reason for each).
  - When: sensitivity, FOV, volume and UI scale change.
  - Then: the reliable-RPC counter reads zero for the client; the CRC is identical before and after.
  - Edge cases: positive control: a non-excluded replicated change alters the CRC.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest `NETWORK_TEST_CLASS(LocalSettingsNotTuning, "StealthGame.Tuning.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/LocalSettingsNotTuningTest.cpp` must exist and pass (or a documented playtest under the story 019 fallback).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`, `production/epics/tuning-data-config/story-100-rpc-counter-and-property-crc-helper.md`
- Unlocks: None
- Open verification claims: CQTest PIE under `-nullrhi` is [U] (ADR-0008 Verification 1) until story 019 closes; the exact exclusion list depends on the replicated properties of the real Pawn and PlayerState; the stub stays provisional on the Settings and Accessibility owner. The negative-control and clock helpers (stories 010 and 007) are used but are not direct dependencies in the plan.
