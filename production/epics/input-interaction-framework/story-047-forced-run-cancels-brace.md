# Story 047: AC 63 ForcedRunCancelsBrace

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-005`, `TR-iif-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story handles the two ways an active brace ends early for reasons other than the player's own release: an externally forced run (or traversal) state reported by Locomotion cancels the brace with `Moved`, and an End updates the owner's `FHoldStatus.ServerEndTime` from the cap (`start + 4.0`) to the actual End time. A requested Run from the player is clamped to Walk while Bracing and never cancels (story 034); only a forced run reaches the cancel path, so the player cannot cancel their own brace by sprinting.

It builds on story 045 (the window bounds and the cap and End machinery). In ADR-0011 it is section 5 (Prepare: "an externally forced run or traversal cancels `Moved`") together with section 7 and 8 (`FHoldStatus` carries no cancel reason, and the end status is enqueued in the reply step). The pawn state comes through ADR-0010's `ISharedPawnStateView::IsRunning` and `IsInTraversal` (ADR-0010 section 8). GDD rules: R8 (Prepare cancels), R12 (`FHoldStatus`) and F2.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: While `Bracing`, requested Run is clamped (never cancels) and `Traverse` is rejected; an externally forced run or traversal reported through `ISharedPawnStateView` cancels with `Moved`, and owner-only `FHoldStatus` (no cancel reason) is sent on execution and on every server-side end.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 HIGH, ADR-0010 HIGH)
**Engine Notes**: ADR-0011 rates the command path MEDIUM; ADR-0010 is HIGH because of the 5.8 movement and possession facts (the `UStealthMovementComponent` CMC subclass publishes `IsRunning` and `IsInTraversal`). Neither is replicated (ADR-0010: owner-local copies for prompts only). No V1-V6 gate.

**Control Manifest Rules (this layer)**:
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — ADR-0011
- Required: Action-hub events fire only inside queue slots — ADR-0011
- Forbidden: Never replicate `IsRunning`, `IsInTraversal` or `CommandedDisplacementCm` — ADR-0010
- Forbidden: Never put owner-only data on the pawn — ADR-0011
- Forbidden: Never let the condition sweep cancel a hold itself (it schedules `HoldTimer(ConditionCancel)` items) — ADR-0011
- Guardrail: Command interface: at most one reply RPC per connection per drain — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 63 (R8, Logic, U `ForcedRunCancelsBrace`): **GIVEN** an active brace **WHEN** Locomotion reports an externally forced run **THEN** the brace cancels `Moved`; **AND GIVEN** an End executed at 1.3 s **THEN** `FHoldStatus.ServerEndTime` changes from start + 4.0 to start + 1.3 (± dt).

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 5, 7, 8 and ADR-0010 section 8:*

- **Forced run or traversal.** The condition is detected by reading `ISharedPawnStateView::IsRunning()` / `IsInTraversal()` while the slot is `Bracing` and no Run command was issued (a clamped requested Run leaves `IsRunning` false). The read-only sweep schedules a `HoldTimer(ConditionCancel)` item (same mechanism as story 039); its slot cancels with `Moved`, fires `OnActionCancelled(Prepare, Moved)` once and enqueues an `FHoldStatus` with `Outcome = Cancelled` at drain end. A forced traversal gives the same `Moved` cancel.
- **`FHoldStatus` end update.** The initial status carries `ServerEndTime = t_start + DefenceDurationSec`. When the End executes (after the debounce, so an End sent at about 1.2 s executes at 1.3 s) the second `FHoldStatus` carries `ServerEndTime` equal to the End execution time (+/- dt), `Outcome = Cancelled`, and no cancel reason field. A status whose `ClientCommandSeq` is older than the owner's current hold is ignored by the client (R12).
- **Requested Run is not a cancel (AC 14, story 034).** Run is clamped to Walk while Bracing; assert that a requested Run does not cancel.
- **Test seam.** A Locomotion stub implementing `ISharedPawnStateView` that can report `IsRunning = true` without a Run command; a stub sink capturing `FHoldStatus` entries. Named doubles from story 095.
- **Tuning keys.** `ReleaseDebounceSec` 0.10; `DefenceDurationSec` 4.0 (Assassination-owned).
- **Prototype.** New code for the forced-run rule; the prototype brace (`OnDefencePressed/Released`, `ServerSetDefence`) had no Locomotion input and is replaced, not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 034 (`story-034-brace-clamps-run-traverse.md`): requested Run clamped to Walk and Traverse rejected `NotNow` while Bracing.
- Story 045 (`story-045-brace-window-bounds.md`): window bounds for the three standard holds.
- Story 049 (`story-049-reversal-consumes-brace.md`): the `Reversed` cancel and its `ServerEndTime = ContactServerTime`.
- Story 048 (`story-048-planted-brace-ignores-move.md`): Move input while planted.
- shared-character-locomotion epic (cross-epic): the actual forced-run and traversal state.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandConditionSweep.cpp (forced-run and forced-traversal condition for a brace)
- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (`FHoldStatus` end update)
- Source/StealthGameplay/Private/Tests/Input/ForcedRunCancelsBraceTest.cpp (`FInputForcedRunCancelsBraceTest`, `StealthGame.Input.ForcedRunCancelsBrace`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Logic story — automated test spec (`StealthGame.Input.ForcedRunCancelsBrace`):**

- **AC-1**: An externally forced run cancels the brace and End updates `ServerEndTime`
  - Given: active brace at `t_start` (cap 4.0); Locomotion stub can report forced run (`ISharedPawnStateView::IsRunning = true` without a Run command).
  - When: Locomotion reports a forced run.
  - Then: `OnActionCancelled(Prepare, Moved)` once; `FHoldStatus` `Outcome = Cancelled` enqueued at drain end.
  - Given: separate brace; initial `FHoldStatus` `ServerEndTime = t_start + 4.0`.
  - When: End executed at `t_start + 1.3`.
  - Then: second `FHoldStatus` `ServerEndTime = t_start + 1.3` (+/- dt), `Outcome = Cancelled`, no cancel reason field.
  - Edge cases: a requested Run is clamped and must not cancel (AC 14); forced traversal gives the same `Moved` cancel; "End executed at 1.3" means after debounce (End sent about 1.2).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/ForcedRunCancelsBraceTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-045-brace-window-bounds.md`; cross-epic: shared-character-locomotion (stories not yet written) for `ISharedPawnStateView` and the forced-run state; cross-epic: round-match-framework (stories not yet written) for the resolution queue
- Unlocks: None
