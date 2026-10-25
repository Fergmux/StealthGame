# Story 052: Crowd's Tell Matrix has an SCL-owned row for every SCL tell

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R7.3 pillar constraint: every tell SCL renders has a non-zero NPC baseline rate; AC-SCL-32); Crowd GDD CNB-R16 Baseline and Tell Matrix
**Requirement**: `TR-scl-004` (state restrictions; tells owned by SCL)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (Pillar 1: every observable difference needs an NPC baseline), with ADR-0016 (Crowd owns the matrix)
**ADR Decision Summary**: A tell no NPC ever performs is a confession, not evidence: every SCL-rendered tell (Run, Traversal, sudden stop, abrupt reversal, head hold) must appear in Crowd's Tell Matrix with SCL as an owner and a stated NPC baseline, so the deviation is evidence, never proof.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (document lint; no engine API)
**Engine Notes**: None. The lint reads a Markdown file under `design/`, which is file I/O: implement it as a CI lint script or an editor-only test, not a unit test of game code.

**Control Manifest Rules (this layer)**:
- Required: each feature GDD adds an acceptance criterion pointing at its Tell Matrix row.
- Forbidden: none specific.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-32** [U] `TellMatrixRows` (doc lint): GIVEN `design/gdd/crowd-npc-behaviour.md`, WHEN the lint parses its Baseline and Tell Matrix, THEN rows exist for Run, Traversal, sudden stop, abrupt reversal and head hold (5 of 5), each Owner cell reads "Shared Character & Locomotion", and each NPC-baseline cell is non-empty and not TBD.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the criterion's wording is stale against the current document, where CNB-R16 is one table (`#`, Advantage, Visible deviation, NPCs do it?, Target P(player | seen), Owner) and Owner cells read "SCL + CNB" or "SCL". The lint targets CNB-R16 by name, matches rows T1, T2, T11, T12 and T13, requires the Owner cell to contain the whole token "SCL", and requires the NPC cell to be non-empty and not TBD, with "n/a" accepted only for T11 (sudden stop is impossible by design). The GDD owner should update AC-SCL-32's wording through `/propagate-design-change`.

---

## Implementation Notes

*Derived from the GDD and CNB-R16:*

- Write the lint as a small script (Python, run with `-I`, or an editor-only automation test) that finds the CNB-R16 table, parses it as pipe rows and asserts the five rows by id. Add a negative control: a fixture copy with one Owner cell blanked must fail.
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The matrix content and rates (Crowd epic). Story 053: observed NPC tell counts.

---

## Files

- `tools/ci/lint_tell_matrix.py` (planned; or an editor-only automation test)
- `Source/StealthGameEditor/Private/Tests/TellMatrixRowsSpec.cpp` (alternative, planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.TellMatrixRows`.*

Test: Tell Matrix rows (AC-SCL-32, doc lint)
  Given: `design/gdd/crowd-npc-behaviour.md` and a fixture copy with one Owner cell blanked
  When: the lint parses CNB-R16
  Then: rows T1, T2, T11, T12 and T13 (Run, Traversal, sudden stop, abrupt reversal, head hold) exist; each Owner cell contains the whole token "SCL"; each NPC cell is non-empty and not TBD, with "n/a" accepted only for T11; the blanked-Owner fixture fails the lint
  Edge cases: the current CNB-R16 table passes this as written; the AC wording itself is stale (flagged to the GDD owner)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the `StealthGame.Locomotion.TellMatrixRows` lint (CI script or editor-only automation test) — must exist and pass, with a recorded failure on the blanked-Owner fixture

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None within this epic; `design/gdd/crowd-npc-behaviour.md` CNB-R16 (exists)
- Unlocks: None
