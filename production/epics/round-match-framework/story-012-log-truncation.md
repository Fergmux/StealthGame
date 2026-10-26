# Story 012: The 4096-entry ring evicts oldest ordinary entries first, protects header and WinLatched, and counts TruncatedCount

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-013` (Event Log ring bound; kill timeline protected from eviction; `TruncatedCount`; telemetry section). GDD AC 33. Rules: R19 (Truncation, protected entries), F8.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**Playtest instrumentation**: this story feeds the ADR-0023 playtest instrumentation: the telemetry header and export copy the per-type `TruncatedCount` so a truncated round is visible to analysis (ADR-0023 section 5).

**ADR Governing Implementation**: ADR-0023: Playtest telemetry hooks (also ADR-0007: Code module layout, composition root and include boundaries)
**ADR Decision Summary**: A bounded server-only `NeverReleased` telemetry section of the Round Event Log store, kind-blind probes/detectors, exported after termination off the game thread. This story applies ADR-0023 section 5 (answering AQ-16): the R19 ring is at most about 400 KB at 4096 entries, the kill timeline (`KillResolved`, `Reversal`, `StateTransition`, `StrikeLimit`, `ObjectiveStage`, `FinalCompleted`, `WinLatched`) occupies ring slots but is never dropped, and when the ring is full the oldest non-protected entry is dropped and the per-type `TruncatedCount` increments; the telemetry section never shares or evicts ring slots.
**ADR Version**: ADR-0023 2026-10-24; ADR-0007 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: None touched. The ring is plain C++ with no engine API. ADR-0023's verification items (TEL-V1 export off the game thread, TEL-V2 zero leakage, TEL-V3 detector parity) and its engine facts (`TJsonWriter`, `FFileHelper::SaveStringToFile`, `UE::Tasks::Launch`) concern the telemetry export and detectors, which this story does not build. `UE_LOGF` (5.8) is not used for log content.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Event Log types are plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`) with no text conversion besides the `Ended`/`Reset` disk writer (ADR-0007)
- Forbidden: Never add telemetry entries to the R19 ring, and never write telemetry through a log category, file logger, engine analytics provider or Insights channel (ADR-0023)
- Guardrail: Event Log ring at most about 400 KB at 4096 entries (F8, ADR-0023 section 5); telemetry at most 0.15 ms mean listen-host game thread (ADR-0023); eviction must be O(1) amortised so a flood cannot spike the drain
- Manifest 2026-10-25g re-read at implementation: it changes no Foundation queue or Event Log rule (its changes are the S10 replication rules)

**Why this story**: A 10-source invalid-command flood over a whole round can write about 7,200 `Voided` entries (ADR-0023 section 5), more than the 4096-slot ring. Without protection the flood would evict the kill timeline, the one record of why the round ended. R19 therefore protects the timeline and the header and evicts only ordinary entries oldest-first, counting what it dropped. This story proves the eviction rule with the AC's exact arithmetic. It depends on story 011's store. Exit A.

