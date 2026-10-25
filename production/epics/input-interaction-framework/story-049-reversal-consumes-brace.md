# Story 049: AC 77 ReversalConsumesBrace

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-006`, `TR-iif-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story defines what the IIF does when Assassination resolves an Attack against a bracing defender as a reversal. The consumed brace is cancelled with the hub reason `Reversed`, the defender's slot becomes `Countering` (pose-locked, every discrete command `Busy`), the defender's owner-only `FHoldStatus` end carries `ServerEndTime = ContactServerTime` and no cancel reason, and `OnActionCompleted(Pawn, Prepare)` never fires. The pending `CapEnd` and `DebounceExpiry` items of the consumed brace are voided by the `ActionEpoch`.

It depends on story 039 (cancel plumbing and the hub) and story 045 (the brace window and its timer items). In ADR-0011 it is section 4 (`Countering` rejects every discrete command `Busy`), section 5 (reversal consumes the brace) and sections 6 and 7 (the `Reversed` member of `ECancelReason`, `FHoldStatus` with no reason). The reversal decision, `ContactServerTime` and the `Countering` duration (`CounterDurationSec`) belong to Assassination (ASN-R7 / ASN-R14); the IIF only reacts. GDD rules: R8 (Prepare cancels), R11 (events) and R12 (`FHoldStatus`).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: A reversal consumes the defender's brace: the hub fires `OnActionCancelled(Prepare, Reversed)` inside the Attack's slot, the slot becomes `Countering`, and the owner-only `FHoldStatus` end is sent through the one reply step carrying `ServerEndTime = ContactServerTime`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility; the input-layer HIGH does not apply. No V1-V6 gate. `ContactServerTime` is a `double` server time and also feeds `FActionState` (fixed-width serialiser, ADR-0003), but the `FActionState` write belongs to Assassination and story 069.

**Control Manifest Rules (this layer)**:
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — ADR-0011
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — ADR-0011
- Required: Hold ends are `ScheduleAt` queue items; a stale `ActionEpoch` voids a timer silently — ADR-0011
- Forbidden: Never put owner-only data on the pawn — ADR-0011
- Forbidden: Never relay action-hub events to a client except through the Visibility Policy — ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — ADR-0011
- Guardrail: Command interface: at most one reply RPC per connection per drain — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 77 (R8/R11, Logic, U `ReversalConsumesBrace`): **GIVEN** a bracing defender and an Attack that Assassination resolves as a reversal at `ContactServerTime` **WHEN** the drain resolves **THEN** `OnActionCancelled(Pawn, Prepare)` fires once with `CancelReason = Reversed`; **AND** the slot becomes `Countering`; **AND** the defender's `FHoldStatus` end carries `ServerEndTime = ContactServerTime` and no cancel reason; **AND** no `OnActionCompleted(Pawn, Prepare)` fires.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 4, 5, 6, 7 and 8:*

- **Trigger.** Assassination's Attack resolution (inside the attacker's slot) calls back into the IIF to consume the defender's brace; the IIF must not decide the reversal. The defender's Prepare key is lower than the Attack's (the brace was executed earlier in key order), which is the ordering story 058 asserts end to end.
- **Effects, in order inside the one slot.** (1) `OnActionCancelled(Pawn, Prepare, Reversed)` once; (2) slot -> `Countering` (every discrete command `Busy` until Assassination ends it); (3) `FHoldStatus { Outcome = Cancelled, ServerEndTime = ContactServerTime }` appended to the defender's sink buffer and flushed at drain end (the owner's brace indicator ends at that server time, not on receipt, ASN-R23); (4) the brace's `CapEnd` and `DebounceExpiry` are voided by the `ActionEpoch`; (5) no `OnActionCompleted(Prepare)`.
- **Outcome value, to confirm.** The AC text does not name the `Outcome` field value; it is derived from R12 and R8 (a `Reversed` cancel is a `Cancelled` outcome, since `EHoldOutcome = {Active, Completed, Cancelled}` has no other end value and `FHoldStatus` carries no reason). The qa-lead flags this for confirmation.
- **Edge cases from the qa-lead.** A reversal during a pending End (the End is superseded; its timer is voided); the reply is enqueued at drain end; `ContactServerTime` is greater than the drain time, so `ServerEndTime` is a future server time.
- **Test seam.** An Assassination stub resolving the Attack as a reversal with a chosen `ContactServerTime`; sink capture for `FHoldStatus`. Named doubles come from story 095.
- **Prototype.** New code, no prototype counterpart (the prototype had no reversal; `ServerAssassinate` and `ServerSetDefence` are replaced, not ported).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 058 (`story-058-round-queue-order.md`): the key-order outcomes of a defender Prepare versus an attacker Attack in one drain.
- Story 053 (`story-053-brace-sampled-at-execution.md`): a late Prepare after the Attack executed.
- Story 047 (`story-047-forced-run-cancels-brace.md`): `Moved` cancels and the `ServerEndTime` update on an End.
- Assassination epic (cross-epic): the reversal rule, `CounterDurationSec` and `ContactServerTime`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h (`Countering` slot state, consume-brace entry for the Attack resolver)
- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp
- Source/StealthGameplay/Private/Tests/Input/ReversalConsumesBraceTest.cpp (`FInputReversalConsumesBraceTest`, `StealthGame.Input.ReversalConsumesBrace`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Logic story — automated test spec (`StealthGame.Input.ReversalConsumesBrace`):**

- **AC-1**: A reversal consumes the brace
  - Given: defender bracing (Prepare key lower than the Attack's); Assassination stub resolves the Attack as a reversal with `ContactServerTime = Tc` (greater than drain time).
  - When: the drain resolves.
  - Then: `OnActionCancelled(Pawn, Prepare)` once with `Reversed`; slot = `Countering` (every discrete command `Busy`); `FHoldStatus` end: `Outcome = Cancelled`, `ServerEndTime = Tc`, no reason field; no `OnActionCompleted(Prepare)`.
  - Edge cases: `Outcome` value is not in the AC text (derived from R12/R8: a `Reversed` cancel is a `Cancelled` outcome), confirm; reversal during a pending End; the later `CapEnd`/`DebounceExpiry` timers are voided by `ActionEpoch`; reply enqueued at drain end.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/ReversalConsumesBraceTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`, `production/epics/input-interaction-framework/story-045-brace-window-bounds.md`; cross-epic: assassination (stories not yet written) for the reversal resolution and `ContactServerTime`; cross-epic: round-match-framework (stories not yet written) for the resolution queue
- Unlocks: None
