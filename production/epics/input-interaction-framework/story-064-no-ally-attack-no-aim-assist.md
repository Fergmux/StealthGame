# Story 064: AC 61 NoAllyAttackNoAimAssist

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation, Enhanced Input edges and batched owner replies. The client builds the Attack payload only from Camera's `GetAimRay()`: always `WorldPoint`, a `PawnRefGuid` only if the ray hit a pawn the client renders (eye-mask alpha above 0) that is not `IsMarkedAlly`; there is no lock-on, snapping, magnetism or aim assist on any device.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH, command path MEDIUM)
**Engine Notes**: The client half of this story is the Enhanced Input builder on `AStealthPlayerController` (ADR-0011 section 9; E9: `SetupInputComponent` is virtual). The gamepad half needs the owner's control rotation sampled each frame from `IA_Look` through Camera's view envelope (ADR-0015), so the criterion is deterministic only with a fixed frame step and a seeded start rotation. No post-cutoff API is introduced here beyond the already-listed Enhanced Input items (V1 gates button-edge behaviour, not this story).

**Control Manifest Rules (this layer)**:
- Required: The Attack payload comes only from Camera's `GetAimRay()`; `PawnRef` is honoured only through `CanViewerKnow` and `PawnRefAgreementCm` — source: ADR-0011, ADR-0015
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand`; `PawnRef` travels as a NetGUID value — source: ADR-0011
- Forbidden: Never add lock-on, snapping, magnetism or aim assist — source: ADR-0011, ADR-0015
- Forbidden: Never use `UPlayerMappableInputConfig` or the Raw Input plugin; never make a gameplay command a Common UI action — source: ADR-0011

This story is the two-client behavioural proof of GDD R10's client rules. Part one: when the camera ray rests on a marked teammate, pressing Attack must not send an Attack carrying that teammate's `PawnRef`; the owner already knows its allies from their markers, so the client never targets them (Teammate Markers TM-R10) and the server can never be asked to resolve a teammate strike by reference. Part two: with a gamepad and a pawn standing 5 degrees off the ray, identical recorded stick input must give a bit-identical control-rotation sequence with and without that pawn present, which proves the absence of aim assist (a stick that bends toward a pawn would be both an unfair advantage and a leak that the pawn is there).

It depends on story 063, which proves the server-side gates, so this story only asserts what the client sends and what its camera does. It unlocks story 073 (strike hint parity), which uses the same ray and ally rule for the hint. It relates to GDD R10 and to ADR-0011 section 9 ("Attack payload (IIF R10)").

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 61 (R10, Integration, P `NoAllyAttackNoAimAssist`): **GIVEN** the ray on a marked teammate **WHEN** Attack is pressed **THEN** no Attack with that `PawnRef` is sent; **AND GIVEN** a gamepad with a pawn 5° off the ray **WHEN** identical stick input is replayed with and without that pawn present **THEN** the resulting control rotations are identical.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 9 (Attack payload, Client view handle) and the Implementation Guidelines:*

- Attack payload: built only from `GetAimRay()` (ADR-0015): always `WorldPoint`; `PawnRefGuid` only if the ray hit a pawn this client renders (Camera eye-mask alpha above 0), that is not `IsMarkedAlly` (Teammate Markers), within `AssassinationRangeCm + CommandRangeSlackCm`. No lock-on, snapping, magnetism or aim assist on any device.
- `IsMarkedAlly` is served only by the local marker model built from the owner's own C-TeamView; it is supplied to `UPawnCommandClient` through `IOwnerCommandClientView::SetAllyMarkerQuery(TWeakInterfacePtr<IAllyMarkerQuery>)` and answers false when none is bound (ADR-0011 section 9, ADR-0021). It is never sent to the server. The test must bind a query that answers true for the teammate.
- The qa-lead's assertions: the Attack that reaches the server command log carries no `PawnRef` (WorldPoint only), and no Attack with the ally's reference is logged. Edge cases: an ally with eye-mask alpha above 0 (still no reference); a non-ally pawn on the ray does send a reference (the control case that proves the gate is the ally rule); the gamepad runs use a fixed frame step and a seeded start rotation; a pawn 0 degrees and 5 degrees off give the same result.
- Control rotation sampling: record the control rotation each frame for run A (pawn present) and run B (pawn absent) from the same recorded stick input; the two sequences must be bit-identical.
- Prototype disposition: the prototype `FindAssassinationTarget` (lock-on-style targeting) and `OnAssassinatePressed` / `ServerAssassinate(Target, StrikePoint)` in `StealthCharacter.cpp` are replaced, not ported (ADR-0011 Migration Plan). This test and the builder it exercises are new code.
- Test seams: the server-side command log (story 095), an `IAllyMarkerQuery` stub, a scripted gamepad stick replay in the CQTest harness, and Camera's `GetAimRay()` (cross-epic, camera-system). The multi-client PIE form is P (no net emulation).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-063-pawnref-gate.md`: the server-side gates on `PawnRef`.
- `production/epics/input-interaction-framework/story-073-strike-hint-parity.md`: the strike hint widget and its pixel parity.
- `production/epics/input-interaction-framework/story-015-default-bindings.md`: the Attack key and button bindings.
- `production/epics/input-interaction-framework/story-030-attack-never-rejected-by-target.md`: server Attack execution against any target.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/PawnCommandClient.h` and `Source/StealthGame/Private/PawnCommandClient.cpp` (client Attack payload builder and the ally exclusion, root module, ADR-0011 Migration Plan)
- `Source/StealthTestHarness/Private/NoAllyAttackNoAimAssistPieTest.cpp` (CQTest `NETWORK_TEST_CLASS(NoAllyAttackNoAimAssist, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.NoAllyAttackNoAimAssist.<Method>`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC 61 NoAllyAttackNoAimAssist (`StealthGame.Input.PIE.NoAllyAttackNoAimAssist`, 2 clients)
  - Given: an owner client with its ray on a teammate (`IsMarkedAlly` true) within range.
  - When: Attack is pressed.
  - Then: the Attack that reaches the server command log carries no PawnRef (WorldPoint only); no Attack with the ally's ref is logged.
  - Given: a gamepad, a pawn 5 degrees off the ray; identical recorded stick input replayed in run A with the pawn, run B without.
  - When: control rotation is sampled per frame.
  - Then: the two control-rotation sequences are bit-identical.
  - Edge cases: an ally with eye-mask alpha > 0 (still no ref); a non-ally pawn on the ray does send a ref (control, proves the gate is the ally rule); fixed frame step and seeded start rotation so replays are deterministic; a pawn at 0 degrees and at 5 degrees off give the same result.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/NoAllyAttackNoAimAssistPieTest.cpp` (CQTest `StealthGame.Input.PIE.NoAllyAttackNoAimAssist`) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-063-pawnref-gate.md`; cross-epic: camera-system (stories not yet written; `GetAimRay`, eye-mask alpha), cross-epic: teammate-markers (stories not yet written; `IsMarkedAlly`)
- Unlocks: `production/epics/input-interaction-framework/story-073-strike-hint-parity.md`
