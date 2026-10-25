# Story 032: Doc lint: every key appears as owned in exactly one GDD

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-002` (text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-3b (CR-15), second bullet; CR-15.3
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

CR-15.3 says every key has a single owner: another GDD using the value references the owner's key and never restates or alters it. The GDD's edge cases add that if two owner GDDs register the same key name, or one quantity under two names, the doc lint (AC-3b) and the schema test fail, and it is resolved by a change request, never by suffixing. This story is the exactly-one-owner bullet of AC-3b: it reads the parser records from story 029 and reports a key owned by zero or by two GDDs. Story 035 composes the AC-3b lints into the gate `StealthGame.Tuning.KnobRegistryLint`.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The owner GDD tables are compared with the schema and registry by a design-doc lint in CI; the lint reports observations and never edits a GDD or a value; a duplicate owner is a change request.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API: a build-time script over Markdown (Python, if used, only as a build-time tool run with `python -I`). Nothing here is an unverified engine claim.

**Control Manifest Rules (this layer)**:
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Required: every lint gate includes a negative control the suite detects (fixtures with a zero-owner key and a two-owner key) — source: ADR-0008
- Forbidden: never make a gate depend on the MCP toolsets or Python editor scripting — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, AC-3b second bullet, scoped to this story:*

- [ ] **AC-3b (CR-15)**: GIVEN `design/gdd/*.md`, WHEN the doc lint runs in CI, THEN every key appears as owned in exactly one GDD; a key owned by zero or two GDDs is reported with both file names.

---

## Implementation Notes

*Derived from CR-15.3, the GDD edge case on duplicate registration, and ADR-0004 section 2:*

- **File.** `tools/ci/lint/knob-lint-owner.*` (extension open; the plan's `TOOLS/lint/`), over the story 029 records. It emits `file:line rule message` observations through the story 011 emitter and never decides pass or fail.
- **What counts as owned.** A key is owned by a GDD when it is a row of that GDD's Tuning Knobs table (a key mentioned elsewhere, for example in prose or Formulas, is a reference, not ownership: spec edge case "key referenced (not owned) in a second GDD"). A key named in two owners' Tuning Knobs tables is reported with both file names (the AC wording; include line numbers too).
- **Zero-owner case (open).** The AC says a key owned by zero GDDs is reported, but a key with no owning row has to come from somewhere. The inputs do not define the key universe for the zero-owner check (candidates: keys named by registry constants, by schema rows once story 041 exists, or by invariant and Formulas references). Decide the source, document it in the script header and make the fixture reflect it; do not invent a universe silently.
- **Renamed key.** The spec lists a renamed key as an edge case: a key that appears under an old name in one GDD and a new name in another is two keys, and the old name will show as zero-owner or as a stray; keep the report factual. Quantities held under two names (CR-15.3) are not detectable by name alone; this lint does not try.
- **Real tree.** Whether the real `design/gdd/*.md` yields zero findings is decided by story 035's gate; this story reports.
- **Selftest (G2 a).** `tools/ci/lint/selftest/` fixtures: a key owned by zero GDDs, a key owned by two (both file names present in the observation), a key owned once (silent), and a key referenced but not owned in a second GDD (silent).
- **Wrapper (G2 b).** `StealthGame.Build.KnobLintOwner` in `Source/StealthFoundation/Private/Tests/Build/KnobLintOwnerTest.cpp` runs the selftest through `FPlatformProcess` and asserts exit code and observations. Foundation (story 001) is not a declared dependency: open.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-029-knob-table-parser.md`: the parser this lint reads
- `production/epics/tuning-data-config/story-031-doc-lint-cr15-columns.md`: AC-3b bullet 1 (columns)
- `production/epics/tuning-data-config/story-033-doc-lint-registry-match.md`: AC-3b bullet 3, registry half
- `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`: AC-3b bullet 3, schema half
- `production/epics/tuning-data-config/story-035-doc-lint-duplicate-fixture.md`: the closing duplicated-key fixture GDD and the gate script

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/lint/knob-lint-owner.sh` (rule script) over the `owner` rule of `tools/ci/lint/knob_lint.py`
- `tools/ci/lint/selftest/knob_lint_owner_selftest.sh` (11 cases)
- `Source/StealthFoundation/Private/Tests/Build/KnobLintOwnerTest.cpp` (G2 wrapper `StealthGame.Build.KnobLintOwner`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a key owned by zero or two GDDs is reported (two-owner with both file names); a single owner is silent
  - Given: fixtures: a key owned by zero GDDs; a key owned by two GDDs; a key owned once
  - When: the lint runs
  - Then: the zero-owner and two-owner keys are each reported (the two-owner one with both file names); the single owner is silent
  - Edge cases: a key referenced (not owned) in a second GDD; a renamed key

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/lint/selftest/` and the wrapper `StealthGame.Build.KnobLintOwner` (`Source/StealthFoundation/Private/Tests/Build/KnobLintOwnerTest.cpp`) must exist and pass

**Status**: [x] Created and passing (`knob_lint_owner_selftest.sh` 11/11 cases; `StealthGame.Build.KnobLintOwner`)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-029-knob-table-parser.md`
- Unlocks: `production/epics/tuning-data-config/story-035-doc-lint-duplicate-fixture.md`
- Open verification claims: the plan has no Note line for this story. Open: the key universe for the zero-owner case, and the Foundation-module dependency of the G2 wrapper.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: a key that is a row of two GDDs' Tuning Knobs tables (or twice in one) is reported once with every file and line; a registry constant that no GDD owns is reported as a zero-owner key at its registry line; a single owner, a key only referenced in a second GDD (consumed table, `**Owned by**` reference row, prose), a retired row and a deprecated constant are silent; a renamed key shows as a zero-owner constant under its old name; the real tree: 21 files scanned, 0 findings)
**Decisions**: the zero-owner key universe is the active (not deprecated) constants of `design/registry/entities.yaml`; a tree without a registry is NOT ASSESSED (exit 3 from the rule), never a pass; quantities held under two names (CR-15.3) are not detectable by name and are not attempted
**Deviations**: none
**Test Evidence**: `production/qa/evidence/story-032-doc-lint-one-owner/` (`selftest.txt` 11/11, `stamp.txt`); `StealthGame.Build.KnobLintOwner` passes (full suite 52/52)
**Run result**: N/A — build-time doc lint; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects found
