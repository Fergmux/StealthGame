# Story 033: Doc lint: every owned key matches an entities.yaml constant

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-002` (text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-3b (CR-15), third bullet (registry half); CR-15.7
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

The third bullet of AC-3b says every owned key matches a schema row and a `design/registry/entities.yaml` constant. The plan splits it in two: this story is the registry half (bullet 3a), story 034 the schema half (bullet 3b). CR-15.7 has the orchestrator mirror each owner's knobs into registry constants with `source:` set to the owner GDD; story 030 completes that mirror, and this lint then keeps owner tables and registry from drifting. The lint compares, per key, the name, source GDD, default, hard range, safe range, audience and latch of the owner GDD row with the registry constant and reports each mismatching field as its own observation. When this lint reports zero findings on the real tree, story 037 can close TD-REG-1.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The owner GDD tables, the generated schema and the registry are compared by a design-doc lint in CI; the lint reports observations and never edits a value or a GDD.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API: a build-time script over Markdown and YAML (Python, if used, only as a build-time tool run with `python -I`). Nothing here is an unverified engine claim.

**Control Manifest Rules (this layer)**:
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Required: every lint gate includes a negative control the suite detects (one fixture mismatch per compared field) — source: ADR-0008
- Forbidden: never make a gate depend on the MCP toolsets or Python editor scripting — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, AC-3b third bullet (registry half), scoped to this story:*

- [ ] **AC-3b (CR-15)**: GIVEN `design/gdd/*.md`, WHEN the doc lint runs in CI, THEN every owned key matches a `design/registry/entities.yaml` constant (name, source GDD, default, hard range, safe range, audience, latch); a mismatch is reported per field.

---

## Implementation Notes

*Derived from CR-15.7, ADR-0004 section 2 and the observed shape of the registry:*

- **File.** `tools/ci/lint/knob-lint-registry.*` (extension open; the plan's `TOOLS/lint/`), reading story 029 records and the `constants:` list of `design/registry/entities.yaml`. It emits `file:line rule message` observations through the story 011 emitter, one per mismatching field, naming the key and the field; it never decides pass or fail.
- **Field mapping (observed in the registry).** GDD Key to constant `name`; owner GDD path to `source`; Default to `value`; Hard range to `hard_range`; Safe range to `safe_range`; Audience to `audience`; Latch to `latch`. The plan's field list does not include Unit, so a unit mismatch is not a finding here.
- **Normalisation (open).** The AC does not define comparison rules, and the two sides are formatted differently: the registry ranges and `value` are free text or numbers (for example `"4,000-10,000"` with a thousands comma, and safe ranges that carry prose such as `"4,000-4,400 on the generator (hand-built maps up to 8,000 ...)"`), while the GDD cells have their own formatting. The spec names one rule: numeric formatting equivalence, 60 equals 60.0. Anything further (thousands separators, dash styles, prose after a range, units inside the cell) must be decided by the developer against the real tree, documented in the script header, and covered by the selftest; do not weaken a field to make the real tree pass.
- **Missing constant.** A key with no constant at all is reported as a missing constant (spec edge case), as one observation, not one per field.
- **Real tree.** With story 030 done the real tree is expected to report zero findings; that expectation is the precondition of story 037. Whether it does is decided by the story 035 gate, not by this story.
- **Selftest (G2 a).** `tools/ci/lint/selftest/` fixtures with a mismatch in each of name, source GDD, default, hard range, safe range, audience and latch (one observation per field, naming key and field) plus a matching key (silent) and a 60 versus 60.0 pair.
- **Wrapper (G2 b).** `StealthGame.Build.KnobLintRegistry` in `Source/StealthFoundation/Private/Tests/Build/KnobLintRegistryTest.cpp` runs the selftest through `FPlatformProcess` and asserts exit code and observations. Foundation (story 001) is not a declared dependency: open.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-029-knob-table-parser.md`: the parser this lint reads
- `production/epics/tuning-data-config/story-030-registry-mirror-completion.md`: adding the missing constants
- `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`: the schema-row half of AC-3b bullet 3
- `production/epics/tuning-data-config/story-035-doc-lint-duplicate-fixture.md`: the composed gate `StealthGame.Tuning.KnobRegistryLint` and the duplicated-key fixture
- `production/epics/tuning-data-config/story-037-close-td-reg-1.md`: closing TD-REG-1 once this lint is clean

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/lint/knob-lint-registry.*`
- `tools/ci/lint/selftest/` (registry-match selftest and fixtures)
- `Source/StealthFoundation/Private/Tests/Build/KnobLintRegistryTest.cpp` (G2 wrapper `StealthGame.Build.KnobLintRegistry`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: each mismatching field between an owned key and its registry constant is reported; a matching key is silent
  - Given: fixtures with a mismatch in each of name, source GDD, default, hard range, safe range, audience, latch
  - When: the lint runs against `entities.yaml`
  - Then: one observation per mismatching field naming the key and the field; a matching key is silent
  - Edge cases: a constant missing entirely; numeric formatting equivalence (60 versus 60.0)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/lint/selftest/` and the wrapper `StealthGame.Build.KnobLintRegistry` (`Source/StealthFoundation/Private/Tests/Build/KnobLintRegistryTest.cpp`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-029-knob-table-parser.md`, `production/epics/tuning-data-config/story-030-registry-mirror-completion.md`
- Unlocks: `production/epics/tuning-data-config/story-035-doc-lint-duplicate-fixture.md`, `production/epics/tuning-data-config/story-037-close-td-reg-1.md`
- Open verification claims: the plan has no Note line for this story. Open: the range and default normalisation rules (only 60 versus 60.0 is specified), and the Foundation-module dependency of the G2 wrapper.
