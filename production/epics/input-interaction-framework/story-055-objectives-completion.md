# Story 055: AC 42 ObjectivesCompletion

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-008`, `TR-iif-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story verifies the completion path of an Interact hold: when the `HoldTimer(Complete)` resolves with the hold intact, `OnActionCompleted(Pawn, Interact)` fires once and the Objectives owner applies its effect exactly once inside that same slot, so Objectives' completion inherits the `HoldTimer(Complete)` queue key (a change request to Round R13 and Objectives). A later End is a no-op (no event, no reply, slot free) and a second completion is impossible. The `AllowNoEffect` variant gives identical events and no advance (story 028).

It depends on story 039 (hold cancel plumbing and the hub). In ADR-0011 it is section 6 (`IInteractTargetOwner`: the IIF calls `OnInteractEnded(..., Completed, CurrentKey())` inline in the slot that ends the hold, before `OnActionCompleted` fans out, for every pawn kind) and section 5 (`Complete` at `t_start + D`). GDD rules: R8 (Interact effect, duration `D` from the target owner) and F1. Objectives' real effect is verified in the Objectives epic.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: The Interact target owner supplies `D` and applies its effect inline in the `HoldTimer(Complete)` slot under that item's key; hub events fire only inside queue slots and the IIF keeps no tick of its own for holds.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 HIGH, ADR-0002 MEDIUM)
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility; input-layer HIGH does not apply. No V1-V6 gate. `IInteractTargetOwner` is a consumer-owned `UINTERFACE(CannotImplementInterfaceInBlueprint)` in Core registered weakly with `UActionHub::RegisterInteractTargetOwner`. U-form automation test with stubs.

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items; a stale `ActionEpoch` voids a timer silently — ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — ADR-0011
- Required: Feature predicates are registered weakly from the composition root per `EActionType` — ADR-0011
- Forbidden: Never let `AllowNoEffect` vs `Reject` depend on anything but the site and legality class — ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp — ADR-0011
- Forbidden: Never relay action-hub events to a client except through the Visibility Policy — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case, inside the 0.5 ms queue line — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 42 (Integration, U `ObjectivesCompletion`): **GIVEN** an Interact on a site whose predicate returns `Allow` **WHEN** its `HoldTimer(Complete)` resolves **THEN** `OnActionCompleted` fires once, Objectives advances exactly once inside the same slot, and a later End is a no-op.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 5 and 6 (Interact target owner) and GDD R8:*

- **Completion slot.** When `HoldTimer(Complete)` resolves with the hold intact (legality, range and target validity re-checked), the IIF calls the target owner's `OnInteractEnded(Pawn, FCourtFixtureId, EInteractEnd::Completed, FQueueKey)` inline with `CurrentKey()` as the key, then fires `OnActionCompleted(Pawn, Interact)`; the Objectives stub counts advances and records the key, which must equal the `HoldTimer(Complete)` key (same slot).
- **Later End is a no-op.** After completion the slot is free; an End with no matching active action produces no event, no reply and no state change (Ends are idempotent and never rate-limited, ADR-0011 section 3.2 step 5). A second completion is impossible (the `ActionEpoch` voids any other timer).
- **End inside the debounce window before `D` still completes.** A release less than `ReleaseDebounceSec` before `D` still completes because the End is pending when `Complete` resolves (R8); the same 0.10 s for every pawn.
- **`AllowNoEffect` variant.** Identical events and timers, no advance (story 028 owns the parity assertion; this story checks the `Allow` path only).
- **Claim accounting.** `D` is read once at execution from the target owner (`GetInteractHoldSec()`, no court parameter); a missing owner is a fault and `InteractBegin` resolves `Reject(NotNow)`.
- **Test seam.** An Objectives stub implementing `IInteractTargetOwner` and subscribed to `OnActionCompleted`, counting advances with the slot key; Interact predicate = Allow. Named doubles from story 095.
- **Tuning keys.** `ObjectiveInteractSec` 3.0 (Objectives-owned, hard range 2.0-6.0); `ReleaseDebounceSec` 0.10.
- **Prototype.** Replaces `OnInteractPressed/Released` and `ServerSetInteracting` (the prototype completed holds from client-set state); not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 028 (`story-028-allow-no-effect-parity.md`): `Allow` versus `AllowNoEffect` parity.
- Story 037 (`story-037-hold-completion-epsilon.md`): completion epsilon and stamping.
- Story 050 (`story-050-hold-timer-key-order.md`): key order against a same-frame kill.
- Story 060 (`story-060-hidden-site-probe.md`): server-side probing with hidden, non-site and rival ids.
- Objectives epic (cross-epic): the real `ObjectiveCompletion` and `IInteractTargetOwner` implementation.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (`HoldTimer(Complete)` resolver and target-owner hook ordering)
- Source/StealthGameplay/Public/Server/Core/ActionHub.h (`RegisterInteractTargetOwner`; owned by story 003, used here)
- Source/StealthGameplay/Private/Tests/Input/ObjectivesCompletionTest.cpp (`FInputObjectivesCompletionTest`, `StealthGame.Input.ObjectivesCompletion`; U-form automation test)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Logic / Integration story — automated test spec (`StealthGame.Input.ObjectivesCompletion`):**

- **AC-1**: Interact completion applies the effect once, in-slot
  - Given: Interact on a site, predicate Allow; Objectives stub subscribed to `OnActionCompleted` counting advances with the slot key.
  - When: `HoldTimer(Complete)` resolves with the hold intact; later End sent.
  - Then: `OnActionCompleted` once; Objectives advance count = 1, recorded under the `HoldTimer(Complete)` key (same slot); the later End is a no-op (no event, no reply, slot free).
  - Edge cases: `AllowNoEffect` variant gives identical events and no advance (AC 9); End inside the debounce window before D still completes; second completion impossible.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Input/ObjectivesCompletionTest.cpp` (U-form automation test `StealthGame.Input.ObjectivesCompletion`) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`; cross-epic: objectives (stories not yet written) for the real target owner and completion effect; cross-epic: round-match-framework (stories not yet written) for the resolution queue
- Unlocks: `production/epics/input-interaction-framework/story-060-hidden-site-probe.md`
