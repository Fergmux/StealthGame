# Story 049: Team ping rings, edge rings and sender labels painted by the L0 leaf (HUD-E22)

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: spike UI-V1 (teammate-markers story 018) has not reported
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-003`, `TR-hud-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (user decision 2026-10-25, Technical Setup 2026-10-25b)
**ADR Decision Summary**: World-anchored drawing, including the Informant Pings ground rings, edge rings and sender labels (HUD-E22), happens in the one volatile `SStealthWorldAnchorLeaf::OnPaint` at ZOrder 10, beneath the teammate markers and collapsed with L0 in the neutral view. Ring state is the plain C++ `FTeamPingRingModel` keyed by `SenderHandle` with `LastShownSeq`; a ring is created only for a new `PingSeq` younger than `PingLifetimeSec`, and the sender's own ring comes only from the server echo. A ping never gets a feed line, plate, card, counter or per-ping widget.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Rests on ADR-0021 verification item UI-V1 (zero-lag projection of the L0 leaf, spike story `teammate-markers/story-018`; the engine fact E3 says Slate paints after the camera update of the same frame, so the leaf sees the rendered frame's view) and on E4/E12 (`ForceVolatile`, release the Slate widget in `ReleaseSlateResources`). The ring must also stay under the screen fade and above nothing else (UI-V3, story 012 and story 013): `AddToPlayerScreen` puts L0 in the Player slot (E1), and the camera fade cannot cover UMG (E2), which is why the HUD fade layer at ZOrder 20 draws it. `s_ui` is applied once by the leaf (DPI rule result is cached per viewport size, E5). `UE_LOGF` is preferred over `UE_LOG` in 5.8 for any diagnostics.

**Control Manifest Rules (this layer)**:
- Required: Team ping rings, edge rings and sender labels are painted by the L0 world-anchor leaf from the own controller's `UTeamPingViewComponent`, beneath markers, collapsed with L0; each `(SenderHandle, PingSeq)` is shown once, for `PingLifetimeSec` from its `ServerTime` on `EstimatedServerNow()`; entries present at a rebuild are marked shown — source: ADR-0021 (user decision 2026-10-25)
- Forbidden: Never add a ping feed type, plate, card or counter to `FHudViewModel` or the F8 feed queue (the concealed-refusal plate is input feedback, never shown for a received or echoed ping) — source: ADR-0021 (Technical Setup 2026-10-25b)
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

This is the HUD side of the team ping rings: the Informant Pings epic owns the ring content, the model and the painters (its stories 016 to 021), and this story proves that the HUD layer hosts them correctly. It wires the ring slot into the L0 leaf, ties the ring lifecycle to the HUD step (`Ended`, `Reset`, travel, rebuild), keeps pings out of every HUD surface that would turn them into feed lines or plates, and retains the HUD-E22 screenshots.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (HUD-R11 ping slot, HUD-R17) and UX spec `design/ux/hud.md` (E22, UXA-HUD-33 ping part), scoped to this story:*

- [ ] **AC1** (derived from ADR-0021 Technical Setup 2026-10-25b and Informant Pings PNG-R7, `StealthGame.HUD.TeamPingRingWiring`): **GIVEN** `FTeamPingRingModel` (plain C++ beside `FTeammateMarkerModel`) keeps `LastShownSeq` per `SenderHandle` **WHEN** entries arrive **THEN** a ring is created only for a new `PingSeq` whose age `ServerNowEst - ServerTime` is below `PingLifetimeSec`, older new entries are marked shown and never drawn, there is one ring per sender (a new ping replaces it at once, no cross-fade, no local prediction), and one 2D UI-bus tick is emitted per created ring.
- [ ] **AC2** (derived from UX spec E22, `StealthGame.HUD.TeamPingRingWiring`): **GIVEN** rings of different ages, a sender that is retired, an edge ring and the sender's own ping **WHEN** drawn **THEN** age is shown only by stroke weight at thirds (6 / 4 / 3 px) at constant opacity, the label is the sender's `DisplayName` (or "Ally"), dropped at the sender's `OnAllyMarkerRetired` while the ring stays to expiry, edge rings carry no label, and the sender's own ring comes only from the server echo.
- [ ] **AC3** (UXA-HUD-33 ping part, `StealthGame.HUD.TeamPingRingWiring` plus retained screenshots): **GIVEN** `Phase = Ended`, `Reset`, travel start or a rebuild **WHEN** the HUD steps **THEN** rings hide in the `Step` that sees `Phase = Ended`, the model clears on `Reset` and travel start, a rebuild sets `LastShownSeq := PingSeq` for every present entry, and no ping ever gets a feed line, plate or per-ping widget; retained screenshots show a ring with its label and an edge ring.

---

## Implementation Notes

