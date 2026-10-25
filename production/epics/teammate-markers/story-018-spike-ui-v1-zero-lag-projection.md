# Story 018: Spike UI-V1, zero-lag projection and tracking

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

ADR-0021 is conditionally accepted: before dependent implementation, verification item UI-V1 must report that a marker painted by the L0 Slate leaf stays on its ally's anchor with no frame of lag during fast camera turns and boom collisions at 60 and 144 Hz ("spike on the graybox"). The decision rests on a verified frame order (the player camera updates after all actors tick, and Slate's paint runs later in the same engine-loop iteration, so a paint pass sees the camera of the frame being rendered), but the ADR keeps UI-V1 as an observation on the graybox. This story is that spike, and it is also the observation behind AC-TMK-22 (no-lag tracking, a Visual BLOCKING criterion, "the most visible failure" per the GDD's Game Feel). It builds a minimal `SStealthWorldAnchorLeaf` that paints one glyph at a projected anchor, measures it against an engine-rendered reference sphere, and writes a PASS or FAIL. Story 019 and every painted story stay Blocked until it reports PASS; on FAIL the recorded fallback is to paint from a `FSceneViewExtension` view captured for the same frame.

**GDD**: `design/gdd/teammate-markers.md` (AC-TMK-22, Game Feel, VA-2 projection contract)
**Requirement**: `TR-tmk-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 2; verification item UI-V1)
**ADR Decision Summary**: One volatile Slate leaf projects markers inside `OnPaint` from `ULocalPlayer::GetProjectionData`; zero lag follows from the verified frame order (E3); fallback if it ever fails: paint from a `FSceneViewExtension` hook's view data captured on the game thread for the same frame.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine facts E3 (frame order) and E4 (`ForceVolatile`) are [V]; the zero-lag claim is the [U] that this spike closes. Measure in a cooked Development standalone or a 2-client PIE at fixed timestep (`-fixedfps`); PIE and editor numbers are not performance evidence, but a pixel-offset measurement is not a performance claim. The prototype spike code and graybox map `L_Prototype` are the test bed; never resave the sample's camera assets (`production/never-resave-assets.md`).

**Control Manifest Rules (this layer)**:
- Required: World-anchored drawing happens in `SStealthWorldAnchorLeaf::OnPaint` from the rendered frame's camera view; the leaf is volatile and never inside an invalidation or retainer box — source: ADR-0021
- Forbidden: Never compute world-anchored positions in a UMG binding, widget tick or game-thread tick; never use one widget per marker (one frame of lag) — source: ADR-0021
- Guardrail: Spikes owning [U] claims in this layer: UI-V1..V4 (ADR-0021)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-22) and ADR-0021 UI-V1, scoped to this story:*

- [ ] **AC1** (AC-TMK-22, [PIE-2][1P/3P][Vis] `NoLagTracking`): **GIVEN** an ally sprinting across the view while the viewer pans at the camera's maximum look rate (the maximum the Camera tuning allows; record the value in deg/s in the evidence) **WHEN** a fixed-timestep 60 fps frame sequence is captured with a test-only unlit reference sphere attached to the ally at the anchor height (rendered by the engine in the same frame, so the harness does not reuse the marker's own projection) **THEN** the drawn tip is within 1 px of the sphere's rendered top plus the gap on every frame, in 1P and in 3P, with no smoothing.
- [ ] **AC2** (UI-V1, revised): **GIVEN** the same capture at a fixed timestep of 1/144 s, and on every frame in which the 3P boom collision shortens the arm **THEN** the drawn tip is within 1 px of the sphere's rendered top plus the gap on every frame (no frame of lag).
- [ ] **AC3** (spike report): **GIVEN** the runs **THEN** a PASS or FAIL verdict is written to `production/qa/spikes/` (with the retained frame sequences in `production/qa/evidence/tmk-tracking-*.png` and lead sign-off in `production/qa/evidence/tmk-tracking-evidence.md`), and on FAIL the ADR-0021 §2 fallback (paint from a `FSceneViewExtension` view captured for the same frame) is recorded as the decision and a note is raised against ADR-0021.

*(Revised after QL-STORY-READY: the pan rate is recorded rather than left as "maximum", the 144 Hz and boom-collision cases carry the same 1 px threshold, and the evidence path and sign-off were added.)*

---

## Implementation Notes

*Derived from ADR-0021 §2 and the VA-2 projection contract:*

Build the smallest `SStealthWorldAnchorLeaf : SLeafWidget` that works: `ForceVolatile(true)`, `ComputeDesiredSize`, a `UWidget` wrapper that builds it in `RebuildWidget` and releases it in `ReleaseSlateResources` (E12), added to the player screen at ZOrder 10 with `AddToPlayerScreen`. In `OnPaint` read the view with `ULocalPlayer::GetProjectionData` (location, rotation, the view's actual FOV, aspect constraint and the player's sub-rect, after the boom and camera-manager update), project the anchor `R + (0, 0, TeammateMarkerAnchorCm)` and draw one glyph. Do **not** project in a UMG binding, a widget tick or a game-thread tick. The reference sphere is a test-only unlit mesh attached to the ally at the anchor height (compiled out of Shipping); the comparison reads pixel positions from the captured frames, not from the marker's own projection.

Cases to run: 1P and 3P, ally sprinting across the view, the viewer panning at the maximum look rate, 60 fps and 144 fps fixed timestep, a 3P boom collision that shortens the arm mid-pan. Record the build, the map, the clock and the pan rate in the spike note. If the first run fails, try the documented fallback before concluding, and record both results.

Prototype replaced: the prototype drew markers in an `AHUD` canvas pass (`StealthHUD.cpp:164`), which is projected during the scene viewport draw and sits under every UMG layer; its feasibility is evidence only. The spike leaf is throwaway scaffolding for story 019, which builds the production leaf (the leaf class is shared with the hud epic).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-paint-markers-in-world-anchor-leaf.md`: the production painting, glyphs and DPI handling.
- UI-V3 (layer order) and UI-V4 (DPI): owned by the hud epic.
- `story-026-host-parity-of-marker-set.md`: host versus client equality.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Spikes/MarkerProjectionSpike.cpp` (throwaway, compiled out of Shipping with `#if !UE_BUILD_SHIPPING`)
- `Source/StealthTestHarness/Private/Markers/NoLagTrackingTest.cpp` (CQTest, `StealthGame.Markers.PIE.NoLagTracking.*`)
- `production/qa/spikes/<date>_UI-V1.md` (the verdict and the measurements)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Setup: fixed-timestep 60 fps; an ally sprinting; the viewer panning at the maximum look rate (recorded); a test-only unlit reference sphere at the anchor height; run in 1P and 3P. Verify: per frame, compare the drawn tip with the sphere's rendered top plus the gap. Pass condition: within 1 px on every frame, no smoothing, frames retained as `production/qa/evidence/tmk-tracking-*.png` with lead sign-off.
- **AC2**: Setup: the same scene at a fixed timestep of 1/144 s, and a 3P boom collision that shortens the arm. Verify: per frame, the tip against the sphere. Pass condition: within 1 px on every frame, including the frames where the arm length changes.
- **AC3**: Setup: spike run complete. Verify: a PASS or FAIL file exists in `production/qa/spikes/`. Pass condition: file present; on FAIL, the ADR-0021 §2 `FSceneViewExtension` fallback is recorded as the decision.

QL-STORY-READY: GAPS, revised in place per the agent's proposal (pan rate recorded, 144 Hz and boom cases given a threshold, evidence path and sign-off added); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained frame captures in `production/qa/evidence/tmk-tracking-*.png` and a sign-off in `production/qa/evidence/tmk-tracking-evidence.md`; plus the spike verdict in `production/qa/spikes/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-004-anchor-fed-position-and-null-ref.md`, `story-010-marker-size-f1.md`; cross-epic `production/epics/camera-system/story-006-eye-pivot-boom-collision.md` (3P boom collision), `story-003-stick-look-response-no-aim-assist.md` (look rate), `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`
- Unlocks: `production/epics/teammate-markers/story-019-paint-markers-in-world-anchor-leaf.md`, `story-021-hatch-on-body-hide-frame.md`, `story-022-name-label-plate-rendering.md`, `story-026-host-parity-of-marker-set.md`
