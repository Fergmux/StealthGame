# Story 031: Doc lint: every Tuning Knobs table has the CR-15 columns

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-002` (text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-3b (CR-15), first bullet
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

AC-3b is the design-document lint that keeps the owner GDDs, the generated schema and the entity registry from drifting apart. Its first bullet is the cheapest and the one every other bullet depends on: if a Tuning Knobs table lacks a CR-15 column (for example Latch), a later lint cannot compare that field at all. CR-15.1 defines the columns as `Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low`. This story is the column check only; it reads the parser's header cells from story 029 and reports each missing column as an observation naming the file and the column. Story 035 composes the AC-3b lints into the gate (`StealthGame.Tuning.KnobRegistryLint`).

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The owner GDD tables are compared with the schema and registry by a design-doc lint in CI; the lint reports observations and never edits a GDD or a value.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API: a build-time script over Markdown (Python, if used, only as a build-time tool run with `python -I`). Nothing here is an unverified engine claim.

**Control Manifest Rules (this layer)**:
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Required: every lint gate includes a negative control the suite detects (a fixture table missing one column) — source: ADR-0008
- Forbidden: never make a gate depend on the MCP toolsets or Python editor scripting — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, AC-3b first bullet, scoped to this story:*

- [ ] **AC-3b (CR-15)**: GIVEN `design/gdd/*.md`, WHEN the doc lint runs in CI, THEN every Tuning Knobs table has the CR-15 columns (`Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect`), and a table missing one is reported as an observation naming file and column.

---

## Implementation Notes

*Derived from ADR-0004 section 2 and CR-15.1:*

- **File.** `tools/ci/lint/knob-lint-columns.*` (extension open; the plan's `TOOLS/lint/`), reading the story 029 parser output for header cells and positions. It emits `file:line rule message` observations through the story 011 emitter; it never decides pass or fail.
- **Column match.** The AC lists the last column as `Effect`, while CR-15.1 spells it `Effect / breaks if too high or low`. Match the column by the leading word `Effect`; confirm and document this in the script header. Ten columns are required.
- **Order and extras.** The spec edge cases list reordered columns, an extra column and several tables in one file. The AC says "has the CR-15 columns", which is a presence check: whether a reordered table is a finding is not stated: open. Proposal for the dev to confirm: presence only (reordered and extra columns are not findings), because story 029 maps by header name.
- **Owner-level preamble tables.** The LSC and Generator GDDs use an owner-level preamble style (story 029). Whether those tables must show all ten columns or are handled as their preamble form is a question of story 029's definition of that style: follow its output and record how it is reported.
- **Real tree.** The AC-level statement for the real `design/gdd/*.md` (zero findings, or the findings listed as observations) is decided by story 035's gate, not here; this story reports.
- **Selftest (G2 a).** `tools/ci/lint/selftest/` fixture with a table missing one of the ten columns (reported, naming file and column) and a complete table (silent), per the spec.
- **Wrapper (G2 b).** `StealthGame.Build.KnobLintColumns` in `Source/StealthFoundation/Private/Tests/Build/KnobLintColumnsTest.cpp` runs the selftest through `FPlatformProcess` and asserts exit code and observations. Foundation (story 001) is not a declared dependency: open.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-029-knob-table-parser.md`: the parser this lint reads
- `production/epics/tuning-data-config/story-032-doc-lint-one-owner.md`: AC-3b bullet 2 (exactly one owning GDD)
- `production/epics/tuning-data-config/story-033-doc-lint-registry-match.md`: AC-3b bullet 3, registry half
- `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`: AC-3b bullet 3, schema half
- `production/epics/tuning-data-config/story-035-doc-lint-duplicate-fixture.md`: the closing duplicate-key negative control and the gate script `StealthGame.Tuning.KnobRegistryLint`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/lint/knob-lint-columns.*`
- `tools/ci/lint/selftest/` (column-lint selftest and fixtures)
- `Source/StealthFoundation/Private/Tests/Build/KnobLintColumnsTest.cpp` (G2 wrapper `StealthGame.Build.KnobLintColumns`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a Tuning Knobs table missing a CR-15 column is reported naming file and column; a complete table is silent
  - Given: a fixture table missing one of `Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect`
  - When: the lint runs
  - Then: an observation names the file and the missing column; a complete table gives none
  - Edge cases: reordered columns; an extra column; multiple tables in one file

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/lint/selftest/` and the wrapper `StealthGame.Build.KnobLintColumns` (`Source/StealthFoundation/Private/Tests/Build/KnobLintColumnsTest.cpp`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-029-knob-table-parser.md`
- Unlocks: `production/epics/tuning-data-config/story-035-doc-lint-duplicate-fixture.md`
- Open verification claims: the plan has no Note line for this story. Open: the `Effect` header match, whether reordered or extra columns are findings, and the Foundation-module dependency of the G2 wrapper.
