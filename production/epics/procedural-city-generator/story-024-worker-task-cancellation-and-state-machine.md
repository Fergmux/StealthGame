# Story 024: Worker task, cancellation and the generator state machine

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0006 for the thread-local `FPublicOnlyScope`)
**ADR Decision Summary**: S1-S12 and G-PRE are pure functions over plain C++ data run as **one `UE::Tasks::Launch` task per attempt**; the task opens its own `FPublicOnlyScope` (the guard is `thread_local`), uses only `FIntStream` stage streams, checks an atomic cancel flag at stage boundaries and returns `{Manifest entries, socket set, skeleton transform, metrics}` or a failure code to the game thread. Spawning, collision, navmesh, Validate and Select stay on the game thread. A cancelled task's late result is discarded.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014)
**Engine Notes**: `UE::Tasks::Launch` is [V] in `Core/Public/Tasks/Task.h` (ADR-0014 section 9.1); the task holds no `UObject` (kit tables are copied into plain structs at `Build` entry). Results never depend on scheduling (AC-G9's 3x slowdown test still applies, story 023). Replaces architecture.md's earlier "time-sliced on the game thread" plan.

**Control Manifest Rules (this layer)**:
- Required: The Generator runs as a single cancellable worker task over pure data, inside `FPublicOnlyScope` — source: ADR-0014, ADR-0006
- Forbidden: Never time-slice the Generator on the game thread (fallback only); never touch a `UObject` from the worker — source: ADR-0014
- Guardrail: worker <= 1 s per attempt off the game thread (J <= 8) — source: ADR-0014

---

## Acceptance Criteria

*From ADR-0014 section 9.1 and 3.4 and the GDD "States and Transitions" table, scoped to this story (the GDD has no numbered AC for the task wrapper):*

- [ ] **AC1** (ADR-0014 9.1, `Worker.PureDataTask`): **GIVEN** `BeginAttempt` **THEN** S1-S12 and G-PRE run as one `UE::Tasks::Launch` task over plain data copied at `Build` entry (no `UObject` on the worker), the task opens its own `FPublicOnlyScope`, and a `GetServerSubSeed` call on the worker asserts.
- [ ] **AC2** (ADR-0014 3.4, `Worker.CancelAndLateResult`): **GIVEN** a cancel **WHEN** the flag is set mid-attempt **THEN** the task stops at the next stage boundary, `CancelAttempt` is idempotent, and a result that completes late is discarded.
- [ ] **AC3** (GDD States and Transitions, `Worker.StateTable`): **GIVEN** scripted runs **THEN** the states Idle, Checking, Drafting(k, j), PreValidated, Spawning, NavBuilding, HandedOff, TearingDown, Exhausted and ConfigInvalid transition exactly as the GDD table says and any state cancelled by the watchdog goes to TearingDown.

---

## Implementation Notes

*Derived from ADR-0014 section 9.1 and the GDD state table:*

`BeginAttempt` runs on the game thread in `FPublicOnlyScope`, checks config (`Checking`, story 025), copies the kit table and params into a plain `FCityGenTaskInput`, and launches one task. The task owns an `FPublicOnlyScope` (thread_local), runs the candidate loop (story 023), checks `std::atomic<bool> bCancel` at every stage boundary, and returns its result through a thread-safe handoff the game thread polls in `TickAttempt`. `CancelAttempt` is idempotent and also ignores late results by attempt id (a generation counter). The state machine is a plain C++ enum plus a transition function table (headless-testable): Idle -> Checking -> Drafting(k, 0) or ConfigInvalid; Drafting -> PreValidated, Drafting(k, j+1) or Exhausted; PreValidated -> Spawning -> NavBuilding -> HandedOff -> Idle (kept) or TearingDown; any state cancelled by the service watchdog goes to TearingDown(k) then Idle. The GDD says any state may be cancelled; what a watchdog hit in Idle does (nothing exists to tear down) is unspecified: take "no-op, stay Idle" and flag it. An illegal transition (for example Idle -> Spawning) asserts. The worker budget is measured in story 042, not asserted here.

Prototype replaced: the prototype generated synchronously on the game thread in `BeginPlay`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-023-candidate-loop-work-unit-cap-and-metrics.md`: the loop the task runs.
- `story-025-failure-modes-and-teardown.md`: failure ids, timeouts and teardown contents.
- `story-026-server-kit-spawn-from-manifest.md`: Spawning and NavBuilding work.
- `story-042-attempt-time-budget-and-t-cand-measurement.md`: worker time measurement.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenWorkerTask.h`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenWorkerTask.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStateMachine.h`
- `Source/StealthGameplay/Private/Tests/CityGen/PureDataTaskTest.cpp` (class `FCityGenWorkerPureDataTaskTest`, name `StealthGame.CityGen.Worker.PureDataTask`)
- `Source/StealthGameplay/Private/Tests/CityGen/CancelAndLateResultTest.cpp` (class `FCityGenWorkerCancelAndLateResultTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/StateTableTest.cpp` (class `FCityGenWorkerStateTableTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Worker.PureDataTask`
  - Given: a task-launch spy and a golden seed.
  - When: `BeginAttempt` runs.
  - Then: there is exactly 1 `UE::Tasks::Launch` per attempt, covering S1-S12 and G-PRE. Every stage runs off the game thread. The input struct has no `UObject*` or `TObjectPtr`, and `UObject` creation on the worker is 0. The `FPublicOnlyScope` depth is 1 at stage entry. `GetServerSubSeed` on the worker fires the assert.
  - Edge cases: mutating the source data after `Build` entry leaves the result unchanged.
- **AC2** `Worker.CancelAndLateResult`
  - Given: a latch blocking the task at stage 5.
  - When: `CancelAttempt` is called, then the latch is released.
  - Then: no stage after 5 starts, so the task stops at the next stage boundary. Repeated `CancelAttempt` calls do nothing further. A late result gives no `OnLayoutReady` and 0 instances spawned.
  - Edge cases: a run with no cancel completes (control). Also cancel before the first stage and after the last.
- **AC3** `Worker.StateTable`
  - Given: scripted runs for every row of the GDD state table.
  - When: each run executes.
  - Then: the transition log equals the table-driven expectation. Paths covered: Idle, Checking, then Drafting(k,0) or ConfigInvalid; Drafting to PreValidated, to Drafting(k,j+1), or to Exhausted; Spawning, NavBuilding, HandedOff, then Idle or TearingDown; a watchdog Timeout from each state goes to TearingDown(k), then Idle. An illegal transition such as Idle to Spawning asserts.
  - Edge cases: J = 1 (the hard minimum) goes to Exhausted at j = 0. A watchdog hit in Idle is a no-op (flagged for the GDD owner).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` (the task and state-table tests run headless with a task-launch spy) OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-002-provider-shell-registration-and-secret-input-isolation.md`, `story-023-candidate-loop-work-unit-cap-and-metrics.md`
- Unlocks: `story-025-failure-modes-and-teardown.md`, `story-026-server-kit-spawn-from-manifest.md`, `story-030-pure-function-determinism-and-stage-hashes.md`
