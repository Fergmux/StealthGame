# Epic: Teammate Markers

> **Layer**: Presentation
> **GDD**: design/gdd/teammate-markers.md
> **Architecture Module**: Teammate Markers in `StealthPresentation` (client-local marker set keyed by `PublicHandle`, anchors, fill states, name label)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories teammate-markers`
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
