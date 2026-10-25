# Story 058: HUD cost capture against the 0.3 ms / 0.2 ms budget

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: ADR-0022 reference-hardware spikes R1-R3 and the named RTX 3060-class machine do not exist yet (gate ADVISORY until profiled)
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (with ADR-0022: Rendering and performance budgets)
**ADR Decision Summary**: ADR-0021 sets the HUD budget (at most 0.3 ms CPU, 0.5 ms with markers, UI GPU at most 0.5 ms at 1080p on the reference GPU, inside ADR-0022's frame budget) and requires every widget to be created once at HUD init and toggled by visibility, with apply-on-change-only updates. ADR-0022 owns the reference hardware (an RTX 3060-class machine), requires performance claims to come from cooked Development standalone captures on the reference preset (never PIE or editor numbers) and requires each owner ADR's code to declare its `Stealth.Budget.<Line>` Insights scope.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Blocked because the named RTX 3060-class reference machine and ADR-0022 spikes R1 to R3 do not exist yet; the prototype first-playable numbers (production/first-playable-report.md) were taken on an RTX 5090 / 16-core machine and are not budget evidence. Profiling uses Unreal Insights with the `Stealth.Budget.HUD` trace scope on a cooked Development standalone build on the reference preset; PIE numbers are forbidden as budget evidence. Slate and UMG cost lives on the game thread (widget update/layout) and the render thread (paint); the volatile L0 leaf paints every frame by design (E4), so its paint cost is part of the "with markers" 0.5 ms case. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Each owner ADR's code declares its `Stealth.Budget.<Line>` Insights scope — source: ADR-0022
- Forbidden: Never cite PIE/editor numbers as budget evidence — source: ADR-0022
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

The capture itself is deferred until reference hardware exists; the story is written now so the HUD code declares its budget scope and construction counter early, and the gate is ADVISORY until a real capture is taken.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC-HUD-22) and UX spec `design/ux/hud.md` (UXA-HUD-34 CPU clauses), scoped to this story:*

- [ ] **AC1** (AC-HUD-22 `StealthGame.HUD.Perf` and UXA-HUD-34 CPU clauses): **GIVEN** 1080p on the reference GPU with the cooldown sweep running, a result plate entering and three feed lines shown, **WHEN** profiled for 600 frames, **THEN** HUD game-thread cost is at most 0.3 ms and render-thread cost at most 0.2 ms (median; 0.5 ms with markers); captures come from a cooked Development standalone build on the reference preset (manifest: no PIE numbers as budget evidence).
- [ ] **AC2** (UXA-HUD-34 GPU clause and AC-HUD-22 construction clause, `StealthGame.HUD.Perf`): **GIVEN** the same scene **WHEN** profiled **THEN** UI GPU is at most 0.5 ms at 1080p, no widget is constructed during `Active` (counted by the widget-construction counter), and the `Stealth.Budget.HUD` Insights scope is declared by the HUD code (manifest rule).

---

## Implementation Notes

*Derived from ADR-0021 Performance Implications and ADR-0022 §4:*

**Code that can be written now (Ready work inside a Blocked story).** (1) Declare the `Stealth.Budget.HUD` Insights scope (a `TRACE_CPUPROFILER_EVENT_SCOPE_STR("Stealth.Budget.HUD")`-style scope, or the project's wrapper macro from ADR-0022) around the presenter step, the widget apply pass and the L0 leaf paint; (2) add a widget-construction counter incremented in the HUD widget factories (Development builds), whose delta over `Active` must be zero because every HUD widget is created once at init (ADR-0021 §1); (3) add the `StealthGame.HUD.Perf` automation entry as the capture driver (it launches the scene and records; the numbers are read from Insights, not asserted in PIE).

**The capture.** On a cooked Development standalone build with the reference preset (ADR-0022), at 1920x1080 on the reference GPU: a scene that shows the cooldown sweep running (brace Cooling, F5), a result plate entering (stepped entrance, F3), and three feed lines (F8, `HudFeedLineCount` 3), 600 frames. Record the median HUD game-thread time (target at most 0.3 ms), render-thread time (at most 0.2 ms) and UI GPU time (at most 0.5 ms), and repeat with teammate markers and ping rings visible (at most 0.5 ms CPU). Document the build id, preset, GPU model and driver in `production/qa/evidence/hud-perf-capture.md`.

**Gate behaviour.** ADVISORY until profiled: until the reference machine and spikes R1 to R3 exist, the story stays Blocked and the code items above can still land. Once profiled, a miss is a defect against the owning cluster story with the numbers attached; do not relax the budget in this story. ADR-0022 owns the reference figure (the ADR-0021 "14.4 ms allocation" note defers to ADR-0022).

**Traps.** A capture taken in PIE or on the RTX 5090 development machine is not evidence and must not be recorded as such. The volatile L0 leaf paints every frame; keep it cheap (no allocation in `OnPaint`, reuse brush/format arrays) because it dominates the "with markers" case. Apply-on-change-only means an idle HUD should cost close to nothing; if the median is high check for per-frame `SetText` calls with equal text.

**Decisions that apply.** None specific. Out of this story: HUD optimisation work (file defects against the cluster stories), the whole-frame budget (ADR-0022 stories).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-019-hud-brace-indicator-widget.md`, `story-022-hud-result-and-defence-plate-flow.md`, `story-035-hud-team-feed-queue-f8-and-removal-lines.md`: the clusters being measured.
- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: layout and area budget (this story measures time).
- Rendering and performance epic work for ADR-0022 (reference hardware, spikes R1 to R3, whole-frame budgets): not yet an epic with stories.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudBudgetScope.h` (the `Stealth.Budget.HUD` scope and the widget-construction counter)
- `Source/StealthPresentation/Private/Hud/StealthHudPresenter.cpp` and `Private/WorldAnchor/SStealthWorldAnchorLeaf.cpp` (scope placement)
- `Source/StealthTestHarness/Private/HUD/HudPerfCaptureTest.cpp` (capture driver, `StealthGame.HUD.Perf`; Development builds only)
- Evidence: `production/qa/evidence/hud-perf-capture.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `StealthGame.HUD.Perf` CPU
  - Given: a cooked Development standalone build on the reference preset, 1080p, the reference GPU, cooldown sweep running, a result plate entering and three feed lines.
  - When: 600 frames are profiled.
  - Then: median HUD game-thread <= 0.3 ms and render-thread <= 0.2 ms (0.5 ms with markers). No PIE numbers count.
  - Edge cases: DEFERRED. The RTX 3060-class machine and spikes R1-R3 do not exist yet. ADVISORY until profiled.
- **AC-2**: GPU and construction
  - Given: the same scene.
  - When: profiled.
  - Then: UI GPU <= 0.5 ms, widget-construction counter delta 0 during `Active`, and the `Stealth.Budget.HUD` Insights scope is declared.
  - Edge cases: DEFERRED, same reason.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the capture driver `Source/StealthTestHarness/Private/HUD/HudPerfCaptureTest.cpp` (`StealthGame.HUD.Perf`) and the retained capture note `production/qa/evidence/hud-perf-capture.md` (build id, preset, GPU, median figures)
- Gate: ADVISORY until profiled on the reference machine (the plan's override of the Integration default)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-019-hud-brace-indicator-widget.md`, `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`, `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`
- Unlocks: None within this epic (see the EPIC Definition of Done)
