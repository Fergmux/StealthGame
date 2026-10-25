# Story 016: Aim ray

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (consumed by ADR-0011 for the Attack payload)
**ADR Decision Summary**: `GetAimRay()` on the camera's client view facade is the only source of the Attack payload's `WorldPoint` and `TargetPawn`. 1P uses the camera-centre ray from the lens; 3P uses the centre ray starting at the point nearest the eye pivot with a `ViewBlock` trace and a pawn query, then re-traces from the pivot to P + 10 cm. A pawn is targetable only if `IsEyeMaskVisible` and the eye ray reaches it; otherwise the result is a `TargetSpot`. `WorldPoint` is the `HideVolume` entry point projected to the ground when the eye-side ray enters a bush volume, else the ground point under the ray clamped to range.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; the ray uses stock `LineTraceSingleByChannel` and `LineTraceSingleByObjectType`, pre-cutoff)
**Engine Notes**: The bush-spot query channel is Concealment's (ADR-0017); coordinate the channel constant. No ADR-0015 verification item gates this story. Cost: at most 4 traces per query (on Attack and Strike-hint refresh).

**Control Manifest Rules (this layer)**:
- Required: The Attack payload comes only from Camera's `GetAimRay()`; a pawn with α = 0 or blocked from the eye produces a `TargetSpot`; the query is identical for occupied and empty bushes — source: ADR-0015, ADR-0011, ADR-0017
- Forbidden: Never add lock-on, snapping, magnetism or aim assist; never treat a resolved `PawnRef` as evidence of visibility — source: ADR-0015, ADR-0011
- Guardrail: ≤ 4 traces per query

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-10, AC-CS-34) and ADR-0015 §5 (derived item marked), scoped to this story:*

- [ ] **AC1** (AC-CS-10, [P] `AimRayCorner`): **GIVEN** a dummy within 2 m that is visible from the 3P lens but not from the pivot **WHEN** Attack is pressed with the crosshair on it **THEN** the command carries `TargetSpot` and no `PawnRef`, and no Strike hint shows; **AND** in 1P at the same spot the result is identical.
- [ ] **AC2** (AC-CS-34, [P] `WorldPointIntoBush`): **GIVEN** the crosshair on an occupied bush and on an empty bush, in 1P and in 3P **THEN** the Attack's `WorldPoint` is the ray's `HideVolume` entry point projected to the ground, within 5 cm, in all four cases; **AND** the client presentation is identical between occupied and empty.
- [ ] **AC3** (derived, ADR-0015 §5 and Guideline 9): **GIVEN** an Attack **THEN** `GetAimRay` is the only source of `WorldPoint` and `TargetPawn`; 1P uses the lens centre ray and 3P the two-step ray (centre ray from the point nearest the pivot, then a re-trace from the pivot to P + 10 cm); a pawn is targetable only if `IsEyeMaskVisible` and the eye ray reaches it, otherwise `TargetSpot`; at most 4 traces per query.

---

## Implementation Notes

*Derived from ADR-0015 Decision §5:*

Return type `FAimRay { FVector Origin; FVector Direction; FVector WorldPoint; TWeakObjectPtr<const AStealthPawn> TargetPawn; bool bTargetSpot; }` on `IStealthCameraView` (a View interface; IIF's payload builder and the Strike hint consume it through that interface, ADR-0007 §4). Reach is IIF's, measured from the pawn, not by the camera. No part of the ray reads team, binding or kind; the `PawnRef` agreement (`PawnRefAgreementCm`) and `CanViewerKnow` are IIF/IVP. Pings use the same function (informant-pings-team-comms PNG-R3: the ping point is the `ViewBlock` aim-ray hit dropped to walkable ground).

Prototype replaced: the prototype attack's range-and-cone targeting (`StealthCharacter.cpp` `OnAssassinatePressed` / `ServerAssassinate`, `StealthRules` targeting); retired with IIF `story-093-static-scan-and-prototype-retirement.md`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- IIF: payload builder, `CanViewerKnow` gating, Strike hint rendering (`input-interaction-framework/story-073-strike-hint-parity.md`, `story-064-no-ally-attack-no-aim-assist.md`).
- `story-013-mask-live-world-is-eye-mask-visible.md`: `IsEyeMaskVisible`.
- concealment-bushes epic: `HideVolume` and the bush-spot channel.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthCameraView.h` (`IStealthCameraView`, `FAimRay`)
- `Source/StealthGameplay/Private/Core/Camera/StealthAimRay.cpp`
- `Source/StealthGameplay/Private/Tests/Camera/AimRayCornerTest.cpp` (functional test, name `StealthGame.Camera.AimRayCorner`)
- `Source/StealthGameplay/Private/Tests/Camera/WorldPointIntoBushTest.cpp` (functional test, name `StealthGame.Camera.WorldPointIntoBush`)
- `Source/StealthGameplay/Private/Tests/Camera/AimRayContractTest.cpp` (class `FCameraAimRayContractTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC2: "client presentation identical between occupied and empty" names no fields, and the AC does not say whether `TargetPawn` may be set for an occupant whose foliage ignores `ViewBlock`. QA-lead proposed rewrite (unconfirmed; the author confirms the occupant case): for both bushes `TargetPawn` is null with `bTargetSpot` true, no Strike hint shows, and crosshair and marker state are equal.
- Scenario sketches: AC1 dummy at 1.5 m (also 2.0 and 2.1 m), `bTargetSpot` true and no `TargetPawn`, no Strike hint, 1P identical; a dummy visible from both lens and pivot sets `TargetPawn` (positive control). AC2 all four cases within 5 cm of the HideVolume entry point projected to the ground. AC3 1P origin at the lens, 3P starts at the point nearest the pivot then re-traces from the pivot to P + 10 cm, a pawn with α > 0 but blocked, or reachable with α = 0, gives `TargetSpot`, ≤ 4 traces per query (1P ≤ 2), and the Attack payload builder takes `WorldPoint` and `TargetPawn` only from `GetAimRay` (static check); a pawn between lens and body is not targetable; range clamp.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three tests above under `Source/StealthGameplay/Private/Tests/Camera/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-007-viewblock-channel-collision-profiles.md`, `story-013-mask-live-world-is-eye-mask-visible.md`; cross-epic: concealment-bushes (`HideVolume`, bush-spot channel), input-interaction-framework (Attack payload builder, `FAttackPayload`)
- Unlocks: input-interaction-framework Attack and Strike-hint stories; informant-pings-team-comms ping point; `production/epics/camera-system/story-023-playtests-scout-read-own-pawn.md`
