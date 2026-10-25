# Story 039: AC 16 InteractCancelMatrix

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-005`, `TR-iif-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story builds the cancel side of the Interact hold: every cause in the GDD R8 cancel table ends an active Interact through a queue slot, fires `OnActionCancelled` once with the reason named in that table, and the listed non-causes (Look, camera toggle, an approaching pawn, being targeted by an Attack whiff) leave the hold running. It is the first story that exercises the condition sweep and the `HoldTimer(ConditionCancel)` item together with the action hub on a live hold, which is why it carries size L and why six further stories (040, 049, 050, 055, 057, 066) build on it.

It sits after story 037 (hold completion epsilon: the `HoldTimer(Complete)` schedule and `ActionEpoch` voiding) and story 003 (the `UActionHub` that the cancel events fire through). Within ADR-0011 it implements section 5 (Interact cancels, the condition sweep, early release resets progress) and section 4 (a legality-class or phase change cancels `Illegal` in the same slot; pawn removal cancels `PawnRemoved`), with the hub event vocabulary from section 6. GDD rules covered: R8 (cancel table) and R11 (`ECancelReason` set).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: Pawn commands, hold timers, the action slot and the server-only action hub run in one server-side total order: discrete commands enter through seven server-validated entry points with no caller identity, hold ends are `ScheduleAt` queue items, and `UActionHub` events fire only inside queue slots in key order.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0011 rates the command path itself MEDIUM (pre-cutoff RPC and CMC mechanisms, re-verified); the HIGH rating comes from the input layer (Enhanced Input / Common UI unification), which this story does not touch. No verification item V1-V6 gates it. Relevant engine fact: E10 (server RPC bodies run at the top of the world tick, so a command received this frame is in its mailbox before the `TG_PostPhysics` drain). The cancel logic lives in plain-C++ `FPawnCommandCore` so ADR-0008 headless specs cover it.

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — ADR-0011
- Required: Feature predicates are registered weakly from the composition root per `EActionType` — ADR-0011
- Forbidden: Never let the condition sweep cancel a hold itself (it schedules `HoldTimer(ConditionCancel)` items) — ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp — ADR-0011
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener — ADR-0011
- Forbidden: Never relay action-hub events to a client except through the Visibility Policy — ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case (about 5 items/frame), inside the 0.5 ms queue line; at most one reply RPC per connection per drain — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 16 (R8, Logic, U `InteractCancelMatrix`): **GIVEN** an active Interact **WHEN** each R8 cause is applied in turn **THEN** `OnActionCancelled` fires with the reason given in R8's cancel table:

    | Cause | Reason |
    |---|---|
    | End | `Released` |
    | Commanded displacement | `Moved` |
    | Run | `Moved` |
    | Traverse | `Moved` |
    | Beyond the cancel range | `OutOfRange` |
    | Attack, Prepare or Hide | `Superseded` |
    | Class change | `Illegal` |
    | Target invalidated | `TargetInvalid` |
    | Pawn removed | `PawnRemoved` |

    **AND** Look (a 180° turn), camera toggle, an approaching pawn and being targeted by an Attack whiff do not cancel.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 4, 5 and 6 and its Implementation Guidelines:*

