# Story 037: AC 32 HoldCompletionEpsilon

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story implements hold completion as a due-time queue item (GDD F1, R8; ADR-0011 section 5; ADR-0002 section 4). When an Interact Begin executes the IIF calls `ScheduleAt(DueTime = t_start + D, HoldTimer(Complete))` and keeps no per-drain check, no tick and no `FTimerManager` of its own. Round's due-time heap stamps the item: an item with `DueTime <= DrainStartTime` is stamped before the drain's intake items, and it is treated as due when `DueTime <= t_now + epsilon` with epsilon = 1 ms, a Round R12 constant that the IIF cites and does not redefine (Round AC 45 tests the comparison itself). At D = 3.0 s the hold completes on tick 180 at 60 Hz and tick 90 at 30 Hz, never on tick 179 or 89.

It follows story 035 (the slot arbitration the hold executes in) and is the base of the other hold stories: early release (038), cancel matrix (039), debounce (044) and key order (050). Because ADR-0002 owns the heap and Round owns epsilon, the governing ADR for this story is ADR-0002; ADR-0011 section 5 defines how the IIF uses it.

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**ADR Decision Summary**: One total-order resolution queue per round keyed (ServerTickIndex, Seq) with per-source mailboxes and a due-time heap, drained once per frame by a `TG_PostPhysics` tick function; every timed item (hold ends, windows, timers) is scheduled with `ScheduleAt` and no gameplay timer uses the engine timer manager or a feature tick.
**ADR Version**: 2026-10-24

**Other ADRs**: ADR-0011: Pawn command interface and input pipeline (hold actions and timers: `ScheduleAt(DueTime, HoldTimer{Complete | CapEnd | DebounceExpiry}, ActionEpoch)`; the IIF has no tick of its own for holds).

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (highest of ADR-0002 MEDIUM and ADR-0011 HIGH)
**Engine Notes**: ADR-0002 E6: server `UWorld::TimeSeconds` is a double (it pauses but does not dilate in this game), so hold times are doubles. ADR-0002 E1/E8: server RPC bodies run in dispatch before `TimeSeconds` advances for the frame, so a world-time stamp in an RPC body is biased by up to one server frame, which is why `ActionStartServerTime` is stamped at queue resolution and not at receipt. Float accumulation matters (180 x 1/60 is not exactly 3.0); epsilon absorbs it. No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — source: ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp; never debounce releases client-side — source: ADR-0011
- Forbidden: Never let the condition sweep cancel a hold itself (it schedules `HoldTimer(ConditionCancel)` items) — source: ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case (about 5 items/frame), inside the 0.5 ms queue line — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 32 (F1, Logic, U `HoldCompletionEpsilon`): **GIVEN** D=3.0 and injected dt of 1/60, 1/30, and a fixed varying table (0.0083, 0.05, 0.02, repeated) **WHEN** ticking **THEN** exactly one `HoldTimer(Complete)` is scheduled at Begin with `DueTime = t_start + 3.0` and is stamped on the first drain with `DueTime ≤ t_now + ε`, where ε is Round R12's 1 ms (cited, not redefined; Round AC 45 tests the comparison itself; 180 ticks at 60 Hz, 90 at 30 Hz, never tick 179 or 89); **AND** when `DueTime ≤ DrainStartTime` it is stamped before that drain's intake items; **AND** P stays in [0,1]; **AND** the completion offset from `DueTime` is in [−1 ms, dt).

---

## Implementation Notes

*Derived from ADR-0011 Decision section 5, ADR-0002 section 4 and GDD F1:*

On execution the server records `ActionStartServerTime = ServerNow()` (double); no client timestamp is accepted and nothing is back-dated. Every time-driven end is a queue item scheduled with `UResolutionQueue::ScheduleAt(DueTime, HoldTimer{Complete | CapEnd | DebounceExpiry, ActionEpoch})`; equal `DueTime`s order by the key of the hold's Begin, `Complete` before `DebounceExpiry` at an exact tie. The IIF has no tick of its own for holds. A timer whose `ActionEpoch` no longer matches is `Voided` silently. Interact: `Complete` at `t_start + D`, where `D` comes from the target owner (`IInteractTargetOwner::GetInteractHoldSec`, read once at execution; Objectives `ObjectiveInteractSec` 3.0). ADR-0002 section 4: at drain start, items with `DueTime <= DrainStartTime` are stamped before that frame's intake; items that become due during the drain are stamped after intake; within a group the order is `(DueTime, TieRank)` except `HoldTimer` items with equal `DueTime` ordered by the Begin key.

