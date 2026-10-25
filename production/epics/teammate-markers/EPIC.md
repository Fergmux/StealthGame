# Epic: Teammate Markers

> **Layer**: Presentation
> **GDD**: design/gdd/teammate-markers.md
> **Architecture Module**: Teammate Markers in `StealthPresentation` (client-local marker set keyed by `PublicHandle`, anchors, fill states, name label)
> **Status**: Ready
> **Stories**: 34 stories (23 Ready, 11 Blocked), see Stories below
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 18 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Teammate Markers is the client-local presentation system that draws one turquoise, downward-pointing pentagon above each of the viewer's teammates and nothing above anyone else. It is the only person-anchored element in the game before the round is decided (art bible 7.1 rule 1) and shows three states of the marked ally: filled (Active), hatched (concealed in a bush, where the ally sees no body) and outline-only (Informant), always on top at a constant pixel size and clamped to the screen edge when the ally is off-screen (F3 clamp, bias ramp and hysteresis; F4 edge spreading is cuttable). A reconnecting ally keeps its filled marker and only its name label gains a "reconnecting" suffix (TM-R11, MVP item 10).

It owns no replicated data. Its single input is the viewer's own C-TeamView (`{PublicHandle, State, PawnRef}` per teammate, the Concealment flag, team-only `RetireAtServerTime` and `bReconnecting`, and the Policy's quantised fallback feed when a `PawnRef` is unresolved), which the server copies only into teammates' owner-only controllers: an opponent's client never creates a marker because it never receives the data, which is enforced by absence rather than by hiding a widget. It never iterates world pawns, anchors on the resolved `PawnRef`'s visual root, gates on Camera `GetViewState`, and exposes the client-local `IsMarkedAlly(Pawn)` (used by IIF) and `OnAllyMarkerRetired(PublicHandle)`.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0021: UI/HUD layering, CommonUI and Enhanced Input | UMG layer/ZOrder stack drawn after post-process, Common UI for menus only, client-local settings hooks, ping rings in HUD L0. | HIGH |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

6 of 6 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-tmk-001 | Marker set only from the local C-TeamView (+ feed); no world iteration | ADR-0001 ✅ |
| TR-tmk-002 | Anchor on the resolved `PawnRef`'s visual root; null semantics | ADR-0021 ✅ |
| TR-tmk-003 | Fill states from C-TeamView state and phase (`RoundEnded` before `Ended` = no change) | ADR-0021 ✅ |
| TR-tmk-004 | Screen-space overlay after post-process, no depth test, constant size, edge clamp | ADR-0021 ✅ |
| TR-tmk-005 | `IsMarkedAlly` and `OnAllyMarkerRetired` client-local API | ADR-0021 ✅ |
| TR-tmk-006 | View gate through Camera `GetViewState` | ADR-0021 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** the prototype markers drawn on the canvas in `StealthHUD.cpp` and driven by `AStealthCharacter::IsTeammateOfLocalViewer()`, which reads a team field replicated on every pawn (the leak the first-playable report records as known issue 2: any client could mark every player). Prototype networked test E (teammate sees the marker, enemy sees an empty bush) is the behavioural reference. The production system is UMG screen-space and C-TeamView-sourced; nothing in the prototype is promoted.

## Build Order and Gates

- **Depends on (epics)**: participant-state-roster (C-TeamView), information-visibility-policy (team position fallback feed), camera-system (`GetViewState`), concealment-bushes (client latch `IsAllyBodyHidden`), tuning-data-config (client snapshot); hud (soft, `HudSafeMarginPct` and UI scale). Built after the HUD layer root; informant-pings-team-comms reads its F3 clamp and `OnAllyMarkerRetired`.
- **Gates and risks**: ADR-0021 conditional gate: UI-V1..UI-V4 and the HUD and Teammate Markers GDD re-reviews. UMG screen-space overlay after post-process with no depth test is HIGH risk (ZOrder vs camera fade `[U]`). Effort was raised from S to M by user decision 2026-10-25 (TM OQ12); the F4 edge spreading is cuttable. The `ShowTeamNames` hold action moved to Vertical Slice (OQ11); focus labels stay MVP. Carries the Markers part of MVP item 10 (name-label suffix TM-R11, about 0.5 d).
- **Estimate**: about 8 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 4.8 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 36 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/teammate-markers.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done

