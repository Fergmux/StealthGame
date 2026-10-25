# Story 094: SafeRangeLint with a shrink-only baseline

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-004`; no GDD AC id (the criterion is ADR-0004 s4; the GDD basis is CR-6 and CR-15.5, safe ranges are warn-only and must satisfy every invariant)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: A safe-range lint, run as an editor automation test and as a CI step and never at runtime, probes every key at its safe extremes, midpoint and every enum, bool or small-int value with the other keys at defaults, and fails CI on any finding not in a committed baseline whose entries each cite the pending GDD change request or OQ that will remove them. The baseline may only shrink.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API for this story. `SafeMin` and `SafeMax` metadata exists only under editor-only data (`WITH_METADATA` is `WITH_EDITORONLY_DATA` [V `CoreMiscDefines.h:31`; reader `FField::GetMetaData`, `Field.h:946`]), which is why the lint is an editor module automation test and not a Foundation T1 test. The schema table carries the numeric `SafeMin`/`SafeMax` columns too; whether the lint reads the table or the metadata is not fixed by the inputs (the ADR names metadata as the reason for editor placement). It is a design choice to make at implementation and record in the test header.

**Control Manifest Rules (this layer)**:
- Forbidden: Never silence a safe-range lint finding by editing the baseline without citing a GDD change request or OQ — source: ADR-0004
- Required: Invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range); invariants reject or warn, never repair — source: ADR-0004
- Required: Lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008

---

## Acceptance Criteria

*From ADR-0004 section 4 (test `StealthGame.Tuning.SafeRangeLint`), as amended (amendment 094 REVISED, G3 and AMENDMENTS 2):*

- [ ] **ADR-0004 s4**: the editor test probes each key at SafeMin, SafeMax, the midpoint and every enum/bool value (every value for an int safe range of 16 or fewer), with the other keys at defaults and bIsPublic true, and fails on any finding not listed in `TuningLintBaseline.json`, each baseline entry citing a pending GDD change request or OQ; the baseline may only shrink, enforced per G3 (any entry absent from the base version of the file fails: the base is `HEAD` when run from the pre-commit hook and the push's before-SHA, else `HEAD~1`, in CI; creation of the file at the base-less first commit is exempt), and the resolved C-W14, C-W21 and OQ-12 items must not appear.

---

## Implementation Notes

*Derived from ADR-0004 section 4 and Implementation Guideline 9, ADR-0008 section 1, and amendment G3:*

- Test: `StealthGame.Tuning.SafeRangeLint`, class `FTuningSafeRangeLintTest`, file `Source/StealthGameEditor/Private/Tests/Tuning/SafeRangeLintTest.cpp`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`. It is also run as a CI step (ADR-0004 s4).
- Probe procedure (ADR-0004 s4): base point is the schema defaults, which must themselves pass every invariant with zero warnings (AC-30, story 096). For each key k the probe set is `SafeMin(k)`, `SafeMax(k)`, the midpoint, and every declared value of an enum or bool; for an `int32` safe range of 16 or fewer values, every value. For each probe, set only k, run F-1 and every non-offline invariant (`GetTuningInvariants()`), with `bIsPublic = true` (the strictest scope), and report each `(key, probe value, invariant Id, severity)` that fails. Single-key probing only; joint corners are not linted.
- Which severities count as a failing finding (Reject only, or Warn as well) is not stated precisely in the inputs: the ADR frames the question as whether a safe value produces an accepted preset, which points to Reject, while AC-30 wants zero warnings only at defaults. Treat it as open and settle it with the TDC owner before the baseline is first seeded; record the decision in the test header.
- The lint reports and never fixes (`.claude/scripts` observation rule): it never edits a value, a GDD or the baseline.
- Baseline file: `Source/StealthFoundation/Private/Tests/Data/TuningLintBaseline.json` (the path ADR-0004 s4 names). It lives in Foundation's private tree but is read by an editor-module test through the project directory path, not an include. Each entry cites a pending GDD change request or OQ. ADR-0004 lists today's expected citations as C-W2, C-W17, C-W18, C-W19 and C-W22 where still open; C-W14 (duplicate latency key), C-W21 (renames) and OQ-12 (`NpcKillStrikeLimit` minimum) were resolved by the user on 2026-10-25 and must not appear. An entry with no citation fails. A stale entry (listed but no longer found) fails.
- Shrink-only (G3, AMENDMENTS 2): the working-tree baseline is compared with its content at the base ref (`HEAD` in the pre-commit hook; the push before-SHA, `github.event.before`, else `HEAD~1`, in CI) and any entry not present at the base fails. Creation of the file at the base-less first commit is exempt. Implement the comparison as a pure function taking the current and base JSON so fixtures can exercise it (entry added, entry removed, no base). Obtaining the base content (`git show <ref>:<path>`) is the gate step's job; the plan names no script for it, so which hook or CI script supplies the base is open and belongs with stories 025 and 022.
- Also reported by ADR-0004 s4 but not in this story's acceptance criterion: a safe range outside the hard range, a default outside the safe range, and a key read by an invariant whose owner row lacks "bounded by <Id>" (story 036). Which story covers the first two is not stated in the plan (open).
- Negative control (ADR-0008 s3): a fixture schema with one key whose safe extreme breaks an invariant, absent from the fixture baseline, must be reported; the test passes only if the control is detected.
- Logging: findings go to `LogStealthFoundation` or the editor log through `UE_LOGF`; counts and keys only.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`: the I-1 to I-5 invariant table the lint evaluates.
- `production/epics/tuning-data-config/story-044-schema-complete.md`: the schema row and default-inside-range checks (AC-3a).
- `production/epics/tuning-data-config/story-036-doc-lint-bounded-by.md`: the "bounded by <Id>" doc lint.
- `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`: the other shrink-only baseline (`knob-schema-pending.txt`) under the same G3 rule.
- `production/epics/tuning-data-config/story-096-owner-invariants-full-schema-closure.md`: Default validating with no warning over the full schema.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameEditor/Private/Tests/Tuning/SafeRangeLintTest.cpp
- Source/StealthFoundation/Private/Tests/Data/TuningLintBaseline.json

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: safe-range lint with shrink-only baseline
  - Given: each key probed at SafeMin, SafeMax, midpoint and every enum/bool value (every value for an int safe range of 16 or fewer), other keys at default, bIsPublic true; `TuningLintBaseline.json` with a GDD change request or OQ citation per entry.
  - When: validated; the baseline is compared with the base version.
  - Then: fails on any finding not in the baseline; an entry without a citation fails; an entry not in the base version fails; the resolved C-W14, C-W21 and OQ-12 are absent.
  - Edge cases: initial creation exempt; a stale baseline entry fails.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Logic
**Required evidence**:
- Logic: the automation test `StealthGame.Tuning.SafeRangeLint` in `Source/StealthGameEditor/Private/Tests/Tuning/SafeRangeLintTest.cpp` must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`, `production/epics/tuning-data-config/story-044-schema-complete.md`
- Unlocks: None
- Open verification claims: whether Warn findings count against the baseline is not stated; which script supplies the base-version baseline content is not named; coverage of "safe range outside hard range" and "default outside safe range" is not assigned in the plan.