**F1 (GDD).** `P = clamp((t_now - t_start) / D, 0, 1)`; the end of the hold is a due-time item, not a poll. Completion lands in [D - epsilon, D + dt) after `t_start`. At D = 3.0 and 60 Hz tick 179 is at 2.9833 s and 2.9833 + 0.001 < 3.0, so tick 180 at 3.0 s completes.

**Rationale.** Timing a hold on Round's queue (instead of an IIF tick or `FTimerManager`) puts completion inside the same total order as every strike and transition, which is what makes "key order decides" (P3) hold; a client-timed hold would let the client own hold timing (ADR-0011 Alternative 3).

**Oracle split (QL-STORY-READY).** The stamping claims (tick 180 / 90, "stamped on the first drain with DueTime <= t_now + epsilon") are properties of Round's due-heap, which Round AC 45 owns; with a stub queue they test the stub. The qa-lead proposes splitting the oracle: (i) IIF assertions on the `ScheduleAt` call: item `HoldTimer(Complete)`, `DueTime = t_start + D` exactly, carrying `ActionEpoch` and the Begin key; (ii) with the Round-R12 due-heap stub (or the real heap when it exists), Complete resolves on tick 180 at 60 Hz and 90 at 30 Hz when Begin executes on tick 0, never 179 or 89; (iii) the varying-dt table asserts only the offset in [-1 ms, dt of that tick). Also state `t_start` alignment to a tick boundary and the dt for the offset row. The GDD AC text stays the authority.

**Tuning and keys.** `D` is `ObjectiveInteractSec` (Objectives-owned, 3.0, hard 2.0-6.0). No IIF key supplies epsilon: Round R12's 1 ms is cited, not redefined here.

**Prototype superseded.** New code, no prototype counterpart (the prototype held `ServerSetInteracting` state with no server hold timer).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-038-interact-early-release-resets.md`: early release resetting progress to 0
- `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`: cancel reasons and the condition sweep
- `production/epics/input-interaction-framework/story-044-debounce-merge.md`: the release debounce and `DebounceExpiry`
- `production/epics/input-interaction-framework/story-050-hold-timer-key-order.md`: key order against an Attack in the same frame (AC 69)
- Round's due-heap comparison and epsilon: cross-epic round-match-framework (Round AC 45)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h` (hold execution schedules `HoldTimer(Complete)`)
- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp`
- `Source/StealthGameplay/Private/Tests/Input/HoldCompletionEpsilonTest.cpp` (class `FInputHoldCompletionEpsilonTest`, name `StealthGame.Input.HoldCompletionEpsilon`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- The stamping claims ("stamped on the first drain with DueTime <= t_now + eps", tick 180/90) are properties of Round's due-heap, which Round AC 45 owns; with a stub queue they test the stub.
- t_start alignment to a tick boundary and the dt for the "completion offset" row are unstated.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): split the oracle: (i) IIF assertions on the ScheduleAt call: item HoldTimer(Complete), DueTime = t_start + D exactly, carries ActionEpoch and Begin key; (ii) with the Round-R12 due-heap stub (or real heap when it exists), Complete resolves on tick 180 @60 Hz / 90 @30 Hz when Begin executes on tick 0, never 179/89; (iii) varying-dt table asserts only offset in [-1 ms, dt_of_that_tick).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/HoldCompletionEpsilonTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-035-slot-priority.md`
  - cross-epic: round-match-framework (due-time heap, `ScheduleAt` and Round R12 epsilon; stories not yet written)
  - cross-epic: objectives-chain (`IInteractTargetOwner::GetInteractHoldSec`; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-038-interact-early-release-resets.md`
  - `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`
  - `production/epics/input-interaction-framework/story-042-prepare-cooldown-stamped.md`
  - `production/epics/input-interaction-framework/story-044-debounce-merge.md`
