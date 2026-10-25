# Story 017: Net emulation profiles StealthLeak1, StealthLeak2, StealthClean

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 5; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Net profiles are engine packet-simulation profiles, not a project struct: `[PacketSimulationProfile.StealthLeak1|StealthLeak2|StealthClean]` in `Config/DefaultEngine.ini` (PktLag 100 ms with PktLoss 1 % and 2 % for the leak profiles), applied by T3 with `-PktEmulationProfile=<Name>` and by T2 through `FPacketSimulationSettings`; the harness story measures the resulting RTT once and records it in the profile comment.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Verified in 5.8.4 source [V]: packet simulation in non-Shipping builds reads `-PktLag=`, `-PktLagVariance=`, `-PktLoss=` (int percent) and `-PktEmulationProfile=<Name>`, and named profiles come from `[PacketSimulationProfile.<Name>]` in `DefaultEngine.ini` (`Engine/Private/Net/NetEmulationHelper.cpp:384-460, 534-570`; `NetDriver.cpp:785-788`). Unverified [U]: lag semantics (the ADR reads PktLag 100 as one-way on the outgoing side of the process it is set on, to match ADR-0002's "symmetric PktLag 100 ms" acceptance profile) and the name and signature of `FPacketSimulationSettings::LoadEmulationProfile` in 5.8 (the spec asks to confirm it when implementing). The measured RTT resolves the semantics question for this project.

**Control Manifest Rules (this layer)**:
- Required: T2/T3 tests are bound by frames or server time and assert and record the active replication system (source: ADR-0008)
- Forbidden: Never use wall-clock time as a test pass/fail predicate (only as a failure ceiling) (source: ADR-0008; the RTT here is recorded, with no threshold)
- Required: Test names `StealthGame.<System>.<Scenario>`; T3 nodes `StealthGame.<System>.MP.<Scenario>`, run under these profiles by the nightly job (source: ADR-0008)
- Guardrail: Per-commit pipeline at most 10 minutes; nightly cook plus T3 estimated 1-2 h [U] (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), as revised in `tdc-plan-amendments.md`:*

- [x] `Config/DefaultEngine.ini` defines `[PacketSimulationProfile.StealthLeak1]` (PktLag 100, PktLagVariance 0, PktLoss 1), `[PacketSimulationProfile.StealthLeak2]` (PktLag 100, PktLagVariance 0, PktLoss 2) and `[PacketSimulationProfile.StealthClean]` (PktLag 0, PktLagVariance 0, PktLoss 0); each loads by name through FPacketSimulationSettings::LoadEmulationProfile, and the measured RTT of StealthLeak1 and StealthLeak2 is recorded in a comment beside each profile (recorded, no threshold).

---

## Implementation Notes

*Derived from ADR-0008 section 5, Key Interfaces (profile keys) and the verified engine facts:*

- **File**: `Config/DefaultEngine.ini` only. At the time of writing it has no `PacketSimulationProfile` section. Add three sections with integer keys `PktLag` (ms), `PktLagVariance` (ms) and `PktLoss` (percent), with exactly the values in the criterion. `StealthClean` is all zeros: the inputs state no other meaning of "clean", so do not add keys beyond these three. No duplicate section may exist.
- **RTT comment**: the measurement method is not specified by the inputs (the ADR assigns the measurement to "the harness story", yet this story has no dependency on the harness). Measure once with the prototype game: a listen host and one client with the profile applied through `-PktEmulationProfile=<Name>` on one side per the profile definition, and read the engine-reported round-trip time. Record in the comment beside each leak profile: the RTT value, which process had the profile applied, the method and the date. If the one-way versus both-sides semantics turn out different from the ADR's reading, say so in the comment (this resolves the [U] item). `StealthClean` needs no RTT comment (the AC asks for the two leak profiles).
- **Load check**: confirm by name that each profile loads and that the loaded values equal the stated ones. The spec names `FPacketSimulationSettings::LoadEmulationProfile`; confirm its 5.8 signature before relying on it (the ADR marks the name [U]). The plan lists no test file for this story, so the check is recorded in the smoke record rather than added as new test code; how it is performed (a one-off call, or reading the engine's log of the applied profile when launched with `-PktEmulationProfile=`) is not fixed by the inputs: record the method used.
- **Edit discipline**: this is a text config file, tracked as normal text by ADR-0009 (`*.ini text`); edit it directly. It is not a `.uasset`, so the lock and single-writer rules do not apply, but follow the repository's commit conventions when committing (commit only on user instruction).
- **Consumers** (per ADR-0008 section 5 and the plan): T3 applies a profile with `-PktEmulationProfile=StealthLeak2` (the nightly job of story 023 runs the T3 suites under `StealthLeak2` and `StealthClean`; story 020 launches the T3 smoke), and story 071 measures under `StealthLeak2`; T2 loads the same profile into `FPacketSimulationSettings` and passes it to `WithPacketSimulationSettings`. `LeakNetProfile` in GDD and ADR text means `StealthLeak1`/`StealthLeak2`; the clean profile is the baseline.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-018-replication-system-assertion.md`: the harness that writes the active net profile into its result JSON
- `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`: first T2 test (no profile required for the proof)
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: T3 launch that applies a profile
- `production/epics/tuning-data-config/story-023-nightly-workflow.md`: nightly T3 suites under `StealthLeak2` and `StealthClean`
- `production/epics/tuning-data-config/story-071-snapshot-atomic-spike.md`: the first test whose result depends on `StealthLeak2`

---

## Files

- `Config/DefaultEngine.ini` (`[PacketSimulationProfile.StealthLeak1|StealthLeak2|StealthClean]`, measured RTT and the PktLag semantics in the comments)
- `production/qa/smoke-2026-10-25_net-emulation-profiles.md` (the smoke record)
- Measurement tooling (scope addition, see Deviations): `Source/StealthTestHarness/Private/Scenarios/StealthHarnessRun.{h,cpp}` (T3 harness runner, scenario `Rtt`, the `-StealthHarnessProfileCheck` load check), `Source/StealthTestHarness/Private/StealthTestHarnessModule.cpp` (starts the runner only when `-StealthHarnessScenario=` is given), `Source/StealthTestHarness/StealthTestHarness.Build.cs` (`EnhancedInput`, `InputCore` for the IVP scenarios), `tools/ci/mp/run-mp.ps1`, `tools/ci/mp/harness_records.py`, `tools/ci/mp/analyze_rtt.py`
- Evidence: `production/qa/evidence/story-017-net-emulation-profiles/`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: the three profiles exist with the stated values, load by name, and the RTT of the two leak profiles is recorded beside them
  - Setup: `Config/DefaultEngine.ini` with `StealthLeak1` (PktLag 100, PktLagVariance 0, PktLoss 1), `StealthLeak2` (PktLag 100, PktLagVariance 0, PktLoss 2) and `StealthClean` (PktLag 0, PktLagVariance 0, PktLoss 0)
  - Verify: each profile loads by name through `FPacketSimulationSettings::LoadEmulationProfile` and the loaded values equal the stated ones; the measured RTT of Leak1 and Leak2 is recorded in a comment beside each
  - Pass condition: three profiles load, values match, two RTT comments exist (recorded, no threshold); confirm the 5.8 signature of `LoadEmulationProfile` when implementing; the ini shows no duplicate section

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: a smoke check pass recorded in `production/qa/smoke-*.md` (qa.level is full; a smoke record, not new test code, is the evidence for this story type)

**Status**: [x] Smoke record `production/qa/smoke-2026-10-25_net-emulation-profiles.md` (PASS, 2026-10-25)

---

## Dependencies

- Depends on: None
- Unlocks: None (no later story lists 017 in its dependencies, although stories 023 and 071 use these profiles: see the sequencing note)
- Sequencing note (not a plan dependency): the profiles must exist before the first run that applies `-PktEmulationProfile=` (story 020 or 023) and before story 071's `StealthLeak2` measurement.
- Open verification claims from the plan's Note lines: PktLag semantics (one-way, outgoing side) are [U]; this story measures and records the RTT. Also open: the `LoadEmulationProfile` 5.8 signature, and the RTT measurement method.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: three profiles, one section each, values as stated; each loads by name through `FPacketSimulationSettings::LoadEmulationProfile` with the loaded values equal to the stated ones and a missing name rejected; the measured RTT of StealthLeak1 and StealthLeak2 is in the comment beside each profile)
**Decisions**: load check performed by a harness boot check (`-StealthHarnessProfileCheck`) in every process of the five measurement runs, recorded in the smoke record rather than as new test code (the story asks for a smoke record). RTT measured with the engine-reported `UNetConnection::AvgLag` and `APlayerState::ExactPing` in a listen host + 1 client multi-process run, profile on the client only and on both sides, so the [U] PktLag semantics are settled: outgoing side only (client only ~120 ms, both ~226 ms, clean 17 ms). 5.8.4 signature confirmed: `bool LoadEmulationProfile(const TCHAR* ProfileName)`.
**Deviations**: the story has no harness dependency, but measuring the RTT needed a multi-process runner; the T3 runner (`StealthHarnessRun`), launcher and analysers were built here and are reused by the IVP spikes S1/S9 the same day. Gauntlet wiring stays story tdc-020.
**Test Evidence**: smoke record + `production/qa/evidence/story-017-net-emulation-profiles/` (rtt-analysis.txt, gzipped records and launch lines of runs rtt2-*). Build `Result: Succeeded`; full `StealthGame.` suite: see the batch test run in the commit.
**Run result**: N/A — config and measurement only; nothing player-visible changes (profiles apply only when a test or launch passes them)
**Reviewed by**: focused self-review (unattended run): runner registers nothing unless the scenario switch is present; records written on the game thread only; the profile sections do not duplicate.
