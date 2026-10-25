# Story 018: Edge ring metrics and clamp

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R7 edge ring clause, VA-PNG-2; AC-PNG-21)
**Requirement**: `TR-png-015`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§2 L0 leaf with in-paint projection; §5 Teammate Markers F3 clamp, bias ramp and hysteresis), with ADR-0015 (camera-space depth)
**ADR Decision Summary**: The L0 leaf projects world anchors inside its paint pass from the rendered frame's view; off-screen and behind-camera anchors are clamped to the HUD safe rectangle by Teammate Markers' F3 clamp including its behind-camera bias ramp and re-entry hysteresis. Pings reuse that clamp and add their own ring size `W = H = max(32, round(32 * u * s_ui))` px.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0021 overall; this story is pure maths over injected projection data)
**Engine Notes**: No engine API: the metrics and clamp are pure functions over viewport size, UI scale, `HudSafeMarginPct`, projected position and camera-space direction. Projection itself (`ULocalPlayer::GetProjectionData`) and its UI-V1 verification belong to the painter (stories 019 and 020), so this story is Ready.

**Control Manifest Rules (this layer)**:
- Required: off-screen or behind-camera rings clamp to the safe rectangle as unlabelled edge rings using the Teammate Markers F3 clamp, bias ramp and hysteresis — source: ADR-0021, GDD VA-PNG-2
- Forbidden: Never draw an arrow or rotate an edge ring; never copy the F3 clamp (call the owner's function, the constants stay Teammate Markers') — source: art bible 7.5, GDD VA-PNG-2

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-21, VA-PNG-2), scoped to this story:*

- [ ] **AC1** (AC-PNG-21 a, `[Logic]` `StealthGame.Pings.EdgeRingMetrics`): **GIVEN** viewports 1920x1080, 2560x1440 and 1280x720 at UI scale 1.0 and 1920x1080 at 1.5 **WHEN** the edge-ring metrics are computed **THEN** `W = H` is 32, 43, 32 and 48 px, `delta_h` is 4, 5, 3 and 4 px, and `x_min` at 1920x1080 scale 1.0 is 112 px (`HudSafeMarginPct` 5).
- [ ] **AC2** (VA-PNG-2 clamp): **GIVEN** the same inputs **WHEN** the ring centre is computed **THEN** it calls the Teammate Markers F3 clamp (no copy), including its behind-camera bias ramp (`k_b` 0.25, `epsilon_b` 0.5) and the `|d| < 1e-4` fallback `d = (0, k_b)`, and equals that function's output for identical inputs; sampling the viewer yaw at 89.9, 90.0 and 90.1 degrees gives clamped ring centres that differ by <= 2 px between adjacent samples at 1920x1080 and scale 1.0 (the ramp prevents the roughly 212 px jump of an unramped clamp).
- [ ] **AC3** (AC-PNG-21 b): **GIVEN** a ping point oscillating +-2 px across the safe-rectangle boundary **WHEN** evaluated for 60 frames **THEN** the edge variant does not toggle (hysteresis `delta_h`), and the edge ring is static, unrotated and carries no label.

---

## Implementation Notes

*Derived from GDD VA-PNG-2 and ADR-0021 §5:*

`u = H_v / 1080`; `W = H = max(32, round(32 * u * s_ui))`; `delta_h = round(4 * u)`; `x_min = m * W_v + W/2` with `m = HudSafeMarginPct / 100`. Return to on-screen drawing uses the Teammate Markers hysteresis (`delta_h` px inside the rectangle and `z_c > 0`). Do not re-derive the clamp: call the Teammate Markers F3 function with ring `W`, `H`; if that function has not landed when this story is built, implement it behind the same signature in the shared presentation maths header named by the teammate-markers epic and let that epic own it, so the two cannot drift. AC2's test compares this story's output with the F3 function for identical inputs including `z_c <= 0`, `D_f` at the bias-ramp boundary, and `|d| < 1e-4`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 020: painting edge rings, arrival-order draw, beneath-marker order and UI-scale application.
- teammate-markers epic: owner of F3, its constants, edge spreading (F4, which pings do not use).
- Story 016: when a ring exists at all.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/StealthPingEdgeRing.h` and `StealthPingEdgeRing.cpp` (metrics and the call into the F3 clamp)
- `Source/StealthPresentation/Private/Tests/Pings/PingEdgeRingMetricsTest.cpp` (class `FPingsEdgeRingMetricsTest`, name `StealthGame.Pings.EdgeRingMetrics`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 edge-ring metrics (AC-PNG-21 a)**
- Given: viewports 1920x1080, 2560x1440, 1280x720 at UI scale 1.0, and 1920x1080 at 1.5; `HudSafeMarginPct` 5.
- When: compute `W = H`, `delta_h` and `x_min`.
- Then: `W = H` is 32, 43, 32, 48 px; `delta_h` is 4, 5, 3, 4 px; `x_min` at 1080p / 1.0 is 112 px.
- Edge cases: the 720p result is the 32 px floor (21.3 px raw); `delta_h` depends on viewport height only (4 stays 4 at scale 1.5).

**Test: AC2 uses the Teammate Markers F3 clamp**
- Given: identical inputs to the ring path and to the Markers F3 function.
- When: compare centres across a sweep of directions, including behind-camera and `|d| < 1e-4`; sample the yaw at 89.9, 90.0 and 90.1 degrees.
- Then: the ring centre equals the F3 output exactly; the call goes through the F3 function (static scan, no copy); bias ramp `k_b` 0.25, `epsilon_b` 0.5; the fallback `d = (0, k_b)`; adjacent yaw samples differ by <= 2 px at 1920x1080 scale 1.0.
- Edge cases: `d` exactly zero; a point directly behind the camera.

**Test: AC3 hysteresis (AC-PNG-21 b)**
- Given: a ping point oscillating +-2 px across the safe-rectangle boundary, starting from each side.
- When: evaluate for 60 frames.
- Then: the edge variant never toggles; the edge ring model is static, unrotated and carries no label.
- Edge cases: an oscillation of +-5 px does toggle once per crossing outside the `delta_h` band (negative control).

QL-STORY-READY: GAPS (revised) — AC2's continuity clause was given a tolerance as above on the qa-lead's proposal.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Pings/PingEdgeRingMetricsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-016-ring-model-and-age.md`; cross-epic teammate-markers epic (F3 clamp, bias ramp, hysteresis), hud epic (safe rectangle, `HudSafeMarginPct`, UI scale)
- Unlocks: `production/epics/informant-pings-team-comms/story-020-edge-ring-painter-and-ui-scale.md`
