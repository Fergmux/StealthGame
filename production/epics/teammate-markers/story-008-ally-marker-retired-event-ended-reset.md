# Story 008: OnAllyMarkerRetired once, retire at Ended, reset and travel clear

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The HUD times an ally's removal line at the later of the `State` change's arrival and the marker's retirement, so the marker module publishes one client-local event, `OnAllyMarkerRetired(PublicHandle)`, exactly once per retirement of a teammate entry's binding: at the hold end (`t_end`) when the exit fade starts (also when the marker was not drawn, such as `Stale` or a closed view gate), and immediately at `t_r` when no marker existed for the handle. It never fires at `Reset`, at seamless travel, or when a hold is cancelled. Two lifecycle cut-offs sit here too: once Round's phase is `Ended` a marker in `Held` or `Exiting` completes its exit at once (and a retirement first detected in `Ended` exits at once), keeping every other marker's frozen fill; and at `Reset` or travel start every marker goes to `Absent` on the same frame with no hold and no fade.

**GDD**: `design/gdd/teammate-markers.md` (TM-R14, TM-R6 steps 5 and 6)
**Requirement**: `TR-tmk-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 5)
**ADR Decision Summary**: `OnAllyMarkerRetired(PublicHandle)` is a native client-local delegate on the markers presenter, consumed by the HUD presenter in the same module (HUD-R10); never replicated, never called by server code. The model is cleared with no hold or fade on `Phase = Reset` and on travel start.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: A plain C++ multicast delegate (not a `DYNAMIC` one) so no reflection or replication path exists; tests subscribe a spy.

**Control Manifest Rules (this layer)**:
- Required: `OnAllyMarkerRetired` is a client-local event consumed by the HUD in the same module — source: ADR-0021
- Forbidden: Never replicate it or call it from server code; never write anything replicated from Presentation — source: ADR-0021, ADR-0001
- Guardrail: Exactly once per retirement (an overcount would duplicate a HUD removal line)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-31, AC-TMK-34, TM-R6 steps 5 and 6, TM-R14), scoped to this story:*

- [ ] **AC1** (AC-TMK-31, [U] `RetiredEventOnce`): **GIVEN** retirements (a) with a drawn marker, (b) with the view gate `Neutral`, (c) with no marker existing for the handle, (d) cancelled by the same pawn returning, (e) at `Reset` and at travel **WHEN** `OnAllyMarkerRetired` is observed **THEN** it fires exactly once at `t_end` in (a) and (b) (`t_r + T_hold`, later for a bush strike), exactly once at `t_r` in (c), and never in (d) or (e).
- [ ] **AC2** (AC-TMK-34, [U] `RetireAtEnded`, revised): **GIVEN** (a) a marker in `Held`, (b) a marker in `Exiting`, (c) a retire update for another ally arriving 0.1 s after the phase became `Ended` **WHEN** the phase arrives (or the update lands) **THEN** in each case no marker remains for that handle on that frame (no remaining hold, no fade), a spy subscriber receives the event exactly once, and every other marker keeps its frozen last fill. *(The HUD's "creates no line" half is the hud epic's AC-HUD-30.)*
- [ ] **AC3** (TM-R6 step 5, TM-R14, derived): **GIVEN** Round phase `Reset` or the start of a seamless travel **THEN** every marker goes to `Absent` on the same frame with no hold and no fade, and the event is a native client-local delegate that is never replicated and never called by server code (checked by reflection and a grep).

---

## Implementation Notes

*Derived from TM-R14, TM-R6 and ADR-0021 §5:*

Fire at `t_end` (story 007) when the exit fade starts, whether or not the marker is drawn (view gate `Neutral`, `Stale`); fire immediately at `t_r` when no marker object existed for the handle at detection; do not fire on hold cancel, `Reset` or travel. Track a per-retirement "fired" flag so the event cannot fire twice (an `Ended` cut-off after the normal `t_end`, for example). On `Ended`: for markers in `Held` or `Exiting` complete the exit now, fire the event once, and leave all other markers frozen in their last fill (markers are not all hidden at `Ended`; only a retiring marker is cut short). The HUD drops that ally's removal line at `Ended` (HUD-R10) because `FRoundResult.TeamSummary` already counts the ally as eliminated; that is HUD's behaviour, asserted in AC-HUD-30.

Reset and travel: the bootstrap rebinds when a new local controller arrives (ADR-0005 replaces controllers at travel), and the model is cleared with no hold on `Phase = Reset` and on travel start; subscribe to Round's public state (`FRoundPublicState` phase) and the travel-start notification (session-travel).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-retire-contact-alignment.md`: computing `t_end`.
- `story-024-ended-and-reset.md` and `story-025-travel-and-viewer-rejoin-rebuild.md`: the multi-client PIE versions.
- the hud epic: the removal line itself (HUD-R10, AC-HUD-30).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerRetireEvents.h` and `MarkerRetireEvents.cpp`
- `Source/StealthPresentation/Private/Markers/TeammateMarkerModel.cpp` (phase and travel inputs)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerRetiredEventTest.cpp` (names `StealthGame.Markers.RetiredEventOnce`, `StealthGame.Markers.RetireAtEnded`, `StealthGame.Markers.ResetAndTravelClear`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: retirements (a) with a drawn marker, (b) with the view gate `Neutral`, (c) with no marker for the handle, (d) cancelled by the same pawn returning, (e) at `Reset` and at travel. When: the delegate is observed. Then: exactly one call at `t_end` in (a) and (b) (`t_r + T_hold`, later for a bush strike), exactly one at `t_r` in (c), none in (d) and (e). Edge cases: (a) with a bush-strike `RetireAtServerTime`.
- **AC2**: Given: (a) `Held`, (b) `Exiting`, (c) a retire update 0.1 s after `Ended`, with other markers present. When: the phase arrives or the update lands. Then: no marker remains for the handle on that frame, a spy subscriber receives exactly one call, and other markers keep their frozen fills. Edge cases: the `Ended` cut-off after the normal `t_end` already fired (no second call).
- **AC3**: Given: Round phase `Reset`, then the start of a seamless travel, with several markers. When: it occurs. Then: every marker is `Absent` on the same frame with no hold or fade, and the delegate is native, client-local, never replicated and never called by server code (lint or reflection). Edge cases: markers in `Held` and `Exiting` at that moment.

QL-STORY-READY: GAPS, revised in place per the agent's proposal (AC2 observed through a spy; the HUD clause moved to the hud epic); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerRetiredEventTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-006-lifecycle-retire-hold-takeover-direct-switch.md`, `story-007-retire-contact-alignment.md`; cross-epic `production/epics/round-match-framework/story-029-public-state-replication.md` (the phase)
- Unlocks: `production/epics/teammate-markers/story-024-ended-and-reset.md`, `story-025-travel-and-viewer-rejoin-rebuild.md`; the hud epic's removal-line stories (HUD-R10)
