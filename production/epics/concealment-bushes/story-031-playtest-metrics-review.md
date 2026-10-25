# Story 031: Logged internal matches satisfy the parity, hiding-is-a-pause and ambush metrics

> **Epic**: Concealment (Bushes)
> **Status**: Blocked — BLOCKED: needs at least 10 logged internal playtest matches and the crowd-npc-behaviour and objectives-chain epics built (no spike is involved; the data does not exist yet)
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R12 review metric, CON-R14, F2, AC-CON-21, AC-CON-24)
**Requirement**: `TR-con-010`, `TR-con-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§10 log, §11 switch and parity)
**ADR Decision Summary**: The concealment log feeds the measured entry rates, dwell, time concealed, multi-occupant share and ambush share; there is no automatic fallback if Crowd misses IVP F5 (the case escalates to the creative director and the user), and `bConcealServerCull = false` is never an accepted pass. Failures of the hiding-is-a-pause metrics are fixed by more objective pressure (Objectives Chain), never by a timer or a reveal.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; this story is analysis of logged data)
**Engine Notes**: No engine API. IVP F5 (the hide-posterior check) is defined in the Information Visibility Policy GDD, not in this GDD; cite it in the report.

**Control Manifest Rules (this layer)**:
- Required: Metrics come from the Event Log store only — source: ADR-0017
- Forbidden: Never ship `bConcealServerCull = false` outside internal development builds; never fix a metric failure with a dwell timer or reveal — source: ADR-0017, GDD P4
- Guardrail: Fewer than 30 bush strikes with candidates is inconclusive, not a pass

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-21, AC-CON-24), scoped to this story:*

- [ ] **AC1** (AC-CON-21, [Stat][Dep:Crowd] `StealthGame.Concealment.EntryLogFeedsF5`): **GIVEN** ≥ 10 logged internal matches **THEN** the report shows per-kind entry rates and mean dwell, and IVP F5 evaluated at the measured n_P and n_N gives `P_hide ≤ k·b` (k = 1.5) at the 95 % upper confidence bound; on failure no build leaves the team and the case escalates (IVP R7.6); `bConcealServerCull = false` is never an accepted pass.
- [ ] **AC2** (AC-CON-24, [Stat][Dep:Objectives] `StealthGame.Concealment.HidingIsAPause`): **GIVEN** ≥ 10 internal playtest matches **THEN** (a) mean time concealed ≤ 8 % of round time per player and r_P ≤ 0.24/min; (b) no round is won by a team whose surviving players were all concealed for the final 60 s; (c) the multi-occupant share m ≤ max(40 %, m0 + 10 points) at each match's measured λ (45 % at defaults; inconclusive below 30 bush strikes with candidates); (d) kills from Emerging ≤ 25 % of all kills. A failure of (a) or (b) is fixed by more objective pressure (Objectives Chain), never by a timer or a reveal; (c) and (d) go to the creative director.

---

## Implementation Notes

*Derived from the GDD F2, CON-R12 and CON-R15:*

Run the calculators of story 030 over the logged matches and write the report to `production/qa/evidence/con-playtest-metrics-<date>.md` with the sample size, the per-match λ, and each metric against its threshold. F2: `r_N,req` is evaluated at each build's measured `TeamSize` and `NpcCount`, never the fixed 0.633. The design target r_P ≤ 0.24/min is a target in its own right, separate from the time-concealed cap (many short hides could stay under 8 % while pushing r_P near 0.8/min).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-030-concealment-log-and-metric-calculators.md`: the calculators.
- Crowd epic: NPC bush rate and dwell tuning (Crowd `story-022-bush-herring.md`).
- Objectives Chain epic: the pressure that fixes (a) and (b) failures.

---

## Files

- `production/qa/evidence/con-playtest-metrics-<date>.md` (the report)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; deferred to when the data exists).*

- **AC1**: EntryLogFeedsF5
  - Given: at least 10 logged internal matches.
  - When: the report is built.
  - Then: it shows entry rates and mean dwell per pawn kind, and P_hide is at most k × b (k = 1.5) at the 95 % upper bound.
  - Edge cases: `bConcealServerCull=false` is never an accepted pass.
- **AC2**: HidingIsAPause
  - Given: at least 10 playtest matches.
  - When: the report is built.
  - Then: (a) mean time concealed is at most 8 % per player and r_P at most 0.24/min; (b) no round is won by a team that was all concealed for the final 60 s; (c) m is at most max(40 %, m0 + 10 points), inconclusive below 30 strikes; (d) Emerging kills are at most 25 %.
  - Edge cases: the (a) and (b) fix route is "more objective pressure", never a timer.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a documented playtest report `production/qa/evidence/con-playtest-metrics-<date>.md` covering ≥ 10 matches

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-030-concealment-log-and-metric-calculators.md`; crowd-npc-behaviour epic (Crowd `production/epics/crowd-npc-behaviour/story-022-bush-herring.md`); objectives-chain epic (stories not yet written)
- Unlocks: None
