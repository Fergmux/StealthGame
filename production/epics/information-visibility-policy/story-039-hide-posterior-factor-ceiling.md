# Story 039: HidePosteriorFactor outside 1.0-1.5 fails the harness before AC-IVP-25 runs

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The hide-posterior parity gate (AC-IVP-25, story-040) compares how often NPCs and players enter bushes, against a threshold derived from `HidePosteriorFactor` (k in F5). A larger k makes the threshold easier to pass, so an over-generous k would let a leaky crowd through. The policy therefore caps k at 1.5 (ruling A) and the harness must refuse to run the statistical gate at all if the loaded value is outside 1.0-1.5. This story builds that ceiling check as a pure range rule and wires it into the harness config loader, so a bad config fails loudly before any statistics are computed.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test automation and leak parity (sections 1, 3 and 9: T4 statistical and batch, mandatory negative controls, pre-registered priors and thresholds)
**ADR Decision Summary**: T4 statistical gates run offline analysis over T3 captures with fixed, checked-in seed sets and priors; pre-registered priors and thresholds are checked in beside the tools before the data is collected; every leak, parity, statistical and lint gate ships with a negative control the suite detects.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff API is involved: a plain-C++ range check with an `EAutomationTestFlags` application-context flag on the T1 test (`EAutomationTestFlags` is an enum class and a test without an application-context flag fails a `static_assert`, ADR-0008 Engine Compatibility). The harness config loader reads the value before any T3 or T4 run starts.

**Control Manifest Rules (this layer)**:
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Required: **Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct**; a hardcoded gameplay literal is a defect — source: ADR-0004
- Forbidden: **Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail** — source: ADR-0008
- Guardrail: **Tests**: per-commit pipeline ≤ 10 min; nightly cook + T3 estimated 1-2 h [U] — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-43), scoped to this story:*

- [ ] **AC-IVP-43** [Unit] `StealthGame.Visibility.HidePosteriorFactorCeiling`: GIVEN the test-harness config, WHEN `HidePosteriorFactor` is loaded, THEN 1.0-1.5 is accepted and any value outside it (including 2.0) fails the harness before AC-IVP-25 runs. (F5, ruling A)

---

## Implementation Notes

*Derived from ADR-0008 sections 1, 3, 4 and 9, and IVP F5 (ruling A):*

- Structure: the range rule is a pure function in a `StealthFoundation` plain-C++ core (for example `FHidePosteriorFactorCheck::Validate(double)`), so the T1 test in `StealthFoundation` does not depend on `StealthTestHarness` (no runtime module depends on the harness, ADR-0007); the harness config loader in `StealthTestHarness` calls the same function and fails the harness start-up, with a fault-sink report, when it returns invalid. Do not duplicate the bounds.
- The bounds 1.0 and 1.5 are inclusive and are values of the harness config (data-driven), not literals scattered in code. The config file location and key name are chosen by this story and recorded here and in story-040's pre-registration file (`tools/ci/stats/`), which must read the same key.
- Tests: boundaries 1.0 and 1.5 inclusive accepted; 0.99, 1.51 and 2.0 rejected (and a non-finite or negative value rejected). The negative control is the out-of-range set itself: the suite passes only if each out-of-range value is rejected, and a harness-only variant of the loader that skips validation must be detected.
- The failure must precede AC-IVP-25: assert that a run of story-040's entry point with an out-of-range factor stops before computing any rate (a stub run record shows no result written).
- Pre-registered priors and thresholds sit beside the analysis tools before data collection (ADR-0008 section 9); this story's check protects that pre-registration from being weakened by editing k.
- T1 test: `StealthGame.Visibility.HidePosteriorFactorCeiling` (class `FVisibilityHidePosteriorFactorCeilingTest`), flags `EditorContext | ProductFilter`.
- Prototype: Replaces: nothing. Promotes: nothing.
- Cross-epic: needs tuning-data-config Exit A (module skeleton, harness and CI) only.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-040: the hide-posterior parity statistical run itself
- story-031: the player-only-cull ship switch that reads the run record
- story-041: the Crowd tell-matrix bush row
- crowd-npc-behaviour epic: NPC bush use and `CrowdBushIntervalSec`

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthFoundation/Public/Server/StealthHidePosteriorFactorCheck.h`
- `Source/StealthFoundation/Private/Visibility/StealthHidePosteriorFactorCheck.cpp`
- `Source/StealthTestHarness/Private/Config/` harness config loader hook (location and key name fixed by this story)
- `Source/StealthFoundation/Private/Tests/Visibility/HidePosteriorFactorCeilingTest.cpp` (T1, `FVisibilityHidePosteriorFactorCeilingTest`, `StealthGame.Visibility.HidePosteriorFactorCeiling`)

*Note: the `StealthFoundation` and `StealthTestHarness` module folders are created by tuning-data-config Exit A (they do not exist yet).*

---

## QA Test Cases

- **AC-1**: Hide posterior factor ceiling (AC-IVP-43)
  - Given: the test-harness config.
  - When: `HidePosteriorFactor` is loaded.
  - Then: 1.0 to 1.5 is accepted; any value outside that range, including 2.0, fails the harness before AC-IVP-25 runs.
  - Edge cases: boundaries 1.0 and 1.5 inclusive; test 0.99, 1.51, 2.0.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/HidePosteriorFactorCeilingTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: tuning-data-config Exit A (module skeleton, harness and CI)
- Unlocks: `production/epics/information-visibility-policy/story-040-hide-posterior-parity.md`