## Stories

Granularity `balanced` (2-4 acceptance criteria per story; the GDD's 34 acceptance criteria are split by sub-case, and criteria derived from an ADR where the GDD has none are labelled "derived"). Stories embed Control Manifest version 2026-10-25f (the manifest header's current version; this epic's header still says 2026-10-25e, and the Presentation rules this epic uses did not change between the two). Pure-math formula stories (F1, F3, F4, F5, F6, F7, F8) and the model stories are Logic and stay Ready. Story 018 is the ADR-0021 UI-V1 spike (zero-lag projection) that this epic owns; the painted stories (019, 020, 022, 026) are Blocked on it and on UI-V3/UI-V4, which the hud epic owns and which have not reported. Other blocked stories wait on ADR-0017 S-CON-1/R5 (021), the hud epic's UI-V3 (023), session-travel spikes S3 and S4 (025), ADR-0001 spike S1's full-parity FAIL (028), ADR-0003 spikes S9 and S10 (029, 030) and ADR-0022 R1-R3 (032). QL-STORY-READY: 20 ADEQUATE (002, 003, 004, 009, 012, 014, 016, 017, 019, 021, 023, 024, 025, 026, 029, 030, 031, 032, 033, 034), 14 GAPS (001, 005, 006, 007, 008, 010, 011, 015, 018, 020, 027, 028 revised in place per the agent's proposals and not re-run through the gate; 013 and 022 accepted as written because neither the GDD nor the UX spec fixes the label plate's width cap), 0 INADEQUATE. `TR-tmk-???` marks stories whose requirement has no registry entry (001, 006, 013, 022, 031, 033), and `TR-tmk-007` exists in the registry but is missing from this epic's GDD Requirements table above. Story 020's tester count (3 blinded testers) is a proposal. Evidence for UI and Visual/Feel stories is in `production/qa/evidence/` and cites `design/ux/hud.md` items (17) and (17b).

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [Marker tuning keys, constants and local-setting hooks](story-001-marker-tuning-keys-and-local-settings.md) | Config/Data | Ready | ADR-0021 |
| 002 | [Marker model, C-TeamView-only source and validation](story-002-marker-model-teamview-source-and-validation.md) (AC-TMK-08, 09) | Logic | Ready | ADR-0021 |
| 003 | [Fill precedence and RoundEnded freeze](story-003-fill-precedence-and-roundended-freeze.md) (AC-TMK-10) | Logic | Ready | ADR-0021 |
| 004 | [Anchor, fed position and null-reference interpretation](story-004-anchor-fed-position-and-null-ref.md) (AC-TMK-20, 30 model half) | Logic | Ready | ADR-0021 |
| 005 | [Fallback feed staleness](story-005-fallback-feed-staleness.md) (F5) | Logic | Ready | ADR-0021 |
| 006 | [Marker lifecycle, retire hold, takeover and direct pawn switch](story-006-lifecycle-retire-hold-takeover-direct-switch.md) (AC-TMK-12 a-e, 32) | Logic | Ready | ADR-0021 |
| 007 | [Retire contact alignment from RetireAtServerTime](story-007-retire-contact-alignment.md) (AC-TMK-12 f-j) | Logic | Ready | ADR-0021 |
| 008 | [OnAllyMarkerRetired once, retire at Ended, reset and travel clear](story-008-ally-marker-retired-event-ended-reset.md) (AC-TMK-31, 34) | Logic | Ready | ADR-0021 |
| 009 | [Fade, draw order and no motion of its own](story-009-fade-draw-order-no-motion.md) (AC-TMK-33) | Logic | Ready | ADR-0021 |
| 010 | [Marker size formula F1](story-010-marker-size-f1.md) (AC-TMK-19) | Logic | Ready | ADR-0021 |
| 011 | [Edge clamp F3](story-011-edge-clamp-f3.md) (AC-TMK-17) | Logic | Ready | ADR-0021 |
| 012 | [Edge de-overlap F4 (cuttable)](story-012-edge-de-overlap-f4.md) (AC-TMK-18) | Logic | Ready | ADR-0021 |
| 013 | [Label focus F8 and the reconnecting-suffix text](story-013-label-focus-and-reconnecting-text.md) (AC-TMK-23 logic, 26 text) | Logic | Ready | ADR-0021 |
| 014 | [IsMarkedAlly and the IAllyMarkerQuery interface](story-014-is-marked-ally-query.md) (AC-TMK-24) | Logic | Ready | ADR-0021 |
| 015 | [View-gate model](story-015-view-gate-model.md) (AC-TMK-16a model half) | Logic | Ready | ADR-0021 |
| 016 | [Presenter samples only the local controller](story-016-presenter-local-controller-only.md) (AC-TMK-07 instrumented half) | Integration | Ready | ADR-0021 |
| 017 | [Include and grep ban, no replicated fields, no pawn component](story-017-include-grep-ban-no-replicated-fields.md) (AC-TMK-03, 06) | Logic | Ready | ADR-0001 |
| 018 | [Spike UI-V1, zero-lag projection and tracking](story-018-spike-ui-v1-zero-lag-projection.md) (AC-TMK-22) | Visual/Feel | Ready | ADR-0021 |
| 019 | [Paint markers in the L0 world-anchor leaf](story-019-paint-markers-in-world-anchor-leaf.md) (AC-TMK-14) | UI | Blocked | ADR-0021 |
| 020 | [Glyph states and edge variant evidence](story-020-glyph-states-and-edge-variant-evidence.md) (AC-TMK-21) | Visual/Feel | Blocked | ADR-0021 |
| 021 | [Hatch on the body-hide frame](story-021-hatch-on-body-hide-frame.md) (AC-TMK-11) | Visual/Feel | Blocked | ADR-0017 |
| 022 | [Name label plate rendering](story-022-name-label-plate-rendering.md) (AC-TMK-23 UI) | UI | Blocked | ADR-0021 |
| 023 | [Layer order and view gate with the fade](story-023-layer-order-and-view-gate-with-fade.md) (AC-TMK-16a PIE half) | Integration | Blocked | ADR-0021 |
| 024 | [Ended and Reset](story-024-ended-and-reset.md) (AC-TMK-16b) | Integration | Ready | ADR-0021 |
| 025 | [Travel and viewer-rejoin rebuild](story-025-travel-and-viewer-rejoin-rebuild.md) (AC-TMK-25a, 25b) | Integration | Blocked | ADR-0021 |
| 026 | [Host parity of the marker set](story-026-host-parity-of-marker-set.md) (AC-TMK-07 serialised half) | Integration | Blocked | ADR-0021 |
| 027 | [Leak audit, no opponent marker objects or canaries](story-027-leak-audit-marker-objects-and-canaries.md) (AC-TMK-01, 05, 28) | Integration | Ready | ADR-0001 |
| 028 | [Leak audit, reflection dump and packet capture](story-028-leak-audit-reflection-and-packet-capture.md) (AC-TMK-02, 04) | Integration | Blocked | ADR-0001 |
| 029 | [Feed handoff and null-reference semantics on a real connection](story-029-feed-handoff-and-null-ref-on-real-connection.md) (AC-TMK-15, 30) | Integration | Blocked | ADR-0003 |
| 030 | [Informant return timing](story-030-informant-return-timing.md) (AC-TMK-13) | Integration | Blocked | ADR-0021 |
| 031 | [Ally disconnect leaves the marker unchanged](story-031-ally-disconnect-marker-unchanged.md) (AC-TMK-26) | Integration | Ready | ADR-0021 |
| 032 | [Performance budget](story-032-performance-budget.md) (AC-TMK-27, ADVISORY) | Integration | Blocked | ADR-0021 |
| 033 | [Marker attention playtest](story-033-attention-playtest.md) (AC-TMK-29, ADVISORY) | Integration | Ready | ADR-0023 |
| 034 | [Retire the prototype marker drawing](story-034-retire-prototype-marker-drawing.md) | Integration | Ready | ADR-0021 |
