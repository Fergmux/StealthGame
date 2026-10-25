# Story 022: Downtime and residual telemetry report from internal matches

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§7 Event Log and telemetry hooks), with ADR-0023 (playtest telemetry hooks; export of `PostRoundEligible` rows)
**ADR Decision Summary**: Penalty writes server-only `PostRoundEligible` Event Log rows (`StrikeCounted`, `RespawnScheduled`, `InformantRespawn`, `RespawnCancelled`, `PawnLost`); LSC writes `RespawnStarved`; Round writes `StrikeLimit` and the `KillResolved` rows. The telemetry export (ADR-0023) reads those rows after a round; nothing passes through `UE_LOG` or a client channel.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API: the aggregation is an offline script or editor-side tool over the exported rows. ADR-0023's own memory and export constraints apply (telemetry memory budget 1.6 MB per ADR-0023/manifest 2026-10-25f).

**Control Manifest Rules (this layer)**:
- Required: telemetry reads only `PostRoundEligible` Event Log rows through the ADR-0023 export — source: ADR-0023, ADR-0019
- Forbidden: no telemetry or log line through `UE_LOG`/client channels from Penalty; the report gates nothing — source: ADR-0007, GDD AC-NKP-23 ("report-only")
- Guardrail: the export stays inside ADR-0023's memory budget — source: ADR-0023

This is the playtest calibration story: the GDD's balance questions (OQ-NKP-3: is a 15 s respawn right; how often do players reach tally L without exile; how often does the informant share of `WrongTarget` victims exceed 2 × q) can only be answered with data. The report is advisory: it never blocks a build or a story.

**Prototype replaced:** none (the prototype has no telemetry; the first-playable report was written by hand from 3-player bot runs).

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-23) and ADR-0019 §7 / ADR-0023 (derived item marked), scoped to this story:*

- [ ] **AC1** (AC-NKP-23, [Playtest/ADVISORY, report-only] downtime and residual telemetry): **GIVEN** internal matches until at least 200 informant respawns and 30 exiles are logged, or a match cap (proposed: 40 matches; the first checkpoint at about 10 matches is reported but not judged) is reached, whichever comes first **WHEN** Event Log rows are aggregated **THEN** the report gives median and p95 `T_black` against the F3 budget (≤ 30 s, ≤ 40 s), the share of Active players with tally ≥ L and no exile against F4 (to estimate `b`), the informant share of `WrongTarget` victims against 2 × q (F5), graced versus after-grace kills of released bodies, and body-blocking incidents (OQ-NKP-3; a body-blocking incident is an observer-form entry, with the form defined in the playtest protocol, for an informant or released pawn standing in a passage of one pawn width or less and obstructing a player's path), saved as a retained file under `production/qa/`.
- [ ] **AC2** (derived, ADR-0019 §7 and ADR-0023): **GIVEN** a finished internal match **WHEN** the aggregation reads the Event Log rows through the ADR-0023 export **THEN** every report field except body-blocking incidents is computed from `PostRoundEligible` rows (`T_black` from `RespawnScheduled` and `InformantRespawn`; tally ≥ L without exile from `StrikeCounted`; the informant share from `KillResolved`; graced from `StrikeCounted.Graced`), body-blocking incidents come from the observer log or questionnaire, the report is report-only and gates nothing.

---

## Implementation Notes

*Derived from GDD AC-NKP-23, F3-F5 and OQ-NKP-3/13:*

Build the aggregation as a small tool (a script under `tools/` or an editor commandlet in `StealthGameEditor`; choose per ADR-0023's export format) that reads the exported rows of each match and produces the report. Definitions:
- `T_black` = time from the participant's loss to the takeover `Applied` for each respawn (`InformantRespawn.PickLatencySec` plus the delay `T_due - t_lost`): the F3 budget compares the total black-screen time.
- F4 residual: the share of Active players whose tally reaches L without an exile (`StrikeCounted.TallyAfter >= L` with `StrikeAfter < L`) estimates `b`, the informant-or-graced share, against the F4 table.
- F5: the informant share of `WrongTarget` victims (from `KillResolved.VictimKind` rows) against 2 × q where q is the informant share of the population.
- Graced versus after-grace kills of released bodies from `StrikeCounted.Graced` (OQ-NKP-2/3: if after-grace kills are frequent, the exit timing is reviewed, never the grace length).
- Respawn-region informant share versus city-wide share (OQ-NKP-13; escalate to the creative director at > 2 × on ≥ 10 matches).
- Body-blocking incidents (OQ-NKP-3): collected by playtest observers or a questionnaire item: Event Log has no row for it. Record which test groups had voice (OQ-NKP-8).

This story's deliverable is the tool, a dry-run on synthetic rows (so it is verified before real matches exist) and the report format. The real report is filled when the playtests have run: leave the report file `production/qa/penalty-telemetry-[date].md` as the evidence slot with the first-checkpoint section marked "not yet run". It is advisory: no gate, no blocked story. Needs no new Event Log rows beyond story 004's.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-event-log-rows.md`: the rows themselves.
- ADR-0023's telemetry export and hooks (their own epic or stories).
- Changing any tuning default based on the report: that is a design decision for the game-designer and creative director, not this story.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/telemetry/penalty_report.py` (or the commandlet named by the ADR-0023 stories)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyTelemetryReportTest.cpp` (class `FPenaltyTelemetryReportTest`, name `StealthGame.Penalty.TelemetryReport`, only if the aggregation is C++; otherwise the script's own synthetic-row self-test)
- `production/qa/penalty-telemetry-[date].md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-23) Playtest report (retained file under `production/qa/`)
  - Given: internal matches until at least 200 respawns and 30 exiles, or the 40-match cap, whichever comes first; a checkpoint at about 10 matches is reported but not judged.
  - When: the Event Log rows are aggregated.
  - Then: the report states n for each figure, the stop reason, median and p95 `T_black` against F3 (<= 30 s, <= 40 s), the share of Active players with tally >= L and no exile (F4), the informant share of `WrongTarget` victims against 2 x q (F5), graced against after-grace kills, and the body-blocking count (an observer-form entry as defined in AC1).
  - Edge cases: report-only, nothing fails on its numbers.
- **AC2** (derived, `StealthGame.Penalty.TelemetryReport`)
  - Given: a finished-match export through ADR-0023 as a fixture, plus a seeded non-`PostRoundEligible` row.
  - When: the aggregation runs.
  - Then: `T_black = (T_due - t_lost) + PickLatencySec` from `RespawnScheduled` and `InformantRespawn`; tally >= L without exile from `StrikeCounted`; the informant share from `KillResolved`; graced from `StrikeCounted.Graced`; the seeded row changes nothing; body-blocking comes from the observer log; the aggregation gates nothing.
  - Edge cases: an empty export, a match with zero respawns, rounds with different L.

QL-STORY-READY: ADEQUATE after the AC1 rewrite (body-blocking definition) was applied; the rewrite was the QA lead's own and was not re-reviewed.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: a documented playtest: the retained report `production/qa/penalty-telemetry-[date].md` and the tool's synthetic-row self-test

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-004-event-log-rows.md`, `story-012-respawn-commit-sequence.md`, `story-005-exile-sequence-in-attack-slot.md`; cross-epic: ADR-0023 telemetry export (telemetry hooks stories), round-match-framework `story-011-event-log-content.md`
- Unlocks: None
