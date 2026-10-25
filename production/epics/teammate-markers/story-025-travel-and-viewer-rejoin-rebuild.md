# Story 025: Travel and viewer-rejoin rebuild

> **Epic**: Teammate Markers
> **Status**: Blocked — BLOCKED: session-travel spikes S3 (seamless-travel persistence) and S4 (rejoin PlayerState adoption) have not reported (`production/epics/session-travel/story-003-spike-s3-seamless-travel-persistence.md`, `story-004-spike-s4-rejoin-playerstate-adoption.md`); seamless-travel evidence is Gauntlet-only (ADR-0008).
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

Seamless travel and a viewer's own rejoin both replace the client's controller and invalidate every pawn binding. Markers must not persist across either. At travel start every marker goes to `Absent` immediately with no hold and no fade (no pawn exists to hold on), the fallback feed is not sent because no pawn exists, and the set is rebuilt only from the re-pushed C-TeamView; markers appear only once `GetViewState()` is `Live` in the new round. A viewer who disconnects and rejoins mid-round is in the neutral view until its own binding; `RebindView` re-pushes its C-TeamView, the marker set is rebuilt from it, and every marker fades in together when the view goes live. This story is the multi-process verification of both.

**GDD**: `design/gdd/teammate-markers.md` (TM-R6 step 5, TM-R8, Edge Cases connection, travel and phase; AC-TMK-25a, AC-TMK-25b)
**Requirement**: `TR-tmk-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 5: Reset/travel)
**Also governed by**: ADR-0005 (session, seamless travel and match-scoped state stores; replaces controllers at travel; Accepted)
**ADR Decision Summary**: The bootstrap is re-bound when a new local controller arrives; the model is cleared with no hold or fade on `Phase = Reset` and on travel start.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Seamless travel cannot be proven in single-process PIE; use the Gauntlet multi-process node (tuning-data-config story 020; ADR-0008 Gauntlet-only evidence). `NotifyLoadedWorld` and the seamless-travel controller swap are engine behaviours that the session-travel spikes S3 and S4 characterise.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers; the bootstrap re-binds on a new local controller — source: ADR-0021, ADR-0005
- Forbidden: Never let a marker persist across travel or a controller swap; never attach to a previous-round pawn — source: GDD TM-R6 step 5
- Guardrail: Gauntlet nodes are named `StealthGame.Markers.MP.<Scenario>` (test-standards)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-25a, AC-TMK-25b), scoped to this story:*

- [ ] **AC1** (AC-TMK-25a, [PIE-2] `TravelRebuild`): **GIVEN** a match moving to its next round by seamless travel **WHEN** the controller swap and re-push complete **THEN** every marker was removed at travel start with no hold, no fade and no `OnAllyMarkerRetired`, the set is rebuilt only from the re-pushed view, no marker attaches to a pawn of the previous round, and markers appear only once `GetViewState()` is `Live`.
- [ ] **AC2** (AC-TMK-25b, `ViewerRejoinRebuild`): **GIVEN** a viewer who disconnects and rejoins mid-round **WHEN** `RebindView` re-pushes its C-TeamView **THEN** the marker set equals the server's teammate list, every marker fades in together when the view goes live, and no marker from before the disconnect persists.

---

## Implementation Notes

*Derived from TM-R6 step 5 and the Edge Cases:*

Drive a Gauntlet node with a host, a same-team client and a second ally. Travel: record the model each frame from just before travel start through the first `Live` frame of the new round; assert zero markers at travel start, zero `OnAllyMarkerRetired` calls, and that every marker pawn reference in the new round is a new-round pawn. Rejoin: kill the viewer's connection, rejoin within the grace, and assert the set equals the host's teammate list (handles, states) and that all markers share one entrance fade start. Edge cases: the re-push arriving before the view goes `Live` (markers wait); an ally whose pawn is gone at the viewer's rejoin (no marker until a valid entry exists).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-008-ally-marker-retired-event-ended-reset.md`: the unit-level clear.
- `participant-state-roster/story-041-team-view-repush-on-controller-swap.md` and `story-058-mp-travel-persistence.md`: the Roster side of the re-push.
- `session-travel` stories: the travel and rejoin mechanism itself.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Markers/TravelAndRejoinRebuildTest.cpp` (Gauntlet node `StealthGame.Markers.MP.TravelRebuild` and CQTest `StealthGame.Markers.PIE.ViewerRejoinRebuild.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: a seamless travel to the next round. When: the controller swap and re-push complete. Then: every marker was removed at travel start with no hold, fade or `OnAllyMarkerRetired`; the set is rebuilt only from the re-pushed view; no marker attaches to a previous-round pawn; markers appear only once `GetViewState()` is `Live`. Edge cases: the re-push arrives before the view goes `Live`.
- **AC2**: Given: a viewer disconnecting and rejoining mid-round. When: `RebindView` re-pushes the C-TeamView. Then: the marker set equals the server's teammate list, all fade in together at `Live`, and none from before the disconnect persists. Edge cases: an ally's pawn is gone at rejoin.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the Gauntlet and CQTest nodes in `TravelAndRejoinRebuildTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-008-ally-marker-retired-event-ended-reset.md`, `story-016-presenter-local-controller-only.md`; cross-epic `production/epics/session-travel/story-003-spike-s3-seamless-travel-persistence.md`, `story-004-spike-s4-rejoin-playerstate-adoption.md`, `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: None
