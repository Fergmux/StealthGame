# Story 011: Edge clamp F3

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

When an ally's anchor is off-screen or behind the camera, the marker is clamped to the safe rectangle (the HUD's 5 % margin) as the same pentagon with a stepped base. It does not rotate and is not an arrow (arrows pointing at a figure are banned, art bible 7.5). F3 computes the clamped centre from the screen-space direction when the anchor is in front, and from the camera-space direction with a downward bias ramp when it is behind, so an ally exactly behind lands bottom-centre and never at the mirrored front position, and so the marker slides continuously as the viewer turns through the camera plane (the 2026-10-25 re-review fixed a discontinuity here). A hysteresis band stops the stepped-base variant flickering at the boundary. Informant Pings reuses this clamp rule for its edge rings.

**GDD**: `design/gdd/teammate-markers.md` (F3, TM-R9)
**Requirement**: `TR-tmk-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 2)
**ADR Decision Summary**: The leaf applies F2-F4 of the Markers GDD (anchor offset, edge clamp with hysteresis, edge de-overlap, far-to-near draw order) in `OnPaint`; sizes are physical pixels.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure vector math on the camera-space direction `D` (`D_r`, `D_u`, `D_f`) and the viewport size; the function takes those as plain values so no engine type is needed. The caller (story 019) supplies them from `ULocalPlayer::GetProjectionData`.

**Control Manifest Rules (this layer)**:
- Required: World-anchored drawing happens in `SStealthWorldAnchorLeaf::OnPaint` from the rendered frame's camera view — source: ADR-0021
- Forbidden: Never rotate the edge glyph or turn it into an arrow — source: art bible 7.5 (restated in GDD TM-R9)
- Guardrail: `m` (margin) is the HUD's `HudSafeMarginPct / 100` (read, not owned; default 0.05)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-17, F3), scoped to this story:*

- [ ] **AC1** (AC-TMK-17 a, [U] `EdgeClamp`): **GIVEN** 1920 x 1080 and 3440 x 1440 viewports and anchors to the right, above, behind-left, exactly behind (anchor above and below the camera) and in each corner **WHEN** F3 runs **THEN** every glyph lies wholly inside the 5 % safe rectangle (at 1920 x 1080 with a 28 x 32 glyph: x 110..1810, y 70..1010), the right-hand example `q = (2400, 300)` lands at (1810, 398), the behind-left example `(D_r, D_u) = (-0.70, -0.10)` lands at (110, 844), and an exactly-behind anchor lands at bottom-centre (960, 1010) whether above or below the camera, never at the mirrored front position or the screen centre.
- [ ] **AC2** (AC-TMK-17 b, revised): **GIVEN** a viewer turning through 360 degrees **WHEN** an ally crosses the camera plane **THEN** the edge marker moves continuously: no jump larger than `L` px between consecutive frames at 60 fps and 180 deg/s for `D_u` in [-0.5, 0.25] at the crossing, and at most 1.5 `L` for `D_u` up to +0.5, where `L = W + H + e` from F4 (64 px at 1080p with the default edge gap 4) and `D_u` is the camera-up component of the unit direction to the anchor; and the F3 crossing example reproduces (1810, 540), (1810, 650), (1810, 785).
- [ ] **AC3** (AC-TMK-17 c): **GIVEN** an anchor oscillating +/-2 px across the safe-rectangle boundary **THEN** the edge variant does not toggle (hysteresis `delta_h = round(4u)` px, 4 px at 1080p) and the glyph never rotates.

*(Revised after QL-STORY-READY: `L` and `D_u` are now defined.)*

---

## Implementation Notes

*Derived from F3:*

`inside = (z_c > 0) and x_min <= q_x <= x_max and y_min <= q_y <= y_max`, with `x_min = m*W_v + W/2`, `x_max = (1-m)*W_v - W/2`, `y_min = m*H_v + H/2`, `y_max = (1-m)*H_v - H/2`. If not inside: `d = q - c` when `z_c > 0`; when `z_c <= 0` use the camera-space direction `d = (D_r, -D_u + sigma * max(0, k_b + D_u))` with bias ramp `sigma = clamp(-D_f / eps_b, 0, 1)`, `k_b = 0.25`, `eps_b = 0.5`. If `|d| < 1e-4` use `d = (0, k_b)`. Then `t = min(h_x / |d_x|, h_y / |d_y|)` with `h_x = x_max - c_x`, `h_y = y_max - c_y` (drop a term whose denominator is 0; no `min(1, ...)` cap) and the clamped centre is `q' = c + t * d`. Hysteresis: a clamped marker returns to on-screen drawing only when `q` is at least `delta_h` px inside the rectangle and `z_c > 0`. Worked examples are in the GDD F3 section: exactly behind gives `d = (0, max(-D_u, 0.25))`, `q' = (960, 1010)`; the camera-plane crossing slides the marker down the right edge by about 245 px over about 30 degrees of turn.

Viewport size is read every frame (resize, DPI, ultrawide). Test viewports 1920 x 1080 and 3440 x 1440; give the second a hand-computed expected table in the test file.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-edge-de-overlap-f4.md`: spreading several edge markers (cuttable).
- `story-019-paint-markers-in-world-anchor-leaf.md`: supplying the view and drawing the stepped-base variant.
- the informant-pings-team-comms epic: reuse of this rule for ring clamping.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerEdgeClamp.h` and `MarkerEdgeClamp.cpp`
- `Source/StealthPresentation/Private/Tests/Markers/MarkerEdgeClampTest.cpp` (name `StealthGame.Markers.EdgeClamp`, class `FMarkersEdgeClampTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: 1920 x 1080 and 3440 x 1440 viewports; anchors right, above, behind-left, exactly behind (above and below the camera) and each corner. When: F3 runs. Then: every glyph lies inside the safe rectangle (1920 x 1080: x 110..1810, y 70..1010), the right-hand example is (1810, 398), the behind-left example is (110, 844), and exactly-behind is (960, 1010) above and below the camera, never mirrored or at screen centre. Edge cases: input anchors are the GDD F3 worked examples.
- **AC2**: Given: a 360 degree turn at 180 deg/s, 60 fps, with `D_u` swept over [-0.5, 0.5]. When: the ally crosses the camera plane. Then: no frame-to-frame jump over `L` = 64 px for `D_u` in [-0.5, 0.25], at most 96 px up to +0.5, and the crossing examples (1810, 540), (1810, 650), (1810, 785). Edge cases: `D_r` near 0.
- **AC3**: Given: an anchor oscillating +/-2 px across the safe-rectangle boundary. When: frames run. Then: the edge variant never toggles (`delta_h` = 4 px at 1080p) and the glyph rotation stays 0. Edge cases: oscillation larger than `delta_h` does toggle.

QL-STORY-READY: GAPS, revised in place per the agent's proposal (`L` and `D_u` defined); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerEdgeClampTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-010-marker-size-f1.md`; soft: the hud epic's `HudSafeMarginPct` key (read through `GetClientTuning()`, default 0.05 until the hud epic lands)
- Unlocks: `production/epics/teammate-markers/story-012-edge-de-overlap-f4.md`, `story-019-paint-markers-in-world-anchor-leaf.md`; the informant-pings-team-comms epic
