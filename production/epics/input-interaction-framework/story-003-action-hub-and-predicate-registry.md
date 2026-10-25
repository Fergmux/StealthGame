# Story 003: UActionHub registers predicates weakly and fires native events only

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
**ADR Decision Summary**: A server-only world subsystem `UActionHub` exposes native multicast delegates `OnActionStarted/Cancelled/Completed` fired only inside queue slots, and feature owners register an `ICommandPredicate` per `EActionType` weakly from the composition root; a missing predicate is a fault and resolves `Reject(NotNow)`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path MEDIUM, input layer HIGH. `TWeakInterfacePtr<T>` requires a UHT `UINTERFACE` (ADR-0002 E7), which is why `ICommandPredicate` is declared as `UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))` with an API-exported `ICommandPredicate`. `UWorldSubsystem` with `NotBlueprintType, NotBlueprintable` is pre-cutoff. No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Feature predicates are registered weakly from the composition root per `EActionType` — source: ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Forbidden: Never relay action-hub events to a client except through the Visibility Policy — source: ADR-0011
- Forbidden: Never let `AllowNoEffect` vs `Reject` depend on anything but the site and legality class — source: ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — source: ADR-0011
- Guardrail: Command interface stage 2 a few microseconds per frame at the IIF F3 worst case; one predicate call per item — source: ADR-0011

This story builds the two consumer-owned seams through which feature systems plug into the command interface: the `ICommandPredicate` registry (Assassination for Prepare and Attack, Objectives for Interact, Concealment for Hide) and the `UActionHub` event hub that every feature subscribes to once, instead of once per pawn (GDD R11). The registry answers "may this pawn begin this action right now" without the IIF learning anything about the feature; the hub tells features "an action started, was cancelled or completed". Both are server-only and neither carries caller identity (R2).

It sits third because it needs only the types of story 001. Later stories consume it: stage-two resolution (027), interact cancel matrix (039), concealment hears attack (054), no look event (065). Predicate behaviour with real owners is cross-epic (assassination, objectives, concealment, stories not yet written); here tests use stub predicates only. The ADR also defines `IInteractTargetOwner` and `RegisterInteractTargetOwner` (amendment 2026-10-24); those belong to the interact stories (037-041, 055), not here.

---

## Acceptance Criteria

*ADR-derived (no numbered GDD AC). Source: ADR-0011 Decision §6 (weak per-`EActionType` registration, missing predicate resolves `Reject(NotNow)`, native non-Blueprint delegates) and Key Interfaces (`UActionHub`):*

- [ ] **GIVEN** a `UActionHub` with a stub `ICommandPredicate` registered per `EActionType` **WHEN** each type is resolved, a type has no predicate or its registered predicate object has been destroyed (stale weak pointer), and the hub's reflection is inspected **THEN** a registered predicate's verdict (Allow, AllowNoEffect, Reject(Busy)) is returned, a missing or stale predicate resolves `Reject(NotNow)` with no crash and one logged fault, and `UActionHub` is `NotBlueprintType` with zero `FMulticastDelegateProperty` or BlueprintAssignable properties and three native `TMulticastDelegate` events with signatures (`AStealthPawn*, EActionType`), (`AStealthPawn*, EActionType, ECancelReason`) and (`AStealthPawn*, EActionType`).

---

## Implementation Notes

*Derived from ADR-0011 Decision §6, Key Interfaces and Implementation Guidelines:*

"`ICommandPredicate` is a consumer-owned interface in Core (architecture.md TD concern 1): Assassination (Prepare, Attack), Objectives (Interact), Concealment (Hide) implement it and the composition root registers them per `EActionType` with `RegisterPredicate(EActionType, TWeakInterfacePtr<ICommandPredicate>)` (weak, as ADR-0002's resolvers; ADR-0007). A missing predicate is a fault and resolves `Reject(NotNow)`. `AllowNoEffect` runs exactly like `Allow` (same pose, timer, events) and only its completion changes nothing; the choice between `AllowNoEffect` and `Reject` may depend only on the site and the legality class." The consumer-owned placement is why the IIF can ship before its feature owners exist: it depends on the interface, they depend on it, and the composition root wires them.

