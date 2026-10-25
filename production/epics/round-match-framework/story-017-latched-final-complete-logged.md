# Story 017: A second final Complete after the latch returns Rejected_Latched and is logged TendCompleted with Latched = true

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-004` (Single-assignment latch and the two-step round end) and `TR-round-013` (Event Log ring bound; kill timeline protected from eviction; `TruncatedCount`; telemetry section). Also `GDD AC 63` (cross-review C-W35), `R19` (`TendCompleted` with `Latched` flag), `R15` (latch).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging; also ADR-0023: Playtest telemetry hooks
**ADR Decision Summary**: ADR-0002: one total-order queue keyed `(ServerTickIndex, Seq)`, single-assignment latch storage with Round's record authoritative, and after the latch every pending or newly submitted item resolves `Rejected_Latched` (logged). ADR-0023: the objective-win share (Objectives F5) must count every completion attempt, so a latched final `Complete` is logged rather than dropped silently. This story applies ADR-0002 section 8.1 (after the latch the queue is closed and every item resolves `Rejected_Latched`) together with R19's `TendCompleted(..., NoEffect)` plus `Latched = true` entry.
**ADR Version**: ADR-0002 2026-10-24; ADR-0023 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (governing ADR-0002)
**Engine Notes**: ADR-0002 post-cutoff APIs: none (`TG_NewlySpawned` handling and `TWeakInterfacePtr` re-verified in 5.8.4). This story is the headless queue core with stub resolvers, so verification items (1) spawning a CMC pawn inside `TG_PostPhysics`, (2) the client estimator under net emulation and (3) one drain per server frame on the listen host are not touched. ADR-0023 has no post-cutoff APIs. This story feeds the playtest instrumentation (ADR-0023) because the objective-win share needs every completion attempt.

**Control Manifest Rules (this layer)**:
- Required: Only Round's `TryTerminate` calls `Latch`; after the latch the queue rejects every item with `Rejected_Latched` except parked `HostCommand`s, the post-latch lane and the `ConcealmentTransition` exemption (ADR-0002).
- Forbidden: Never re-submit or re-stamp an outcome (ADR-0002).
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms on the listen-host game thread (ADR-0002).

**Prototype disposition**: none (new Foundation code). In Exit A the latch in this test is produced by a stub resolver; the real latch (Round's `TryTerminate` and `FRoundPhaseCore`) is Core story 052 and replaces the prototype's first-come `EndRound` in `StealthGameMode.cpp` there, not here.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 63, scoped to this story:*

- [ ] GIVEN a final `HoldTimer(Complete)` that latches the round and a second final `Complete` in the same drain, THEN the second returns `Rejected_Latched` and the Event Log holds `TendCompleted(ParticipantId, court, NoEffect)` with `Latched = true` for it, so the objective-win share counts both attempts.

---

## Implementation Notes

*Derived from ADR-0002 sections 2a and 8.1, ADR-0002 section 8.9 and GDD R19:*

- Item shape: `EQueueItemType::HoldTimer` with `FHoldTimerPayload { FPawnHandle Pawn; EHoldTimerKind Kind /* Complete */; uint8 ActionType; uint32 ActionEpoch; FQueueKey BeginKey; }` (ADR-0002 section 2a). A "final" completion is a Complete on the final objective; Round does not know what a command means (R13), so the test uses stub resolvers (story 004) registered for `HoldTimer` that the first item treats as the latching completion and that the queue then rejects for the second.
- After the latch step the queue is closed: every item still pending, and every item submitted before the round-end step, resolves `Rejected_Latched` (ADR-0002 section 8.1, `EVoidReason::Rejected_Latched`), and the result is logged. Hold completion items of frozen holds can only return `Rejected_Latched`.
- Logging rule (R19): "a final `HoldTimer(Complete)` item that returns `Rejected_Latched` is also written as `TendCompleted(ParticipantId, court, NoEffect)` with `Latched = true`, so Objectives F5's objective-win share counts every completion attempt and not only the one that latched." The first (latching) completion is logged with `Latched = false`. The entry goes through `IRoundLogWriter::Append` (story 011), game thread, from inside the slot, in key order. The `TendCompleted` entry type carries `Counted | NoEffect` and a `Latched` flag (ADR-0002 section 8.9). Open: confirm against GDD R19 and ADR-0002 section 8.9 whether the queue's `Rejected_Latched` path or the owning resolver writes the `TendCompleted` entry; the Foundation test uses stubs, so the writer under test is the one the queue core exposes for a rejected final `HoldTimer(Complete)`.
- The `Rejected_Latched` entry itself and `LateWinSuppressed` are separate log types (R19); this story asserts only the `TendCompleted` entry and the result code, plus that both attempts are counted toward the objective-win share.
- Do not re-stamp or re-submit the rejected item; a rejected item keeps its key (ADR-0002: never re-submit or re-stamp an outcome).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 008 (`production/epics/round-match-framework/story-008-resolver-contract.md`): the resolver return contract (`Voided`, in-slot, deferred outcomes and `Seq` consumption) this story builds on.
- Story 011 (`production/epics/round-match-framework/story-011-event-log-content.md`): the Event Log store and entry types.
- Story 045 (`production/epics/round-match-framework/story-045-same-tick-final-vs-elimination.md`) and story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`): the real latch, `WinLatched`, and the round-end step in Core.
- Objectives F5 computation of the objective-win share (Objectives epic) and the real `TendCompleted` producer (Feature layer).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthFoundation/Private/Tests/Queue/LatchedFinalCompleteLoggedTest.cpp` (class `FRoundLatchedFinalCompleteLoggedTest`, test name `StealthGame.Round.LatchedFinalCompleteLogged`)
- Production change in the Event Log and queue cores built by stories 008 and 011 under `Source/StealthFoundation/Private/Queue/` and `Source/StealthFoundation/Private/EventLog/` (the `Latched` flag on `TendCompleted`; Open: file names are set by those stories)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A second final Complete after the latch returns Rejected_Latched and is logged TendCompleted with Latched = true.
  - Given: a final `HoldTimer(Complete)` that latches the round, and a second final `Complete` in the same drain.
  - When: the drain resolves both.
  - Then: the second returns `Rejected_Latched`.
  - Then: the log holds `TendCompleted(ParticipantId, court, NoEffect)` with `Latched = true` for the second.
  - Then: both attempts count toward the objective-win share.
  - Edge cases: the first (latching) completion is logged with `Latched = false`; in Exit A the latch is produced by a stub resolver (the real latch is Core story 052).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Queue/LatchedFinalCompleteLoggedTest.cpp` — must exist and pass (automation test `StealthGame.Round.LatchedFinalCompleteLogged`, class `FRoundLatchedFinalCompleteLoggedTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-008-resolver-contract.md`, `production/epics/round-match-framework/story-011-event-log-content.md`
- Unlocks: None

Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
