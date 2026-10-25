# Story 033: Marker attention playtest

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The marker is deliberately small and quiet, but the creative-director review (CD-GDD-ALIGN item 4) asked a question that no automated test can answer: do players watch the crowd or their allies' markers? Markers could dominate attention, read as a radar, or cluster the team so tightly that clustering itself becomes a Pillar 2 tell. This story is the advisory playtest that answers it during the first blending playtest, with development-only telemetry and a development-only console variable that caps edge markers by distance for an A/B; the escalation rule routes the opt-in "hide markers except on hold" option (OQ5) and an edge-marker distance cap to the creative-director. Nothing here ships: the telemetry hooks and the console variable compile out of shipping and are not tuning keys.

**GDD**: `design/gdd/teammate-markers.md` (Tuning Knobs deliberately absent list, Open Questions OQ5; AC-TMK-29)
**Requirement**: `TR-tmk-???` (the tone and attention check has no registry entry)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0023: Playtest telemetry hooks (Accepted; the telemetry section lives in the Event Log store)
**Also governed by**: ADR-0021 (local-only presentation, nothing replicated)
**ADR Decision Summary**: Playtest telemetry is development-only, written through the Event Log and fault-sink facilities, and compiled out of shipping builds.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Telemetry is per-player client-side logging of three quantities derived from the marker model and the camera; the console variable is registered under `#if !UE_BUILD_SHIPPING` and is read by the leaf only to cap edge markers for the A/B session.

**Control Manifest Rules (this layer)**:
- Required: Each owner's telemetry goes through the Event Log hooks of ADR-0023 and is compiled out of shipping — source: ADR-0023
- Forbidden: Never ship the edge-marker distance cap or any distance fade or opacity state; adding one is a creative-director change (Pillar 1, art bible 3.4) — source: GDD Tuning Knobs, art bible 3.4
- Guardrail: Telemetry never reaches the server or other clients

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-29), scoped to this story:*

- [ ] **AC1** (AC-TMK-29, [Playtest] `MarkerAttentionCheck`): **GIVEN** the first blending playtest (at least 6 matches, observers and post-round interviews) **THEN** the playtest report in `production/qa/` states the share of rounds in which players say they watched markers rather than the crowd (fewer than 6 matches is recorded as incomplete).
- [ ] **AC2** (AC-TMK-29): **GIVEN** development telemetry **THEN** it logs per player (i) the share of round time an ally marker is within 15 % of screen height of the centre, (ii) the share of time edge markers are shown, and (iii) the median ally pair distance compared with a no-marker baseline session; the telemetry hooks and a development-only console variable that caps edge markers by distance are compiled out of Shipping (checked on a Shipping configuration) and are not tuning keys.
- [ ] **AC3** (AC-TMK-29): **GIVEN** results where markers dominate (more than 1 in 3 rounds), any player calls them "radar" or "minimap", or the median ally pair distance falls by more than 25 % against the baseline **THEN** the report routes OQ5's opt-in "hide markers except on hold" and an edge-marker distance cap (tested with the development-only console variable) to the creative-director for a decision.

---

## Implementation Notes

*Derived from AC-TMK-29 and OQ5:*

Telemetry: per player per round log `ally_marker_center_share` (time with any ally marker within 15 % of screen height of the centre, divided by round time), `edge_marker_share`, and `ally_pair_median_distance_cm` (the median, over the round, of the distance between each pair of teammates' pawns on that player's team), written through the ADR-0023 hooks. Baseline session: the same pairing with markers disabled by a development-only switch, run once per cohort. The development-only console variable (for example `stealth.Markers.DevEdgeDistanceCapM`, 0 = no cap) caps edge markers by distance for the A/B; it is registered under `#if !UE_BUILD_SHIPPING`.

Report template: use `/playtest-report`. Observers record whether gaze and decisions follow the crowd or the markers; interviews ask the "radar" and "minimap" question without leading. Escalation thresholds are the three in AC3 (exactly 1 in 3 and exactly 25 % are not escalations).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Vertical Slice: the opt-in "hide markers except on hold" setting itself (OQ5 stays a decision for the creative-director).
- the informant-pings-team-comms epic: ping attention.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerTelemetry.h` and `MarkerTelemetry.cpp` (development-only, `#if !UE_BUILD_SHIPPING`)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerTelemetryShippingTest.cpp` (name `StealthGame.Markers.TelemetryCompiledOut`, class `FMarkersTelemetryCompiledOutTest`)
- `production/qa/playtest-markers-<date>.md` (the report)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: the first blending playtest (6 or more matches, observers, interviews). When: the report is written. Then: it states the share of rounds in which players say they watched markers rather than the crowd. Edge cases: fewer than 6 matches is recorded as incomplete.
- **AC2**: Given: development telemetry. When: a session runs. Then: it logs per player the share of time an ally marker is within 15 % of screen height of centre, the share of time edge markers are shown, and the median ally pair distance versus a no-marker baseline; the hooks and the distance-cap console variable are compiled out of Shipping and are not tuning keys (check on a Shipping configuration). Edge cases: no baseline session.
- **AC3**: Given: the playtest results. When: markers dominate (over 1 in 3 rounds), any player says "radar" or "minimap", or the median pair distance falls by more than 25 %. Then: the report routes OQ5's "hide markers except on hold" and an edge-marker distance cap to the creative-director. Edge cases: exactly 1 in 3 and exactly 25 %.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration (ADVISORY gate)
**Required evidence**:
- Integration: the playtest report `production/qa/playtest-markers-<date>.md` (a documented playtest), plus `MarkerTelemetryShippingTest.cpp` for the Shipping check

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-019-paint-markers-in-world-anchor-leaf.md` (a drawn marker to observe); cross-epic the ADR-0023 telemetry hook stories (not yet located in an epic; ADR-0023 is Accepted)
- Unlocks: None