`UActionHub` is a `UWorldSubsystem`, `NotBlueprintType`, `NotBlueprintable`, server-only, one per world, in `Public/Server/`. Delegates: `FOnPawnAction` (`DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPawnAction, AStealthPawn*, EActionType)`) for Started and Completed, `FOnPawnActionCancelled` (`..._ThreeParams(..., AStealthPawn*, EActionType, ECancelReason)`). Events fire "only inside queue slots, in key order" with "no dynamic/Blueprint-assignable delegate, no client copy, and no listener may relay an event to a client except through its own Visibility Policy audience." The firing discipline (inside slots) is enforced by the callers in later stories; this story proves the type shape and the registry behaviour.

`CanBegin` must be `const` and side-effect-free because `QueryCanBegin` (the AI dry run, ADR-0011 §3.3) calls the same predicate outside a slot; the stub in the test asserts it mutates nothing. QL-STORY-READY note: re-registering a type is unspecified; the signature implies replace, so pin "replace" in the implementation and test it. Registering `nullptr` is rejected with a fault. `ECancelReason` is declared by story 001.

GDD rules: R11 (generic action events, line 167), R9 (slot and priority, line 157, for context only), R2. Prototype: new code, no prototype counterpart (the prototype has no hub; features read the character directly). Tuning keys: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-001-command-reply-types.md`: `EActionType`, `ECancelReason`, reply types.
- `story-027-stage-two-at-resolution.md`: the stage-2 order that calls the predicate at step 4.
- `story-039-interact-cancel-matrix.md`: the Interact cancel causes that fire `OnActionCancelled`.
- `story-054-concealment-hears-attack.md`: the real Concealment listener (cross-epic owner).
- `story-065-no-look-event.md` and `story-067-no-event-relay.md`: no Look event and no relay of hub events to clients.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/CommandPredicate.h` (`ICommandPredicate`, `FPredicateResult`, `EPredicateVerdict`)
- `Source/StealthGameplay/Public/Server/ActionHub.h`
- `Source/StealthGameplay/Private/Core/ActionHub.cpp`
- `Source/StealthGameplay/Private/Tests/Input/ActionHubPredicateRegistryTest.cpp` (class `FInputActionHubPredicateRegistryTest`, name `StealthGame.Input.ActionHubPredicateRegistry`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Logic — automated test spec** (QL-STORY-READY: ADEQUATE; test `ActionHubPredicateRegistry`, derived from ADR 6):

- **AC-1**: UActionHub registers predicates weakly and fires native events only
  - Given: a hub with a stub predicate registered for each `EActionType`.
  - When: each type is resolved.
  - Then: the registered predicate's verdict is returned (Allow, AllowNoEffect, Reject(Busy) in turn).
  - Given: no predicate for a type, or the registered predicate object destroyed (GC'd; weak pointer stale).
  - When: resolved.
  - Then: result is Reject(NotNow), no crash, fault logged once.
  - Given: `UActionHub` reflection.
  - Then: class is NotBlueprintType; zero `FMulticastDelegateProperty` / BlueprintAssignable properties; the three delegates are native `TMulticastDelegate` (compile-time `static_assert` on type), with signatures (AStealthPawn*, EActionType) / (..., ECancelReason) / (AStealthPawn*, EActionType).
  - Edge cases: register nullptr (rejected, fault); register the same type twice (replace; confirm); the predicate is called const and the stub asserts it mutates nothing; unregister-by-destruction mid-iteration of a broadcast.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/ActionHubPredicateRegistryTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-001-command-reply-types.md`; cross-epic: tuning-data-config (stories not yet written; module skeleton); cross-epic: assassination, objectives, concealment predicate owners (stories not yet written; stub predicates only here)
- Unlocks: `production/epics/input-interaction-framework/story-006-no-caller-identity-parameter.md`, `production/epics/input-interaction-framework/story-027-stage-two-at-resolution.md`, `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`, `production/epics/input-interaction-framework/story-054-concealment-hears-attack.md`, `production/epics/input-interaction-framework/story-065-no-look-event.md`