**Prototype disposition**: none (new Foundation code).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` Acceptance Criteria no. 33, scoped to this story (one criterion, `story_granularity: fine`):*

- [x] GIVEN 4,999 ordinary entries plus `WinLatched` with the 4096 cap (the header outside the ring, `WinLatched` inside it and protected), THEN the header and `WinLatched` survive, the oldest entries were dropped first and `TruncatedCount` = 904. `StealthGame.Round.LogTruncation` [U]

---

## Implementation Notes

*Derived from GDD R19 (Truncation) and F8, ADR-0002 section 8.9 (Eviction) and ADR-0023 section 5:*

- Header sits outside the ring (uncapped); the ring holds `RoundLogMaxEntriesCount` entries (default 4096, hard 512 to 16384, safe 2048 to 8192, `ServerOnly`; GDD Tuning Knobs). The test uses cap 4096.
- Protected entry types occupy ring slots and are never evicted: `KillResolved`, `Reversal`, `StateTransition`, `StrikeLimit`, `ObjectiveStage`, `FinalCompleted`, `WinLatched` (R19, ADR-0002 section 8.9). When the ring is full the oldest non-protected entry is dropped and `TruncatedCount` (kept per entry type) increments. Eviction order is by age among unprotected entries only.
- Arithmetic of the AC: 4,999 ordinary plus one `WinLatched` is 5,000 entries into 4,096 slots, so 904 ordinary entries are evicted (5,000 minus 4,096), the earliest surviving ordinary entry is the 905th, and the sum of the per-type counters is 904. Assert the total and, since the ordinary entries are one type in the fixture, the per-type value.
- Both orders must work: `WinLatched` logged last, and `WinLatched` logged mid-stream (it must survive later eviction pressure).
- Implementation shape (suggestion, not a rule): keep protected and ordinary entries in the ring in insertion order and evict by scanning from the oldest, or maintain an ordinary-entry index so eviction does not scan protected entries; either must meet the drain-cost guardrail with a flooded ring. Do not allocate per `Append` once the ring is at capacity.
- Never route eviction counters or entries through `UE_LOG`/`UE_LOGF`, `ensureMsgf` text or trace channels. The test reads the counters through the store's read API at Ended (story 011 gating), not a back door.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 011 (`production/epics/round-match-framework/story-011-event-log-content.md`): the store, header fields, `WinLatched` content and read gating (this story depends on it).
- Story 013 (`production/epics/round-match-framework/story-013-log-size-bound.md`): mean entry size against F8 and the 16,384 clamp of the ring.
- Story 015 (`story-015-voided-rate-limit.md`): the one-per-second-per-source limit on `Voided` entries that makes a flood smaller.
- Telemetry section storage, its `DroppedByType[]` and its own capacity (`kTelemetryMaxEvents` = 16384): ADR-0023 telemetry work in the Event Log store, not here; the telemetry section never shares ring slots.
- Story 016 (`story-016-pacing-metrics.md`): pacing metrics that read the log.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/EventLog/StealthRoundEventLog.h` (ring at the clamped capacity, protected set, O(1) oldest-ordinary eviction, per-type `TruncatedCount`)
- `Source/StealthFoundation/Private/EventLog/StealthRoundEventLog.cpp`
- `Source/StealthFoundation/Private/Tests/Round/LogTruncationTest.cpp` (test `StealthGame.Round.LogTruncation`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN 4,999 ordinary entries plus `WinLatched` with the 4096 cap (the header outside the ring, `WinLatched` inside it and protected), THEN the header and `WinLatched` survive, the oldest entries were dropped first and `TruncatedCount` = 904.
  - Given: cap 4096, header outside the ring, 4,999 ordinary entries plus one `WinLatched` inside the ring.
  - When: the entries are logged and the log is read at Ended.
  - Then: the header and `WinLatched` are present.
  - Then: the oldest 904 ordinary entries were evicted, and the earliest surviving ordinary entry is the 905th.
  - Then: `TruncatedCount` = 904.
  - Edge cases: `WinLatched` logged last and logged mid-stream both survive; eviction order is by age among unprotected entries only.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Round/LogTruncationTest.cpp` — automation test `StealthGame.Round.LogTruncation` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundLogTruncationTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [x] Created and passing (`StealthGame.Round.LogTruncation`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-011-event-log-content.md`
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: None

---

## Completion Notes
**Completed**: 2026-10-26 (round-match-framework Exit A batch 001-017)
**Criteria**: 1/1 passing (`StealthGame.Round.LogTruncation`)
**Notes**: With `WinLatched` last and mid-stream: 4,096 entries kept, header and `WinLatched` present, the earliest surviving ordinary entry is the 905th, survivors contiguous, `TruncatedCount` 904 (type and total), no protected type truncated. Eviction never allocates once the ring is full.
**Build/test**: see `production/qa/evidence/story-012-log-truncation/full-suite.txt`
