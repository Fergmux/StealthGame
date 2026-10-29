# Story 030: The concealment log rows go to the Event Log only and feed the F1, F2 and review-metric calculators

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R15, CON-R12 review metric, F1, F2)
**Requirement**: `TR-con-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§10 concealment log)
**ADR Decision Summary**: Server-only rows `{Tick, BushId, PawnKind, Event ∈ {Commit, ExitCross, Emerge, Killed, BushStrike}}` (with candidate-set sizes before and after the reach filter on `BushStrike`, and a kill flag on `Emerge`) are appended to the Round Event Log slice (ADR-0002/ADR-0007 Event Log store), never through `UE_LOG`/`UE_LOGF`, never released to clients in MVP. `PawnKind` is read only here, for metrics.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; plain C++ over the Event Log store)
**Engine Notes**: `UE_LOG` is still supported in 5.8 but Epic asks for `UE_LOGF`; concealment rows use neither (Event Log store only). Include-lint rule (i) bans naming an Event Log type or a server-secret identifier in a log/trace macro argument.

**Control Manifest Rules (this layer)**:
- Required: Concealment log rows go to the Event Log store only — source: ADR-0017
- Forbidden: Never log concealment rows through `UE_LOG`/`UE_LOGF`; never release rows to clients — source: ADR-0017, ADR-0007
- Guardrail: Negligible server CPU inside the 1.0 ms misc line

The log is the source of F2's measured `r_P`, of dwell and time-concealed metrics, of the CON-R12 multi-occupant metric and of the ambush metric (kills by a pawn in `Emerging` as a share of all kills, reviewed by the creative director above 25 %).

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (CON-R15, F1, F2, CON-R12), scoped to this story:*

- [ ] **AC1** (derived, CON-R15): **GIVEN** Commit, ExitCross, Emerge, Killed and BushStrike events **THEN** rows `{Tick, BushId, PawnKind, Event}` are appended to the ServerOnly Event Log slice; BushStrike rows carry the candidate-set size before and after the reach filter (0 is valid), Emerge rows whether the strike killed; no `UE_LOG`/`UE_LOGF` is used (lint rule (i)); nothing is released to clients.
- [ ] **AC2** (derived, F1/F2, CON-R12, [U] `StealthGame.Concealment.MetricCalculators`): **GIVEN** a checked-in fixture (N_B 9, n_P 5, n_N 50, r_N 0.20/min, D_N 0.345 min, r_P 0.24/min, D_P 0.345 min, k 1.5) **THEN** E[O] = 0.429, m0 = 1 − e^−λ = 0.349, the review cap max(40 %, m0 + 10 points) = 44.9 %, and r_N,req = 0.152/min (0.633 × r_P), each within ±0.001 (cap ±0.1 point). **GIVEN** a checked-in log fixture of one 720 s round for a player with three concealments of 20 s, 25 s and 12.6 s and 50 bush strikes with candidates of which 22 held ≥ 2 **THEN** mean time concealed = 57.6 s (8.0 % of round time), r_P = 3 / 12 min = 0.25/min and the multi-occupant share m = 44.0 %, each within ±0.001 (shares ±0.1 point). Boundary fixtures: 29 and 30 bush strikes with candidates (m inconclusive below 30) and ambush shares of 25.0 % and 25.1 % (flag above 25 %).

---

## Implementation Notes

*Derived from ADR-0017 Decision §10 and GDD F1/F2/CON-R12:*

F1: `E[O] = (n_N·r_N·D_N + n_P·r_P·D_P) / N_B` (NPC term 0.383, opponent term 0.046 at the fixture), `m0 = 1 − e^(−λ)`. F2: `r_N,req = r_P·n_P·(1 − k·b) / (k·b·n_N)`, `b = n_P / (n_P + n_N)` (= 0.152 at the fixture). The CON-R12 review cap is `max(40 %, m0 + 10 points)` with fewer than 30 strikes inconclusive. `AssassinationRangeCm` is not an input of any output here. Rows are appended by the Commit, Cross, Emerge, kill and strike resolvers (stories 004, 005, 014, 017 and Assassination's `GetSpotOccupants` consumer for `BushStrike`). The review of the metrics against logged matches is story 031.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-031-playtest-metrics-review.md`: evaluating logged matches.
- Round epic: the Event Log store and flush barrier.
- Crowd epic: the NPC bush rate that feeds F2's pass condition.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentLog.h`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentLog.cpp`
- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentMetrics.h`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentMetrics.cpp`
- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentCore.h`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentCore.cpp`
- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentService.h`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentService.cpp`
- `Source/StealthGameplay/Features/Assassination/Public/Server/BushSpotQuery.h`
- `Source/StealthGameplay/Features/Assassination/Public/Server/VictimResolverCore.h`
- `Source/StealthGameplay/Features/Assassination/Private/VictimResolverCore.cpp`
- `Source/StealthGameplay/Features/Assassination/Private/AttackResolverCore.cpp`
- `Source/StealthGameplay/Private/Tests/Assassination/EncounterTestRig.h`
- `Source/StealthGameplay/Private/Tests/Concealment/ConcealmentLogTest.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/MetricCalculatorsTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS on AC2 of the first draft; the qa-lead's proposed rewrite is applied unchanged with the fixture values supplied here, not re-gated; specs derived from it).*

- **AC1**: LogRows
  - Given: Commit, ExitCross, Emerge, Killed and BushStrike events.
  - When: the rows are appended.
  - Then: rows `{Tick, BushId, PawnKind, Event}` go to the ServerOnly Event Log slice only; BushStrike rows carry the candidate-set size before and after the reach filter; Emerge rows carry whether the strike killed; no `UE_LOG`/`UE_LOGF` (lint rule (i)); nothing is released to clients.
  - Edge cases: a candidate-set size of 0.
- **AC2**: MetricCalculators
  - Given: the F1/F2 fixture and the log fixture above.
  - When: each calculator runs.
  - Then: E[O] 0.429, m0 0.349, cap 44.9 %, r_N,req 0.152/min; mean time concealed 57.6 s (8.0 %), r_P 0.25/min, m 44.0 %, within the stated tolerances.
  - Edge cases: 29 strikes with candidates gives m inconclusive and 30 gives a value; ambush share 25.0 % does not flag and 25.1 % flags; zero strikes does not divide by zero.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `ConcealmentLogTest.cpp` and `MetricCalculatorsTest.cpp` under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-004-hide-enter-execution-and-commit.md`, `story-014-attack-breaks-concealment.md`; round-match-framework epic Event Log stories
