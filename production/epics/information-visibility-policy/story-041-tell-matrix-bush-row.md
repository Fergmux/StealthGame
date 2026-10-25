# Story 041: Crowd's Tell Matrix carries the bush enter/leave red-herring row tied to F5 at k = 1.5

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The concealment cull only hides players if NPCs also visibly enter and leave bushes, so Crowd's Baseline and Tell Matrix must carry a bush enter/leave row marked as a red herring, with the NPC dwell and exit distributions referenced to IVP F5 at k = 1.5. This is a document review gate, not a code change: the row already exists as T5 in `design/gdd/crowd-npc-behaviour.md`, and this story verifies that its text matches the criterion and records the review. The dwell and exit distributions are recorded as advisory (not a transport test, per the F5 note). The review keeps the Crowd and Visibility Policy documents consistent before the statistical gate (story-040) relies on them.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test automation and leak parity (sections 1 and 9: pre-registered priors and thresholds beside the analysis tools)
**ADR Decision Summary**: Statistical gates pre-register their priors and thresholds before data is collected; the F5 hide-event posterior (IVP F5, AC-IVP-25) is a milestone and nightly gate whose inputs (the NPC bush rate at k = 1.5 and the recorded NPC stay and exit distributions) must be written down in the design documents first.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Not applicable: a design-document review with no engine API involved.

**Control Manifest Rules (this layer)**:
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Forbidden: **Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail** — source: ADR-0008
- Guardrail: **Tests**: per-commit pipeline ≤ 10 min; nightly cook + T3 estimated 1-2 h [U] — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-34), scoped to this story:*

- [ ] **AC-IVP-34** [Doc][Dep:Crowd] `StealthGame.Visibility.TellMatrixBushRow`: GIVEN Crowd's Baseline and Tell Matrix, THEN it has the row "bush enter/leave: red herring yes" with NPC dwell and exit distributions referenced to F5 at k = 1.5 (Crowd T5 cites k = 1.5 and r_N 0.19/min since its 2026-10-24 review; dwell and exit distributions are recorded, advisory, not a transport test, F5 note).

---

## Implementation Notes

*Derived from ADR-0008 sections 1 and 9 and the Crowd GDD Baseline and Tell Matrix:*

- This story changes no code. It reads `design/gdd/crowd-npc-behaviour.md`, "Baseline and Tell Matrix", row T5 ("Bush enter / leave"). At the time of writing the row reads: red herring "Yes (red herring)", shade-seeking at 0.20/min (300 s, D7), occupancy-blind; NPC stay uncapped log-normal (median 15 s, sigma 0.8), exits by the same side rule; budget cell "at most 3/22, about 0.136 (IVP F5 at k = 1.5, 50 NPCs; 0.136 at r_P 0.3)"; owners "Concealment + IVP + CNB (AC-CON-15, AC-IVP-34)".
- Verify three items and record each in the review note: (1) the row text conveys "bush enter/leave: red herring yes" (the Crowd wording is "Bush enter / leave ... Yes (red herring)"; the review decides whether the difference is cosmetic and records the decision); (2) the NPC dwell (log-normal, median 15 s, sigma 0.8) and exit (same side rule) distributions are referenced to F5 at k = 1.5; (3) r_N is stated consistently: the criterion cites r_N 0.19/min, which is the required rate (`60 / CrowdBushIntervalSec >= r_N,req`, 0.19 at r_P 0.3 with 50 NPCs, so the interval must be at most 315 s), while the T5 row cites the configured 0.20/min (300 s, decision D7 of 2026-10-25) which satisfies that requirement. The review records that both figures are present and consistent.
- Do not edit `design/gdd/crowd-npc-behaviour.md` in this story. If the review finds a mismatch that is more than cosmetic, record it in the evidence note and raise a change request to the Crowd GDD owner (propagation is coordinated by the producer, coordination rule 4); the story then stays open until the row matches.
- The dwell and exit distributions are advisory and recorded only; they are not a transport test (F5 note), and this story does not run any statistics (story-040 does).
- Recorded review: write the review note to `production/qa/evidence/tell-matrix-bush-row-evidence.md` with the date, the reviewer, the verbatim row text, and the three item outcomes. Config/Data evidence also requires a smoke-check line: a line in the next `production/qa/smoke-*.md` stating the doc review passed.
- Prototype: Replaces: nothing. Promotes: nothing.
- Cross-epic: reads the crowd-npc-behaviour epic's design document (`design/gdd/crowd-npc-behaviour.md`); no build dependency.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-040: the hide-posterior parity statistical run (AC-IVP-25)
- story-039: the `HidePosteriorFactor` ceiling check
- story-047: the leak catalogue review
- crowd-npc-behaviour epic: authoring or changing the Crowd GDD and its NPC bush behaviour

---

## Files

*The files this story creates or changes (planned):*

- `production/qa/evidence/tell-matrix-bush-row-evidence.md` (the recorded review)
- a smoke-check line in `production/qa/smoke-*.md`

---

## QA Test Cases

*Config/Data doc review — manual verification steps:*

- **AC-1**: Tell Matrix bush row (AC-IVP-34)
  - Setup: open Crowd's Baseline and Tell Matrix (row T5).
  - Verify: the row text reads "bush enter/leave: red herring yes"; dwell and exit distributions are referenced to F5 at k = 1.5, with r_N 0.19/min.
  - Pass condition: all three items match; the review is recorded in `production/qa/evidence/`.

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass (`production/qa/smoke-*.md`) plus the recorded review under `production/qa/evidence/` (`tell-matrix-bush-row-evidence.md`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None
- Unlocks: None
