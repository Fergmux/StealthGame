# Story 014: No log device, ClientMessage or trace channel carries a sentinel; dumplog is refused before Ended

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-008` (Server-only Event Log ring with release classes; never through logs or traces). Also `GDD AC 60` (Foundation half, revised text in `rmf-ac-overrides.md`), `R19` (Secrets are not stored; Host), `F8`.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries; also ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: ADR-0007: four runtime modules, `Public/View/` vs `Public/Server/` with a compile guard, consumer-owned interfaces wired by the composition root, plain-C++ cores behind thin facades, and a logging policy. The decision this story applies is section 6: Event Log types are plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`, no `ToString`, `LexToString`, `operator<<` or `FString` conversion), the only text conversion is the `Ended`/`Reset` disk writer inside the Event Log store, and Event Log content, `DealDigest`, sub-seeds, bindings and mailbox order never reach `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/stat names. ADR-0003 contributes the replication allowlist audit principle: nothing from the log is a replicated property.
**ADR Version**: ADR-0007 2026-10-24; ADR-0003 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (governing ADR-0007; ADR-0003 is HIGH but only its allowlist principle applies here)
**Engine Notes**: ADR-0007 section 6 names `UE_LOGF` (5.8, post-cutoff, `LogMacros.h:199`) as the logging macro for new code; this story touches it only negatively (nothing in the log path may call it with log content) and uses an `FOutputDevice` capture to prove that. ADR-0007 verification items (1) to (4) (PrivateDefinitions guard reach, `TargetConfigurationDenyList`, Blueprint audit walk, `[CoreRedirects]`) are not touched. ADR-0003 spikes S9, S10, S12, S13 are not touched: no replicated property or relevancy rule is added.

**Control Manifest Rules (this layer)**:
- Required: Event Log types are plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`) with no text conversion besides the `Ended`/`Reset` disk writer (ADR-0007).
- Forbidden: Never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names (ADR-0007).
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms on the listen-host game thread (ADR-0002); log appends happen inside drain slots, so this story adds no per-entry text work.
- Manifest 2026-10-25g re-read at implementation: it changes no Foundation queue or Event Log rule (its changes are the S10 replication rules)

**Prototype disposition**: none (new Foundation code). The prototype logs round facts with `UE_LOG(LogStealth, ...)` in `StealthGameMode.cpp` (for example in `EndRound`); that pattern is the one this story's test proves the new code does not use. It is not promoted.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 60 (Foundation half; the `dumplog` phase rule moves to story 027), scoped to this story:*

- [x] GIVEN a full fixture round of sentinel ids and hashes, WHEN every log device, `ClientMessage` and trace channel is captured, THEN none contains a sentinel; no log file is written before `Ended` or `Reset`; and no sub-seed derived from the fixture secret appears in any log entry, header or file.

---

## Implementation Notes

*Derived from ADR-0007 Implementation Guidelines and section 6, ADR-0002 section 8.9 and GDD R19:*

