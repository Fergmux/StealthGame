# Story 046: The Iris rerun gate blocks an Iris build that lacks recorded Iris passes

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The whole leak proof is built on the legacy NetDriver; Iris is pinned off and every spike result assumes it. If anyone ever turns Iris on, the S1, S2, S7, S9, S10 results and the three leak audits (AC-IVP-01, 02, 03) no longer cover the build. This story is a release gate: inert while the harness reports the legacy replication system, and a hard block on any Iris-enabled build that lacks recorded Iris passes for all eight items. It is cheap, it has fixtures for both cases, and it keeps Pillar 1 safe against a single stray config flip.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-010` (Iris rerun gate if Iris is enabled)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (section 5); ADR-0001: Crowd identity rules (section 7)
**ADR Decision Summary**: On every T2/T3 start the harness reads `UNetDriver::IsUsingIrisReplication()` for the game net driver and compares it with the pinned expectation (legacy); a mismatch fails the run, and AC-IVP-29 (the Iris rerun gate) activates automatically if the expectation is ever switched to Iris. ADR-0001 pins the game NetDriver to the legacy replication system with Iris off (`net.Iris.UseIrisReplication=0`).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `UNetDriver::IsUsingIrisReplication()` exists [V `Engine/NetDriver.h:2014`]. Iris is off unless `net.Iris.UseIrisReplication=1`, and `BaseEngine.ini` marks the game driver `bCanUseIris=true` (ADR-0001 E10) [V]; Iris is Beta-flagged in 5.8 and its condition behaviour is unverified. The Iris descriptor path and parity reruns (S7, S11) belong to ADR-0024, not here.

**Control Manifest Rules (this layer)**:
- Required: **The game NetDriver runs the legacy replication system and the harness asserts it at startup** — source: ADR-0001
- Forbidden: **Never switch to Iris now** — off by default in 5.8.4, Beta-flagged, condition behaviour unverified; deferred to ADR-0024 — source: ADR-0001, ADR-0003
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Required: **Lints and analysis scripts emit observations only; pass/fail lives in the gate script** — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-29, scoped to this story (retyped Logic: a fixture-driven gate-script test, T0/T1):*

- [ ] **AC-IVP-29** [Doc][S11] `StealthGame.Visibility.IrisRerunGate`: GIVEN any build with Iris enabled, WHEN the release gate runs, THEN recorded Iris passes exist for S1, S2, S7, S9, S10 and AC-IVP-01, 02, 03; missing passes block the build.

---

## Implementation Notes

*Derived from ADR-0008 sections 1, 3, 5, 8 and ADR-0001 section 7:*

- Build a release-gate script (location: `tools/ci/gate/`, new folder beside the T0 scripts in `tools/ci/lint/`; the story may place it under `tools/ci/lint/` if the devops story prefers, but the gate script, not any lint, decides pass or fail). Input: the replication-system value the harness already writes into every result JSON (`replicationSystem`, ADR-0008 Key Interfaces) or the pinned expectation, and a record of Iris passes (one entry per item: S1, S2, S7, S9, S10, AC-IVP-01, AC-IVP-02, AC-IVP-03). Output: pass or block. Scripts emit observations; the gate decides (ADR-0008 section 1 T0, section 8).
- Behaviour: Iris off (legacy) -> the gate is inert and never blocks; Iris on -> it requires all eight recorded Iris passes and blocks on any missing one. This is the "activates automatically" rule of ADR-0008 section 5.
- Wrap the script in a fixture-driven test `StealthGame.Build.IrisRerunGate` (system name `Build`, the lint/probe wrapper family in ADR-0008 section 2) with checked-in fixtures: Iris off; Iris on with all eight passes; Iris on with each of the 8 passes removed singly.
- Negative control (ADR-0008 section 3, required by the GDD criterion's own control): the Iris-enabled, passes-missing fixture must block; the suite passes only if that block is detected.
- Fixtures and test data: ADR-0008 amendment 2026-10-24 says T1 tests do no file I/O (the allowlist is the one data-file exception because T0 scripts also read it). The wrapper therefore either embeds the fixtures as `constexpr` records and exercises the gate logic, or runs the script against files as a `Build`-system harness-side test; decide this with the technical-director before implementing and record it in the story. Do not hide a file read inside a T1 test.
- Do not enable Iris anywhere to test this; the Iris-enabled cases are fixtures only. Iris re-evaluation and the Iris rerun content itself are ADR-0024's.
- Prototype: not applicable. Replaces: none (new).
- Cross-epic needs: tuning-data-config Exit A (module skeleton, harness `replicationSystem` field in the result JSON, CI gate wiring).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-001, story-002, story-004, story-005: the spikes whose Iris reruns the gate would require
- story-015, story-044, story-043: AC-IVP-01, 02 and 03 themselves
- story-047 and story-049: the leak catalogue review and the external-build critical gate
- Iris adoption and the Iris descriptor path (S7, S11): ADR-0024

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `tools/ci/gate/` (Iris rerun gate script and fixtures)
- `Source/StealthFoundation/Private/Tests/Build/IrisRerunGateTest.cpp` (test `StealthGame.Build.IrisRerunGate`, class `FBuildIrisRerunGateTest`)

---

## QA Test Cases

- **AC-1**: AC-IVP-29 `StealthGame.Build.IrisRerunGate` (fixture-driven gate-script test, T0/T1)
  - Given: a build with Iris enabled and a build with it disabled, as fixtures.
  - When: the release gate runs.
  - Then: with Iris enabled, recorded Iris passes must exist for S1, S2, S7, S9, S10 and AC-IVP-01, 02, 03, and missing passes block the build; with Iris off the gate is inert and does not block; control: the Iris-enabled, passes-missing fixture must block.
  - Edge cases: remove each of the 8 passes singly.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Build/IrisRerunGateTest.cpp` (the studio docs' "tests/unit/build/") must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: tuning-data-config Exit A (module skeleton, harness result JSON with `replicationSystem`, CI gate wiring)
- Unlocks: None
