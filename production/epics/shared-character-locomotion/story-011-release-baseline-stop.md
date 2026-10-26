# Story 011: Release-to-stop baseline, identical for AI and for owner prediction

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R5.3, F2; AC-SCL-06; ADR-0010 spike P1)
**Requirement**: `TR-scl-004`, `TR-scl-010` (smoothing parity and input-starvation rule)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6, "Prediction state (spike P1)")
**ADR Decision Summary**: The pivot latch, pace-quantiser hysteresis state, last commanded input and starvation flag are movement state saved in `FSavedMove_Stealth` (`SetMoveFor`/`PrepMoveFor`) and restored on client replay, so owner prediction matches the server within one frame at 100 ms lag. Only if a server correction must carry them is a custom response container added.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Gate **P1** (ADR-0010 Verification Required): this story's owner-vs-server comparison is the P1 verification. A divergence above 1 frame at 100 ms lag means the saved-move state is incomplete; record the result in `production/qa/spikes/` for ADR-0010's gate. `AllocateNewMove`/`FSavedMove_Character` and `UpdateFromCompressedFlags` are the override points (E8).

**Control Manifest Rules (this layer)**:
- Required: movement shaping is one function per rule for every pawn, runs identically on the server and in owner prediction; shaping state used by prediction is saved/restored in `FSavedMove_Stealth` (ADR-0010).
- Forbidden: never move a pawn through `RequestDirectMove`/`RequestPathMove` or AI path following (`bUseAccelerationForPaths`); AI movement enters only through `UPawnCommandComponent::Move`.
- Guardrail: shaping under 5 % of the 3.0 ms CMC line.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-06** [P] `ReleaseBaselineStop`: GIVEN a pawn at 140 cm/s (and a second run at 112 cm/s) at 60 fps, WHEN input is released, THEN it reaches 0 in 0.75 s (0.60 s at 112) ± 1 frame over 53 ± 3 cm (34 ± 3 cm), peak deceleration ≤ 187 cm/s² + 5 %, an AI pawn given the same commands differs by ≤ 1 frame and 1 cm, and the owner-predicted path at 100 ms lag matches the server within 1 frame (IIF AC 24).

*QA clarification (create-stories 2026-10-25, not a GDD change):* "the owner-predicted path matches the server" means stop-completion time within 1 frame and final stop position inside the 53 ± 3 cm (34 ± 3 cm at 112) band.

---

## Implementation Notes

*Derived from ADR-0010 section 6 and its Validation gates (P1):*

- Implement `FSavedMove_Stealth` carrying the pivot latch, pace-quantiser hysteresis state, last commanded input and starvation flag; restore them in `PrepMoveFor` on replay. Replay-only fields need no custom move-data container (engine gate).
- Measure at 60 fps with a simulated 100 ms lag (net-emulation `PktLag`) on one PIE client; the AI comparison drives an NPC pawn with identical `Move` commands and the same release frame.
- The 140 and 112 cm/s cases come from F2 with the stop floor of story 009: 0.75 s over 53 cm at 140 and 0.60 s over 34 cm at 112; peak deceleration must not exceed 187 cm/s² plus 5 %.
- Recorded divergence feeds the P1 spike entry; if it fails, the ADR's pre-decided path is to add the missing field to the saved move, then (only if corrections must carry it) a custom response container.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 009: the stop-floor formula.
- Stories 050 and 051: input-starvation braking and recovery (Blocked on the net-emulation spike).
- Story 012: turn shaping and pivot latch behaviour (this story only saves/restores its state).

---

## Files

- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.h`
- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthLocomotionModel.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnMovementComponent.cpp`
- `Source/StealthGameplay/Private/Tests/Locomotion/LocomotionTestWorld.h`
- `Source/StealthGameplay/Private/Tests/Locomotion/ReleaseBaselineStopTest.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/Locomotion/LocomotionPieTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.ReleaseBaselineStop` (the lag part under `PIE.`).*

Test: release-to-stop baseline and owner prediction (AC-SCL-06, spike P1)
  Given: a pawn at 140 cm/s and a second run at 112 cm/s at 60 fps
  When: input is released (also with a simulated 100 ms lag on one PIE client)
  Then: it reaches 0 in 0.75 s (0.60 s at 112) ± 1 frame over 53 ± 3 cm (34 ± 3 cm); peak deceleration ≤ 187 cm/s² + 5 %; an AI pawn given the same commands differs by ≤ 1 frame and 1 cm; the owner-predicted path at 100 ms lag has stop-completion time within 1 frame of the server and final stop position inside the 53 ± 3 cm (34 ± 3 cm at 112) band
  Edge cases: the stop figures assume the stop floor of story 009 is satisfied; record the divergence figure for the P1 spike entry

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/ReleaseBaselineStopTest.cpp`, test name `StealthGame.Locomotion.ReleaseBaselineStop` — must exist and pass; the P1 owner-vs-server divergence figure recorded in `production/qa/spikes/`; a retained screenshot of the stop in `production/qa/evidence/` (player-observable)

**Status**: [x] Created and passing (`StealthGame.Locomotion.ReleaseBaselineStop`; `StealthGame.Locomotion.PIE.LocomotionPie.ReleaseBaselineStopUnderLag`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-009-stop-floor.md`, `story-010-speed-tiers.md`; `production/epics/input-interaction-framework/EPIC.md` (IIF AC 24 owner-predicted path)
- Unlocks: `production/epics/shared-character-locomotion/story-012-turn-shaping.md`, `story-027-handover-blend.md`

---

## Completion Notes
**Completed**: 2026-10-26 (SCL batch: 003, 007, 009-018, 046, 047)
**Criteria**: 1/1: 140 -> 0 in 0.75 s over 52.5 cm, 112 -> 0 in 0.60 s over 33.6 cm, peak deceleration 186.7 cm/s^2, AI and owner paths identical; at PktLag 100 ms the owner stops in 0.746 s over 50.2 cm and the server in 0.751 s over 51.7 cm, rest positions 0.13 cm apart
**Deviations**: PktLag 100 ms is applied to every driver (stricter than client-only). FSavedMove_Stealth carries Run (FLAG_Custom_0) and restores the pace, stop latch, reversal and pivot latches on replay; no custom move-data container was needed (P1).
**Test Evidence**: `StealthGame.Locomotion.ReleaseBaselineStop`; `StealthGame.Locomotion.PIE.LocomotionPie.ReleaseBaselineStopUnderLag`; full suite 171/171 StealthGame.* Success (`production/qa/evidence/story-010-speed-tiers/full-suite.txt`)
**Run result**: OBSERVED - production/qa/evidence/story-010-speed-tiers/ (stop stills)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review
