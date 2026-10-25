# Story 019: Ring painter in the L0 leaf

> **Epic**: Informant Pings & Team Communication
> **Status**: Blocked — BLOCKED: ADR-0021 verification items UI-V1 (zero-lag projection of the L0 leaf) and UI-V3 (layer order: the screen fade covers the ring and stays under plates, cards and menus) have not reported; both are gates named in this epic's Build Order and Gates. Unblocks when the hud and teammate-markers epics' UI-V spike stories report.
> **Layer**: Feature
> **Type**: UI
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R7 draw rules, VA-PNG-1, VA-PNG-3, VA-PNG-4, VA-PNG-5, PNG-UI-1 and PNG-UI-3). The GDD's UX flag says UI stories cite `design/ux/hud.md` (the L0 ping slot); read that spec when it exists.
**Requirement**: `TR-png-015` (also the tick of `TR-png-014` and the no-feed rule of `TR-png-017`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§1 layers, §2 L0 volatile Slate leaf with in-paint projection)
**ADR Decision Summary**: UMG overlay layers are added per local player at the HUD-R4 ZOrders through `AddToPlayerScreen`, hit-test-invisible and never activatable; one volatile Slate leaf in L0 projects markers, the interact ring, the context prompt and the ping rings inside its paint pass from the rendered frame's view (`ULocalPlayer::GetProjectionData`); the screen fade is a HUD layer that Camera drives, so it covers L0 and sits under plates, cards and menus; all widgets are created at HUD init and toggled.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0021: Enhanced Input and Common UI unified in 5.8; the L0 leaf is custom Slate)
**Engine Notes**: Facts E1-E4 and E12 of ADR-0021 apply (`AddToPlayerScreen` sits below `AddToViewport`, the paint pass sees the camera of the frame being rendered, `ForceVolatile`, the wrapper releases the Slate widget in `ReleaseSlateResources` and implements `ComputeDesiredSize`). Fallback if UI-V1 fails: paint from a scene view extension's captured view data (view source only changes). Never use `AddToViewport`; never create a widget per ping.

**Control Manifest Rules (this layer)**:
- Required: team ping rings, edge rings and sender labels are painted by the L0 world-anchor leaf from the own controller's `UTeamPingViewComponent`, beneath markers, collapsed with L0; each `(SenderHandle, PingSeq)` is shown once — source: ADR-0021
- Required: one 2D UI-bus tick per created ring; the ring model is the plain C++ `FTeamPingRingModel` (story 016) — source: ADR-0021
- Forbidden: Never encode ring age in opacity, cross-fade a replaced ring, or add a ping feed type, plate, card or counter; never spawn a world sound or effect for a ping — source: ADR-0021, ADR-0003
- Guardrail: widgets apply values only on change; no per-frame allocation in the paint pass — source: ADR-0021, ADR-0022

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (VA-PNG-1, VA-PNG-3, VA-PNG-4, VA-PNG-5; revised at QL-STORY-READY), scoped to this story:*

- [ ] **AC1** (VA-PNG-1, VA-PNG-3): **GIVEN** an on-screen ring **WHEN** painted **THEN** it is an open ring with a 60 degree gap fixed to world north, flat on the ground, world radius >= 4 m and screen minimum 32 px, Marigold with an Obsidian keyline (3 px, 4 px beyond 20 m), no centre mark, stroke 6, 4, 3 px at the age thirds at constant opacity, and static: within one age third, 60 consecutive 60 Hz frame captures for a stationary point and camera show identical ring diameter, stroke, gap angle and centre (<= 1 px); the name label is Bone text (18 px floor) on a Turquoise-keylined plate below the ring, ellipsised and never larger than the ring.
- [ ] **AC2** (VA-PNG-4): **GIVEN** rings and teammate markers **WHEN** painted **THEN** rings draw in HUD L0 beneath markers with no depth test (a ring behind a wall is readable), collapse with L0 in the `Neutral` view, are hidden on entering `Ended`, and the screen fade covers them.
- [ ] **AC3** (VA-PNG-5): **GIVEN** a new ring appears on a receiving client (the sender on its echo) **THEN** one 2D tick plays on the UI submix at the level in the audio spec (UI-bus gain in dB, supplied by audio with the HUD cue list), verified from the audio-event log as 2D, UI submix and nothing on any world bus, none on expiry or replacement removal; all ring widgets are created at HUD init and toggled (widget count constant across pings, no per-ping widget).
- [ ] **AC4** (prototype retirement, derived): **GIVEN** the production painter **THEN** `AStealthHUD::DrawPings`, the prototype ring drawing and the `FStealthPing` storage in `AStealthPlayerController` are removed.

---

## Implementation Notes

*Derived from ADR-0021 §2 and the GDD Visual/Audio requirements:*