- This is a negative-property test over the Event Log store built by story 011 (`FRoundEventLog`, plain C++, owned by one facade subsystem through a `TUniquePtr`; every producer appends only through `IRoundLogWriter::Append(const FRoundLogEntry&)`). The test drives a scripted fixture round through Lobby, Preparing, Active, Ended and Reset using the fake clock (story 001) and the fixed `ServerSecret` (story 002) and fills entries and header with the sentinel ids and hashes from the fixture constant files (story 005, `RoundTestSeeds`, never inlined).
- Capture points named by the AC: (a) every log device: register a capturing `FOutputDevice` with `GLog` for the run and also read back the log file target if one is configured; (b) `ClientMessage`: a spy on the test controller/host sink; (c) trace channels: Open: confirm the capture mechanism (a trace-channel capture hook, or an assertion that the enabled channel set excludes Round); the AC names the channels but not how they are captured.
- No file before `Ended`/`Reset`: the only writer is the Event Log store's disk writer (ADR-0007 section 6; GDD R19 "written to disk server-side on entering `Ended` or `Reset`, never mid-round"). A file written exactly at `Reset` is allowed. The telemetry export (ADR-0023) follows the same guard and is out of this story's scope.
- Sub-seeds: the sub-seeds are computable from the fixed fixture secret (ADR-0006 `GetServerSubSeed`), so the check is exact-value: derive the expected 64-bit values for every `ESeedDomain` in the test and assert that neither the decimal nor the hex rendering of any of them occurs in any entry, header, file or captured output. The log holds `Seed` (public) and hashes only (R19 "Secrets are not stored").
- Logging policy compliance: any diagnostic the code under test does emit uses `UE_LOGF` with `LogStealthFoundation` and carries no log content (ADR-0007 section 6). The include/log-macro lint of ADR-0007 section 3.2 (extended with `FRoundLogEntry`, `FRoundLogHeader`, `IRoundLogWriter`) is a separate CI gate; this test is its runtime counterpart. Add a planted-leak negative control (a deliberately leaking fixture sink in the test module only, never compiled into Shipping) so the capture is proven able to fail (ADR-0008: every leak gate includes a negative control).
- The `stealth.round.dumplog` phase matrix is NOT asserted here (story 027). The command itself is `#if !UE_BUILD_SHIPPING`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 011 (`production/epics/round-match-framework/story-011-event-log-content.md`): the Event Log store, header fields, `WinLatched`, and the pre-`Ended` read refusal; this story consumes it.
- Story 027 (`production/epics/round-match-framework/story-027-reset-command-matrix.md`): the `stealth.round.dumplog` refusal in Lobby, Preparing and Active and its allowance in Ended and Reset (the refusal code name is a GDD item to confirm).
- Stories 012, 013, 015, 016, 017: truncation, size bound, Voided rate limit, pacing metrics, latched-final-complete logging.
- Telemetry section content and export (ADR-0023, Feature-layer producers); the CI include/log-macro lint script itself.
- Replicated-property allowlist diff of `FRoundPublicState` (story 080).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthFoundation/Public/Server/EventLog/StealthRoundEventLog.h` (the single disk writer, refused unless Ended/Reset)
- `Source/StealthFoundation/Private/EventLog/StealthRoundEventLog.cpp`
- `Source/StealthFoundation/Private/Tests/Round/LogNeverLeaksTest.cpp` (test `StealthGame.Round.LogNeverLeaks`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: No sentinel, early log file or stored sub-seed appears anywhere in the Foundation log paths.
  - Given: a full fixture round of sentinel ids and hashes, with every log device, `ClientMessage` spy and trace channel captured.
  - When: the round runs through Lobby, Preparing, Active, Ended and Reset.
  - Then: no captured output contains a sentinel.
  - Then: no log file is written before Ended or Reset.
  - Then: no sub-seed derived from the fixture secret appears in any log entry, header or file.
  - Edge cases: the sub-seeds are computable from the fixed secret, so the check is exact-value. Open: confirm the trace-channel capture mechanism (a trace-channel capture hook, or assert that the channel's enabled set excludes Round), because the AC names the channels but not how they are captured. A file written exactly at Reset is allowed.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Round/LogNeverLeaksTest.cpp` — must exist and pass (automation test `StealthGame.Round.LogNeverLeaks`, class `FRoundLogNeverLeaksTest`)

**Status**: [x] Created and passing (`StealthGame.Round.LogNeverLeaks`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-011-event-log-content.md`
- Unlocks: None

Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (round-match-framework Exit A batch 001-017)
**Criteria**: 1/1 passing (`StealthGame.Round.LogNeverLeaks`)
**Notes**: A fixture round (queue with secret S3 writing into the real store, sentinel ids and hashes) runs Lobby to Reset with every GLog line captured: no line carries a sentinel or any sub-seed rendering; the disk writer is refused in Lobby, Preparing, Active and the latch step and writes only at Ended and Reset; no file or header holds a sub-seed. ClientMessage and trace channels: the Queue and Event Log sources call no `UE_LOG`/`UE_LOGF`, `ClientMessage`, `TRACE_`, `CSV_`, `SCOPE_CYCLE_COUNTER`, `ensureMsgf` or `checkf` (static scan; the include lint (i) is the CI half) - the story's Open item on the capture mechanism (decision log). A planted leaking sink is detected (negative control).
**Build/test**: see `production/qa/evidence/story-014-log-never-leaks/full-suite.txt`
