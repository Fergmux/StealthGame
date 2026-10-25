# Story 065: AC 23 NoLookEvent

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-008`, `TR-iif-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation, Enhanced Input edges and batched owner replies. Move and Look travel only in the CMC's `ServerMove`: no RPC of their own and no Look event, because a per-turn event would be a gaze tell source.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH, command path MEDIUM)
**Engine Notes**: ADR-0011 E8: human Move and Look reach the server through the CMC's `ServerMove` (unreliable, batched) with the control rotation, and custom gait data rides `FSavedMove_Character` compressed flags. Nothing in this story touches the post-cutoff Enhanced Input items; it is a negative test on the action hub and on the RPC surface.

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand` — source: ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn — source: ADR-0011
- Forbidden: Never relay action-hub events to a client except through the Visibility Policy — source: ADR-0011

This story guards one of the Pillar-1 anti-tells: looking around must say nothing. A Look event on the hub would let any listener (Concealment, Crowd, Objectives, the HUD) react to turns, and a per-turn RPC would put a human's gaze behaviour on the wire in a way an NPC's would not be. Crowd's gaze catalogue makes a turning head not a player tell, but only if no system can observe the turn as an event. The test applies 100 Look changes of mixed magnitude, including a 180 degree turn and a zero delta, and asserts that no hub event fires and no RPC is sent.

It depends on story 003 (the action hub and predicate registry) because the counting listeners attach to `UActionHub`'s three native delegates. It relates to GDD R3 (Move and Look are not routed through RPCs of their own; no Look action event), to TR-iif-002 (Look through CMC saved moves, AI through the same entry points) and to ADR-0011 sections 1, 2 and 6.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 23 (R3, Logic, U `NoLookEvent`): **GIVEN** 100 Look changes **WHEN** the hub is observed **THEN** zero events fire.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 1, 2 and 6 and the Implementation Guidelines:*

- `Look(FRotator Rotation)` is a continuous entry point: for a human it is the owner's control rotation set each frame by Camera's view envelope in `AStealthPlayerController::UpdateRotation` from `IA_Look` and carried by `ServerMove`; for AI the crowd controller writes the control rotation (pitch included) through this function. It is never a queue item (ADR-0002 section 3) and therefore can never reach the hub, which fires only inside queue slots.
- `UActionHub` exposes only `OnActionStarted`, `OnActionCancelled` and `OnActionCompleted`, with `EActionType = {Interact, Prepare, Attack, HideEnter, HideExit}`; there is no Look action type and none may be added. No dynamic or Blueprint-assignable delegate exists.
- Test per the qa-lead: a command component plus a `UActionHub` with counting listeners on all three native delegates and an RPC-send spy; apply 100 Look changes (mixed magnitudes, including a 180 degree turn and a zero delta); assert zero hub events and zero recorded RPCs. Edge cases: Look issued by the AI path gives the same result; Look during an active hold fires no event and does not touch the hold (GDD AC 16's non-cancel clause is story 039's).
- The RPC-send spy is a test seam, not an engine facility: use the outgoing-RPC send hook of story 095 (an injectable sender on the controller, not a patched engine function). Without it the "zero RPCs" half has no observable; the Logic test can still assert the hub half and the absence of a Look path in the component API.
- Prototype disposition: new code, no prototype counterpart (the prototype has no Look event).
- Tuning keys: none relevant (Look has no knob; sensitivity is client-local, not tuning).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-066-event-hub-order.md`: hub events fire in queue-key order and `CancelReason` never reaches a client.
- `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`: Look does not cancel an Interact.
- `production/epics/input-interaction-framework/story-081-look-latency.md`: the camera responds in the same frame (AC 58).
- `production/epics/input-interaction-framework/story-085-look-parity-observed.md`: observed rotation, pitch and yaw parity between a human and an NPC.
- `production/epics/input-interaction-framework/story-006-no-caller-identity-parameter.md`: no identity in any entry point.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/PawnCommandComponent.h` and `Source/StealthGameplay/Private/Core/PawnCommandComponent.cpp` (the `Look` entry point, changed only if the test exposes a defect)
- `Source/StealthGameplay/Private/Tests/Input/NoLookEventTest.cpp` (class `FInputNoLookEventTest`, name `StealthGame.Input.NoLookEvent`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC 23 NoLookEvent (`StealthGame.Input.NoLookEvent`)
  - Given: a command component plus `UActionHub` with counting listeners on all three native delegates and an RPC-send spy.
  - When: 100 Look changes are applied (mixed magnitudes, including a 180 degree turn and a zero delta).
  - Then: zero hub events fire and zero RPCs are recorded.
  - Edge cases: Look issued by the AI path (same result); Look during an active hold (no event, hold unaffected; see GDD AC 16's non-cancel clause).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/NoLookEventTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`
- Unlocks: None