- **Where each cancel resolves.** Every cancel ends the hold in a queue slot, never inline. Cancels caused by a condition (range, displacement) are detected by the IIF's **condition sweep**, a tick function in `TG_PostPhysics` ordered immediately before ADR-0002's drain. The sweep only **reads** state; for a hold whose condition has failed it schedules `HoldTimer(ConditionCancel, ActionEpoch, Reason)` with `DueTime = ServerNow()`, so the item is stamped in the drain's pre-intake group with Begin-key ordering. The `ConditionCancel` slot re-checks the `ActionEpoch` and the condition and then cancels (`Moved` or `OutOfRange`). It has no reply sink; after the latch it returns `Rejected_Latched` with no `NotNow` (holds are frozen). `ConditionCancel` is a new `HoldTimer` sub-kind and an amendment request to ADR-0002's closed item set (ADR-0011 Related); code against the amended set and record in the story if the amendment has not landed.
- **Cause to reason mapping (R8 table, ADR-0011 sections 4-6).** End after the debounce -> `Released` (the `DebounceExpiry` slot, story 044 owns the debounce itself); commanded displacement beyond `InteractMoveCancelDistCm` after `InteractMoveGraceSec`, `Gait = Run` and `Traverse` -> `Moved`; leaving the cancel range -> `OutOfRange`; an Attack or Prepare by the same pawn -> `Superseded` (higher priority preempts, ADR-0011 section 4); a legality-class or phase change -> `Illegal` in the same slot; the target invalidated by its owner -> `TargetInvalid`; the pawn removed -> `PawnRemoved`. `ECancelReason = {Released, Moved, Superseded, Illegal, OutOfRange, TargetInvalid, PawnRemoved, Reversed}` is fixed (story 001).
- **Known GDD defect, not fixed here.** R8's cancel table and AC 16 list **Hide** as a `Superseded` cause, but Hide is the lowest priority: R9 and AC 20 (story 035) reject a Hide against an active Interact as `Busy` and the hold continues. The Hide clause is unsatisfiable and contradicts story 035. Implement and test the Attack and Prepare rows for `Superseded`; do not implement a Hide preemption of Interact. The qa-lead proposes rewriting the row to "Attack or Prepare -> Superseded" (see QA Test Cases); the GDD text above stays the authority until the GDD is amended.
- **Non-causes.** Look never cancels (after Begin only distance is re-checked); the camera toggle is client-local with no server stimulus (AC 29, story 014); an approaching pawn and a crowd capsule push never count as commanded displacement (external pushes cancel only through the range check, F6); being targeted by an Attack whiff is undetectable by design.
- **Interact target owner hooks.** `OnInteractEnded(Pawn, FCourtFixtureId, EInteractEnd::Cancelled, FQueueKey)` runs inline in the ending slot **before** `OnActionCancelled` fans out (ADR-0011 section 6); outside a slot (round-end step, teardown) the call carries an invalid key and is a claim release only.
- **Early release resets progress to 0.** No partial progress is kept (R8); story 038 owns that assertion.
- **Test seams.** The qa-lead needs: an Allow predicate stub, Roster legality-class stub (Active -> Informant), a target-owner stub that can invalidate, and a `CommandedDisplacementCm(SinceServerTime)` stub on `ISharedPawnStateView`. Story 095 owns the shared named doubles; until it lands declare test-local stubs against the interfaces from story 002. A fresh active Interact per case; tick the stub clock until the queue drains because each cancel resolves in a queue slot.
- **Relevant tuning keys (defaults).** `InteractRangeCm` 150, `CommandRangeSlackCm` 40, `InteractCancelHysteresisCm` 50 (cancel range 240), `InteractMoveCancelDistCm` 15, `InteractMoveGraceSec` 0.25. Read from tuning data at test time, never literals.
- **Prototype.** Replaces `OnInteractPressed/Released` and `ServerSetInteracting` in `StealthCharacter.cpp` (the prototype lets the client choose the hold state, with no queue slot, cancel reasons or hub). Not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 037 (`story-037-hold-completion-epsilon.md`): `HoldTimer(Complete)` scheduling and the completion epsilon.
- Story 038 (`story-038-interact-early-release-resets.md`): early release resets progress.
- Story 040 (`story-040-interact-hysteresis.md`): reach and cancel-range thresholds (190 / 240 cm, 60 degrees) and the `Moved` distance boundary.
- Story 041 (`story-041-interact-grace-on-arrival.md`): the grace period on arrival.
- Story 035 (`story-035-slot-priority.md`): the 16-pair slot priority matrix, including Hide against Interact as `Busy`.
- Story 057 (`story-057-roster-legality-class.md`): Roster legality-class binding and the unbound-pawn rule.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h (cancel table, `ECancelReason` use)
- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp
- Source/StealthGameplay/Private/Core/PawnCommandConditionSweep.cpp (read-only sweep scheduling `HoldTimer(ConditionCancel)`)
- Source/StealthGameplay/Public/Server/Core/ActionHub.h (event firing only; owned by story 003)
- Source/StealthGameplay/Private/Tests/Input/InteractCancelMatrixTest.cpp (`FInputInteractCancelMatrixTest`, `StealthGame.Input.InteractCancelMatrix`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- The Superseded row names "Attack, Prepare or Hide", but Hide cannot supersede Interact: Hide is lowest priority and R9 / AC 20 reject it `Busy` against an active Interact. The clause is unsatisfiable and contradicts story 035. GDD R8's cancel table and AC 16 need the same fix.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Attack or Prepare -> Superseded"; add "Hide Enter during Interact -> Busy, hold continues" (already AC 20, cross-reference only). Everything else is testable as written.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/InteractCancelMatrixTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-037-hold-completion-epsilon.md`, `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`; cross-epic: round-match-framework (stories not yet written) for the real resolution queue; cross-epic: shared-character-locomotion (stories not yet written) for `ISharedPawnStateView`; cross-epic: participant-state-roster (stories not yet written) for the legality class; cross-epic: tuning-data-config (stories not yet written) for the module skeleton and IIF knobs
- Unlocks: `production/epics/input-interaction-framework/story-040-interact-hysteresis.md`, `production/epics/input-interaction-framework/story-049-reversal-consumes-brace.md`, `production/epics/input-interaction-framework/story-050-hold-timer-key-order.md`, `production/epics/input-interaction-framework/story-055-objectives-completion.md`, `production/epics/input-interaction-framework/story-057-roster-legality-class.md`, `production/epics/input-interaction-framework/story-066-event-hub-order.md`
