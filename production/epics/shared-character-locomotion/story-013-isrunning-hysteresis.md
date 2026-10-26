# Story 013: IsRunning with hysteresis

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R19 Published state, F4 IsRunning with hysteresis; AC-SCL-08)
**Requirement**: `TR-scl-012` (published server state)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 8)
**ADR Decision Summary**: On the server, for every pawn, `UStealthMovementComponent` exposes `IsRunning()` (F4 with hysteresis, from commanded-locomotion horizontal velocity, forced false in traversal, `PoseLocked`, falling or `Disabled`, initialised false at spawn and at every hand-over). It is never replicated; the owner computes it locally for prompt legality only. The IIF reads it through `ISharedPawnStateView`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `RunSpeedThresholdCmPerSec` (300) and `RunExitHysteresisCmPerSec` (25) are `ServerOnly` keys (exit 275); invariant I-SCL-1 keeps `WalkSpeedCmPerSec < threshold − hysteresis` and `threshold < RunSpeedCmPerSec`.

**Control Manifest Rules (this layer)**:
- Required: published state is exposed through the `ISharedPawnStateView` Core header (ADR-0011).
- Forbidden: never replicate `IsRunning`, `IsInTraversal` or `CommandedDisplacementCm` (owner-local copies for prompts only) (ADR-0010).
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-08** [P] `IsRunningHysteresis`: GIVEN speed ramps 0 → 450 → 0 and then dithers ± 10 cm/s around 290 for 5 s, WHEN `IsRunning` is sampled, THEN it turns true above 300, stays true down to 275, turns false below 275, does not toggle during the dither, falls 0.29 s ± 1 frame after Run is released at 450, and is false throughout a vault and a strike root motion even when horizontal speed exceeds 300.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the dither is run from both initial states (true and false); the vault and strike clause uses a forced root-motion montage stub until stories 047/048 land.

---

## Implementation Notes

*Derived from ADR-0010 section 8 and GDD F4/R19:*

- `IsRunning` is a pure hysteresis function of commanded-locomotion horizontal speed, evaluated each server step; it is derived from speed, not from the `Run` state, so a pawn slowing from a run stays `IsRunning` until below the exit threshold. Add it to the plain-C++ model so the dither case runs headless.
- Forced-false conditions: `IsInTraversal`, a `PoseLocked` action pose (`Strike`, `Counter`, `BushEnter`, `BushExit`), `Falling`, `Disabled`; initial false at spawn and at every hand-over. The vault and strike assertions need those states: until stories 047/048 land, use a stub traversal flag and a stubbed `FActionState` pose; the real traversal path is asserted again by story 048.
- Expose it on the server through `ISharedPawnStateView` (declared by the IIF epic); the owner copy is local and unreplicated.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 016: commanded displacement.
- Story 047 and 048: real traversal state.
- The IIF's legality use of the value (IIF epic).

---

## Files

- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.h`
- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthLocomotionModel.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnMovementComponent.cpp`
- `Source/StealthGame/Private/StealthCharacter.cpp`
- `Source/StealthGameplay/Private/Tests/Locomotion/LocomotionTestWorld.h`
- `Source/StealthGameplay/Private/Tests/Locomotion/IsRunningHysteresisTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.IsRunningHysteresis`.*

Test: IsRunning hysteresis (AC-SCL-08)
  Given: a speed ramp 0 → 450 → 0, then a ± 10 cm/s dither around 290 for 5 s, run from both initial states (true and false)
  When: `IsRunning` is sampled
  Then: it turns true above 300, stays true down to 275, turns false below 275; it does not toggle during the dither; it falls 0.29 s ± 1 frame after Run is released at 450; it is false throughout a vault and a strike root motion even when horizontal speed exceeds 300
  Edge cases: the vault and strike clause uses a forced root-motion montage stub until stories 047/048

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/IsRunningHysteresisTest.cpp`, test name `StealthGame.Locomotion.IsRunningHysteresis` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Locomotion.IsRunningHysteresis`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-010-speed-tiers.md`
- Unlocks: `production/epics/shared-character-locomotion/story-028-informant-respawn-takes-over-unseen-crowd-pawn.md` (eligibility reads `IsRunning`), `production/epics/input-interaction-framework/EPIC.md` (legality)

---

## Completion Notes
**Completed**: 2026-10-26 (SCL batch: 003, 007, 009-018, 046, 047)
**Criteria**: 1/1: true above 300, true down to 275, false below; no toggle in the +-10 dither from either state; falls 0.300 s after Run released at 450 (0.29 +- 1 frame); false under a Strike pose and in the traversal mode above 300
**Deviations**: The traversal is the MOVE_Flying stub and the strike a stubbed pose (stories 047/048 and the IIF action state re-assert them). ISharedPawnStateView's server adapter over the movement component is the IIF epic's; the prototype AStealthCharacter::IsRunning now reads the production value.
**Test Evidence**: `StealthGame.Locomotion.IsRunningHysteresis`; full suite 171/171 StealthGame.* Success (`production/qa/evidence/story-010-speed-tiers/full-suite.txt`)
**Run result**: N/A - IsRunning is unreplicated server state with no presentation
**Reviewed by**: lead-programmer focused review (sonnet) + self-review
