# Story 065: AC-29 CatalogClean (CI script)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-004` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-29, CR-6

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-6 says the runtime clamp is only a safety net: a catalog preset that needs any clamp is a CI failure, and the same holds for rejection. The GDD adds that each HostSelectable preset and `Default` must also be free of I-4 (blind-guess) warnings, because I-4 is a reject in a public preset and a warning elsewhere. Story 056 creates the three presets and the catalog; this story is the gate that keeps them clean as designers edit them. The GDD lists AC-29 among the CI scripts rather than automation tests: an editor automation test (`StealthGame.Tuning.CatalogClean`) does the validation, and `tools/ci/catalog-clean.ps1` runs it headless and emits observations that a gate decides (ADR-0008 observation rule).

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The safe-range lint covers single-key probes; the catalog check covers the presets that actually ship, validated by the same `FTuningPipelineCore`; the lint reports and never fixes, and the gate script decides.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Editor automation over data assets, all pre-cutoff. The result-line contract that the script parses (`Test Completed`, `Result={Fail}`, and "No automation tests matched" as not assessed) comes from ADR-0008 section 8 and the per-commit gate (story 021); the exact log text is [V] only as far as story 021 observes it. None of ADR-0004's three Verification Required items is touched.

**Control Manifest Rules (this layer)**:
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Required: every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008
- Required: tests named `StealthGame.<System>.<Scenario>`; runs select with `^StealthGame.` or `Group:`, never `RunFilter` — source: ADR-0008
- Guardrail: per-commit pipeline at most 10 minutes (a script that cannot fit moves to nightly by amendment, never by disabling) — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-29 (CR-6)**: GIVEN every catalog preset, WHEN validated in CI, THEN each reports zero clamps and zero rejections, and each `HostSelectable` preset and `Default` also reports zero I-4 warnings. (`StealthGame.Tuning.CatalogClean`)

---

## Implementation Notes

*Derived from ADR-0004 sections 3 and 4 and ADR-0008 sections 3 and 8, with amendment G2:*

- **Editor test.** `Source/StealthGameEditor/Private/Tests/Tuning/CatalogCleanTest.cpp`, class `FTuningCatalogCleanTest`, name `StealthGame.Tuning.CatalogClean`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS` (it reads the catalog and preset assets, so it belongs in the editor module). It loads the catalog, and for each entry builds a value preset and resolves it through `FTuningPipelineCore` with the entry's `bIsPublic` (Default or HostSelectable) and `PresetId`, reading the `FValidationReport`: any clamp, any defaulted value, any rejection by Id, and for HostSelectable presets and `Default` any I-4 warning, is a finding.
- **Scope of the I-4 clause.** I-4 warns in a non-selectable preset and rejects in a public one. A non-selectable preset with a clean I-3 warning (for example `Solo`, `TeamSize` 1 with `NpcCount` 50) is not an I-4 finding (QA edge case). Do not widen the finding to all warnings; warnings other than I-4 on a HostSelectable preset or `Default` are not named by the criterion and are open (the plan's edge text says only I-4).
- **Empty catalog.** A catalog with zero entries is NOT ASSESSED, not a pass (QA edge case): the test reports it and the script maps it to a non-zero outcome. The test itself should fail with a clear message in that case.
- **Script.** `tools/ci/catalog-clean.ps1` (the plan's `TOOLS/` is `tools/ci/`) runs `UnrealEditor-Cmd -nullrhi` with `Automation RunTests StealthGame.Tuning.CatalogClean` once, parses `Test Completed` and `Result={Fail}` lines, and emits one observation per finding. It does not decide. The gate (a small gate section or a shared gate under `tools/ci/lint/gate.sh` from story 011, the plan does not say which) decides: NOT ASSESSED nonzero when no test matched, FAIL on any `Result={Fail}`, PASS only with a `Test Completed` and no failure. Because a plain `RunTests StealthGame.` is a substring match [V], use the full test name so the run cannot select engine tests.
- **Negative control (ADR-0008 section 3).** A seeded dirty preset fixture must make the check fail, and the suite passes only if that failure is detected. Build the dirty preset in memory inside the test (for example `NpcCount` outside its hard range 6 to 100, which the F-1 stage must clamp) and wrap it in `FStealthNegativeControl` (story 010) so the enclosing test fails if the dirty fixture is reported clean. Do not create a dirty `.uasset` in `Content/`.
- **G2 (script story).** Ship both: (a) a selftest under `tools/ci/selftest/` (or beside the script) that feeds canned automation log fixtures to the script and asserts a clean pass, a seeded finding, a no-match NOT ASSESSED and a failing result are each classified correctly; and (b) a wrapper automation test `StealthGame.Build.CatalogClean` in `Source/StealthFoundation/Private/Tests/Build/` that runs the script through `FPlatformProcess` and asserts exit code and observations. A wrapper that relaunches the editor from inside an editor test would be recursive, so the wrapper should run the script's selftest mode over canned fixtures and not the full editor run: this is a proposal and the wrapper design is open for `/dev-story`. The selftest is run by the T0 gate step (story 021).
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-056-default-preset-assets-and-catalog.md`: the presets and catalog themselves
- `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`: the I-1 to I-5 table (including I-4) the validator evaluates
- `production/epics/tuning-data-config/story-094-safe-range-lint-baseline.md`: single-key safe-range probing with a baseline
- `production/epics/tuning-data-config/story-021-per-commit-gate-script.md`: the per-commit gate that runs the T0 selftests
- `production/epics/tuning-data-config/story-010-negative-control-helper.md`: `FStealthNegativeControl`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameEditor/Private/Tests/Tuning/CatalogCleanTest.cpp`
- `tools/ci/catalog-clean.ps1`
- `tools/ci/selftest/` (selftest for the script, name proposed; G2)
- `Source/StealthFoundation/Private/Tests/Build/CatalogCleanScriptTest.cpp` (G2 wrapper `StealthGame.Build.CatalogClean`, name proposed)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: every catalog preset validates clean (test `StealthGame.Tuning.CatalogClean`, an editor test run by `catalog-clean.ps1`)
  - Given: every catalog preset; a seeded dirty preset fixture
  - When: validated
  - Then: zero clamps and zero rejections for every preset; zero I-4 warnings for HostSelectable presets and `Default`; the seeded dirty fixture makes the check fail
  - Edge cases: an empty catalog is NOT ASSESSED; a non-selectable preset with a clean I-3 warning is not an I-4 finding

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameEditor/Private/Tests/Tuning/CatalogCleanTest.cpp` must exist and pass (`StealthGame.Tuning.CatalogClean`)
- Script (G2): the selftest for `tools/ci/catalog-clean.ps1` (clean pass and seeded violation detected) and the wrapper test `StealthGame.Build.CatalogClean` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-056-default-preset-assets-and-catalog.md`, `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. Open items found while writing: the wrapper design under G2 (recursive editor launch), which gate script decides, whether non-I-4 warnings on public presets count, and the exact automation result lines (observed by story 021).
