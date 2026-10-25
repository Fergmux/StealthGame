# Story 036: Doc lint: invariant-read keys must say "bounded by <Id>"

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-004` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD CR-15 (Knob Registry Convention), Formulas "Invariants" and "Owner invariants"; ADR-0004 section 4 (the C-W18 pattern)

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: Presets are one data-asset class of per-owner `_Public`/`_Server` sub-structs with a generated schema table; a plain-C++ validator runs the CR-6 ladder and cross-field invariants reject or warn but never repair; a safe-range lint proves every safe range satisfies every invariant. The doc lint (AC-3b family) also reports a key read by an invariant whose owner GDD row does not say "bounded by <Id>".
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Script-only story, no engine API. The only engine-adjacent claim is the G2 wrapper test (`FPlatformProcess`, automation T1), which is pre-cutoff and stable. No unverified claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Every reject/warn invariant carries its GDD ID; a new cross-field constraint goes through a TDC change request and a row in `TuningInvariants.cpp` (source: ADR-0004)
- Required: Lints and analysis scripts emit observations only; pass/fail lives in the gate script (source: ADR-0008)
- Forbidden: Never silence a safe-range lint finding by editing the baseline without citing a GDD change request or OQ (source: ADR-0004)
- Guardrail: The T0 lint runs with no engine in seconds, inside the 10 minute per-commit budget (source: ADR-0008)

---

## Acceptance Criteria

*From the plan amendment for story 036 (REVISED, with Amendments 2), scoped to this story:*

- [ ] The doc lint takes the key-to-invariant mapping from the parser's extraction of the TDC GDD Formulas "Owner invariants" table plus I-1..I-5 (backtick-quoted key names in each invariant row that exist in the key set) and reports, per key, each owner GDD Tuning Knobs row that is read by an invariant but lacks the text "bounded by <invariant Id>"; a fixture shows a conforming row passes and a non-conforming row is reported; the real-tree result is emitted as observations (baseline per G3).

---

## Implementation Notes

*Derived from ADR-0004 section 4 and the plan amendments G2, G3 (Amendments 2) and G6:*

- Input source: the story 029 knob-table parser extracts (a) the Tuning Knobs records of every GDD and (b) the invariant rows of the TDC GDD Formulas section: the `**I-1**` to `**I-5**` bullets and the "Owner invariants" table (`ID | Owner | Constraint | On failure`). For each invariant, the keys it reads are the backtick-quoted names in its text that exist in the parser's key set. Read the GDD Formulas section by line range (Grep `^#` then Read), do not hard-code the key-to-invariant map.
- Check: for every (key, invariant Id) pair, the key's owner GDD Tuning Knobs row (the Effect column) must contain the text `bounded by <Id>` (the C-W18 pattern). One observation per missing pair, in the shape `file:line rule message`, naming the owner GDD and the key. A key read by two invariants needs the text for both Ids (spec edge case). The accepted wording variants are not defined by the inputs (open): match the literal `bounded by <Id>` and settle any variant with a fixture row.
- Baseline (Amendments 2): `tools/ci/lint/knob-bounded-by-baseline.txt`, entries `<gdd-path> | <key> | <invariant-id> | <citation>`. Real-tree findings that are not in the baseline FAIL the gate. Per G3 the gate compares the working-tree baseline with its content at `HEAD` when run from the pre-commit hook, and at the push's before-SHA (`github.event.before`, else `HEAD~1`) in CI; any entry absent at the base fails (creation at the base-less first commit is exempt). The baseline may only shrink.
- Gate: the lint emits observations through the story 011 emitter; the gate script (story 011 `gate.sh` or a thin wrapper of it) is the only place pass or fail is decided, including NOT ASSESSED when no GDD was scanned.
- The same condition is also reported by `SafeRangeLint` (ADR-0004 section 4, story 094); this story owns the doc-lint side only.
- G6: a seeded fixture violation is reported by file:line. The fixture pair (conforming row, non-conforming row) lives under `tools/ci/lint/fixtures/` (sub-path not fixed by the plan).
- G2: (a) a selftest under `tools/ci/lint/selftest/` run by the T0 gate step (clean pass and seeded violation detected, including the baseline-growth failure against a temp repository with a base commit); (b) an automation wrapper `StealthGame.Build.KnobLintBoundedBy` in `Source/StealthFoundation/Private/Tests/Build/KnobLintBoundedByTest.cpp` that runs the script via `FPlatformProcess` and asserts exit code and observations. Class `FBuildKnobLintBoundedByTest`, flags `EditorContext | ProductFilter`, in `#if WITH_DEV_AUTOMATION_TESTS`.
- The parser is Python used only at build time and run with `python -I`. The lint never edits a GDD.
- Prototype: none (no `Prototype:` line in the plan).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-029-knob-table-parser.md`: the Tuning Knobs table parser this lint consumes.
- `production/epics/tuning-data-config/story-031-doc-lint-cr15-columns.md`, `production/epics/tuning-data-config/story-032-doc-lint-one-owner.md`, `production/epics/tuning-data-config/story-033-doc-lint-registry-match.md`, `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`: the other AC-3b lints.
- `production/epics/tuning-data-config/story-035-doc-lint-duplicate-fixture.md`: the AC-3b gate and the duplicated-key negative control.
- `production/epics/tuning-data-config/story-094-safe-range-lint-baseline.md`: the editor-side `SafeRangeLint` and its JSON baseline.
- Editing owner GDD rows to add the missing "bounded by" text is a design change request, not part of this story.

---

## Files

- `tools/ci/lint/knob-lint-bounded-by.sh` (rule script, with the G3 shrink-only check) over the `bounded-by` rule of `tools/ci/lint/knob_lint.py`
- `tools/ci/lint/knob-bounded-by-baseline.txt` (147 entries, each citing TD-REG-2)
- `tools/ci/lint/fixtures/knob/base/` (conforming and non-conforming row fixtures, by mutation in the selftest)
- `tools/ci/lint/selftest/knob_lint_bounded_by_selftest.sh` (22 cases)
- `Source/StealthFoundation/Private/Tests/Build/KnobLintBoundedByTest.cpp` (G2 wrapper `StealthGame.Build.KnobLintBoundedBy`)
- `docs/tech-debt-register.md` (new row TD-REG-2, the citation of every baseline entry)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: The doc lint reports each owner GDD row read by an invariant but lacking "bounded by <invariant Id>".
  - Given: invariant ids and keys from the parser's extraction of the TDC GDD Formulas owner-invariants table plus I-1..I-5 (backticked key names that exist in the key set); fixture rows with and without "bounded by <invariant Id>".
  - When: the lint runs.
  - Then: a conforming row passes; a non-conforming row is reported per key and owner GDD; the real-tree result is emitted as observations.
  - Edge cases: a key read by two invariants; the baseline file path and failure rule are `tools/ci/lint/knob-bounded-by-baseline.txt`, real-tree findings not in the baseline fail the gate, baseline per G3 (Amendments 2).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the automation wrapper `Source/StealthFoundation/Private/Tests/Build/KnobLintBoundedByTest.cpp` (`StealthGame.Build.KnobLintBoundedBy`) must exist and pass; the selftest under `tools/ci/lint/selftest/` must pass in the T0 gate step (G2).

**Status**: [x] Created and passing (`knob_lint_bounded_by_selftest.sh` 22/22 cases; `StealthGame.Build.KnobLintBoundedBy`)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-029-knob-table-parser.md`
- Unlocks: None
- Open verification claims from the plan Note lines: none (the plan has no Note line for this story). The wording variants of the C-W18 "bounded by" text are an open design point, not a verification claim.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: the key-to-invariant map is read from the TDC GDD Formulas section (the `**I-1**` to `**I-5**` bullets and the Owner invariants table; 64 invariant rows, offline rows skipped), keys are the backtick-quoted names that exist in the key set; a conforming row passes, a non-conforming row is reported per key and invariant at its file:line, a key read by two invariants needs both ids, an id range row is satisfied by any id of the range; the baseline hides a listed pair, reports a stale entry and a malformed entry; a baseline that grows fails the shrink-only check against a temporary repository with a base commit, a shrinking baseline and the creating commit are clean; real tree: 20 files scanned, 0 findings beyond the baseline)
**Decisions**: wording accepted is the literal "bounded by <Id>", case-insensitive, with an optional "above"/"below" and at most three words before the id ("bounded below by Generator I-PCG-2" conforms; "I-AB-10" does not satisfy "I-AB-1"); offline invariant rows are skipped; the real tree has 147 (gdd, key, invariant) pairs whose owner rows do not say it, all accepted in `tools/ci/lint/knob-bounded-by-baseline.txt` citing the new tech debt row TD-REG-2 (adding the text is a change request to each owner GDD, which the story puts out of scope); the baseline id of a range row is its first id
**Deviations**: none
**Test Evidence**: `production/qa/evidence/story-036-doc-lint-bounded-by/` (`selftest.txt` 22/22, `stamp.txt`); `StealthGame.Build.KnobLintBoundedBy` passes (full suite 52/52)
**Run result**: N/A — build-time doc lint; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): the message for a range row was reworded so it does not name a single id as required; no defects left
