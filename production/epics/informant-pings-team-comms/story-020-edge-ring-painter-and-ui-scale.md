# Story 020: Edge ring painter and UI scale

> **Epic**: Informant Pings & Team Communication
> **Status**: Blocked — BLOCKED: ADR-0021 verification items UI-V1 (zero-lag projection) and UI-V4 (DPI rule: Slate scale `H/1080`, root scaler applies `s_ui` once) have not reported; both are gates named in this epic's Build Order and Gates. Unblocks when the hud and teammate-markers epics' UI-V spike stories report.
> **Layer**: Feature
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R7 edge ring clause, VA-PNG-2, PNG-UI-2; GDD UX flag points to `design/ux/hud.md`)
**Requirement**: `TR-png-015`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§2 L0 leaf, §6 DPI and scale)
**ADR Decision Summary**: The L0 leaf projects anchors in its paint pass and clamps off-screen ones to the safe rectangle; one custom DPI rule gives Slate scale `H/1080` and a root scaler applies the client UI scale `s_ui` exactly once, reacting to a settings change without a resize; text floors hold (18 px).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0021 overall)
**Engine Notes**: ADR-0021 E5: a `UDPICustomScalingRule` result is cached per viewport size outside the editor, so a runtime-changing factor must live in the root scaler, not the rule; PIE keeps editor UI unscaled (UI-V4). The painter must not apply `s_ui` a second time on top of the scaler.

**Control Manifest Rules (this layer)**:
- Required: off-screen or behind-camera rings clamp to the safe rectangle as unlabelled edge rings using the Teammate Markers F3 clamp, bias ramp and hysteresis, drawn in L0 beneath edge markers — source: ADR-0021
- Forbidden: Never draw an arrow or rotate an edge ring; never add a label to an edge ring; never apply UI scale twice — source: art bible 7.5, ADR-0021

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (VA-PNG-2, PNG-UI-2; revised at QL-STORY-READY), scoped to this story:*

- [ ] **AC1** (VA-PNG-2): **GIVEN** a ping point off-screen or behind the camera **WHEN** painted **THEN** a broken ring of `W = H` (story 018) is drawn at the clamped position inside the HUD safe rectangle, static, unrotated, never an arrow, with no label.
- [ ] **AC2** (VA-PNG-2 draw order): **GIVEN** two overlapping edge rings and an edge marker at the same spot **WHEN** painted **THEN** the rings are not spread apart, a painter draw-call order log shows arrival order with the newest ring last (on top) and edge markers after the rings, and one screenshot of the overlap shows no offset between ring centres.
- [ ] **AC3** (UI-V4, scale applied once): **GIVEN** viewport heights 720, 1080 and 1440 at UI scale 1.0 and 1.5 (six captures) **WHEN** edge rings are painted **THEN** the measured ring `W` equals the story 018 formula within +-1 px (32, 32, 43 at scale 1.0 and 32, 48, 64 at scale 1.5), the stroke equals the story 019 stroke times the same `u * s_ui` factor within +-1 px, and the label text floor (18 px) holds on the on-screen ring in each capture.

---

## Implementation Notes

*Derived from GDD VA-PNG-2 and ADR-0021 §6:*

Add the edge variant to the ping slot of the L0 leaf: when story 018's function reports `Edge`, draw the same broken ring at `W = H` at the clamped centre, with the gap fixed to world north in screen space (unrotated, never pointing), no label plate, constant opacity, stroke by age third. Edge rings draw beneath edge markers and in arrival order, newest on top; do not run the Teammate Markers F4 de-overlap. Size and stroke use the HUD's single scale source (the root scaler's `s_ui` and the DPI rule's `u`); read the factor, do not recompute it. Verify the numbers in the AC3 table by capture and by a paint-record log (the painter writes `W`, stroke and centre per ring to the test hook), not only by eye.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 018: the maths (metrics, clamp, hysteresis).
- Story 019: on-screen ring painting, labels, collapse and tick.
- Story 021: the retained evidence set.
- hud epic: DPI rule, root scaler, safe rectangle.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/StealthPingRingPainter.cpp` (edge variant; shared with story 019)
- `Source/StealthPresentation/Private/` L0 leaf ping slot (exact file names per the HUD epic's layout; planned)
- `production/qa/evidence/` captures named in story 021

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Manual check: AC1 edge ring appearance**
- Setup: ping a point off-screen and one behind the camera; capture.
- Verify: a broken ring of `W = H` (story 018) at the clamped position inside the HUD safe rectangle, static, unrotated, not an arrow, no label.
- Pass condition: measured `W = H` matches story 018 (+-1 px); no label or rotation visible.

**Manual check: AC2 overlap order**
- Setup: two overlapping edge rings (older, then newer) plus an edge marker at the same spot.
- Verify: not spread apart; the draw log shows arrival order, newest on top, markers last.
- Pass condition: log order correct and the screenshot shows no offset between ring centres.

**Manual check: AC3 UI scale**
- Setup: the six captures (720p, 1080p, 1440p at scale 1.0 and 1.5).
- Verify: ring `W` and stroke against the formula values; label text height.
- Pass condition: all within +-1 px; label text >= 18 px in every capture.

QL-STORY-READY: GAPS (revised) — AC2 and AC3 were rewritten as above on the qa-lead's proposals (draw-log verification; explicit expected values).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots of each state touched (edge ring off-screen, behind the camera, overlap, six scale captures) in `production/qa/evidence/`, plus the painter's draw-order and paint-record log

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-018-edge-ring-metrics-and-clamp.md`, `story-019-ring-painter-l0-leaf.md`; cross-epic hud epic (DPI rule, root scaler, safe rectangle), teammate-markers epic (edge marker order), ADR-0021 UI-V1 and UI-V4 reports
- Unlocks: `production/epics/informant-pings-team-comms/story-021-ring-ui-evidence-and-blind-sort.md`
