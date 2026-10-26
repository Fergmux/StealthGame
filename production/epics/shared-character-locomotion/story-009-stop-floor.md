# Story 009: Stop floor: every stop to rest lasts at least StopMinSec

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R5.3, F2 Acceleration, braking and stop distance, decision D2, OQ15 closed 2026-10-25; AC-SCL-50)
**Requirement**: `TR-scl-004` (CMC locomotion, speed tiers, state restrictions)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6, "Stop floor (closeout D2, SCL OQ15)")
**ADR Decision Summary**: The effective deceleration is `d_eff = min(d, v / StopMinSec)` with `StopMinSec` = 0.5 s (TDC key, safe 0.4-0.6 after I-SCL-16, hard 0.2-1.0), so every stop to rest lasts at least 0.5 s, including sub-pace stops and key taps. `GetMaxBrakingDeceleration` returns `d_eff` for every pawn (human, AI, starved, hand-over), and Crowd consumes the same key.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `GetMaxBrakingDeceleration` and `GetMaxAcceleration` are the CMC override points (E8). The floor decelerates at a constant rate latched at the start of each stop; the latch is movement state and must be saved in `FSavedMove_Stealth` (spike P1, story 011) so owner prediction matches.

**Control Manifest Rules (this layer)**:
- Required: movement shaping is one function per rule for every pawn, reads only tuning values, and runs identically on the server and in owner prediction; shaping state used by prediction is saved/restored in `FSavedMove_Stealth` (ADR-0010).
- Forbidden: never read controller kind, PlayerState or team in a movement rule (ADR-0010); no sudden-stop path for any input.
- Guardrail: movement shaping well under 5 % of the 3.0 ms CMC game-thread line.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-50** [Logic/BLOCKING] `StopFloor` (F2, D2): GIVEN stops to rest from v₀ = 20, 37, 60, 93, 112 and 140 cm/s at walk and 200 and 450 cm/s at run, THEN each stop lasts `max(v₀ / d_g, StopMinSec)` ± 1 frame (0.5 s for 20-93 cm/s at walk and 200 cm/s at run; 0.60 s, 0.75 s and 0.75 s for 112, 140 and 450), the deceleration is constant through each stop (latched at its start), and the same input sequence gives the same speed trace on a player pawn and an NPC pawn.

*QA clarification (create-stories 2026-10-25, not a GDD change):* "constant deceleration" means the per-frame deceleration equals the latched value within a float epsilon pinned in the fixture, at a fixed 1/60 s step; the player and NPC speed traces are equal within 1 frame.

---

## Implementation Notes

*Derived from ADR-0010 section 6 and GDD F2:*

- Build the plain-C++ `FStealthLocomotionModel` (no `UWorld`, ADR-0010 section 6) with the F2 functions: walk deceleration `d` from the slowest pace (`WalkSpeed × WalkSpeedScaleMinMult / WalkStopSec`, 187 cm/s² at defaults), run braking from `RunStopSec`, and `d_eff = min(d, v₀ / StopMinSec)` latched at the start of each stop. `UStealthMovementComponent::GetMaxBrakingDeceleration` calls it; nothing else decides a brake rate.
- Gate type is **BLOCKING** (`testing.strict` default for Logic): the test must exist and pass before the story closes.
- The player-versus-NPC equality is asserted by driving two pawns through the same `Move` sequence (player path through the saved-move input, NPC path through `UPawnCommandComponent::Move`) and comparing the speed traces frame by frame; a stub for the command component is fine until the IIF epic lands.
- Stops are bounded above by `T_stop,max = max(WalkStopSec / WalkSpeedScaleMinMult, RunStopSec)` (0.75 s at defaults), which I-SCL-12 and I-SCL-15 rely on: do not lengthen any stop beyond it.
- **Prototype replaced**: the prototype `StealthMovementComponent.*` sets braking without a floor; its values are not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 011: release-to-stop distance and owner-prediction match at 100 ms lag (P1).
- Story 010: speed tiers and pace quantisation.
- Crowd's use of the key for NPC stop distances (Crowd epic).

---

## Files

- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.h`
- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthLocomotionModel.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnMovementComponent.cpp`
- `Source/StealthGameplay/Private/Tests/Locomotion/LocomotionTestWorld.h`
- `Source/StealthGameplay/Private/Tests/Locomotion/StopFloorTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.StopFloor`.*

Test: stop floor (AC-SCL-50, F2, D2)
  Given: stops to rest from v₀ = 20, 37, 60, 93, 112 and 140 cm/s at walk and 200 and 450 cm/s at run, at a fixed 1/60 s step
  When: each stop is simulated through the F2 function and the movement component in a minimal world
  Then: each stop lasts `max(v₀ / d_g, StopMinSec)` ± 1 frame (0.5 s for 20, 37, 60 and 93 cm/s at walk and for 200 cm/s at run; 0.60 s, 0.75 s and 0.75 s for 112, 140 and 450); per-frame deceleration equals the latched value within the fixture epsilon; the same input sequence gives the same speed trace on a player pawn and an NPC pawn (within 1 frame)
  Edge cases: the floor binds only below the free-deceleration time (93 cm/s at walk, 200 cm/s at run); 37 and 60 cm/s are interior points; epsilon is pinned in the fixture

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/StopFloorTest.cpp`, test name `StealthGame.Locomotion.StopFloor` — must exist and pass (BLOCKING)

**Status**: [x] Created and passing (`StealthGame.Locomotion.StopFloor`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md`, `story-003-tuning-keys-registered.md`
- Unlocks: `production/epics/shared-character-locomotion/story-010-speed-tiers.md`, `story-011-release-baseline-stop.md`, `story-012-turn-shaping.md`

---

## Completion Notes
**Completed**: 2026-10-26 (SCL batch: 003, 007, 009-018, 046, 047)
**Criteria**: 1/1: 20/37/60/93 cm/s walk and 200 run stop in 0.50 s, 112 in 0.60, 140 and 450 in 0.75 (+- 1 frame), deceleration constant (latched), owner path and Move path traces equal frame by frame
**Deviations**: None.
**Test Evidence**: `StealthGame.Locomotion.StopFloor`; full suite 171/171 StealthGame.* Success (`production/qa/evidence/story-010-speed-tiers/full-suite.txt`)
**Run result**: OBSERVED - see production/qa/evidence/story-010-speed-tiers/ (walk/run/stop stills, shared batch capture)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review
