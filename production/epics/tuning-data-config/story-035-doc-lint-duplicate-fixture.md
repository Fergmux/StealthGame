# Story 035: Doc lint negative control: a fixture GDD with a duplicated key fails

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-002` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-3b, closing sentence (CR-15 Knob Registry Convention)

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (section 3, mandatory negative controls; section 1, T0 static tier)
**ADR Decision Summary**: Every lint gate ships with a negative control the suite detects, and lints only emit observations (`file:line rule message`) while one gate script decides pass or fail. A gate never watched to fail is not a gate.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0008 governs; the story itself touches no engine API beyond the automation wrapper)
**Engine Notes**: No post-cutoff API in the lint scripts (T0 runs with no engine). The G2 wrapper test uses `FPlatformProcess` and a T1 automation test; ADR-0008 Verification Required (5) (`Automation RunTests` accepts `^` prefix filters, [V `AutomationCommandline.cpp:135-227`]) is the only engine claim touched, and it is verified. `EAutomationTestFlags` is an enum class and every test needs an application-context flag [V `AutomationTest.h:93-144`].

**Control Manifest Rules (this layer)**:
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects (source: ADR-0008)
- Required: Lints and analysis scripts emit observations only; pass/fail lives in the gate script (source: ADR-0008)
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail (source: ADR-0008)
- Guardrail: Per-commit pipeline at most 10 min; the lint and its selftest run in seconds with no engine (source: ADR-0008)

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-3b (closing sentence), scoped to this story:*

- [ ] AND a fixture GDD with a deliberately duplicated key fails the lint. (`StealthGame.Tuning.KnobRegistryLint`, CI script) *Scope per plan: the real `design/gdd/*.md` pass, and the gate script (`tools/ci/lint/knob-registry-lint.sh`) decides the outcome from the observations.*

---

## Implementation Notes

*Derived from ADR-0008 sections 1, 3, 8 and the plan amendments G2 and G6:*

- This story closes AC-3b. Stories 031 to 034 each add one lint over the Tuning Knobs tables (CR-15 columns, one owner, registry match, schema-row match). This story adds the gate `tools/ci/lint/knob-registry-lint.sh`, which runs those lints (the plan's dependencies imply this aggregation; the exact call order is not fixed by the inputs) and is the only place pass or fail is decided, using the story 011 emitter and gate (`tools/ci/lint/emit.*`, `gate.sh`: exit 0 with no findings, nonzero with any, NOT ASSESSED nonzero when no input file was scanned).
- Negative control: `tools/ci/lint/fixtures/knob/dup-key-gdd.md` holds a deliberately duplicated key. Gate on the fixture must exit nonzero and print the duplicate-key observation naming the key and the file. How the duplicate is formed (the same key twice in one fixture table, or a key that the real tree also owns) is not specified by the inputs; choose one, keep the fixture minimal, and make the observation name both occurrences (story 032 reports "both file names").
- Real tree: the gate over `design/gdd/*.md` must pass. This depends on the registry mirror (story 030) and on the pending-owner baseline (story 034) being in place; if the real tree reports findings the story is not done, it is not weakened.
- Empty input: when no GDD file is scanned the gate reports NOT ASSESSED with a nonzero exit, never a pass.
- The parser (story 029) is Python used only as a build-time tool, run with `python -I`; the gate itself is a shell script (Git Bash on the dev machine).
- G2 (script stories ship both): (a) a selftest under `tools/ci/lint/selftest/` run by the T0 gate step, covering a clean pass and the seeded duplicate being detected; (b) an automation wrapper test `StealthGame.Build.KnobRegistryLint` in `Source/StealthFoundation/Private/Tests/Build/KnobRegistryLintTest.cpp` that runs the script through `FPlatformProcess` and asserts the exit code and the observations. Class `FBuildKnobRegistryLintTest`, flags `EditorContext | ProductFilter`, whole file in `#if WITH_DEV_AUTOMATION_TESTS`. No wall-clock predicate (a process-run ceiling only as a failure ceiling).
- The doc lint, like the other T0 lints, never edits a GDD or the registry.
- Prototype: none. This story has no `Prototype:` line in the plan.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-031-doc-lint-cr15-columns.md`: the CR-15 columns lint.
- `production/epics/tuning-data-config/story-032-doc-lint-one-owner.md`: the exactly-one-owner lint.
- `production/epics/tuning-data-config/story-033-doc-lint-registry-match.md`: the `entities.yaml` match lint.
- `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`: the schema-row match lint and the pending-owner baseline.
- `production/epics/tuning-data-config/story-036-doc-lint-bounded-by.md`: the "bounded by <Id>" lint (its own gate and baseline).
- `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`: the shared emitter and gate this story reuses.

---

## Files

- `tools/ci/lint/fixtures/knob/dup-key-gdd.md`
- `tools/ci/lint/knob-registry-lint.sh` (the gate)
- `tools/ci/lint/selftest/` (G2 selftest for the gate; file name not fixed by the plan)
- `Source/StealthFoundation/Private/Tests/Build/KnobRegistryLintTest.cpp` (G2 wrapper)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: A fixture GDD with a deliberately duplicated key fails the lint; the real `design/gdd/*.md` pass; the gate decides from the observations.
  - Given: `fixtures/knob/dup-key-gdd.md` with a deliberately duplicated key; the real `design/gdd/*.md`.
  - When: the gate (`knob-registry-lint.sh`, CI script `StealthGame.Tuning.KnobRegistryLint`) runs on each.
  - Then: the fixture fails with the duplicate observation; the real GDDs pass; the gate decides from the observations (the lints emit, they never decide).
  - Edge cases: no GDD input is NOT ASSESSED (nonzero), never a pass.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the automation wrapper `Source/StealthFoundation/Private/Tests/Build/KnobRegistryLintTest.cpp` (`StealthGame.Build.KnobRegistryLint`) must exist and pass, and the selftest under `tools/ci/lint/selftest/` must pass in the T0 gate step (clean pass and seeded duplicate detected, G2).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-031-doc-lint-cr15-columns.md`, `production/epics/tuning-data-config/story-032-doc-lint-one-owner.md`, `production/epics/tuning-data-config/story-033-doc-lint-registry-match.md`, `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`
- Unlocks: None
- Open verification claims from the plan Note lines: none (the plan has no Note line for this story).
