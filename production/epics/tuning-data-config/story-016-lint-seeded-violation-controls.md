# Story 016: Seeded violation per lint rule watched to fail

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 3 and ADR-0007 Validation Criteria 4 and 7; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: "A gate you have not watched fail is not a gate": every T0 lint ships one seeded violation per rule that the suite must detect, so the include lint (rules a-j) is only trusted once a fixture tree holding exactly one seeded violation per rule is shown to fail the gate while the real tree passes.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API in the lint or the fixtures; the wrapper test uses `FPlatformProcess` and `EAutomationTestFlags` (application-context flag mandatory). The fixture tree is plain text outside `Source/`, so UBT never compiles it. No unverified claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects; T0 lint: one seeded violation per rule (source: ADR-0008)
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail (source: ADR-0008)
- Required: The include lint (rules a-j) runs in CI and pre-commit (source: ADR-0007)
- Required: Lints emit observations only; pass/fail lives in the gate script (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), from `tdc-plan.md`:*

- [ ] A fixture tree holding exactly one seeded violation per include-lint rule (a)-(j) makes the lint report each rule's finding, and the gate script fails on the fixture and passes on the real tree (the control is detected, ADR-0007 Validation 4 and 7).

---

## Implementation Notes

*Derived from ADR-0008 sections 1 and 3; ADR-0007 Validation Criteria 4 and 7; amendments G2 and G6:*

- **Fixture tree** `tools/ci/lint/fixtures/seeded/**`: a miniature repository layout that mimics `Source/<Module>/...` so the path-based rules apply, holding exactly one seeded violation for each of the ten rules (a)-(j) and nothing else that violates any rule. Rules (g) and (h) have several sub-variants in their stories' own fixtures; here each contributes one seed. Because the lint rules are path-based, the fixture needs its own module folders (for example a Foundation-like module with a header directly in `Public/`).
- **Dependency on the rule scripts**: stories 012-015 each expose their script with a repository-root argument and use the ADR-0007 section 3 rule letter as the rule id. This story relies on that contract. If one of them chose differently, adapt this story's selftest to the shipped contract and record the difference.
- **Selftest** `tools/ci/lint/selftest/seeded-controls.sh`: run all four rule scripts against the fixture root and then the gate. Assertions: exactly ten findings, one per rule id a-j; the gate exits nonzero on the fixture; the gate exits 0 on the real tree (this also needs story 011's NOT ASSESSED guard not to trigger, so the real tree must contain scannable files). Spec edge cases: removing any single seed makes that rule's finding disappear (the control is detected: automate this as a loop that deletes each seed in a temporary copy and checks that exactly that rule id vanishes); the fixture is excluded from the real-tree scan (its location under `tools/` is outside the G5 scan scope; keep it that way, and keep header names in the fixture from colliding under rule (f) when the fixture root is scanned on its own).
- **G2**: the selftest above is the script-side evidence; add the wrapper automation test `StealthGame.Build.LintSeededControls` (class `FBuildLintSeededControlsTest`, `Source/StealthFoundation/Private/Tests/Build/LintSeededControlsTest.cpp`; name proposed) running it through `FPlatformProcess::ExecProcess` and asserting exit code and the ten observations. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`. It is the gate evidence that ADR-0007 Validation 4 (a seeded violation of each rule is watched to fail once) and 7 (a seeded `UE_LOGF` with an Event Log entry argument is watched to fail once) were met.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`: emitter and gate
- `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`: rules (a)-(c) and their per-rule fixtures
- `production/epics/tuning-data-config/story-013-include-lint-rules-d-f.md`: rules (d)-(f) and their per-rule fixtures
- `production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md`: rules (g)-(h) and their per-rule fixtures
- `production/epics/tuning-data-config/story-015-include-lint-rules-i-j.md`: rules (i)-(j) and their per-rule fixtures
- The Blueprint-audit seeded bypasses of ADR-0007 Validation 5 are not part of this epic's stories 001-017

---

## Files

- `tools/ci/lint/fixtures/seeded/` (the ten-seed fixture tree)
- `tools/ci/lint/selftest/seeded-controls.sh`
- `Source/StealthFoundation/Private/Tests/Build/LintSeededControlsTest.cpp` (G2 wrapper)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: a fixture tree with one seeded violation per rule (a)-(j) is reported rule by rule, the gate fails on it and passes on the real tree
  - Given: a fixture tree with exactly one seeded violation per rule (a)-(j)
  - When: the lint and the gate run on the fixture, then on the real tree
  - Then: ten findings, one per rule id; the gate is nonzero on the fixture; the gate exits 0 on the real tree
  - Edge cases: removing any one seed makes that rule's finding disappear (control detected); the fixture is excluded from the real-tree scan

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `tools/ci/lint/selftest/seeded-controls.sh` (run by the T0 gate step) and the G2 wrapper `StealthGame.Build.LintSeededControls` (`Source/StealthFoundation/Private/Tests/Build/LintSeededControlsTest.cpp`) must exist and pass (qa.level is full, so tests are required)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`, `production/epics/tuning-data-config/story-013-include-lint-rules-d-f.md`, `production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md`, `production/epics/tuning-data-config/story-015-include-lint-rules-i-j.md`
- Unlocks: None
- Open verification claims from the plan's Note lines: none (no Note line on 016). The rule-script contract (repository-root argument, rule letter as rule id) is assumed from stories 012-015.
