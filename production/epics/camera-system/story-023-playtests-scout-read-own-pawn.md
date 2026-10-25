# Story 023: Playtests: scout parity, read parity and own-pawn finding

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md` (Open Questions OQ2, OQ11)
**Requirement**: `TR-camera-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (Validation Criteria: playtests AC-CS-25, AC-CS-26)
**ADR Decision Summary**: The third-person camera must show nothing the body's own eyes could not see. The playtests check the outcome of that rule on people: 3P is not an advantage for scouting (paired detection time), 1P and 3P read the crowd equally well (≤ 15 % relative gap in either direction), and a new player can find their own pawn in a crowd of same-card pawns without decoration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; this story runs humans on the built mask and rig)
**Engine Notes**: Needs the graybox corner scenario and a 5v5 graybox with the 60-character pool. Logs include visible-neighbour counts within 3 m in the 3P frame and in the 1P frame, and glance counts under 0.2 s (OQ2 fallback trigger).

**Control Manifest Rules (this layer)**:
- Required: Playtest protocol is the GDD's (paired design, counterbalanced order, stated statistics) — source: GDD AC-CS-24/25/26
- Forbidden: None specific; a result that triggers OQ2/OQ5 fallbacks or an OQ11 default-mode change goes to the user, never auto-applied
- Guardrail: With fewer than 12 testers the AC-CS-24 result is advisory only

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-24, AC-CS-26, AC-CS-25), scoped to this story:*

- [ ] **AC1** (AC-CS-24, [PT] `ScoutParity`): **GIVEN** the graybox corner scenario with 12 or more testers each playing both modes in counterbalanced order (paired design) **WHEN** they report a walker approaching from behind a corner **THEN** the paired difference in detection time (3P minus 1P) is not significantly negative (one-sided Wilcoxon signed-rank, α = 0.05); with fewer than 12 testers the result is advisory only.
- [ ] **AC2** (AC-CS-26, [PT] `ReadParity`, two-sided): **GIVEN** 12 testers split between 1P and 3P and swapped after one session, playing the graybox 5v5 for at least 6 rounds per mode **WHEN** correct identifications of enemy players and strikes on wrong targets (NPC kills plus reversed strikes) per round are compared **THEN** the two modes differ by ≤ 15 % relative on both measures in either direction; if 3P is ahead by more, the OQ2 fallback and the OQ5 partial-masking fix are triggered; if 1P is ahead by more, the mask's false-hide budget and neighbour visibility are reviewed and changing the default mode goes to the user (OQ11); the run also logs the number of visible neighbours within 3 m in the 3P frame versus 1P.
- [ ] **AC3** (AC-CS-25, [PT] `OwnPawnFinding`): **GIVEN** 6 or more new players in 3P in a crowd of same-card pawns, over at least 20 events in total (spawn, takeover, leaving an occluder, returning from a look beyond +10° pitch) **WHEN** each player locates their own pawn by pointing on screen **THEN** the 90th-percentile time is ≤ 3 s and the median is ≤ 2 s, and no own-pawn decoration exists.

---

## Implementation Notes

*Derived from the GDD AC section (PT tags) and `.claude/skills/playtest-report`:*

Write the test protocol and the report in `production/qa/playtests/` using the project's `/playtest-report` template; keep raw per-tester data as CSV beside it. Instrument the build with a Shipping-excluded logger for detection time, identifications, wrong-target strikes, neighbour counts and glance counts; the logger must not send anything to the server and must not log other players' identity (Information Visibility Policy). Analysis scripts live in `tools/` or inline in the report; the Wilcoxon test and the relative-gap computation are stated in the report.

This is a documented playtest, not an automated test; `/story-done` accepts the signed-off report as evidence for an Integration story. Recruit and schedule the testers outside the codebase (a user decision); the story stays Ready because everything it needs is specified, and its dependencies gate when it can start.

Prototype replaced: none. Earlier prototype playtests (`production/first-playable-report.md`) are not evidence for these criteria.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The built features under test: stories 008, 009, 013, 015, 016.
- Fixes that a failing result triggers (OQ2 `ViewHeadLeadMaxDeg`, OQ5 partial masking): new stories, decided by the user.
- character-pool-appearance OQ-6 own-pawn design decisions beyond measuring.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/playtests/camera-parity-playtest-protocol.md`
- `production/qa/playtests/camera-parity-playtest-report.md` (and the CSV data)
- `Source/StealthGame/Private/Telemetry/CameraPlaytestLogger.cpp` (Shipping-excluded, local-only logger)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS (documented playtests, deferred until the graybox build and 12 or more human testers exist)
- AC2: "differ by ≤ 15 % relative" has no denominator, no zero handling and no variance rule; at 12 testers × 6 rounds the counts are noisy and wrong-target strike counts can be near 0. QA-lead proposed rewrite (unconfirmed): relative difference = |m3P − m1P| / the 1P mean; where the 1P mean is below 1 per round use an absolute difference; the author may add a confidence interval.
- Manual check AC1: at least 12 testers play both modes in counterbalanced order (6/6) on the graybox corner scenario; detection time = report keypress minus the first logged frame with an unblocked eye ray to the walker (proposed definition); one-sided Wilcoxon signed-rank test on (3P − 1P), H1 negative, α = 0.05. Pass: p ≥ 0.05; fewer than 12 testers is advisory only; also report the median difference and confidence interval (non-significance at n = 12 is weak evidence of parity).
- Manual check AC3: at least 6 players in 3P in a same-card crowd, at least 20 events in total; time from event to the player's on-screen point on their own pawn, logged by hit test. Pass: P90 ≤ 3 s and median ≤ 2 s over the pooled events (also report per player); an art and code audit confirms no own-pawn decoration (outline, ring or marker).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: a documented playtest report in `production/qa/playtests/` with the stated statistics

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-009-first-person-own-head-hide.md`, `story-015-mask-apply-path.md`, `story-016-aim-ray.md`, `story-021-camera-mode-not-observable.md`; cross-epic: level-socket-contract (graybox map), character-pool-appearance (pool deal), round-match-framework (5v5 rounds)
- Unlocks: closing OQ2, OQ5, OQ11 and Character Pool OQ-6
