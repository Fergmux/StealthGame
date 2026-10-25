# Story 045: Graybox fairness playtest (per-zone win interval)

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md` (AC-55, Open Question 10, D8)
**Requirement**: `TR-lsc-009`
*(At write time: TR-lsc-009 = Server-only query API for site assignment and team spawns)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Zone -> team is random per round, so fairness is judged per zone (each zone is used in about half the rounds), not per team; the secret assignment is drawn uniformly (story 022) and the listen-server host can read every secret (ADR-0014 Risk R-8, accepted for the prototype).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Playtest evidence, ADVISORY. **GDD defect flagged (QL-STORY-READY):** with about 90 appearances per zone at 180 rounds a perfectly fair zone passes the 0.35-0.65 Wilson test only about 66% of the time (about 20% for all four zones together); hence the two-tier sample rule below.

**Why this story**: It is the only check that the whole system (layout, selection, placement) produces a fair game in practice. Read first: AC-55, Open Question 10, D8; `design/gdd/systems-index.md` MVP playtest notes.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Selection is a uniform random draw (ADR-0014 5.5)
- Forbidden: Never use per-team results as the fairness measure (zone -> team is random) (GDD AC-55)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-55 (revised after QL-STORY-READY: sample size and a two-tier interval), scoped to this story:*

- [ ] GIVEN graybox playtest rounds (each zone used in about half of them), THEN with at least 360 rounds (about 180 appearances per zone) the 95% Wilson interval of `ObjectiveWin` per zone lies within 0.35-0.65; with 180 to 359 rounds it lies within 0.30-0.70 (a fair zone then passes about 95% of the time) (`Fairness.ZoneWinInterval`, proposed; ADVISORY)
- [ ] THEN the MVP playtest plan notes that the listen-server host can read every secret (Open Question 10)

---

## Implementation Notes

- A round counts toward a zone's tally when that zone is used; each zone needs at least 45 appearances for any interval claim. Record the telemetry through the playtest hooks (ADR-0023); win attribution is by zone, not team.
- The Wilson interval is computed offline from the telemetry export; the two-tier rule (360 rounds at 0.35-0.65; 180-359 rounds at 0.30-0.70) is stated explicitly in the report.
- Playtests need the playable loop (Round, Objectives, Crowd, Concealment, Penalty): schedule this after those epics deliver a playable graybox round; until then it cannot run, which is a sequencing dependency, not a design gap.
- Add the host-visibility note to the MVP playtest plan (`production/` playtest plan or the playtest-report template), per Open Question 10.

---

## Out of Scope

- Story 038: uniform-fairness batch audits; the Generator's fairness (Generator epic)

---

## Files

- `production/qa/evidence/level-socket/fairness-playtest-report.md`
- `tools/analysis/zone_win_wilson.py` (offline interval calculation)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision; ADVISORY).*

- **AC-1** `Fairness.ZoneWinInterval` (proposed)
  - Given: graybox playtest rounds, each zone used in about half.
  - When: `ObjectiveWin` per zone is tallied with a 95% Wilson interval.
  - Then: with >= 360 rounds the interval lies within 0.35-0.65; with 180 <= n < 360 rounds within 0.30-0.70.
  - Edge cases: state the two-tier rule explicitly; each zone needs >= 45 appearances.
- **AC-2** playtest plan note (manual check)
  - Setup: open the MVP playtest plan.
  - Verify: it notes the listen-server host can read every secret (Open Question 10).
  - Pass condition: note present.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration (ADVISORY): `production/qa/evidence/level-socket/fairness-playtest-report.md` with the per-zone intervals; or a playtest document (`/playtest-report`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-029-start-placement-same-function-zone-fill.md`, `production/epics/level-socket-contract/story-022-selection-secret-draw-streams.md`
- Cross-epic: `production/epics/round-match-framework/EPIC.md`, `production/epics/objectives-chain/EPIC.md`, `production/epics/crowd-npc-behaviour/EPIC.md` (playable loop required)
- Unlocks: None
