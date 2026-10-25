# Story 032: Performance budget

> **Epic**: Teammate Markers
> **Status**: Blocked — BLOCKED: ADR-0022 conditional gates R1, R2 and R3 (rendering budget on RTX 3060-class hardware at 1080p) have not reported, and the manifest accepts only cooked Development standalone captures on the reference preset as budget evidence; the painted path (story 019) is also required.
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The marker module must stay cheap: with four allies on screen, labels and edge markers, at most 0.05 ms of game-thread time per frame (p95) on the reference PC, zero per-frame heap allocations, and at most two draw batches; the HUD as a whole is held to 0.5 ms CPU with markers. The gate is ADVISORY (Integration, AC-TMK-27) and the figures only count when taken from a cooked Development standalone capture on the reference preset, never from PIE or the editor. The module also declares its `Stealth.Budget.<Line>` Insights scope as every owner ADR's code must, using the line name ADR-0022 assigns it.

**GDD**: `design/gdd/teammate-markers.md` (Formulas performance budget, AC-TMK-27)
**Requirement**: `TR-tmk-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (Performance Implications)
**Also governed by**: ADR-0022 (rendering performance budgets; `Stealth.Budget.<Line>` scopes; Accepted, conditional on R1-R3)
**ADR Decision Summary**: HUD at most 0.3 ms CPU, at most 0.5 ms with markers; UI GPU at most 0.5 ms at 1080p on the reference GPU; performance claims come from cooked Development standalone captures.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Use Unreal Insights with the declared scope and the engine's allocation tracking (LLM or `-trace=memory`) to prove zero per-frame allocations; draw batches from the Slate stats (`stat slate`) or RenderDoc on the reference scene. PIE and editor numbers are not budget evidence.

**Control Manifest Rules (this layer)**:
- Required: Each owner ADR's code declares its `Stealth.Budget.<Line>` Insights scope — source: ADR-0022
- Forbidden: Never cite PIE or editor numbers as budget evidence — source: ADR-0022
- Guardrail: HUD at most 0.5 ms CPU with markers; markers module at most 0.05 ms game thread per frame (p95), zero per-frame heap allocation, at most 2 draw batches

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-27) and the control manifest guardrail, scoped to this story:*

- [ ] **AC1** (AC-TMK-27, [PIE-2] Integration, ADVISORY `PerfBudget`): **GIVEN** four allies on screen with labels and edge markers on the reference PC **WHEN** profiled for 60 s **THEN** the marker module's game-thread cost is at most 0.05 ms per frame (p95), with zero per-frame heap allocations and at most 2 draw batches.
- [ ] **AC2** (manifest guardrail, derived): **GIVEN** a cooked Development standalone capture (never PIE) **THEN** HUD CPU with markers is at most 0.5 ms, and the module declares its `Stealth.Budget.<Line>` Insights scope with the line name ADR-0022 assigns (a placeholder `<Line>` is replaced by that name in the capture notes).

---

## Implementation Notes

*Derived from AC-TMK-27 and ADR-0022:*

Capture scene: four allies on screen, one of them focused with a label, one or two off-screen to exercise the edge clamp and spread, 60 s on the reference PC (RTX 3060-class, 1080p, reference preset). Report the module's p95 game-thread time from the declared Insights scope, allocation counts per frame (must be 0 after warm-up) and draw batches. Keep per-frame code allocation-free: fixed-size arrays for at most four markers, no `TArray` growth, no `FString` building per frame (labels re-laid-out only when the name or the scale changes).

The line name `Stealth.Budget.<Line>` is assigned in ADR-0022's CPU budget table; look it up when the story starts and record it in the story's Files.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- the hud epic: the whole-HUD 0.3 ms and 0.2 ms render-thread figures (AC-HUD-22).
- ADR-0022's rendering spikes R1-R3 themselves.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/TeammateMarkerPresenter.cpp` (the `Stealth.Budget.<Line>` scope declaration)
- `Source/StealthTestHarness/Private/Markers/PerfBudgetTest.cpp` (the capture driver, `StealthGame.Markers.PIE.PerfBudget.*`)
- `production/qa/evidence/tmk-perf-*.md` (the capture notes)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: four allies on screen with labels and edge markers on the reference PC. When: profiled for 60 s. Then: game-thread cost is 0.05 ms or less per frame (p95), 0 per-frame heap allocations, at most 2 draw batches. Edge cases: all four allies at the screen edge.
- **AC2**: Given: a cooked Development standalone capture, not PIE. When: profiled. Then: HUD CPU with markers is 0.5 ms or less and the module declares its `Stealth.Budget.<Line>` Insights scope. Edge cases: the placeholder `<Line>` needs the real name from ADR-0022.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration (ADVISORY gate)
**Required evidence**:
- Integration: capture notes in `production/qa/evidence/tmk-perf-*.md` from a cooked Development standalone on the reference preset OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-019-paint-markers-in-world-anchor-leaf.md`, `story-022-name-label-plate-rendering.md`; cross-epic the ADR-0022 R1-R3 spike stories (rendering and performance epics, not yet created)
- Unlocks: None