*Derived from ADR-0021 Decision §2 (L0 leaf), Technical Setup 2026-10-25b and Informant Pings PNG-R7:*

**Ownership split.** `FTeamPingRingModel` and the ring painters belong to the Informant Pings epic (`production/epics/informant-pings-team-comms/story-016-ring-model-and-age.md`, `story-017-ring-labels-and-label-drop.md`, `story-018-edge-ring-metrics-and-clamp.md`, `story-019-ring-painter-l0-leaf.md`, `story-020-edge-ring-painter-and-ui-scale.md`), planned at `Source/StealthPresentation/Private/StealthPingRingModel.h` and `StealthPingRingPainter.h`. Reuse those files. If they have not landed when this story starts, declare the model exactly as ADR-0021 Technical Setup 2026-10-25b specifies and let the pings stories adopt it; never keep two ring models. This story owns everything between the HUD step and the leaf: the ring slot hosted inside `SStealthWorldAnchorLeaf` (shell from story 011), the HUD-side lifecycle and the cross-surface rules below.

**Input path.** `UStealthHudPresenter` samples the ring entries every frame by value from the own controller's `UTeamPingViewComponent` (the ping part of C-TeamView, `FTeamPingView` entries: `SenderHandle`, `PingSeq`, point, `ServerTime`) through `FStealthClientViewHandles::PingView`; it never searches the world, never reads `UTeamPingService`, and works identically for the listen-server host (no RepNotify dependence, HUD-R1). The presenter core forwards them in `FHudInputs`; the ring model steps on `ServerNowEst` (`EstimatedServerNow()`, never a raw world clock).

**Ring rules (numbers).** `PingLifetimeSec` = 8 s (owned by Informant Pings; read through `GetClientTuning()`, never restated here). Display age is `max(0, a)` with `a = ServerNowEst - ServerTime`; a new entry with `a >= PingLifetimeSec` (age exactly equal included) is marked shown and never drawn. Stroke thirds are lower-inclusive: `a < L/3` gives 6 px, `a < 2L/3` gives 4 px, else 3 px, multiplied by `s_ui` once by the leaf; opacity is constant (never encode age in opacity). Ground ring: open ring with a 60 degree gap fixed in world space (north), world radius at least 4 m, screen minimum 32 px, Marigold, Obsidian keyline 3 px (4 px beyond 20 m), no centre mark, static. Edge ring: same broken ring at `W = H = max(32, round(32 · u · s_ui))` px clamped to the safe rectangle, static, unrotated, no label, arrival order with the newest on top, no de-overlap. Label: minimum 18 px Bone on an Obsidian plate with a 2 px Turquoise stroke, ellipsised at the plate width, never larger than the ring, own ping shows the own name, unresolved reads `Hud.Ally.Unresolved` ("Ally"). Timing: appears in 300 ms stepped when the echo arrives, exit 600 ms ease (stepped under Reduce screen effects, wired by story 052), one quiet 2D UI-bus tick per created ring. At most five rings (one per sender) and at most five labels.

