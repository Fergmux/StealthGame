# Story 054: FRoundResult.TeamSummary is computed at the round-end step from the Event Log and is all-zero before Ended (MVP item 9 data)

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-004` (Single-assignment latch and the two-step round end) and `TR-round-008` (Server-only Event Log ring with release classes; never through logs or traces). Also `GDD AC 12e`, `R9` (`TeamSummary[t]` definition), `R19` (release class `PublicResult`), `R20`.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**MVP item 9 (data half):** this story, with story 011 (Event Log content, which supplies the `KillResolved` rows it reads), is the data half of MVP item 9, the text-only end-of-round summary (user decision 2026-10-25). `FRoundResult.TeamSummary` is computed from the Event Log at the round-end step; the HUD epic carries the display half (HUD-E21 reads only `FRoundResult`).

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging; ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: ADR-0002: one total-order queue per round, typed staged writes, single-assignment latch storage with Round's rule injected, and the R9 two-step round end; section 8.4(a) publishes `FRoundResult {Winner, EndReason (public form), RoundIndex, TeamSummary[2]}` with `TeamSummary` per team `{StepsCompleted, EliminatedCount, WrongTargetCount}`, computed in the latch step and zero before the round-end step. ADR-0007: four runtime modules plus Editor and StealthTestHarness; `Public/View/` for client-safe types and `Public/Server/` (with a compile guard) for server-only ones; Event Log entry types are plain C++.
**ADR Version**: ADR-0002 2026-10-24; ADR-0007 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0002 (no post-cutoff API; verification items on CMC spawn in `TG_PostPhysics`, the client estimator and the listen-host frame are none touched). ADR-0007's post-cutoff items (`UE_LOGF`, C++20 floor) are touched only in that no Event Log content may reach a log: summary content is aggregate counts and is never written to `UE_LOGF`. `FRoundTeamSummary` is an outcome-bearing replicated struct, so ADR-0003's rule applies to its serialisation (fixed bit width, no value-dependent size); ADR-0003's spikes are not touched here.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Round phase flow, Event Log and Forbidden; the Core Layer Rules section holds no Round-specific rule):
- Required: `FRoundResult.TeamSummary[2]` is computed in the latch step and published only in the round-end step (all zero before) (ADR-0002, user decision 2026-10-25).
- Forbidden: Never write a non-zero `TeamSummary` (or any per-team count) into `FRoundPublicState` before `Ended` (ADR-0002, user decision 2026-10-25).
- Guardrail: Round Event Log: entry 64-96 B, about 25 KB typical, about 400 KB at the default 4,096 entries (ADR-0002). `FRoundPublicState` with `FRoundResult` and `TeamSummary` is about 64 B (R9 round-end step budget).

**Prototype disposition**: none (new code). The prototype has no per-team summary; its end banner shows only the winner and the reason (`AStealthHUD`, `StealthHUD.cpp` line about 255), which is a HUD matter.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 12e, scoped to this story:*

- [x] GIVEN a 2v2 round won by team 0 at the final objective, with team 0 having one member `Informant` and two `WrongTarget` results and team 1 having completed 2 stages, both members `Informant` and no wrong targets, WHEN the round-end step runs, THEN `FRoundResult.TeamSummary` = [{3, 1, 2}, {2, 2, 0}] in the first `FRoundPublicState` carrying `Ended`, every earlier received `FRoundPublicState` holds an all-zero `TeamSummary`, and no summary field holds a time or a participant id; with a forfeit as the last removal, the forfeiting member is counted in `EliminatedCount`.

---

## Implementation Notes

*Derived from GDD R9 (`TeamSummary[t]`) and ADR-0002 section 8.4(a):*

- Shape (R9): `FRoundTeamSummary { uint8 StepsCompleted; uint8 EliminatedCount; uint8 WrongTargetCount }`, two of them in `FRoundResult.TeamSummary[2]`, in the field order of the AC tuple `{3, 1, 2}` = {StepsCompleted, EliminatedCount, WrongTargetCount}. Open: confirm against GDD R9 that the AC tuple order is exactly this struct's field order (R9 names the fields in this order; the AC does not name them).
- `StepsCompleted` (0-3): the team's completed stages, the final counted only for the objective winner, read from Objectives' server store (a fake in this test; in the AC team 0's 3 stages are not in the setup, so the fixture must stage 3 completions for team 0, the final counted because team 0 is the objective winner, and 2 for team 1; Open: confirm against Objectives' stage numbering how many of team 0's 3 are preliminary stages).
- `EliminatedCount`: members of team `t` whose `State` is `Informant` in the latch record, every cause alike, forfeits included, counted before `EndRoundAll` (which would otherwise turn every record `RoundEnded`). A forfeit as the last removal is a grace-expiry conversion (`Informant`, Reason `Disconnect`), so the forfeiting member is counted; the second run of the test sets that up (see story 064 for the forfeit path itself).
- `WrongTargetCount`: the count of this round's Event Log `KillResolved` rows (R19, written by Assassination) whose killer is on team `t` and whose result is `WrongTarget`, NPC and informant kills alike, saturating at 255. By construction it equals the sum of team `t`'s members' Penalty `WrongTargetTally` (PEN-R3), and it never reads Penalty's ServerOnly `StrikeCount`. In the test, the `KillResolved` rows are appended through `IRoundLogWriter::Append` (never a private log; control manifest) by a stub producer.
- Timing (R9): computed in the latch step, published only in the round-end step; zero in every earlier `FRoundPublicState`. The AC says the summary is computed "when the round-end step runs"; R9 says "computed in the latch step and published only in the round-end step". For an objective win the two steps are the same server step (story 052), so both readings give the same observable result; for a strike win the latch step is earlier (story 053). Open: confirm against R9 and AC 12e wording which step owns the computation, and keep the observable contract (published at the round-end step, zero before) as the assertion.
- Event Log read (story 011): the log read API refuses every read before `Ended`, but the computation runs in the latch step while the phase is still `Active`. The aggregation must therefore use a server-internal accessor of the Round log store, not the public read API. Open: confirm against story 011 and GDD R19 that such an internal accessor (or an aggregate maintained by the writer) is permitted without weakening the refuse-before-Ended rule.
- Release class (R19, ADR-0002 8.4): `FRoundResult`, including its `TeamSummary` aggregates, is `PublicResult` and the only Event Log content released in MVP. The struct holds team aggregates only: no participant id, no time (IVP R3 `RoundTeamSummary` row; the duration stays in the server log). The AC's last check is a reflection check over `FRoundTeamSummary` and `FRoundResult`: every property is an integer count (`uint8`) and none is a time type or a participant id type.
- Placement and wire: `FRoundTeamSummary` is a client-safe wire type (the HUD reads it), so it belongs with `FRoundResult` and `FRoundPublicState` under a `Public/View/` header (ADR-0007 section 2); the exact module and header are fixed by story 029. Open: confirm the header location against story 029 and ADR-0007 section 2. It serialises to a fixed width (3 bytes per team, ADR-0003 Implementation Guidelines on outcome-bearing structs).
- "Every earlier received `FRoundPublicState`": the test captures every struct write the replication spy sees from `Preparing` through `Ended` and asserts `TeamSummary` is all-zero in each one before the `Ended` write (the control manifest forbids a non-zero per-team count in any earlier write).
- Manifest 2026-10-25f: `TeamSummary[2]` is the only way opponent counts reach a client, and only at `Ended`; C-TeamView carries own-team data only.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 011 (`production/epics/round-match-framework/story-011-event-log-content.md`): the Event Log header, entry types and the refuse-before-Ended read API; this story reads `KillResolved` rows and writes none.
- Story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`): the round-end step itself (result, score, `EndRoundAll`, rejections, `PostRound` viewers).
- Story 053 (`production/epics/round-match-framework/story-053-strike-win-staging.md`): the strike-win staging between the latch step and the death write.
- Story 064 (`production/epics/round-match-framework/story-064-last-disconnect-forfeits.md`): the forfeit outcome and its banner; this story only checks the forfeiting member's count.
- The HUD epic: the on-screen text summary (the display half of MVP item 9; HUD-E21). Objectives' stage store and Assassination's `KillResolved` writer (stubbed here). Penalty's `WrongTargetTally`.
- Per-participant content and the look-to-participant link: the Vertical Slice Post-Round Reveal (R19; OQ4 declined).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/TeamSummaryAtEndedTest.cpp` (class `FRoundTeamSummaryAtEndedTest`, test name `StealthGame.Round.TeamSummaryAtEnded`)
- `Source/StealthGameplay/Public/Round/` team-summary and result headers, or the client-safe `Public/View/` header chosen by story 029 (new; Open: name set when implemented)
- `Source/StealthGameplay/Private/Round/Win/` summary aggregation (new)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `FRoundResult.TeamSummary` is computed at the round-end step from the Event Log and is all-zero before Ended.
  - Given: a 2v2 round won by team 0 at the final objective, with team 0 having one member `Informant` and two `WrongTarget` results, and team 1 having completed 2 stages, both members `Informant` and no wrong targets.
  - When: the round-end step runs.
  - Then: `FRoundResult.TeamSummary` = [{3, 1, 2}, {2, 2, 0}] in the first `FRoundPublicState` carrying `Ended`.
  - Then: every earlier received `FRoundPublicState` holds an all-zero `TeamSummary`.
  - Then: no summary field holds a time or a participant id (a reflection check on the struct).
  - When (second run): a forfeit is the last removal.
  - Then: the forfeiting member is counted in `EliminatedCount`.
  - Edge cases: the AC does not name the tuple fields; I infer {stages completed, `EliminatedCount`, wrong targets}, so confirm (Open: confirm against GDD R9). Team 0's 3 stages are not in the setup, so the fixture must stage 3 completions.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/TeamSummaryAtEndedTest.cpp` — must exist and pass (automation test `StealthGame.Round.TeamSummaryAtEnded`, class `FRoundTeamSummaryAtEndedTest`)

