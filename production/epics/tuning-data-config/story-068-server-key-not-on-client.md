# Story 068: AC-28 ServerKeyNotOnClient

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-28, CR-10

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-10 makes the client face a distinct type holding only public keys, so a server-only key such as `NpcCount` or `TargetRoundMinutes` cannot be expressed in client C++ at all: it is a compile-time guarantee. Any reflective or Blueprint request that still asks for one must return invalid and log through `ensure` (never a `check` that aborts). This story proves both halves: a compile-fail probe reading a server-only key through `GetClientTuning()` must fail to compile, with a positive control reading a public key that compiles, and a reflective request for a server-only key on a client must return invalid, log, and not abort, with the log accounted for by an expected-error hook so an unexpected error would still fail the test.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (with ADR-0007 section 2)
**ADR Decision Summary**: `FStealthClientTuning` is generated from the `_Public` sub-structs only, in `Public/View/`, so `GetClientTuning()->NpcCount` does not compile; reflective or Blueprint requests for a server-only value on a client return invalid and use `ensure`, never `check`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Pre-cutoff APIs; the probe relies on UBT `-SingleFile=` per the story 005 runner ([V `BuildMode.cs:880-943`], observed by story 005). How `ensure` behaves inside an automation run (whether a failed `ensure` is a test failure, and how `AddExpectedError` or the project's expected-error hook accounts for it) is not stated in the inputs: open, and it must be settled by the first run of the reflective half. None of ADR-0004's three Verification Required items is touched.

**Control Manifest Rules (this layer)**:
- Forbidden: never let client or presentation code include a `Public/Server/` tuning header or reference `UStealthTuningData` — source: ADR-0004
- Required: a reflective or Blueprint request for a server-only value on a client returns invalid and uses `ensure`, never `check` — source: ADR-0007
- Required: every leak, parity and lint gate includes a negative control; compile probes carry a positive control — source: ADR-0008
- Forbidden: never disable, skip or reseed a failing test to pass a gate — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-28 (CR-10)**: GIVEN the client accessor, WHEN a compile-fail test tries to read a server-only key through `GetClientTuning()` in C++, THEN compilation fails. AND a reflective request for a server-only key on a client returns *invalid* and logs, asserted with an expected-error hook and without aborting. (`StealthGame.Tuning.ServerKeyNotOnClient`)

---

## Implementation Notes

*Derived from ADR-0004 section 1 (client and server faces), ADR-0007 section 6 (reflective requests) and ADR-0008 section 7 (compile-fail probes):*

- **Compile probe.** `tools/ci/compile-probes/ServerKeyOnClientFace.cpp` (the plan's `TOOLS/` is `tools/ci/`), with header comments `// EXPECT-ERROR: "<substring>"` and `// TARGET-MODULE: <module>` (the ADR-0008 probe format used by story 005). The probe includes the client face and does `GetClientTuning()->NpcCount` (a server-only canonical key per the GDD Canonical Tunables table; `TargetRoundMinutes` is the other). The expected substring is the compiler's missing-member error; its exact text depends on the compiler and is not in the inputs: copy it from the first observed failure. The target module is open (the probe only needs `Public/View/` headers, so it should compile against any module that may include them; the positive control must not be a module where the guard define would disturb it).
- **Positive control.** A second probe or control file reading a public key (for example `GetClientTuning()->DefenceDurationSec`, an `All` key) must compile, so the failure of the main probe cannot come from an unrelated error. Both go in the probe set run by the story 005 runner; do not modify the runner or its gate.
- **Reflective half (in the automation test).** `Source/StealthFoundation/Private/Tests/Tuning/ServerKeyNotOnClientTest.cpp`, class `FTuningServerKeyNotOnClientTest`, name `StealthGame.Tuning.ServerKeyNotOnClient`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. A reflective request for a server-only key on a client must return invalid, log, and not abort. The reflective lookup API itself (its name and signature: for example a by-key-name query on the client face) is not defined in the inputs and is open; ADR-0007 requires it to use `ensure`. The request is made against a client-role service or snapshot in a test (no PIE required), using key names from the schema (server-only: `NpcCount`).
- **Expected-error hook.** The test declares the expected `ensure`/log (QA spec: "the expected-error hook accounts for the log") so it is not a failure, while any other error still fails the test. Story 008's fault sink and its "declare expected faults explicitly" helper are the seam named in the QA spec; story 008 is not a declared dependency of this story (declared: 066, 005). Use it if landed; otherwise use the automation framework's expected-error mechanism. Open.
- **Audience source.** Which keys are server-only comes from the schema table audience (story 038) and the `_Server` sub-structs (story 040), never from a hardcoded list in the test beyond the named probe key.
- **Test placement.** The probe pair is evidenced by the story 005 runner (per-commit when a `Public/Server/` header or Build.cs changed, otherwise nightly, ADR-0008 section 8). The automation test is T1 in the Foundation module.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`: the probe runner, its gate and the guard probe
- `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`: the service and its signature parity
- `production/epics/tuning-data-config/story-069-no-client-asset-reference.md`: the static check that client code holds no reference to `UStealthTuningData` or a `Public/Server/` tuning header
- `production/epics/tuning-data-config/story-054-validated-tuning-frozen-immutable.md`: the other compile-fail probe (mutating a frozen set)
- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the replicated snapshot holding public keys only
- `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`: `IStealthFaultSink`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/compile-probes/ServerKeyOnClientFace.cpp`
- a public-key positive-control probe file beside it (name proposed: `tools/ci/compile-probes/PublicKeyOnClientFaceControl.cpp`)
- `Source/StealthFoundation/Private/Tests/Tuning/ServerKeyNotOnClientTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a server-only key cannot be read through the client face, and a reflective request for one returns invalid without aborting (test `StealthGame.Tuning.ServerKeyNotOnClient` plus the compile probe)
  - Given: the probe `ServerKeyOnClientFace.cpp` reading a ServerOnly key through `GetClientTuning()`; its positive control reading a public key; an expected-error hook
  - When: the probe and the control are compiled; a reflective request for a ServerOnly key is made on a client
  - Then: the probe fails to compile with the expected message; the control compiles; the reflective request returns invalid, logs, does not abort, and the expected-error hook accounts for the log
  - Edge cases: an unexpected error would still fail the test; the story needs the story 005 runner and the story 008 seams

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/ServerKeyNotOnClientTest.cpp` must exist and pass (`StealthGame.Tuning.ServerKeyNotOnClient`); the probe `tools/ci/compile-probes/ServerKeyOnClientFace.cpp` and its control must be reported by the story 005 runner (probe fails with the expected substring, control compiles)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`, `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. It inherits ADR-0008 Verification 4 (`-SingleFile=` behaviour, [V], observed by story 005). Open items found while writing: the expected compiler error substring, the probe's `TARGET-MODULE`, the reflective lookup API for a server-only key, `ensure` behaviour inside an automation run, and the undeclared dependency on story 008's seam named in the QA spec.
