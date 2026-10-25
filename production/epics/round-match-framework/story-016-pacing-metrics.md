# Story 016: PacingRatio, ObjectiveEndShare, AbortedActiveShare and stall/abort flags compute per F7 from fixture logs

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-012` (Playtest instrumentation: brace timing, run share, final camping, team-size bucket, in the Event Log). Also `GDD AC 35`, `F7` (pacing metrics, logged only, never a rule).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0023: Playtest telemetry hooks
**ADR Decision Summary**: A bounded, server-only `NeverReleased` telemetry section of the Round Event Log store, with kind-blind probes and detectors, exported after termination off the game thread. The decision this story applies is section 3: Round's own F7 values at `Ended` (`DurationSec`, `PacingRatio`, `StallFlag`, `EndReason`) are copied from the R19 header and entries, and `ObjectiveEndShare` and `AbortedActiveShare` across rounds, per team-size bucket, are derived offline by the analysis tool. The per-round computation must therefore be a pure function over log data.
**ADR Version**: ADR-0023 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: ADR-0023 lists no post-cutoff APIs. Its verification items TEL-V1 (export off the game thread), TEL-V2 (zero leakage) and TEL-V3 (detector parity) belong to the telemetry export and detector stories, not to this pure computation. This story touches plain C++ arithmetic only. This story feeds the playtest instrumentation (ADR-0023) and the Pillar 4 gate of at least 0.70 `ObjectiveEndShare` over at least 20 rounds in buckets of 4v4 and larger.

**Control Manifest Rules (this layer)**:
- Required: Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct; a hardcoded gameplay literal is a defect (ADR-0004). `StallFlagMinutes` (default 18) and `TargetRoundMinutes` (default 12) are inputs, never literals in the metric code.
- Forbidden: Never let gameplay, AI, HUD or presentation code read the telemetry section; never change tuning at runtime from it (ADR-0023).
- Guardrail: Telemetry export snapshot at most 0.2 ms game thread once per round and about 1.6 MB per round (120 subject keys at 60 characters; corrected from 1.1 MB in manifest 2026-10-25f) (ADR-0023); the F7 computation runs once per round at `Ended`.

**Prototype disposition**: none (new Foundation code). The prototype keeps no round log and computes no pacing metric.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 35, scoped to this story:*

- [ ] GIVEN fixture logs, THEN `PacingRatio` = 1.3 for 936 s; `ObjectiveEndShare` = 0.7 for 7 objective and 3 elimination wins; `AbortedActiveShare` = 2/12 with 2 aborts; forfeits are excluded from `ObjectiveEndShare`; `DurationSec` 1080.0 is not a stall and 1080.001 is at the default `StallFlagMinutes` 18; an Active abort at 540.0 is not flagged and at 540.001 is; `ObjectiveEndShare` is computed per team-size bucket (a 2v2 round is in its own bucket and does not count for the Pillar 4 gate of at least 0.70 over at least 20 rounds in buckets of 4v4 and larger); `DebugForced` rounds are excluded; an empty set reports "n/a".

---

## Implementation Notes

*Derived from GDD F7, R19 and ADR-0023 section 3:*

- Formulas (GDD F7): `PacingRatio = DurationSec / (60 x TargetRoundMinutes)`; `ObjectiveEndShare(B) = N_obj(B) / (N_obj(B) + N_elim(B))` with bucket `B = min(n0, n1)` read from the log header's team rosters at round start, over rounds that ended by a win and are not `Debug`; `N_elim` counts `EndReason = Elimination` only (forfeits go to `N_forfeit` and are excluded, so network drops cannot distort the Pillar 4 measure); `AbortedActiveShare = (N_abort + N_void) / (N_obj + N_elim + N_forfeit + N_abort + N_void)`; any share with denominator 0 reports "n/a", never 0; `StallFlag = DurationSec > 60 x StallFlagMinutes`, and also any Active-phase abort with `DurationSec > 30 x StallFlagMinutes`.
- `DurationSec = latch time - ActivePhaseStart` (F7 variable table). `EndReason` values are those of R19: `ObjectiveWin`, `Elimination`, `ForfeitElimination`, `Aborted`, `Void(Abandoned | KickedLastActive | PawnLost)`, `PreparingFailed(code)`, `SessionEnded`, `DebugForced`. Rounds ended by `PreparingFailed` and `SessionEnded` are not in the AbortedActiveShare denominator as written (F7 lists only abort from `Active` and void); Open: confirm against GDD F7 whether any other `EndReason` enters a count.
- Implementation shape: a plain-C++ pure function module (name set when implemented, in `StealthFoundation/Private/EventLog/` with a `Public/Server/` header, server-only guard first) that takes the round summary read from the Event Log header and `WinLatched` entry plus the two tuning inputs and returns `{PacingRatio, StallFlag, bAbortFlagged, bucket}`; a second function aggregates a set of round summaries into per-bucket shares. The server computes the per-round values at `Ended` (ADR-0023 section 3); cross-round aggregation is offline, but the aggregation function lives here so the fixtures prove the arithmetic. No UObject, no reflection, no `FString` conversion (ADR-0007 section 6).
- Boundaries: use exact integer-millisecond comparison at the 1080 s and 540 s boundaries (1080.0 not a stall, 1080.001 a stall; 540.0 not flagged, 540.001 flagged); use a tolerance for the ratio assertions (1.3, 0.7, 2/12). An empty bucket returns "n/a" (a distinct not-available result), not 0.
- Team-size bucketing: a 2v2 round has `B = 2`; only buckets with `B >= 4` count for the Pillar 4 gate (at least 0.70 over at least 20 such rounds). The aggregation returns per-bucket values and a separate gate evaluation over `B >= 4`; assert that adding 2v2 rounds does not change the gate result.
- `DebugForced` rounds (and `Debug`-flagged entries) are excluded from every share (R22: debug commands are flagged `Debug` and excluded from playtest metrics).
- Fixtures (round logs, bucket layouts, tuning inputs 12 and 18) live in the test constant files (story 005), not inline.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 011 (`production/epics/round-match-framework/story-011-event-log-content.md`): the header and `WinLatched` fields this story reads.
- Story 054 (`production/epics/round-match-framework/story-054-team-summary-at-ended.md`): `FRoundResult.TeamSummary` (MVP item 9 data half); different data from the Event Log.
- Telemetry section probes, detectors and the JSON export (ADR-0023, Feature-layer producers and the export story): this story computes Round's F7 only.
- Tuning keys `TargetRoundMinutes` and `StallFlagMinutes` themselves (epic `tuning-data-config`).
- Acting on a stall: F7 says a stall is reported as an Objectives stall-pressure defect and never answered with a Round timer.

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthFoundation/Private/Tests/EventLog/PacingMetricsTest.cpp` (class `FRoundPacingMetricsTest`, test name `StealthGame.Round.PacingMetrics`)
- `Source/StealthFoundation/Public/Server/` pacing-metrics header (new, server-only guard first, Stealth-prefixed unique name; Open: name set when implemented)
- `Source/StealthFoundation/Private/EventLog/` pacing-metrics implementation (new)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: PacingRatio, ObjectiveEndShare, AbortedActiveShare and the stall/abort flags compute per F7 from fixture logs.
  - Given: fixture round logs.
  - When: the metrics are computed.
  - Then: `PacingRatio` = 1.3 for 936 s.
  - Then: `ObjectiveEndShare` = 0.7 for 7 objective and 3 elimination wins, and forfeits are excluded from it.
  - Then: `AbortedActiveShare` = 2/12 with 2 aborts.
  - Then: `DurationSec` 1080.0 is not a stall and 1080.001 is, at `StallFlagMinutes` 18.
  - Then: an Active abort at 540.0 is not flagged and one at 540.001 is.
  - Then: `ObjectiveEndShare` is computed per team-size bucket, and a 2v2 round does not count toward the 4v4+ Pillar 4 gate (>= 0.70 over >= 20 rounds).
  - Then: `DebugForced` rounds are excluded, and an empty set reports "n/a".
  - Edge cases: use a tolerance for ratios; use exact millisecond or integer-ms comparison at the 1080 and 540 boundaries; an empty bucket gives "n/a", not 0.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/EventLog/PacingMetricsTest.cpp` — must exist and pass (automation test `StealthGame.Round.PacingMetrics`, class `FRoundPacingMetricsTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-011-event-log-content.md`
- Unlocks: None

Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; `TargetRoundMinutes` and `StallFlagMinutes` keys; stories not yet created)