- Unlocks: `story-031-playtest-metrics-review.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1, AC2 passing: StealthGame.Concealment.ConcealmentLog (Commit, ExitCross, Emerge with the kill flag, Killed and BushStrike rows with candidates before / after reach, from Assassination's resolver over IBushSpotQuery::OnBushStrike / OnStrikeKilled; NeverReleased; no UE_LOG / UE_LOGF, scan with a seeded control); StealthGame.Concealment.MetricCalculators (F1 0.429, m0 0.349, cap 44.9 %, F2 0.152 / 0.633 r_P; the 720 s round fixture 57.6 s / 8.0 %, r_P 0.25, m 44.0 %; boundaries 29 / 30 strikes and 25.0 / 25.1 %).
**Deviations**: ADVISORY: m counts strikes whose candidate set AFTER the reach filter (the uniform pick's set) is non-empty and holds >= 2 (CON-R12 reading); the review thresholds (40 %, 10 points, 30 strikes, 25 %) are named GDD constants, not tuning keys.
**Test Evidence**: Source/StealthGameplay/Private/Tests/Concealment/ConcealmentLogTest.cpp, Source/StealthGameplay/Private/Tests/Concealment/MetricCalculatorsTest.cpp
**Gate**: Build Succeeded; full ^StealthGame. suite 594 tests, 593 passed first run (ExitUncullBeforeLeafPlane snap check narrowed to late actors, re-run alone at full counts PASS)
Run result: N/A — server-only Event Log rows and offline calculators: nothing is drawn or heard.
**Reviewed by**: self code review 2026-10-29 (notes in production/qa/evidence/concealment-027-034-2026-10-29/evidence.md)
