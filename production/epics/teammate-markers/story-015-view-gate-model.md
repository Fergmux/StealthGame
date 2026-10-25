# Story 015: View-gate model

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

Markers are drawn only while Camera's client-local `GetViewState()` returns `Live` (the world rendered from a possessed pawn, including the dying-body phase of the viewer's own elimination and the exile beat). They are not drawn while it returns `Neutral` (black screen while awaiting a pawn, during `Preparing`, during a rejoin). The gate depends on the view, never on the viewer's role: an informant viewer keeps every ally's marker including a hatched concealed ally. The marker set itself, `IsMarkedAlly` and the timers keep running while the gate is closed; only drawing stops, and on returning to `Live` every non-`Absent` marker fades in together. This story is the gate logic against a fake camera view; the layer order against the real fade is story 023.

**GDD**: `design/gdd/teammate-markers.md` (TM-R8)
**Requirement**: `TR-tmk-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (sections 1 and 5)
**ADR Decision Summary**: Drawing only while `GetViewState() == Live`; the model (and `IsMarkedAlly`) keeps running while gated; L0 is collapsed while `Neutral`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `GetViewState()` is published by `IStealthCameraView` (ADR-0015, camera-system story 017); the model reads it through a weak interface handle so tests can script it.

**Control Manifest Rules (this layer)**:
- Required: Layers below ZOrder 100 are `HitTestInvisible`; L0 is collapsed while `GetViewState() == Neutral` — source: ADR-0021
- Forbidden: Never gate markers on the viewer's role or on camera mode; never add a spectator or follow-a-teammate case (IVP R13) — source: ADR-0021, ADR-0015
- Guardrail: The gate costs one enum read per frame

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-16a model half, TM-R8), scoped to this story:*

- [ ] **AC1** (AC-TMK-16a, model half): **GIVEN** a scripted `GetViewState` sequence `Neutral` (Preparing), `Live`, `Live` (own dying body), `Neutral` (awaiting a pawn), `Live` (informant pawn) **WHEN** the model is sampled each frame **THEN** markers are drawable iff the state is `Live`, all fade in together on returning to `Live`, and the marker set, `IsMarkedAlly` and the timers are unchanged and running while `Neutral`.
- [ ] **AC2** (TM-R8): **GIVEN** an informant viewer with a hatched concealed ally **THEN** every ally marker is kept including the hatched one, because the gate depends on the view and never on the viewer's role.
- [ ] **AC3** (TM-R8, revised): **GIVEN** the gate **THEN** `drawable(state)` is true only for `Live` for every value of the view-state enum (including any spectate or follow value, should one ever be added); an invalid or expired `IStealthCameraView` handle gives not drawable without a crash; and L0's visibility is `Collapsed` while `Neutral`.

*(Revised after QL-STORY-READY: the absence claim in AC3 became a testable function over the enum.)*

---

## Implementation Notes

*Derived from TM-R8 and ADR-0021 §1, §5:*

The model takes `EStealthViewState` (or a bool from `IStealthCameraView::GetViewState()`) per frame as input and outputs `bDrawable`; it never branches on the viewer's participant state. In `Neutral` nothing is drawn but timers still run (a retire hold keeps counting and `OnAllyMarkerRetired` still fires at `t_end`, story 008). On the transition back to `Live` all non-`Absent` markers start one shared entrance fade (story 009). The marker layer sits below the fade layer (ZOrder 20), which sits below HUD plates, so markers go out under the black; that ordering is story 023.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-023-layer-order-and-view-gate-with-fade.md`: the real fade layer, ZOrder and screenshots (UI-V3).
- `camera-system/story-017-view-state-neutral-view-fade.md`: producing the view state.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerViewGate.h` and `MarkerViewGate.cpp`
- `Source/StealthPresentation/Private/Tests/Markers/MarkerViewGateTest.cpp` (name `StealthGame.Markers.ViewGateModel`, class `FMarkersViewGateModelTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: the scripted `GetViewState` sequence `Neutral`, `Live`, `Live` (dying body), `Neutral`, `Live` (informant pawn). When: sampled each frame. Then: drawable iff `Live`, all markers fade in together on returning to `Live`, and the marker set, `IsMarkedAlly` and the timers run unchanged during `Neutral`. Edge cases: `Neutral` for a single frame.
- **AC2**: Given: an informant viewer with a hatched concealed ally. When: the gate is `Live`. Then: every ally marker is kept, including the hatched one. Edge cases: the viewer's role changes mid-round.
- **AC3**: Given: every value of the view-state enum, an invalid camera handle, and `Neutral`. When: `drawable` and L0 visibility are evaluated. Then: only `Live` is drawable; an invalid handle gives not drawable and no crash; L0 is `Collapsed` while `Neutral`. Edge cases: a handle that expires mid-frame.

QL-STORY-READY: GAPS, revised in place per the agent's proposal (AC3 made testable); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerViewGateTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-002-marker-model-teamview-source-and-validation.md`, `story-009-fade-draw-order-no-motion.md`
- Unlocks: `production/epics/teammate-markers/story-016-presenter-local-controller-only.md`, `story-023-layer-order-and-view-gate-with-fade.md`
