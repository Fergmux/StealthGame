# Story 037: Tell Matrix row citations across the dependent GDDs (advisory check)

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-010 = Baseline and Tell Matrix rates as data with calibration counters.)*

**ADR Governing Implementation**: N/A — documentation traceability check; no ADR governs how to build it (the Tell Matrix is design data owned by `design/gdd/crowd-npc-behaviour.md` R16)
**ADR Decision Summary**: N/A
**ADR Version**: N/A

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: None: this story reads documents only.

**Why this story**: CNB-R16 is the authoritative list of advantages and deviations (T1-T18). Several other GDDs own the tells and must each carry an acceptance criterion citing their rows, otherwise a row can be changed in one document and silently lose its test in another. AC-CNB-30 is the cross-GDD check that none is missing. It is advisory and runs at the cross-GDD review, so it needs no code. Read GDD CNB-R16 and AC-CNB-30 only.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: gameplay design data lives in `design/gdd/`; design docs are not edited by implementation stories (coding standard)
- Forbidden: editing another system's GDD from this story; a change request goes to its author (coordination rule 5)
- Guardrail: none (no runtime content)

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-30 [Review/ADVISORY, at the cross-GDD review], scoped to this story:*

- [ ] (AC-CNB-30) GIVEN CNB-R16, WHEN SCL (AC-SCL-32), IIF (AC 50), LSC (AC-61), Concealment (AC-CON-15), Assassination, Objectives (AC-OC-24), IVP (AC-IVP-34) and Penalty & Informants are reviewed, THEN each named GDD contains an acceptance criterion that cites its row(s) (T1-T18) by number, and a list of (GDD, row, AC id) has no missing entry.
- [ ] T17 and T18 are recorded in the list as no-deviation rows (camera toggle and ping), with the GDD sections that argue them.

---

## Implementation Notes

*Derived from GDD AC-CNB-30 and CNB-R16:*

- Method: Grep each named GDD for its AC ids and the row numbers (`T1` ... `T18`); record `(GDD, row, AC id)` triples in one table. A missing triple is reported as a change request to that GDD's author, never fixed here.
- Output: `production/qa/smoke-<date>-crowd-tell-matrix-rows.md` containing the table and the missing-entry list (empty when complete).
- The GDDs are large (90-180 KB): search by AC id and read only the matching section ranges.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 023: runtime calibration of the rates the rows describe
- Editing any other GDD (change requests only)

---

## Files

- `production/qa/smoke-<date>-crowd-tell-matrix-rows.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). Advisory check; no automated test.*

- **AC-1**: `TellMatrixRows`
  - Setup: the named GDDs and acceptance criteria (SCL AC-SCL-32, IIF AC 50, LSC AC-61, Concealment AC-CON-15, Assassination, Objectives AC-OC-24, IVP AC-IVP-34, Penalty & Informants)
  - Verify: search each for its Tell Matrix row number citation
  - Pass condition: a (GDD, row, AC id) list with no missing entry
- **AC-2**: no-deviation rows
  - Setup: the Tell Matrix rows
  - Verify: read T17 and T18
  - Pass condition: both are recorded as no-deviation rows

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass in `production/qa/smoke-<date>-crowd-tell-matrix-rows.md` (advisory)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None
- Unlocks: None
