# Story 013: Mean entry size is reported against F8 and the ring never exceeds 16,384 entries

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-013` (Event Log ring bound; kill timeline protected from eviction; `TruncatedCount`; telemetry section). GDD AC 34b (revised by the AC overrides file). Rules: R19, F8 (Event Log size sanity check).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**Playtest instrumentation**: this story feeds the ADR-0023 playtest instrumentation by measuring the entry size that ADR-0023's memory budget (section 4) and AQ-16 answer (section 5) rely on.

**ADR Governing Implementation**: ADR-0023: Playtest telemetry hooks
**ADR Decision Summary**: A bounded server-only `NeverReleased` telemetry section of the Round Event Log store, kind-blind probes/detectors, exported after termination off the game thread. This story applies ADR-0023 section 4 and section 5: the R19 ring is at most about 400 KB at 4096 entries (F8) and worst-case ring memory is unchanged at the hard maximum of 16,384 slots (about 1.6 MB); the telemetry section has its own fixed capacity and never shares or evicts ring slots.
**ADR Version**: ADR-0023 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: None touched. The size measurement uses `sizeof`/stored bytes of plain C++ entries; no engine API. ADR-0023 verification items TEL-V1 to TEL-V3 concern the telemetry export and detectors, not the ring bound. `UE_LOGF` (5.8) is not used for log content; the size report must not print entry content.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Event Log types are plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`) with no text conversion besides the `Ended`/`Reset` disk writer (ADR-0007)
- Forbidden: Never add telemetry entries to the R19 ring; never pass telemetry or Event Log content to `UE_LOG`/`UE_LOGF`, `ensureMsgf`/`checkf`, `ClientMessage`, `TRACE_*`, `CSV_*` or a stat/scope name (ADR-0023, ADR-0007)
- Guardrail: Event Log ring about 20-30 KB for a typical round of about 300 entries, at most about 400 KB at the 4096 default and about 1.6 MB at the 16,384 hard maximum (F8); the budget guards server memory since nothing is sent before `Ended`

**Why this story**: F8 estimates an entry at 64 to 96 bytes. That number sizes the ring (about 400 KB at 4096) and ADR-0023's budget, but it is an estimate that "is to be measured" (F8). This story measures it on a scripted 300-entry round and reports it, without asserting on the size (advisory), so the estimate can be corrected with data. It also enforces the hard bound that a mis-set `RoundLogMaxEntriesCount` can never exceed 16,384 entries, so no setting can exhaust server memory. Depends on story 011's store. Exit A.

**Prototype disposition**: none (new Foundation code).

---

## Acceptance Criteria

*From the AC overrides file (AC 34b, revised), one criterion, `story_granularity: fine`:*

- [ ] A scripted 300-entry round reports mean entry size against F8's 64-96 B estimate (advisory, no assertion on the size); AND with RoundLogMaxEntriesCount set above its clamp and 20,000 entries logged, the ring size is never above 16,384 (the setting is clamped to 16,384). `StealthGame.Round.LogSizeBound` [U]

---

## Implementation Notes

*Derived from GDD F8 and Tuning Knobs, GDD R19, ADR-0002 section 8.9 and ADR-0023 sections 4 and 5:*

- F8: `LogBytes ~= HeaderBytes + E x EntryBytes`, `E <= RoundLogMaxEntriesCount`; `HeaderBytes` about 1 KB, `EntryBytes` 64 to 96 B (key 16 B, time 8, epoch 4, type, mailbox, participant ids, data); typical round about 300 entries at 80 B plus 1 KB is about 25 KB. The scripted 300-entry round uses a representative mix of entry types from the R19 list (fixture, no real systems).
- Mean entry size: measure what the store actually holds per entry, in a way that does not depend on the allocator (for example the entry struct size plus the variable-length data it carries, as the store defines it). Report the mean and the F8 range in the automation test output (counts and sizes only, never entry content). Open: confirm the reporting channel (automation info line, versus a file under `Saved/`) and the exact definition of "entry size" with the store's owner, since F8 lists the fields but not the measurement method; the AC requires no assertion on the value.
- Clamp: `RoundLogMaxEntriesCount` has hard range 512 to 16384 (default 4096; GDD Tuning Knobs). The store clamps a larger configured value to 16,384. The test sets a value above the clamp (a fixture constant from story 005's constant file, anything above 16,384), reads back 16,384, then logs 20,000 entries and checks the ring size at every insert never exceeds 16,384.
- Protected entries are still retained at the clamp. If all 20,000 inserted entries are ordinary, the number evicted would be 3,616 (20,000 minus 16,384). Open: confirm that derived figure against R19 and story 012's per-type `TruncatedCount` semantics before asserting it; the AC itself asserts only the size bound.
- Do not hot-path allocate beyond the reserved ring; the ring capacity is reserved at the clamped size when the round opens.
- The clamp is the store's job; the TDC schema owns the knob's declared range, which is a tuning-data-config concern. This story does not edit tuning data.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 011 (`production/epics/round-match-framework/story-011-event-log-content.md`): the store, header and read gating (this story depends on it).
- Story 012 (`production/epics/round-match-framework/story-012-log-truncation.md`): eviction order, protected entries and `TruncatedCount` at the 4096 default.
- Telemetry section capacity (`kTelemetryMaxEvents` = 16384 events, at most 64 B each) and its own bound: ADR-0023 telemetry work in the Event Log store, not here.
- The tuning schema entry for `RoundLogMaxEntriesCount` and its declared range: tuning-data-config epic.
- Story 016 (`story-016-pacing-metrics.md`): metrics computed from the log.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/EventLog/StealthRoundEventLog.h` and `Source/StealthFoundation/Private/EventLog/StealthRoundEventLog.cpp` (extended from stories 011 and 012; capacity clamp, size accessor)
- `Source/StealthFoundation/Private/Tests/EventLog/LogSizeBoundTest.cpp` (new; class `FRoundLogSizeBoundTest`)
- `Source/StealthFoundation/Private/Tests/Fixtures/StealthRoundTestConstants.h` (extended; the above-clamp fixture value, story 005's constant file)
- Tests live under the engine test roots (`tests/README.md`), never under `tests/`. `/dev-story` replaces this planned list with the real one. `Source/StealthFoundation` does not exist yet (tuning-data-config epic creates the skeleton); write under `Source/StealthGame/` until it lands.

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: A scripted 300-entry round reports mean entry size against F8's 64-96 B estimate (advisory, no assertion on the size); AND with RoundLogMaxEntriesCount set above its clamp and 20,000 entries logged, the ring size is never above 16,384 (the setting is clamped to 16,384).
  - Given: a scripted 300-entry round; a second run with `RoundLogMaxEntriesCount` set above its clamp and 20,000 entries logged.
  - When: both runs complete.
  - Then: the mean entry size is reported against F8's 64-96 B estimate, with no assertion on the size.
  - Then: the setting reads back clamped to 16,384.
  - Then: the ring size is at most 16,384 at every check during and after the 20,000 inserts.
  - Edge cases: the oversize setting value is a fixture constant (anything above 16,384); protected entries are still retained; if all 20,000 are ordinary, `TruncatedCount` would be 3,616 (derived; Open: confirm before asserting).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/EventLog/LogSizeBoundTest.cpp` — automation test `StealthGame.Round.LogSizeBound` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundLogSizeBoundTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-011-event-log-content.md`
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: None