**Status**: [x] Created and passing (`StealthGame.Round.TeamSummaryAtEnded`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-011-event-log-content.md`, `production/epics/round-match-framework/story-052-ended-latch-effects.md`
- Unlocks: None

Cross-epic: `production/epics/participant-state-roster` (member `State`, `EndRoundAll`; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

## Completion Notes
**Completed**: 2026-10-26 (RMF round-end and disconnect batch: stories 048-065, two-step round end, TeamSummary, grace/forfeit/void)
**Criteria**: 1/1 passing ([{3,1,2},{2,2,0}] in the first Ended write, zero before; uint8 counts only; a forfeiter counted in EliminatedCount)
**Deviations**: Team 1's two informants are fixture Roster states (unreachable under F2 with an objective win; decision log); computed in the latch step, published in the round-end step; FRoundTeamSummary is in Foundation View/StealthRoundPublicState.h; StepsCompleted reads the new IRoundObjectiveProgress gateway
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/TeamSummaryAtEndedTest.cpp`; full suite: `production/qa/evidence/rmf-round-end-disconnects-2026-10-26/full-suite.txt`
**Run result**: PASS - PIE end banner shows both summary lines (see evidence)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, RMF round-end batch; findings and fixes in production/qa/evidence/rmf-round-end-disconnects-2026-10-26/evidence.md