Add a ping slot to the L0 leaf (HUD-R11 ping slot): for each ring from the model (story 016/017) project the ground point in the paint pass, draw the broken ring as stroked arc geometry with the 60 degree gap centred on world north projected to the screen (so the gap never "points" at anything), keyline first then Marigold stroke, then the label plate. World radius >= 4 m projected, clamped up to the 32 px screen minimum; keyline 3 px (4 px beyond 20 m). Order inside L0: rings, then teammate markers (VA-PNG-4); the leaf is volatile and uses `FSlateDrawElement` with no depth test. Collapse with L0 when the camera view is `Neutral` (CS-R15 via the HUD's existing L0 gating), hide in the `Step` that sees `Ended` (the model already does; the painter draws nothing). The tick is raised by the model on ring creation (story 016) and played by the HUD's cue path on the UI bus (audio asset from the audio-director with the HUD cue list, HUD OQ11). Use palette tokens from the art bible (Marigold, Obsidian, Bone, Turquoise), never inline colours.

Prototype replaced: `AStealthHUD::DrawPings` (`StealthHUD.cpp`: a ring around the projected point plus the sender name) and `FStealthPing` and `AStealthPlayerController::Pings` (the prototype client-side storage, also touched by story 027). AC4 removes them once this painter exists.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 016, 017: the ring list, age, label text and label drop.
- Story 018 and 020: edge rings, UI-scale application and the clamp.
- Story 021: the retained evidence set, the blind sort and the "Not now" plate capture.
- hud epic: the L0 leaf itself, the layer stack, the screen fade layer and the cue bus.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/` L0 leaf ping slot (exact file names per the HUD epic's layout; planned)
- `Source/StealthPresentation/Private/StealthPingRingPainter.h` and `StealthPingRingPainter.cpp`
- `Source/StealthGame/Private/StealthHUD.cpp` and `Source/StealthGame/Public/StealthHUD.h` (remove `DrawPings`, only until the HUD moves to `StealthPresentation`)
- `Source/StealthGame/Public/StealthPlayerController.h` and `Source/StealthGame/Private/StealthPlayerController.cpp` (remove `FStealthPing` and `Pings`)
- `production/qa/evidence/` captures named in story 021

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Manual check: AC1 on-screen ring appearance (VA-PNG-1, VA-PNG-3)**
- Setup: 2v2 PIE; an ally pings a ground point on screen at distance < 20 m; capture at ages ~1 s, ~4 s and ~7 s; capture another ring beyond 20 m.
- Verify: open ring, 60 degree gap fixed to north, flat on the ground, world radius >= 4 m and >= 32 px; Marigold #E3A72F with Obsidian keyline 3 px (4 px beyond 20 m); no centre mark; stroke 6, 4, 3 px at constant opacity; label in Bone text >= 18 px on a Turquoise-keylined plate below, ellipsised, no larger than the ring; 60 consecutive frame captures within one age third show no motion.
- Pass condition: every measured value matches within +-1 px and exact hex colours for fill and keyline; the frame sequence shows identical diameter, stroke, gap angle and centre.

**Manual check: AC2 layering and collapse (VA-PNG-4)**
- Setup: a ring behind a wall with a teammate marker overlapping it; enter the Neutral view; end the round; trigger the screen fade.
- Verify: ring beneath the marker; ring visible through the wall; ring collapses with L0 in Neutral; hidden on entering Ended; fade covers it.
- Pass condition: each of the five observations holds in retained screenshots.

**Manual check: AC3 arrival tick and widget creation (VA-PNG-5)**
- Setup: an ally pings; the sender observes its echo; the ring expires; a ring is replaced.
- Verify: the audio-event log shows one 2D UI-bus tick per new ring (receiver, and sender on echo), none on expiry or replacement removal, nothing on any world bus; HUD init creates all ring widgets once (widget count constant across pings).
- Pass condition: counts exact and level per the audio spec.

**Manual check: AC4 prototype painter removed**
- Setup: search the source tree.
- Verify: `AStealthHUD::DrawPings`, the prototype ring drawing and `FStealthPing` storage in `StealthPlayerController`.
- Pass condition: zero matches (state the denominator: files scanned) and the project builds.

QL-STORY-READY: GAPS (revised) — the "static" and "quiet" criteria were rewritten as above on the qa-lead's proposals; the audio level value is supplied by audio with the HUD cue list, so AC3 cannot close until that spec exists.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots of each state touched (three stroke weights with label, beyond-20 m keyline, ring behind a wall, Neutral view, Ended, fade) in `production/qa/evidence/`; the audio-event log excerpt for AC3

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-016-ring-model-and-age.md`, `story-017-ring-labels-and-label-drop.md`; cross-epic hud epic (L0 leaf, layer stack), teammate-markers epic (marker draw order), ADR-0021 UI-V1 and UI-V3 reports
- Unlocks: `production/epics/informant-pings-team-comms/story-020-edge-ring-painter-and-ui-scale.md`, `story-021-ring-ui-evidence-and-blind-sort.md`
