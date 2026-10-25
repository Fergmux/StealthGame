# Story 029: Parser for the Tuning Knobs tables of every GDD

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-002` (text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); supports GDD AC-3b (CR-15) and CR-15
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This is the first story of Exit B-1 (doc lint and registry completion, TD-REG-1, AC-3b). Every later doc-lint story (030 to 034, 036) consumes the records this parser emits, so it is built first. CR-15 says every GDD that owns a tunable lists it in its own Tuning Knobs table with the columns `Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low`, and the TDC GDD's "Registered Owner Keys" table records, per owner GDD, how many keys it registered. The parser turns those Markdown tables into records and compares per-GDD counts with the Registered Owner Keys table, so a later lint never has to scrape Markdown itself. Two owner GDDs (Level Socket Contract and Procedural City Generator) use an owner-level preamble style rather than the plain table, which the parser must also handle.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: Every gameplay number is a field of a tuning sub-struct with its schema declared once; a design-doc lint (AC-3b) compares each owner GDD's Tuning Knobs rows with the schema and the registry so the three never drift. The lint reports and never edits a value or a GDD.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API: a build-time script over Markdown. ADR-0004 rates its engine risk LOW for this area. Python is allowed only as a build-time tool, run with `python -I`, and is never an editor gate (plan Note; manifest forbids a gate that depends on Python editor scripting). Nothing in this story is an unverified engine claim.

**Control Manifest Rules (this layer)**:
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Required: every lint gate includes a negative control the suite detects (here: a fixture the parser must read exactly, and a seeded discrepancy it must report) — source: ADR-0008
- Forbidden: never make a gate depend on the MCP toolsets or Python editor scripting — source: ADR-0008

---

## Acceptance Criteria

*From the story plan (parser supporting GDD AC-3b), scoped to this story:*

- [ ] A parser reads every `design/gdd/*.md` Tuning Knobs section and emits one record per key (Key, Type, Owner, Default, Hard range, Safe range, Audience, Latch, Unit) including the owner-level preamble style used by the LSC and Generator tables, and the record count per GDD matches the Registered Owner Keys table of the TDC GDD (or the discrepancy is listed as an observation).

---

## Implementation Notes

*Derived from ADR-0004 section 2 (schema and drift guard), the TDC GDD CR-15, and ADR-0008 section 1 (T0 static tier):*

- **Files.** `tools/ci/lint/knob_tables.py` (extension open: the plan allows `.py` or `.sh`; Python is recommended because it must emit structured records) and `tools/ci/lint/selftest/knob_tables_selftest.*`. The plan's `TOOLS/` is `tools/ci/`.
- **Records.** One record per key with exactly: Key, Type, Owner, Default, Hard range, Safe range, Audience, Latch, Unit; plus the source GDD path and line number, so later lints can report `file:line`. The Effect column is not part of the record but its presence is needed by story 031, so the parser also exposes each table's header cells. The record stream format (JSON lines or similar) is not fixed in the inputs: open; it must be stable because stories 030 to 034 and 036 read it.
- **Table styles.** (a) The standard table headed by the CR-15 columns. (b) The owner-level preamble style used by the LSC and Generator GDDs, where owner-level text precedes or stands in for per-row columns; read the two GDDs' Tuning Knobs sections at implementation to define it exactly, since the inputs do not describe it. Column order variants must be tolerated (map by header name, not position).
- **Observations, not verdicts.** The parser emits observations (one `file:line rule message` per finding) and records; it does not decide pass or fail. Observations: a GDD without a Tuning Knobs section; duplicate key rows inside one GDD; per-GDD count differing from the Registered Owner Keys table. The count comparison reads the "Keys" cell of each row of that table (cells begin with a number but also carry free-text notes about later additions, so extraction needs care; the GDD says the counts are as of the 2026-10-24 reconciliation with later changes noted inline). Where the table and the owner table disagree the discrepancy is listed, not resolved; the owner table is authoritative (CR-15).
- **Scope.** All `design/gdd/*.md` files that have a Tuning Knobs section (about 19 headings at writing, including the TDC GDD itself). Files under `design/gdd/reviews/` are not GDDs; whether the glob includes the cross-review files in `design/gdd/` is open (they carry no Tuning Knobs section).
- **Python hygiene.** Invoke with `python -I` (isolated mode: no script-directory or current-directory imports); standard library only; keep it a build-time tool.
- **Selftest (G2 a).** `tools/ci/lint/selftest/knob_tables_selftest.*` runs the parser on fixture GDDs (a standard table, an owner-level preamble table) and on the real tree, asserting the records and that a seeded count mismatch is reported as an observation.
- **Wrapper (G2 b).** `StealthGame.Build.KnobTableParser` in `Source/StealthFoundation/Private/Tests/Build/KnobTableParserTest.cpp` runs the selftest through `FPlatformProcess` and asserts exit code and observations. The wrapper needs Foundation (story 001), which is not a declared dependency; locating Python and bash on Windows from the wrapper is open.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-030-registry-mirror-completion.md`: adding the missing keys to `design/registry/entities.yaml`
- `production/epics/tuning-data-config/story-031-doc-lint-cr15-columns.md`: the CR-15 column check
- `production/epics/tuning-data-config/story-032-doc-lint-one-owner.md`: the exactly-one-owner check
- `production/epics/tuning-data-config/story-033-doc-lint-registry-match.md`: the registry-match check
- `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`: the schema-row match and pending-owner baseline
- `production/epics/tuning-data-config/story-036-doc-lint-bounded-by.md`: the invariant "bounded by" check (also reads the TDC GDD Formulas owner-invariants table)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/lint/knob_tables.py` (the parser; also the table-style, pair-row and invariant extraction used by 031 to 036)
- `tools/ci/lint/selftest/knob_tables_selftest.sh` (29 cases), `tools/ci/lint/selftest/knob-lib.sh` (shared selftest helpers)
- `tools/ci/lint/knob-common.sh` (interpreter launcher shared by the doc-lint rule scripts and selftests)
- `tools/ci/lint/fixtures/knob/base/` (fixture GDDs and registry)
- `Source/StealthFoundation/Private/Tests/Build/KnobTableParserTest.cpp` (G2 wrapper `StealthGame.Build.KnobTableParser`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the parser emits one record per key for the standard and owner-level preamble styles and compares per-GDD counts with the Registered Owner Keys table
  - Given: fixture GDDs with a standard Tuning Knobs table and the owner-level preamble style (LSC and Generator), and the real `design/gdd/*.md`
  - When: the parser runs
  - Then: one record per key with Key, Type, Owner, Default, Hard range, Safe range, Audience, Latch, Unit; the per-GDD record count is compared with the TDC Registered Owner Keys table and discrepancies are printed as observations
  - Edge cases: a GDD without the section; duplicate key rows; table column order variants

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `tools/ci/lint/selftest/knob_tables_selftest.*` (run with `python -I`) and the wrapper `StealthGame.Build.KnobTableParser` (`Source/StealthFoundation/Private/Tests/Build/KnobTableParserTest.cpp`) must exist and pass

**Status**: [x] Created and passing (`knob_tables_selftest.sh` 29/29 cases; `StealthGame.Build.KnobTableParser`)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`
- Unlocks: `production/epics/tuning-data-config/story-030-registry-mirror-completion.md`, `production/epics/tuning-data-config/story-031-doc-lint-cr15-columns.md`, `production/epics/tuning-data-config/story-032-doc-lint-one-owner.md`, `production/epics/tuning-data-config/story-033-doc-lint-registry-match.md`, `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`, `production/epics/tuning-data-config/story-036-doc-lint-bounded-by.md`
- Open verification claims: the plan Note (Python only as a build-time tool, never an editor gate, run with `python -I`) is a rule, not an engine claim. Open: the owner-level preamble layout, the record stream format, whether cross-review files are in scope, and the Foundation-module dependency of the G2 wrapper.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: `python -I tools/ci/lint/knob_tables.py --records` emits one record per key with Key, Type, Owner, Default, Hard range, Safe range, Audience, Latch, Unit (plus Effect, source GDD and line) for the standard style and the owner-level preamble style of the LSC and Generator GDDs; 416 records over 19 GDDs. Per-GDD counts match the TDC Registered Owner Keys table for every GDD except two, listed as observations: LSC (table says 80, owner table has 84 owned rows) and the TDC row, which gives no leading number)
**Decisions**: owned table = Key plus a Hard or Safe range column, so Roster's consumed-elsewhere table gives no record (CR-15.3); struck-through rows are `retired` records and `**Owned by** X` rows are `ref` records, neither counts as ownership (the LSC lists two SCL traversal heights this way); pair rows (`A` / `B`, `A` / `MaxCount`, `...MarketMult`) are expanded per name and every cell is split in step; the preamble style takes Owner, Audience and Latch from the text above the table and lists them in `inherited`; `scope` marks the TDC schema constant and catalog flags (not preset fields); records are JSON lines with `kind` table/knob/ref/retired; the cross-review reports and `systems-index.md` are outside the scan, `game-concept.md` is scanned and reported by `knob-no-section`
**Deviations**: none
**Test Evidence**: `production/qa/evidence/story-029-knob-table-parser/` (`selftest.txt` 29/29, `stamp.txt`); `StealthGame.Build.KnobTableParser` passes (full suite 52/52)
**Run result**: N/A — build-time Markdown parser; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): dead code and a stale docstring removed; no defects left
