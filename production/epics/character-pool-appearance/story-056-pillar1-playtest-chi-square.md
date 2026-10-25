# Story 056: Pillar-1 playtest: participant counts per look fit the deck expectation

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-007 = Statistical indistinguishability gate G-DECK automated.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Pillar-1 evidence is gathered on remote clients (or a dedicated observer client) from logged rounds; the one CPA playtest metric is a chi-square of participant counts per look against the deck expectation `sum(P * c_a / D)`, excluding rounds flagged `DeckBelowFloor` or `HotReloaded` or with `PoolNotResident`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0008)
**Engine Notes**: Needs Round's Event Log (`DealDigest`, active set, per-participant looks released at `Ended` under the Visibility Policy). Not coded yet: this story is DEFERRED until the log exists (QL-STORY-READY); fewer than 50 eligible rounds is "not enough data", not a pass.

**Why this story**: AC-CP-25 is "the single Pillar-1 metric for this system"; the OQ-4 deck grid playtest is its sibling (three `(copies, max looks)` settings). Read GDD AC-CP-25, OQ-4, OQ-3 and Tuning Knobs "Interactions".

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Pillar-1 claims are made on remote clients only (host trust limit)
- Forbidden: Never include flagged rounds; never tune thresholds to pass
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-25 and OQ-4:*

- [ ] **AC-1** GIVEN at least 50 logged rounds with `Default` tuning played with a dedicated observer client or remote clients only (excluding `DeckBelowFloor`, `HotReloaded` and `PoolNotResident` rounds), THEN a chi-square (df = distinct looks - 1, alpha 0.001; about 31.3 critical at df 11) of participant counts per look against the expectation `sum(P * c_a / D)` does not reject; fewer than 50 eligible rounds is reported "not enough data".
- [ ] **AC-2** GIVEN the OQ-4 deck grid `(DeckCopiesFloorCount, DeckActiveAppearancesMaxCount)` = (3, 16), (4, 12), (6, 8), THEN track-loss after 5 s occlusion, same-look characters per frame, unprompted "clone" comments, target-by-look choices and the AC-CP-25 share metric are recorded per setting (record only).

---

## Implementation Notes

- The analysis script reads the Round Event Log export; keep it under `tools/analysis/` and unit-test the chi-square on a synthetic log.
- Playtest procedure and observer client per `production/qa/` conventions; OQ-3 (G-VAR thresholds) is calibrated on the same sessions (story 049).

**Prototype disposition**: none.

---

## Out of Scope

- The Round Event Log itself (round-match-framework epic); story 014 (the headless statistical gate)

---

## Files

- `tools/analysis/pillar1_chi_square.py`
- `production/qa/evidence/pillar1-playtest-<date>.md` and the log export

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; DEFERRED until Round's Event Log exists).*

**Test: Pillar-1 chi-square**
- Given: at least 50 logged rounds with `Default` tuning, observer or remote clients only, excluding flagged rounds.
- When: participant counts per look are compared with `sum(P * c_a / D)`.
- Then: chi-square (df = distinct looks - 1, alpha 0.001) does not reject.
- Edge cases: fewer than 50 eligible rounds is "not enough data", not a pass.

**Test: deck grid playtest record**
- Given: the grids (3,16), (4,12) and (6,8).
- When: played.
- Then: the five observations are recorded per setting (record only).
- Edge cases: none.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: documented playtest `production/qa/evidence/pillar1-playtest-<date>.md` with the analysis output (documented playtest)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-049-g-var-paired-trials.md`, `production/epics/character-pool-appearance/story-035-no-asset-load-on-reveal.md`, `production/epics/round-match-framework/EPIC.md` (Event Log story to be created)
- Unlocks: None
