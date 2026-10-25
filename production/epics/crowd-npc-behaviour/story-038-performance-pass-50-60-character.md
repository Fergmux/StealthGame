# Story 038: Performance pass: 50/60-character server cost on the full catalogue (exit criterion)

> **Epic**: Crowd & NPC Behaviour
> **Status**: Blocked — BLOCKED: the 60-character spike (story 032, ADR-0016 conditional gate AC-CNB-26) has not reported; the pass is this epic's exit criterion and is meaningful only after the spike has set the baseline and decided which cost-cut steps (story 033) apply
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-006 = <= 1.5 ms AI + nav; global-only cuts; never changes replication.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0022 (budgets, evidence standard), ADR-0013 (animation share of the frame), ADR-0008 (harness)
**ADR Decision Summary**: The six `Crowd.*` scopes sum to at most 1.5 ms game-thread time at p99 and 0.8 ms mean on the listen host at 60 pawns and 60 fps (AC-CNB-26), provisional until the 60-character spike reports; the fixed global cut order is the fallback; no cut ever lowers `NpcCount` or changes replication.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Same evidence standard as story 032: cooked Development standalone, 3060-class reference host, host process only, bots in separate processes; PIE, editor or RTX 5090 numbers are not evidence; no reference host means NOT ASSESSED. Use `/perf-profile` for the ranked optimisation list.

**Why this story**: The epic owns the cross-cutting 50-character performance pass (about 6 days bottom-up in the estimate, `production/estimates/2026-10-25_mvp-rebaseline.md`) as an exit criterion. Story 032 measures an early thin slice; this one repeats the measurement with every behaviour implemented (catalogue, red herrings, reactions, schedule, bound drivers), optimises what the capture shows, applies any cut-order step the spike called for, and records the ADR-0016 gate as reported. It is Blocked until story 032 reports because the baseline and the set of steps to apply come from that report. Read GDD F5, OQ10 and AC-CNB-26.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: performance evidence from cooked Development standalone captures on 3060-class reference hardware; cost cuts are global values from the fixed cut order; Crowd budget <= 1.5 ms p99 / <= 0.8 ms mean at 60 pawns, 60 fps (ADR-0016, ADR-0022)
- Forbidden: cuts that depend on distance, visibility, viewer or kind; changing replication settings; lowering `NpcCount`; editing the control manifest or ADR-0016 from this story (change requests only) (ADR-0016)
- Guardrail: the epic's Definition of Done requires every governing ADR's conditional gate to have reported before dependent work is marked Done

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-26 and the epic's Build Order and Gates, scoped to this story:*

- [ ] (AC-CNB-26 at exit) With the full catalogue implemented, the 60-pawn reference-scene capture on the reference host (cooked Development standalone, 60 fps locked, a strike burst every 20 s, 5 min) shows the six `Crowd.*` scopes summing to at most 1.5 ms at p99 and at most 0.8 ms on average; the capture is retained in `production/qa/evidence/`.
- [ ] A 50-NPC run is captured as well; a ranked optimisation list from `/perf-profile` is recorded; any cut-order step applied is recorded with its story 033 / story 034 parity result.
- [ ] The 70-pawn / 30 fps run is reported (advisory) with the cut order applied; PIE, editor or RTX 5090 numbers are never accepted as evidence, and no reference host means NOT ASSESSED.
- [ ] The ADR-0016 conditional-gate status is recorded as reported (or a superseding ADR requested on failure), and any change to the provisional 1.5 ms rule is raised as a control-manifest change request, not edited here.

---

## Implementation Notes

*Derived from ADR-0016 section 10 and Risks:*

- Compare with the story 032 baseline: the delta is the cost of the catalogue and red herrings. Prioritise by scope; the F5 terms to check first are steering (`c_f`), perception rays and decision ticks.
- Likely levers before the cut order: pre-baked edge paths for more legs, batching hash rebuilds, trimming StateTree task allocations, shrinking the due-time list scans. None may alter behaviour statistics; re-run stories 009, 014 and 023 gates after any optimisation that touches them.
- If the 1.5 ms rule cannot be met even with the cut order, escalate to the technical-director and creative-director before any `NpcCount` change (OQ10); do not lower `NpcCount` or alter replication locally.
- Record the CPU model, clock, build configuration and engine changelist with the capture.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Reference only. The prototype's measured 50 characters at 89.6 FPS, 11.2 ms game thread (RTX 5090) is the historical number to compare against, not evidence.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 032: the early spike
- Story 033: building the cut steps
- ADR-0022 GPU, draw-call and animation budgets (a separate performance pass)
- Replication bandwidth (Crowd replicates nothing of its own)

---

## Files

- `production/qa/evidence/<date>_crowd-exit-cost-capture.md` (capture summary and Insights references)
- `production/qa/spikes/<date>_crowd-exit-cost-report.md` (ranked optimisation list and gate record)
- source files touched by optimisation (listed by `/dev-story`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). Evidence-gathering; runs on the reference host after story 032 reports.*

- **AC-1**: `StealthGame.Crowd.MP.ServerCost` (exit)
  - Given: the full catalogue, a 60-pawn reference scene, cooked Development standalone, 60 fps locked, a strike burst every 20 s, on the reference host
  - When: Insights captures the six `Crowd.*` scopes
  - Then: they sum to at most 1.5 ms p99 and 0.8 ms mean; the capture is retained
  - Edge cases: no PIE, editor or RTX 5090 numbers
- **AC-2**: optimisation record
  - Given: a 50-NPC capture and the `/perf-profile` output
  - When: the report is assembled
  - Then: the ranked optimisation list is recorded; any cut-order step applied is recorded with its story 033 / story 034 parity result
  - Edge cases: no step applied
- **AC-3**: seventy-pawn run
  - Given: the 70-pawn / 30 fps run
  - When: captured with the cut order applied
  - Then: reported as advisory; with no reference host the result is NOT ASSESSED
  - Edge cases: a non-reference host's numbers are rejected
- **AC-4**: gate record
  - Given: the capture result
  - When: the ADR-0016 record is updated
  - Then: the conditional-gate status is recorded as reported, or a superseding ADR is requested on failure; any change to the 1.5 ms rule is raised as a control-manifest change request, not edited here
  - Edge cases: borderline pass

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: capture summary in `production/qa/evidence/<date>_crowd-exit-cost-capture.md` plus the report in `production/qa/spikes/`; `/story-done` checks that they exist

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-032-spike-60-character-server-cost.md`, `production/epics/crowd-npc-behaviour/story-033-cost-cut-order.md`, `production/epics/crowd-npc-behaviour/story-023-tell-calibration-gate.md`, `production/epics/crowd-npc-behaviour/story-026-ambient-schedule-and-emergence.md`, `production/epics/crowd-npc-behaviour/story-027-kill-vacancy-and-replacement.md`, `production/epics/crowd-npc-behaviour/story-031-halt-and-handover-epoch.md`
- Unlocks: None
