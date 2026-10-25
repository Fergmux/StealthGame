# Story 011: Event Log header and WinLatched hold the specified fields for every terminal outcome and are unreadable before Ended

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-008` (server-only Event Log ring with release classes; never through logs or traces). GDD AC 32 (scoped by the AC overrides file). Rules: R19 (header, entries, `EndReason`, release classes, host).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**MVP item 9 (data half)**: this story builds the log store that story 054 reads at the round-end step to compute `FRoundResult.TeamSummary` (the text-only end-of-round summary; the HUD epic carries the display half, user decision 2026-10-25). It also feeds the playtest instrumentation (ADR-0023) through the header and `WinLatched`.

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries (also ADR-0023: Playtest telemetry hooks; ADR-0002 section 8.9 owns the Event Log itself)
**ADR Decision Summary**: Four runtime modules (StealthFoundation, StealthGameplay, StealthPresentation, StealthGame) plus Editor and StealthTestHarness; `Public/View/` vs `Public/Server/` with a compile guard; consumer-owned interfaces wired by the composition root; plain-C++ cores behind thin facades; logging policy. This story applies ADR-0007's rules for the Event Log: types are plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`) with no text conversion besides the `Ended`/`Reset` disk writer, in `StealthFoundation` `Public/Server/` behind the server-only guard, and log content never reaches `UE_LOG`, `ensureMsgf`, `ClientMessage` or traces. ADR-0002 section 8.9 supplies the store shape: an uncapped header written once at P3 plus a ring of `RoundLogMaxEntriesCount` entries, a single `IRoundLogWriter::Append`, and `WinLatched(outcome, EndReason, key)` (also written for a void). ADR-0023 is a consumer of the ring and shares no slots with it.
**ADR Version**: ADR-0007 2026-10-24; ADR-0023 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0007; ADR-0023 is LOW)
**Engine Notes**: Touches `UE_LOGF` (5.8, the preferred logging macro) only as a prohibition: Event Log content must never be an argument of any log macro, so the story calls no logging API with entry content. C++20 floor (5.8) applies to all new code. No other post-cutoff API is used. ADR-0007 verification items (PrivateDefinitions guard, harness deny list, include lint) are not touched, but the lint (rules a-j) must include `FRoundLogEntry`, `FRoundLogHeader` and `IRoundLogWriter` as banned log arguments (ADR-0002 section 8.9).

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Event Log types are plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`) with no text conversion besides the `Ended`/`Reset` disk writer (ADR-0007)
- Forbidden: Never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names (ADR-0007)
- Guardrail: Event Log at most about 400 KB at the 4096 default and about 1.6 MB at the 16384 hard maximum (F8, ADR-0002 section 8.9); the drain stays inside 0.5 ms (ADR-0002) so `Append` is O(1) amortised

**Why this story**: The Event Log is the only record of why a round ended and the raw material for playtest analysis (Pillar 4). Before it can be trusted, three things must hold for every way a round can terminate: the header carries the full field list written at P3, `WinLatched` carries the `EndReason` and the key, and nothing can read the log until the round is Ended (so the host cannot peek mid-round). This story builds the store and proves those three. Story 012 (truncation), 013 (size), 014 (leak checks), 015 (Voided rate limit), 016 (pacing), 017 (latched completion) and 054 (TeamSummary) all build on it. Exit A.

**Prototype disposition**: none (new Foundation code). The prototype has no Event Log; it writes `UE_LOG` lines, which this design forbids for round content.

---

## Acceptance Criteria

*From the AC overrides file (AC 32 scoped; the `dumplog` phase matrix moved to story 027), one criterion, `story_granularity: fine`:*

- [ ] For each terminal outcome (ObjectiveWin, Elimination, ForfeitElimination, Void Abandoned, Void KickedLastActive, Void PawnLost, Aborted) the header holds every field of the RoundLogHeaderFields fixture, the WinLatched entry holds the EndReason and the key, and the client/dump log read API refuses every read before Ended (the server-internal accessor Round itself uses to compute TeamSummary in story 054 is a separate path). `StealthGame.Round.EventLogContent` [U]

---

## Implementation Notes

*Derived from ADR-0002 section 8.9 (Event Log), ADR-0007 section 6 (logging policy) and GDD R19:*

- Store: `FRoundEventLog`, plain C++, in `StealthFoundation/Public/Server/` (area folder `EventLog/`), behind `StealthServerOnlyGuard.h` (first include), owned by one facade subsystem through a `TUniquePtr`. Entry = `{FQueueKey Key, double ServerTime, uint32 RoundEpoch, ERoundLogType Type, ParticipantIds, SourceMailbox, data}`; `ERoundLogType` is a closed enum (adding a type is an amendment to ADR-0002 section 8.9). The closed entry-type list is in R19; `Move` and `Look` are never logged.
- Header: uncapped, written once at P3 (R19: `RoundIndex`, `RoundEpoch`, `Seed`, `LayoutId`, `LayoutAttempt`, `LayoutHash`, `ProviderKind`, `LayoutFallbackLevel`, `PresetId`, `TuningHash`, `SchemaVersion`, `FallbackLevel`, `HotReloaded`, tuning warnings, `DealDigest`, the active appearance set and counts, `PoolBelowMvpFloor`, the team rosters, the host `ParticipantId`, per-zone player shares (F5), build version). The test builds a header from the fixture and asserts every field of `RoundLogHeaderFields` (story 005 owns the fixture constant) is present; it does not need real layout, tuning or pool data.
- `WinLatched(outcome, EndReason, key)`: the entry holds the `EndReason` and the key. `EndReason` is one of `ObjectiveWin`, `Elimination`, `ForfeitElimination` (both public as `EliminationWin`), or for a non-result `Aborted`, `Void(Abandoned | KickedLastActive | PawnLost)`, `PreparingFailed(code)`, `SessionEnded`, `DebugForced` (R19). The AC's seven outcomes are table-driven. `WinLatched` is "also written for a void" (R19, ADR-0002 section 8.9). Open: confirm against GDD R19 and AC 32 (the AC text lists `Elimination`, which follows the GDD enum name) that `Aborted` and the three `Void` codes still log `WinLatched` with the `EndReason` and a key even though they have no winner; R19 says "also written for a void" and does not mention an abort explicitly.
- Read gating: every read through the log read API is refused in each pre-Ended state (Lobby, Preparing, Active) and succeeds once Ended. The store receives the phase through an injected gate (Foundation cannot include StealthGameplay, so the phase machine is not named here). Open: confirm against ADR-0002 section 8.9 the name and signature of the read API and of the gate; and confirm against GDD R19 whether reads in `Reset` are allowed (R19 lets `stealth.round.dumplog` run in `Ended` or `Reset`; story 027 holds that command matrix, this story asserts only refusal before Ended and success at Ended). Also confirm against ADR-0002 section 8.9 reader (a): Round itself reads the ring server-side "in the latch step" to compute `TeamSummary` (story 054); that internal read must be a distinct server-side path or occur once the log is readable, so it does not contradict "refuses every read before Ended". Do not decide it here; record the answer in the story 054 notes.
- `IRoundLogWriter::Append(const FRoundLogEntry&)` is the single write path: game thread only, from inside the drain slot that produced the fact, in key order. Story 008 declares the minimal interface; this story builds the real store behind it and the header.
- Release classes are `NeverReleased`, `PostRoundEligible` and `PublicResult` (provisional, R19); this story records the class per entry type only if the store carries it. Nothing is released in MVP except `PublicResult` through `FRoundResult` (story 054).
- Never write header or entry content to `UE_LOG`/`UE_LOGF`, the automation info output or a trace; a failing assertion must not print entry content.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 012 (`production/epics/round-match-framework/story-012-log-truncation.md`): the 4096 ring, eviction order, protected entries and `TruncatedCount`.
- Story 013 (`production/epics/round-match-framework/story-013-log-size-bound.md`): mean entry size against F8 and the 16,384 clamp.
- Story 014 (`production/epics/round-match-framework/story-014-log-never-leaks.md`): sentinel capture across log devices, `ClientMessage` and trace channels; no early log file; no sub-seed in the log.
- Story 015 (`story-015-voided-rate-limit.md`) and story 017 (`story-017-latched-final-complete-logged.md`): the `Voided` rate limit and the `TendCompleted(..., Latched = true)` entry.
- Story 016 (`story-016-pacing-metrics.md`): the F7 pacing metrics computed from fixture logs.
- Story 027 (`production/epics/round-match-framework/story-027-reset-command-matrix.md`): the `stealth.round.dumplog` phase matrix (refused in Lobby, Preparing, Active; allowed in Ended and Reset).
- Story 054 (`production/epics/round-match-framework/story-054-team-summary-at-ended.md`): `FRoundResult.TeamSummary` computed from the log at the round-end step (MVP item 9 data half; the HUD epic carries the display half).
- The telemetry section (ADR-0023 `FPlaytestTelemetryCore`) and the disk writer on `Ended`/`Reset`: separate Event Log store work, not built here.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/EventLog/StealthRoundEventLog.h` and `Source/StealthFoundation/Private/EventLog/StealthRoundEventLog.cpp` (new; `FRoundEventLog`, `FRoundLogHeader`, `FRoundLogEntry`, `ERoundLogType`, `EEndReason`)
- `Source/StealthFoundation/Public/Server/EventLog/StealthRoundLogWriter.h` (extended from story 008; `IRoundLogWriter`)
- `Source/StealthFoundation/Private/Tests/EventLog/EventLogContentTest.cpp` (new; class `FRoundEventLogContentTest`)
- Tests live under the engine test roots (`tests/README.md`), never under `tests/`. `/dev-story` replaces this planned list with the real one. `Source/StealthFoundation` does not exist yet (tuning-data-config epic creates the skeleton); write under `Source/StealthGame/` until it lands.

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: For each terminal outcome (ObjectiveWin, Elimination, ForfeitElimination, Void Abandoned, Void KickedLastActive, Void PawnLost, Aborted) the header holds every field of the RoundLogHeaderFields fixture, the WinLatched entry holds the EndReason and the key, and the client/dump log read API refuses every read before Ended (the server-internal accessor Round itself uses to compute TeamSummary in story 054 is a separate path).
  - Given: the log fed fixture rounds ending in `ObjectiveWin`, `Elimination`, `ForfeitElimination`, Void `Abandoned`, Void `KickedLastActive`, Void `PawnLost` and `Aborted` (the `Elimination` entry follows the GDD enum name).
  - When: each round ends and the log is read.
  - Then: the header holds every field of the `RoundLogHeaderFields` fixture.
  - Then: the `WinLatched` entry holds the `EndReason` and the key.
  - Then: every read through the log read API, attempted in each pre-Ended state, is refused.
  - Then: the same read succeeds once the round is Ended.
  - Edge cases: seven outcomes, table-driven; the refusal is asserted at API level only (the `dumplog` command is story 027); `Void` and `Aborted` outcomes may have no `WinLatched` winner, so Open: confirm against GDD R19 that they still log `WinLatched` with the `EndReason` (the AC says every outcome does).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/EventLog/EventLogContentTest.cpp` — automation test `StealthGame.Round.EventLogContent` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundEventLogContentTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-001-test-seam-fake-clock.md`, `production/epics/round-match-framework/story-005-test-fixture-constants.md`, `production/epics/round-match-framework/story-008-resolver-contract.md`
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: `production/epics/round-match-framework/story-012-log-truncation.md`, `production/epics/round-match-framework/story-013-log-size-bound.md`, `production/epics/round-match-framework/story-014-log-never-leaks.md`, `production/epics/round-match-framework/story-015-voided-rate-limit.md`, `production/epics/round-match-framework/story-016-pacing-metrics.md`, `production/epics/round-match-framework/story-017-latched-final-complete-logged.md`, `production/epics/round-match-framework/story-054-team-summary-at-ended.md`