**Lifecycle in the HUD step.** `Phase = Ended` hides the ring slot in the very `Step` that sees it (the ring slot flag in the view-model world-anchor cluster goes false and the dump shows the ring elements absent, not alpha 0); the model clears with no hold or fade on `Phase = Reset` and on travel start (HUD-R17 and the same instant as the markers' reset, teammate-markers `story-008-ally-marker-retired-event-ended-reset.md`); a HUD rebuild (story 044) sets `LastShownSeq := PingSeq` for every present entry, so there is no ring, no tick and no replay; a departed sender's ring expires normally; the L0 slot is collapsed with L0 while `GetViewState() == Neutral`. `OnAllyMarkerRetired(PublicHandle)` (TM-R14, native client-local delegate on the markers presenter, consumed in the same module) drops that sender's label at the same instant as its removal feed line (story 035), and a new ping from the same sender as an informant carries its label again.

**Traps.** A ping must never create a feed line, plate, card or counter, and the F8 queue and `FHudViewModel` get no ping type (the only ping-related plate is the owner-only concealed-refusal feedback of story 050, which is never shown for a received or echoed ping). The sender's own ring comes only from the server echo: `IA_Ping` creates no local ring or model state, so a dropped ping never flashes on the sender. Nothing may be added to a pawn (`UWidgetComponent` forbidden) and the ring may not be one widget per ring positioned by tick; it is drawn in `OnPaint` only. Edge rings never carry a label (art bible 7.5).

**Prototype disposition.** The prototype `AStealthHUD::DrawPings` and the `FStealthPing`/`Pings` data in `Source/StealthGame/Private/StealthPlayerController.cpp` are not promoted; their removal is pings story 019/027 and, for the canvas as a whole, story 059 (`production/epics/hud/story-059-hud-retire-prototype-canvas-and-message-plumbing.md`).

**Decisions that apply.** D20 (world-anchored markers and ping rings are exempt from the centre-empty rule), D25 (one ring per sender, replaced at once), D23 and D26 as above. Out of this story: the model and painters themselves, edge clamp metrics, the ping send path, the concealed-refusal plate and cue.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`: the leaf shell, its volatility and the UI-V1 dependency.
- `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`: the generic HUD-R17 reset and rebuild (this story only checks the ring part).
- `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`: the removal line and the `OnAllyMarkerRetired` timing of the label drop.
- `production/epics/hud/story-050-hud-ping-refusal-owner-only-pie.md`: the concealed-press refusal plate and cue.
- `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`: the stepped ring exit under Reduce screen effects.
- `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`: the full 2v2 screenshot set (ring with label and edge ring at 16:10).
- Informant Pings epic stories 016 to 021: the ring model, labels, edge metrics and painters.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/StealthPingRingModel.h` and `StealthPingRingModel.cpp` (declared by the pings stories 016 and 017; changed here only for the lifecycle hooks)
- `Source/StealthPresentation/Private/WorldAnchor/SStealthWorldAnchorLeaf.cpp` (ping slot beneath markers, collapsed with L0)
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (`Ended` hide, `Reset` and travel clear, rebuild marking, ring slot flag in the world-anchor cluster)
- `Source/StealthPresentation/Private/Hud/HudInputs.h` (ring entries sampled by value from `PingView`)
- `Source/StealthPresentation/Private/Tests/HUD/TeamPingRingWiringTest.cpp` (class `FHudTeamPingRingWiringTest`, name `StealthGame.HUD.TeamPingRingWiring`; the name is derived, the plan names none; uses a fake `FTeamPingView`)
- `Source/StealthTestHarness/Private/HUD/TeamPingRingsTest.cpp` (CQTest, `NETWORK_TEST_CLASS(TeamPingRings, "StealthGame.HUD.PIE")`, drives the retained captures)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: ring model (`StealthGame.HUD.TeamPingRingWiring`)
  - Given: `FTeamPingRingModel` with entries of varying age and sender.
  - When: entries arrive and a rebuild occurs.
  - Then: a ring is created only for a new `PingSeq` whose age `ServerNowEst - ServerTime` is below `PingLifetimeSec`. Older new entries are marked shown and never drawn. One ring per sender, and a new ping replaces it at once. One 2D UI-bus tick per created ring.
  - Edge cases: age exactly `PingLifetimeSec` is not drawn. Negative age clamps to 0.
- **AC-2**: weight, label, echo
  - Given: ring ages at thirds of `PingLifetimeSec`, a sender retire, an edge ring, the sender's own ping.
  - When: drawn.
  - Then: stroke 6/4/3 px at constant opacity. The label is the sender's `DisplayName` or "Ally", dropped at `OnAllyMarkerRetired` while the ring stays to expiry. Edge rings have no label. The sender's own ring comes only from the server echo.
  - Edge cases: third boundaries are lower-inclusive (age < L/3 gives 6 px).
- **AC-3**: lifecycle
  - Given: `Ended`, `Reset`, travel and a rebuild.
  - When: processed.
  - Then: rings hide in the `Step` that sees `Ended`. The model clears on `Reset` and travel start. A rebuild sets `LastShownSeq := PingSeq` for every present entry. No ping ever gets a feed line, plate or per-ping widget. Screenshots of a ring with its label and of an edge ring are retained.
  - Edge cases: blocked on spike 010. Use a fake `FTeamPingView` (the logic spec does not need the live pings component).

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the logic spec `Source/StealthPresentation/Private/Tests/HUD/TeamPingRingWiringTest.cpp` (`StealthGame.HUD.TeamPingRingWiring`) and the PIE test `Source/StealthTestHarness/Private/HUD/TeamPingRingsTest.cpp`, plus the retained screenshots `production/qa/evidence/hud-ping-ring-1080p.png` and `production/qa/evidence/hud-ping-edge-ring-1080p.png`
- Gate: BLOCKING (Integration default, no ADVISORY override in the plan)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`, `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`, `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`; Informant Pings epic ring stories `production/epics/informant-pings-team-comms/story-016-ring-model-and-age.md`, `story-017-ring-labels-and-label-drop.md`, `story-019-ring-painter-l0-leaf.md`, `story-020-edge-ring-painter-and-ui-scale.md` (`UTeamPingViewComponent`, `FTeamPingView`); teammate-markers epic (F3 clamp `story-011-edge-clamp-f3.md`, `OnAllyMarkerRetired` `story-008-ally-marker-retired-event-ended-reset.md`, TM-R14)
- Unlocks: `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`
