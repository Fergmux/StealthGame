# Story 025: Idle gaze look-input deadzone (decision D31)

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R8.9 look-input tolerance, decision D31 2026-10-25, /review-all-gdds 2026-10-25b W2; AC-SCL-51)
**Requirement**: `TR-scl-015`, `TR-scl-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams (domain 16 `IdleGaze`), with ADR-0004 (the deadzone key and its validator range) and ADR-0001
**ADR Decision Summary**: "Look input has not changed" is evaluated with a deadzone: a control-rotation change below `IdleGazeLookDeadzoneDegPerSec` (0.5 deg/s, measured on the server as the angular distance between the control rotation now and at the start of the idle timer divided by elapsed idle time) counts as no look input. Any change at or above it resets the timer and ends the layer. It is one server-side rule for every pawn kind, so a player's resting-mouse jitter and an NPC's identical re-issued `Look` are the same condition.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure server-side logic. The key is `ServerOnly`, latch `Use`, never replicated. As with story 024, TR-scl-015 is marked Partial in `requirements-traceability.md`; implement exactly GDD R8.9 and flag the ownership gap.

**Control Manifest Rules (this layer)**:
- Required: the deadzone is one server-side rule for every pawn kind; the key is `ServerOnly` and validated by the TDC validator (hard range 0-3).
- Forbidden: never replicate the key or the timer; never evaluate the deadzone on the client or per kind.
- Guardrail: negligible.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-51** [Logic/BLOCKING] `IdleGazeLookDeadzone` (R8.9, D31): GIVEN a pawn standing still at a 1/60 s server step with `IdleGazeLookDeadzoneDegPerSec` = 0.5, WHEN (a) control rotation drifts at 0.3 deg/s for 5 s, THEN the idle scan begins at `IdleGazeDelaySec` (3 s) ± 1 frame as if the input were unchanged; WHEN (b) control rotation drifts at 0.7 deg/s, THEN the timer resets continuously and no scan begins; WHEN (c) the value is set to 0, THEN any non-zero change resets the timer (case (a) never scans); and for a player pawn and an NPC pawn with identical rotation traces the result is identical (same dirty properties, same send ticks); the key is `ServerOnly`, never replicated, and out-of-range values (< 0 or > 3) are rejected by the TDC validator.

---

## Implementation Notes

*Derived from GDD R8.9 (D31):*

- Replace story 024's exact-equality "unchanged" test with the deadzone rule in the one comparison function: drift rate = angular distance between control rotation now and at the idle-timer start, divided by elapsed idle time; below the key it counts as no input and the timer is not reset; at or above it the timer resets and the layer ends.
- A deadzone of 0 means any non-zero change resets the timer (case (c)). Cases (a) and (b) use a synthetic control-rotation trace at a 1/60 s step; the player-versus-NPC identity check compares dirty properties and send ticks on two pawns driven by identical traces.
- Out-of-range validation (below 0 or above 3 rejected) is the TDC validator's behaviour on the schema row registered in story 003; assert it here through the validator's test entry point.
- BLOCKING gate: the test must exist and pass before the story closes.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024: the idle gaze layer, sweeps, stream and parity classifier.
- Story 003: schema row registration (this story asserts its rejection behaviour).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/IdleGazeLayer.cpp` (deadzone comparison)
- `Source/StealthGameplay/Private/Tests/Locomotion/IdleGazeLookDeadzoneTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. Test `StealthGame.Locomotion.IdleGazeLookDeadzone`.*

Test: idle gaze look-input deadzone (AC-SCL-51, D31)
  Given: a standing pawn at a 1/60 s server step with `IdleGazeLookDeadzoneDegPerSec` = 0.5
  When: control rotation drifts as in (a) 0.3 deg/s for 5 s, (b) 0.7 deg/s, (c) with the key set to 0
  Then: (a) the idle scan begins at 3 s ± 1 frame as if the input were unchanged; (b) the timer resets continuously and no scan begins; (c) any non-zero change resets the timer (case (a) never scans); a player pawn and an NPC pawn with identical rotation traces give identical results (same dirty properties, same send ticks); the key is `ServerOnly` and never replicated; values below 0 or above 3 are rejected by the TDC validator
  Edge cases: pin the rate definition (per-frame or windowed) from decision D31 in the fixture; validator boundaries: reject −0.001 and 3.001, accept 0 and 3; exactly 0.5 deg/s is unspecified and not asserted

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic (BLOCKING): `Source/StealthGameplay/Private/Tests/Locomotion/IdleGazeLookDeadzoneTest.cpp`, test name `StealthGame.Locomotion.IdleGazeLookDeadzone` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-024-idle-gaze-parity.md`, `story-003-tuning-keys-registered.md`
- Unlocks: None
