# Story 012: Turn shaping, latched reversal and pivot

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R5.4 Turning, F3 Turn rate and turning radius, States `Pivot`; AC-SCL-07)
**Requirement**: `TR-scl-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6, "Acceleration, braking, turning")
**ADR Decision Summary**: `bOrientRotationToMovement = true`, `bUseControllerRotationYaw = false` for every pawn; the acceleration direction turns at most `WalkTurnRateDegPerSec` / `RunTurnRateDegPerSec`; the reversal latch (stop, pivot, start above `ReversalAngleDeg`, hysteresis `ReversalHysteresisDeg`, released on `Pivot` exit) and the idle pivot (`PivotExitAngleDeg`, `PivotTurnRateDegPerSec`) live in `PhysicsRotation`/`ComputeOrientToMovementRotation` overrides.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `PhysicsRotation` and `ComputeOrientToMovementRotation` are CMC override points (E8). The pivot latch is prediction state saved in `FSavedMove_Stealth` (story 011, spike P1). An exact 180 degree tie turns in the sign of the last non-zero turn, else clockwise.

**Control Manifest Rules (this layer)**:
- Required: movement shaping is one function per rule for every pawn, saved/restored for prediction (ADR-0010); control rotation never turns the body directly.
- Forbidden: never set control rotation from the actor rotation after possession; no kind-dependent turn smoothing (ADR-0016 rejected the shared smoothing-filter alternative; NPC motion is input emulation).
- Guardrail: shaping under 5 % of the 3.0 ms CMC line.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-07** [P] `TurnShaping`: GIVEN a walking pawn, WHEN input turns 90°, THEN heading rate never exceeds 60 °/s and a circle fit over a 1 s arc gives r ≥ 134 cm − 5 % (≥ 107 cm − 5 % at 112 cm/s); running, r ≥ 143 cm − 5 %; WHEN input reverses 119°, THEN no pivot occurs; at 121° and at 150°, THEN the pawn stops, pivots at ≤ 120 °/s and restarts, and no 0.5 s window contains a heading change > 120°; WHEN the input oscillates 115-125° at 10 Hz, THEN the pawn changes between arc and pivot at most once.

---

## Implementation Notes

*Derived from ADR-0010 section 6 and GDD R5.4/F3:*

- Put the turn-rate cap, reversal trigger, latch with hysteresis and idle pivot into `FStealthLocomotionModel` as pure functions of (current heading, input heading, speed, latch state, tuning), called from the CMC overrides. A pawn at or below `PivotSpeedCmPerSec` whose input differs from facing by more than `PivotExitAngleDeg` enters `Pivot` directly.
- The reversal is latched once triggered, released only when `Pivot` exits, and re-triggers only when the angle exceeds `ReversalAngleDeg`; below `ReversalAngleDeg − ReversalHysteresisDeg` it never triggers. The 10 Hz oscillation case is the hysteresis test.
- All angles come from the same function for every pawn: the shaping is not player-only smoothing; AI pawns reach it through `bRequestedMoveUseAcceleration` and `Move` input (R5.7).
- **Prototype replaced**: prototype turning relied on the sample defaults; nothing is ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 017: idle turn-to-look (R5.5) body turn.
- Story 011: saved-move state restore verification.
- Run turn-rate tuning validation (tuning story 003).

---

## Files

- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.h`
- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthLocomotionModel.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnMovementComponent.cpp`
- `Source/StealthGameplay/Private/Tests/Locomotion/LocomotionTestWorld.h`
- `Source/StealthGameplay/Private/Tests/Locomotion/TurnShapingTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. Test `StealthGame.Locomotion.TurnShaping`.*

Test: turn shaping (AC-SCL-07)
  Given: a walking pawn
  When: input turns 90°, reverses 119°, 121° and 150°, and oscillates 115-125° at 10 Hz
  Then: heading rate never exceeds 60 °/s; a circle fit over a 1 s arc gives r ≥ 134 cm − 5 % (≥ 107 cm − 5 % at 112 cm/s), running r ≥ 143 cm − 5 %; at 119° no pivot occurs; at 121° and 150° the pawn stops, pivots at ≤ 120 °/s and restarts with no 0.5 s window containing a heading change > 120°; the oscillation changes between arc and pivot at most once
  Edge cases: pin the circle-fit algorithm in the fixture; exactly 120° is unspecified and not asserted

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/TurnShapingTest.cpp`, test name `StealthGame.Locomotion.TurnShaping` — must exist and pass; a retained screenshot of a walking arc and a pivot in `production/qa/evidence/` (player-observable)

**Status**: [x] Created and passing (`StealthGame.Locomotion.TurnShaping`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-010-speed-tiers.md`, `story-011-release-baseline-stop.md`
- Unlocks: `production/epics/shared-character-locomotion/story-017-idle-turn-in-place.md`, `story-018-ai-path-follow-routes-through-move.md`

---

## Completion Notes
**Completed**: 2026-10-26 (SCL batch: 003, 007, 009-018, 046, 047)
**Criteria**: 1/1: walk 60 deg/s r 133.7 cm (107.0 at 112), run 180 deg/s r 143.3 cm; 119 deg arcs, 121 and 150 deg stop-pivot (120 deg/s)-restart with <= 60 deg per 0.5 s; 115-125 deg at 10 Hz enters the pivot once
**Deviations**: Post-pivot-exit hysteresis added (idle pivot needs PivotExitAngleDeg + ReversalHysteresisDeg until the pawn passes PivotSpeedCmPerSec; decision log). 'Changes between arc and pivot at most once' is measured as stop-pivot entries.
**Test Evidence**: `StealthGame.Locomotion.TurnShaping`; full suite 171/171 StealthGame.* Success (`production/qa/evidence/story-010-speed-tiers/full-suite.txt`)
**Run result**: OBSERVED - production/qa/evidence/story-010-speed-tiers/
**Reviewed by**: lead-programmer focused review (sonnet) + self-review
