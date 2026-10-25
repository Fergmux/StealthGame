# Story 066: AC 25 EventHubOrder

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation, Enhanced Input edges and batched owner replies. `UActionHub` is a server-only world subsystem with native multicast delegates (`OnActionStarted`, `OnActionCancelled`, `OnActionCompleted`) that fire only inside queue slots, in key order, with no client copy.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH, command path MEDIUM)
**Engine Notes**: The hub is a `UWorldSubsystem` with `NotBlueprintType` and native (non-dynamic) multicast delegates; no post-cutoff API is involved. The "no `CancelReason` on the wire" half is an engine reflection scan (`CPF_Net` properties and `UFUNCTION(Client)` signatures), a pre-cutoff facility. Ordering relies on the ADR-0002 drain (`TG_PostPhysics`), which the stub queue models.

**Control Manifest Rules (this layer)**:
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; every discrete command from any source enters through its entry points and the queue — source: ADR-0011
- Forbidden: Never relay action-hub events to a client except through the Visibility Policy — source: ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — source: ADR-0011

This story proves two properties of the action hub. First, order: when three pawns act in one drain, listeners see their events in ascending queue-key order, and every event is fired inside a slot (so `CurrentKey()` is valid). Listeners such as Objectives, Concealment and Assassination subscribe once to the world hub and depend on a total order shared with Round; if the hub ordered events by arrival or by pawn it would break the "arrival order carries no meaning" rule of ADR-0002. Second, privacy: reflection must find no `CancelReason` in any client RPC or replicated property, because the cancel reason (Released, Moved, Superseded, Illegal, OutOfRange, TargetInvalid, PawnRemoved, Reversed) is readable by server listeners and logged, but must never reach any client (a client seeing why an action ended is an identity tell).

It depends on story 039 (the Interact cancel matrix), which establishes the cancel reasons the hub carries, and it unlocks story 067 (no event relay), which tests that handlers never relay an event to a client. It relates to GDD R11 (generic action events) and ADR-0011 section 6.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 25 (R11, Logic, U `EventHubOrder`): **GIVEN** 3 pawns acting in one drain **WHEN** the queue resolves **THEN** hub events fire in key order; **AND** reflection finds `CancelReason` in no client RPC or replicated property.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 6 and the Implementation Guidelines:*

- `UActionHub` (world subsystem, `Public/Server/`, `NotBlueprintType`): native multicast delegates `OnActionStarted(AStealthPawn*, EActionType)`, `OnActionCancelled(AStealthPawn*, EActionType, ECancelReason)`, `OnActionCompleted(AStealthPawn*, EActionType)`, fired only inside queue slots, in key order. `EActionType = {Interact, Prepare, Attack, HideEnter, HideExit}`; `ECancelReason = {Released, Moved, Superseded, Illegal, OutOfRange, TargetInvalid, PawnRemoved, Reversed}`. No dynamic or Blueprint-assignable delegate, no client copy.
- Events fire inside the slot of the command or timer that caused them, depth-first, so an event pair from one pawn (for example `Cancelled` then `Started` through a preempt) keeps slot order. Use `CurrentKey()` to prove the slot; it is one of the three ordering primitives (`Compare`, `GetKey`, `CurrentKey`) that ADR-0002 allows.
- The qa-lead's test, part one: three pawns with distinct queue keys each issue a command in one drain, with a recording listener on the hub; assert events are recorded in ascending queue-key order and every event fires inside a slot (`CurrentKey()` valid). Edge cases: three pawns issuing in reverse arrival order; an event pair from one pawn (Cancelled then Started via preempt) keeps slot order.
- Part two: reflection over all client RPC signatures and replicated properties in the project modules, scanned for the `ECancelReason` type or the name `CancelReason`; none may be found. This pairs with the replicated-property allowlist audit of ADR-0003 section 8 (story 068), but is a separate scan.
- Prototype disposition: new code, no prototype counterpart (the prototype has no hub; its Server RPCs resolve inline).
- Test seams: the stub Round queue with controllable key assignment and mailbox order, and a recording hub listener (story 095 doubles).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`: the cancel causes and the reason each produces.
- `production/epics/input-interaction-framework/story-067-no-event-relay.md`: handlers send no client RPC from within an event.
- `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`: the hub and predicate registry themselves.
- `production/epics/input-interaction-framework/story-068-no-owner-data-on-pawn.md`: the allowlist-level check on the pawn's replicated properties.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/Server/ActionHub.h` and `Source/StealthGameplay/Private/Core/ActionHub.cpp` (event firing order, changed only if the test exposes a defect)
- `Source/StealthGameplay/Private/Tests/Input/EventHubOrderTest.cpp` (class `FInputEventHubOrderTest`, name `StealthGame.Input.EventHubOrder`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC 25 EventHubOrder (`StealthGame.Input.EventHubOrder`)
  - Given: three pawns with distinct queue keys each issuing a command in one drain, and a recording listener on the hub.
  - When: the queue resolves.
  - Then: events are recorded in ascending queue-key order; every event fires inside a slot (`CurrentKey()` valid).
  - Given: reflection over all client RPC signatures and replicated properties in the project modules.
  - When: scanned for `ECancelReason` / the name `CancelReason`.
  - Then: none found.
  - Edge cases: three pawns issuing in reverse arrival order; an event pair from one pawn (Cancelled then Started via preempt) keeps slot order.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/EventHubOrderTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`; cross-epic: round-match-framework (stories not yet written; the resolution queue and key assignment)
- Unlocks: `production/epics/input-interaction-framework/story-067-no-event-relay.md`
