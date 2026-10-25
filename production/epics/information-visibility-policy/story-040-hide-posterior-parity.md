# Story 040: NPCs enter bushes at least 0.633 as often as players, through the same Concealment call site

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: [Dep:Crowd] needs crowd-npc-behaviour NPC bush use and concealment-bushes entry log; nightly crowd runs with fixed seeds.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The concealment cull removes a pawn from non-allies a quarter-second after it hides. If only players hid in bushes, the sudden disappearance would itself identify them. The hedge is that NPCs hide at a comparable rate (a red herring): the hide-event posterior must stay at or below 3/22 (F5, `P_hide <= k*b` at 50 NPCs, `HidePosteriorFactor` k at most 1.5), which requires the NPC to player entry-rate ratio `r_N / r_P` to be at least 0.633. This story is the statistical gate that measures that ratio from nightly crowd runs with fixed seeds against player bush-entry rates from the playtest Concealment entry log, and checks that NPCs use the same Concealment call site as players. Its run record feeds the build switch in story-031.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide); also `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test automation and leak parity (sections 1 T4, 8 gate wiring and 9, determinism and statistical seed sets); ADR-0003: Replication, relevancy and the concealment cull hybrid (section 5.9)
**ADR Decision Summary**: T4 statistical gates run offline analysis over T3 captures, using fixed, checked-in seed sets and pre-registered priors and thresholds checked in beside the tools before any data is collected; the F5 hide-event posterior from evidence playtests is a milestone and nightly gate. The cull applies identically to every pawn kind and there is no automatic fallback if Crowd misses F5 (IVP R7.6).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The crowd runs are T3 multi-process captures (cooked Development, Gauntlet) of Crowd's bush-entry events; the analysis is offline (T4). No post-cutoff API beyond ADR-0008's Gauntlet launch topology and `WITH_DEV_AUTOMATION_TESTS` availability in cooked Development builds [U, ADR-0008 Verification 3 and 7].

**Control Manifest Rules (this layer)**:
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Required: **Lints and analysis scripts emit observations only; pass/fail lives in the gate script** — source: ADR-0008
- Forbidden: **Never use wall-clock time as a test pass/fail predicate** (only as a failure ceiling); never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Forbidden: **Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail** — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-25), scoped to this story:*

- [ ] **AC-IVP-25** [Stat][Dep:Crowd] `StealthGame.Visibility.HidePosteriorParity`: GIVEN nightly crowd runs with fixed seeds and player bush-entry rates from the playtest Concealment entry log, WHEN rates are compared, THEN `r_N / r_P ≥ 0.633` (`P_hide ≤ 3/22`, k·b at 50 NPCs), and NPC entries use the same Concealment call site as player entries. (F5, R7.6) Clarification (story-level, QL-STORY-READY): before the first run a pre-registered file in `tools/ci/stats/` declares the number of seeded runs (default 20), the minimum entry counts (default >= 100 NPC and >= 30 player bush-entries from >= 10 matches, per F5) and that the pass applies to the point estimate with the bootstrap lower bound reported (ADR-0008 section 9).

---

## Implementation Notes

*Derived from ADR-0008 sections 1 (T4), 3, 8 and 9, ADR-0003 section 5.9, and IVP F5/R7.6:*

- Pre-registration first: commit the pre-registered file under `tools/ci/stats/` before the first run, declaring (a) the number of seeded runs (default 20), (b) the minimum entry counts (default at least 100 NPC and at least 30 player bush-entries from at least 10 matches, per F5) and (c) that the pass applies to the point estimate `r_N / r_P >= 0.633` with the bootstrap lower bound reported but not the pass predicate. It records the `HidePosteriorFactor` used (k at most 1.5, checked by story-039) and the seed set. The committed seed set is never reseeded; a failing seed is a defect.
- Data inputs: nightly crowd runs with fixed seeds produce per-run NPC bush-entry counts and durations from Concealment's entry log (the crowd-npc-behaviour epic's NPC bush use, tell-matrix row T5 at 0.20/min); the player rate `r_P` comes from the playtest Concealment entry log (concealment-bushes epic). Playtest data feeding F5 must pass story-048's host-record check before entering the computation.
- Below the minimum entry counts the result is inconclusive, not a pass (QA note; the author confirms with qa-lead). Record "inconclusive" distinctly from pass and fail in the run record.
- Same call site: assert that NPC entries and player entries go through the same Concealment call site (a single entry function, identified by the entry log's call-site tag), not a parallel NPC path; any NPC entry through a different site fails the criterion. The run record format and the `bConcealServerCull` flag key are defined here and consumed by story-031 and story-049; keep both stable.
- Negative control (ADR-0008 section 3): a harness-only synthetic run whose NPC rate is below the threshold (for example an NPC rate of zero) must fail the inner check; the suite passes only if the control is detected. Also run the story-039 negative control (a `HidePosteriorFactor` of 2.0 stops the run before any rate is computed).
- Analysis scripts emit observations only (rates, counts, bootstrap interval); pass or fail is decided in the gate script (ADR-0008). Wiring: nightly and release/milestone gates (ADR-0008 section 8); the latest passing run on the commit is what `bConcealServerCull = true` requires (story-031).
- Prototype: Replaces: nothing. Promotes: nothing. The prototype has no NPC bush use.
- Cross-epic: needs crowd-npc-behaviour's NPC bush use at the F2 rate through the one Hide path (AC-CNB-15), concealment-bushes' entry log and its single Concealment call site, and tuning-data-config Exit A (harness, T3 captures, T4 tool folder).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-039: the `HidePosteriorFactor` ceiling check
- story-031: the player-only-cull ship switch that consumes this run record
- story-041: the Crowd Baseline and Tell Matrix bush row review
- story-048: the playtest host record check that gates the playtest data
- story-049: the external-build critical gate
- crowd-npc-behaviour and concealment-bushes epics: NPC bush behaviour and the entry log

---

## Files

*The code and test files this story creates or changes (planned):*

- `tools/ci/stats/` pre-registration file (seeded run count, minimum entry counts, point-estimate rule, `HidePosteriorFactor` key) and the analysis script (observations only)
- `Source/StealthTestHarness/Private/Scenarios/HidePosteriorParityScenario.cpp` (T3 capture of NPC bush entries, `StealthGame.Visibility.MP.HidePosteriorParity`) and its Gauntlet node in `Build/Scripts/`
- the AC-IVP-25 run record (format and flag key read by story-031 and story-049) under `production/qa/`

*Note: the `StealthTestHarness` module folder is created by tuning-data-config Exit A (it does not exist yet).*

---

## QA Test Cases

- **AC-1**: Hide posterior parity (AC-IVP-25)
  - Given: a pre-registered file in `tools/ci/stats/` declaring the number of seeded runs (default 20), minimum entry counts (>= 100 NPC and >= 30 player bush-entries from >= 10 matches) and that the pass applies to the point estimate with the bootstrap lower bound reported; nightly crowd runs with fixed seeds and player rates from the playtest Concealment entry log.
  - When: rates are compared.
  - Then: `r_N / r_P >= 0.633` on the point estimate; NPC entries use the same Concealment call site as player entries.
  - Edge cases: below the minimum entry counts the result is inconclusive, not a pass (author to confirm); the pre-registration file is committed before the first run.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T3 scenario under `Source/StealthTestHarness/` with the T4 analysis run record OR a documented playtest or spike report (the committed pre-registration file and the run record under `production/qa/`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-039-hide-posterior-factor-ceiling.md`; crowd-npc-behaviour epic (NPC bush use through the one Hide path); concealment-bushes epic (entry log and the single Concealment call site); tuning-data-config Exit A (harness, T3 and T4 folders)
- Unlocks: `production/epics/information-visibility-policy/story-031-player-only-cull-never-ships.md`
