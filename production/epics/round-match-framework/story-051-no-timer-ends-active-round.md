# Story 051: A 24 h fake-clock advance in Active ends nothing and leaves only grace and pawn-loss timers

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-005` (Objective and elimination wins; no draw, no clock) and `TR-round-003` (Drain once per frame in `TG_PostPhysics`; due-time heap; resolvers per item type; inline follow-ons; Roster events flushed before the next item); GDD AC 47; rules R16 (4) ("no match time limit and no timer anywhere that can end or change the outcome of an `Active` round"), R12 (due-time heap).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies section 4: the closed `ETimerKind` set, of which only `GraceExpiry` and `PawnLossRetry` can be live during a contest, and no gameplay timer uses the engine timer manager.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched. A fake clock jumps and the plain C++ queue core drains once; no tick function, `UWorld` or post-cutoff API is exercised. ADR-0002 verification item (3), one drain per server frame on a real listen host, is not exercised and stays with the PIE stories.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Resolution queue and Forbidden Approaches):
- Required: gameplay timers are `ScheduleAt` items in the due-time heap (ADR-0002).
- Forbidden: Never use `FTimerManager`, a feature tick or a latent action for a gameplay timer (ADR-0002).
- Guardrail: queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0022); the 24 h advance is one jump and one drain.

**Prototype disposition**: Replaces the prototype's round-end timing in `Source/StealthGame/Private/StealthGameMode.cpp` (timers via `GetWorldTimerManager()`, for example `SetTimerForNextTick` for `CheckEliminationVictory` and the auto-restart). Production has no timer that can end an Active round and none on the engine timer manager.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 47, scoped to this story:*

- [ ] GIVEN a fake clock advanced 24 h in `Active` with no items, THEN the phase is `Active` and the timer heap holds only grace and pawn-loss retries.

---

## Implementation Notes

*Derived from GDD R16 and ADR-0002 section 4:*

- **No timer can end a contest (R16 (4)).** Round adds no dwell timer, no sudden-death rule and no countdown, and shows no clock. The only Round timers are the disconnect grace, the Preparing watchdog, the world-ready wait and start lead (P7), the pawn-loss emergence retry, `EndedMinDwellSec` and the optional `EndedAutoResetSec`. During `Active` only the grace and the pawn-loss retry can exist, and their effects are a normal `Disconnect` transition or a rebind, both ordered by the queue. The Preparing watchdog is not a queue item and is cancelled at Active entry (ADR-0002 section 8.6); the post-latch lane items are scheduled only from the round-end step.
- **The test.** Reach `Active` through story 042's entry with no items submitted, advance the fake clock (story 001) by 24 h in one call, drain once, and assert (1) the phase is still `Active`, (2) no outcome and no `WinLatched`, and (3) every pending timer in the due-time heap has `ETimerKind` in {`GraceExpiry`, `PawnLossRetry`}.
- **Assert a subset, not equality.** With no disconnects the heap may be empty, so assert the set of pending timer kinds is a subset of those two, which covers the empty case. Open: confirm at /dev-story the read-only heap inspection the queue core exposes to tests (story 008 owns the core); the AC needs only the pending kinds.
- **Advance by jump.** 24 h at 60 Hz is about 5.2 million ticks, so never loop; one `Advance(24 h)` and one drain is the case. If a real timer were due it would fire in that drain, which is exactly what the test would catch.
- **Why this is a Core story, not Foundation.** The closed timer-kind list is in `StealthQueuePayloads.h` (Foundation), but the claim "nothing in Round schedules a round-ending timer" is Round's: the test exercises the phase core plus queue core together, so a regression that adds a Round timer fails here.
- **Where the code goes.** Test-only, unless the heap inspection accessor is added to the queue core: `Source/StealthGameplay/Private/Tests/Round/NoTimerEndsActiveRoundTest.cpp`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): the Active-entry step this test starts from.
- Story 008 (`production/epics/round-match-framework/story-008-resolver-contract.md`): the resolver and queue contract the heap and drain use.
- Story 047 (`production/epics/round-match-framework/story-047-empty-team-no-elimination.md`): the 1v0 case that also advances the clock (there it asserts no elimination).
- Story 060 and 075: the real grace and pawn-loss timers and their timelines.
- Stories 007 and 009: intake ordering rules and follow-on depth.
- Objectives' stall pressure (camping, both teams stalling) is that epic's responsibility and is never a timer (R16 (5)).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/NoTimerEndsActiveRoundTest.cpp` (class `FRoundNoTimerEndsActiveRoundTest`, test name `StealthGame.Round.NoTimerEndsActiveRound`)
- `Source/StealthFoundation/Public/Server/` (queue core heap inspection accessor, only if story 008 has not added one)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A 24 h fake-clock advance in Active ends nothing and leaves only grace and pawn-loss timers.
  - Given: an `Active` round with no items.
  - When: the fake clock advances 24 h in one jump.
  - Then: the phase is `Active`.
  - Then: the timer heap holds only grace and pawn-loss retries.
  - Edge cases: with no disconnects the heap may be empty, so assert its contents are a subset of those two types; advance by jump, because 24 h at 60 Hz is about 5.2 million ticks.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/NoTimerEndsActiveRoundTest.cpp` (automation test `StealthGame.Round.NoTimerEndsActiveRound`, class `FRoundNoTimerEndsActiveRoundTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-008-resolver-contract.md`, `production/epics/round-match-framework/story-042-active-gate.md`
- Unlocks: None
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
