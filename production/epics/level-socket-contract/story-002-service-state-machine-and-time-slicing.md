# Story 002: Layout service state machine and time-sliced work driver

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-001`, `TR-lsc-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-lsc-001 = Layout service world subsystem with registered providers; TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; <= 33 ms slices)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The service runs exactly the LSC States table (`Idle -> Building(k) -> Validating(k) -> Selecting(k) -> Published -> Live -> Released`, plus `AttemptFailed(k)`, `FallbackTravel`, `Aborted` from any state) and bounds every tick by `LayoutSliceWorkUnitsLimit` work units; a wall-clock guard may only defer work.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Pure C++ logic in `FLayoutServiceCore` with an injected `ILayoutClock`; no engine API beyond the tickable subsystem shell of story 001.

**Why this story**: Every later behaviour (timeouts, fallback, placement, emergence) is gated by the state it runs in, and the 33 ms frame cap is a deterministic work-unit cap, not a stopwatch. Read first: States and Transitions, R4 item 3, Acceptance Criteria AC-29 and AC-36.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Time slicing bounds work by `LayoutSliceWorkUnitsLimit`; a wall-clock guard may only defer work (ADR-0014)
- Forbidden: Never run the Generator candidate loop wall-clock bounded (ADR-0014 guideline 9)
- Guardrail: Layout (server, `Preparing`) game-thread frames <= 33 ms; Validate + Select in <= 4 ms GT slices (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-29 and AC-36 (logic half), scoped to this story:*

- [ ] GIVEN each state in the States table, WHEN every event is injected, THEN only the listed transitions occur, including `Aborted` from any state on host loss, `PlacementInfeasible` and `NpcCountTooLow` (`State.TransitionTable`)
- [ ] GIVEN placement requested outside `Published`, or emergence/takeover requested outside `Live`, including a kill resolved in the same tick as round end, THEN the request is rejected with `OutOfState` and logged and no state changes (`State.RejectOutOfState`)
- [ ] GIVEN a graybox-sized `Preparing` run with a stub backend, THEN no slice processes more than `LayoutSliceWorkUnitsLimit` units, and the result is byte-identical with the wall-clock guard on and off (`Network.PreparingNoHitch`, logic half)

---

## Implementation Notes

*Derived from ADR-0014 sections 1 and 5.4:*

- `ELayoutServiceState { Idle, Building, Validating, Selecting, AttemptFailed, Published, Live, Released, FallbackTravel, Aborted }` with attempt index k. The service enters `Live` on Round's `OnRoundActive` and `Released` on `OnRoundEnded`; `Released -> Idle` happens by world teardown (per-round travel), not in-place reset.
- Start placement is served only in `Published`; `RequestEmergence` and `PickRespawnPawn` only in `Live`; any other request returns `OutOfState` and logs (one line, no counters change).
- Work units are cells, rays, Dijkstra relaxations / 64 and combos / 64, with unit weights fixed in code and covered by a unit test. The tick processes at most `LayoutSliceWorkUnitsLimit` units **and** yields early after `LayoutSliceMaxMs` (4 ms, a code constant until spike R4); the work-unit cap is the deterministic bound, the millisecond guard (read from the injected clock) only moves work to the next frame and must never change a result.
- A state x event matrix test drives the table: unlisted pairs are no-ops.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 003: attempt timeout, total budget, reseed counting
- Story 004: abort path dispositions and Event Log header
- Story 011: the measured (advisory) 33 ms and 1 s numbers on the reference machine

---

## Files

- `Source/StealthGameplay/Public/Server/Core/StealthLayoutService.h`
- `Source/StealthGameplay/Private/Core/LayoutServiceCore.cpp`
- `Source/StealthGameplay/Private/Core/LayoutServiceCore.h`
- `Source/StealthGameplay/Private/Tests/Layout/StateMachineTest.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/TimeSliceTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `State.TransitionTable`
  - Given: the service forced into each of the 10 States-table states.
  - When: every event is injected (state x event matrix), including host loss, `PlacementInfeasible` and `NpcCountTooLow`.
  - Then: only table transitions occur; unlisted pairs are no-ops; `Aborted` is reached from all 10 states with `LayoutFallbackLevel` 3.
  - Edge cases: `AttemptFailed(5)` goes to `FallbackTravel`/`Aborted`, never `Building(6)`; graybox vs Generated branch.
- **AC-2** `State.RejectOutOfState`
  - Given: placement requested outside `Published`; emergence/takeover outside `Live`; a kill resolved in the same tick as `Ended`.
  - When: the request is issued.
  - Then: result `OutOfState`, one log line, no state or counter change.
  - Edge cases: request on the exact Published->Live and Live->Released tick.
- **AC-3** `Network.PreparingNoHitch` (logic half)
  - Given: a stub backend with 3 x `LayoutSliceWorkUnitsLimit` + 1 units (4,096 default, so 12,289) and the injected clock.
  - When: Preparing runs fixed-step with the wall-clock guard off, then on (injected clock forced to trip it).
  - Then: max units per slice <= 4,096 in both runs; result bytes identical.
  - Edge cases: exactly 4,096 units is 1 slice; 0 units; limit at 256 and 65,536. The guard must read the injected clock, not real time.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/StateMachineTest.cpp` and `TimeSliceTest.cpp` — automation tests `StealthGame.Layout.State.TransitionTable`, `StealthGame.Layout.State.RejectOutOfState`, `StealthGame.Layout.Network.PreparingNoHitch` must exist and pass

**Status**: [ ] Not yet created

Naming per `.claude/rules/test-standards.md` and ADR-0008 (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, injected clock, no wall-clock pass predicates).

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-001-service-skeleton-provider-registry.md`
- Unlocks: `production/epics/level-socket-contract/story-003-attempt-loop-timeout-budget-reseed.md`, `production/epics/level-socket-contract/story-004-abort-paths-and-event-log-header.md`
