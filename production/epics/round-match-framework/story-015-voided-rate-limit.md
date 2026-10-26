# Story 015: At most one Voided entry per second per source and the kill timeline stays intact

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-008` (Server-only Event Log ring with release classes; never through logs or traces). Also `GDD AC 61`, `R19` (`Voided` entries rate-limited to one per source per second, kill timeline protected from eviction).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: Four runtime modules with `Public/View/` vs `Public/Server/` separation, consumer-owned interfaces, and plain-C++ cores behind thin facades. The decision this story applies: the Event Log store is a plain-C++ core (`FRoundEventLog`, no `UWorld`, no reflection) in `StealthFoundation/Public/Server/`, written only through `IRoundLogWriter::Append` on the game thread, so rate limiting is a deterministic property of the core that headless tests can drive with a fake clock.
**ADR Version**: ADR-0007 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0007 post-cutoff items are `UE_LOGF` (5.8) and the C++20 floor. This story adds no log macro call with log content and no reflection type; the C++20 floor applies to the new code. No engine API is touched beyond plain C++ containers.

**Control Manifest Rules (this layer)**:
- Required: Event Log types are plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`) with no text conversion besides the `Ended`/`Reset` disk writer (ADR-0007).
- Forbidden: Never route queue Event Log entries through `UE_LOG`/`UE_LOGF`, the output log or trace channels (ADR-0002); suppressed `Voided` entries are counted in memory, never printed.
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms on the listen-host game thread (ADR-0002); the rate-limit check is O(1) per `Voided` append.
- Manifest 2026-10-25g re-read at implementation: it changes no Foundation queue or Event Log rule (its changes are the S10 replication rules)

**Prototype disposition**: none (new Foundation code). The prototype has no Event Log and no per-source rate limit.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 61, scoped to this story:*

- [x] GIVEN one source submitting 100 invalid commands per second, THEN at most one `Voided` entry per second is written for it and the kill timeline of the round is intact.

---

## Implementation Notes

*Derived from GDD R19 and ADR-0002 section 8.9 (Round Event Log, ring owner):*

- Rule (R19): "`Voided` entries are rate-limited to one per source per second (the rest are counted)." The source is the entry's `SourceMailbox`. The rate limiter sits inside the Event Log core's append path, keyed by `FMailboxId`, so no producer can bypass it (every producer appends only through `IRoundLogWriter::Append`, game thread, inside the drain slot that produced the fact). A second source has its own budget. Suppressed voids are counted (per source or in the per-type `TruncatedCount`-style counter; Open: confirm against GDD R19 and ADR-0002 section 8.9 which counter holds them) and never logged.
- The time base is the injected `IClock` (story 001), never the wall clock: a 10 s fake-time run with 100 invalid commands per second from one source must write at most one `Voided` entry in any 1 s window.
- Open: confirm against GDD R19 whether the 1-second window is fixed or sliding, and how a command at exactly 1.000 s is treated; the QA spec requires this to be confirmed before the boundary assertion is written. Until confirmed, assert only "at most one in any 1 s window" at non-boundary times.
- Kill-timeline protection (R19, ADR-0023 section 5): `KillResolved`, `Reversal`, `StateTransition`, `StrikeLimit`, `ObjectiveStage`, `FinalCompleted` and `WinLatched` are protected entries and are never dropped by truncation. The test logs fixture kill entries during the flood and asserts all are present, in order, with none evicted or suppressed. The rate limit applies to `Voided` only; it must not suppress any protected type.
- Resolvers return `EResolveResult::Voided` with an `EVoidReason` (ADR-0002 section 2a); the log entry carries `Voided(type, reason)`. The resolver contract itself is story 008; this story only limits how many of those results become entries.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 011 (`production/epics/round-match-framework/story-011-event-log-content.md`): the log store, header and `WinLatched`; this story adds the rate limit on top.
- Story 012 (`production/epics/round-match-framework/story-012-log-truncation.md`): the 4096-entry ring eviction and `TruncatedCount` arithmetic (the 10-source 7,200-entry flood scenario of ADR-0023 section 5 is covered there by eviction, not here by suppression).
- Story 008 (`production/epics/round-match-framework/story-008-resolver-contract.md`): how a resolver returns `Voided`.
- Story 007 (`production/epics/round-match-framework/story-007-intake-ordering-rules.md`): mailbox overflow and stale-connection drops (`MailboxOverflow`, `Dropped(StaleConnection)` entries).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthFoundation/Public/Server/EventLog/StealthRoundEventLog.h` (per-source `Voided` budget on the injected clock, suppressed counts)
- `Source/StealthFoundation/Private/EventLog/StealthRoundEventLog.cpp`
- `Source/StealthFoundation/Private/Tests/Round/VoidedRateLimitTest.cpp` (test `StealthGame.Round.VoidedRateLimit`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: At most one Voided entry per second per source, and the kill timeline stays intact.
  - Given: the fake clock, one source submitting 100 invalid commands per second, and fixture kill entries logged during the run.
  - When: 10 s of fake time elapse.
  - Then: that source has at most one `Voided` entry in any 1 s window.
  - Then: all kill-timeline entries are present, in order, with none evicted or suppressed.
  - Edge cases: Open: confirm against GDD R19 whether the window boundary at exactly 1.000 s is a fixed or a sliding window. A second source has its own budget. Suppressed voids are counted, not logged.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Round/VoidedRateLimitTest.cpp` — must exist and pass (automation test `StealthGame.Round.VoidedRateLimit`, class `FRoundVoidedRateLimitTest`)

**Status**: [x] Created and passing (`StealthGame.Round.VoidedRateLimit`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-011-event-log-content.md`
- Unlocks: None

Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (round-match-framework Exit A batch 001-017)
**Criteria**: 1/1 passing (`StealthGame.Round.VoidedRateLimit`)
**Notes**: 1,000 invalid commands over 10 s from one source write about one `Voided` entry per second with no two inside 1 s; the rest are counted per source; a second source keeps its own budget; every kill-timeline entry is present in order and nothing is evicted. Window rule (Open item): written when at least 1.000 s, in whole milliseconds, after that source's last written one; the budget is spent only when the entry is stored (code-review fix).
**Build/test**: see `production/qa/evidence/story-015-voided-rate-limit/full-suite.txt`
