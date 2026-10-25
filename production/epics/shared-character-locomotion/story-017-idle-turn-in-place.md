# Story 017: Idle turn-in-place after a held look beyond the yaw limit

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R5.5 Idle turn-to-look; AC-SCL-45; Open Question OQ13)
**Requirement**: `TR-scl-004`, `TR-scl-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6, idle pivot and idle turn-to-look in `PhysicsRotation`)
**ADR Decision Summary**: When idle, if the unclamped head-look target stays beyond `LookYawLimitDeg` for `TurnInPlaceDelaySec`, the body turns in place at `PivotTurnRateDegPerSec` until head yaw is within `TurnInPlaceExitFrac × LookYawLimitDeg` (35° at defaults). It is the same rule for the human and the AI focus target; the trigger accepts Camera's elastic overshoot of up to 5° (minimum 2°).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Body yaw is never driven by control rotation (`bUseControllerRotationYaw = false`); the turn is a `PhysicsRotation` override reading the F6 filter's unclamped target.

**Control Manifest Rules (this layer)**:
- Required: control rotation has exactly one consumer on the pawn, the F6 filter; body yaw is never driven by control rotation.
- Forbidden: no pawn uses a controller-yaw-driven body; no presentation code reads control rotation or `GetBaseAimRotation()`.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-45** [P] `IdleTurnInPlace`: GIVEN an idle pawn whose look target is held at 100° yaw, WHEN 0.49 s and then 0.51 s pass, THEN the body has not turned at 0.49 s, starts turning at ≤ 120 °/s after 0.50 s ± 1 frame, and stops when head yaw ≤ 35°; identical for an AI focus target.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the AC's probes at 0.49 s and 0.51 s with a ± 1 frame tolerance cannot both hold at 60 Hz (a frame is 16.7 ms), so at a 1/60 s step the test asserts no body turn through frame 29 (0.483 s) and turning begun by frame 31 (0.517 s); confirm at `/story-readiness`.

---

## Implementation Notes

*Derived from ADR-0010 section 6 and GDD R5.5:*

- Add the idle turn-to-look rule to the model: state `Pivot` entered from `Idle` when the unclamped look target has been beyond `LookYawLimitDeg` for `TurnInPlaceDelaySec` (0.50 s); exit at |head yaw| ≤ `TurnInPlaceExitFrac × LookYawLimitDeg`. The unclamped target comes from the look path (story 021); until it lands, drive the rule from a stubbed target.
- Test twice with the same assertions: a human look input (saved-move path) and an AI focus target (`Look` from a stub crowd controller): the traces must match.
- OQ13 (raising `PivotTurnRateDegPerSec` to 150) is a tuning decision for the first look-parity playtest, not part of this story.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 021: the F6 head-look filter itself.
- Story 012: turn-while-moving shaping.
- Crowd's gaze catalogue (Crowd epic).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp` (idle turn-to-look)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMovementComponent.cpp` (`PhysicsRotation`)
- `Source/StealthGameplay/Private/Tests/Locomotion/IdleTurnInPlaceTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.IdleTurnInPlace`.*

Test: idle turn-in-place (AC-SCL-45)
  Given: an idle pawn whose look target is held at 100° yaw, in one run with a human look input and in another with an AI focus target
  When: time advances past 0.50 s at a 1/60 s step
  Then: the body has not turned through frame 29 (0.483 s) and has begun turning by frame 31 (0.517 s), at ≤ 120 °/s; it stops when head yaw ≤ 35°; results are identical for the AI focus target
  Edge cases: the target is beyond the head clamp (±70.6°), so assert the head pins at the clamp while the body catches up

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/IdleTurnInPlaceTest.cpp`, test name `StealthGame.Locomotion.IdleTurnInPlace` — must exist and pass; retained screenshot of the idle body turning toward a held look in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-012-turn-shaping.md`, `story-021-look-envelope-700-deg-spin.md`
- Unlocks: None
