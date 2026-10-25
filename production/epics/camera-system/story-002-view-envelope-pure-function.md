# Story 002: View envelope pure function

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope
**ADR Decision Summary**: `EvaluateViewEnvelope` (GDD F1) is a pure, stateless-by-argument function in `StealthGameplay/Public/View/Core/StealthViewEnvelope.h`: previous `FViewEnvelopeState`, raw Look delta, `dt`, body yaw, own `bConcealed` and `FViewEnvelopeParams` in; new state, view rotation and control-rotation target out. No `UObject`, no world. Crowd's virtual input layer calls the same function per NPC, so NPC edge push and relax match a human's.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; this story is plain C++ math, pre-cutoff)
**Engine Notes**: No engine API beyond `FRotator`/`FMath`. No ADR-0015 verification item gates it. The pitch range -60/+40 for both modes was confirmed by the user on 2026-10-25 (camera OQ9); any later change is data only.

**Control Manifest Rules (this layer)**:
- Required: The envelope is one pure function shared with Crowd; it is the only writer of a human's control rotation (story 005 wires it) — source: ADR-0015
- Forbidden: Never add aim assist, snapping, magnetism, slowdown, zoom, shake or FOV punches — source: ADR-0015, ADR-0011
- Guardrail: Envelope evaluation is a few microseconds per frame per viewer; no allocation, no world access

This story is the numeric heart of the view limits. It implements GDD F1 (soft edge gain, hard limit at 70° yaw, -60/+40 pitch, the 75° overshoot target, the outward push latch and the relax rate) as a function with no engine dependency, so every number in AC-CS-03/04/38 can be asserted headless at a fixed 1/60 s step. The parts of AC-CS-03 that involve SCL turning the body or an observer reading replicated head pitch need a world and are story 005.

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-03 numeric bullets, AC-CS-04, AC-CS-38), scoped to this story:*

- [ ] **AC1** (AC-CS-03, [U] `EnvelopeClamp`, numeric bullets): **GIVEN** idle body facing 0° and continuous outward yaw input at 90 °/s for 2 s, then pitch input at 60 °/s for 2 s down and 2 s up **THEN** view yaw ≤ 70.0° and view pitch stays within −60.0° and +40.0°; the target yaw is ≤ 75.0° and exceeds 70° while input continues; **AND** when input stops before 0.5 s the target relaxes to 70° within 0.15 s + 0.09 s.
- [ ] **AC2** (AC-CS-04, [U] `SoftEdgeNoDeadZone`): **GIVEN** the F1 gain **WHEN** evaluated at |v| = 60, 65 and 70 **THEN** the gain is 1.0, 0.5 and 0.1; **AND** from a held 75° target one frame of inward input moves the view inward on that same frame; **AND** at 180 °/s raw outward input the view reaches 70° within 0.25 s of crossing 60°.
- [ ] **AC3** (AC-CS-38, [U] `MouseLatchedPush`): **GIVEN** mouse-style input at the yaw limit, 0.08 s bursts at 200 °/s separated by 0.12 s gaps for 1.5 s **THEN** `pushing` stays true throughout and the target stays above 70°; **AND** with 0.2 s gaps the target relaxes in each gap.

---

## Implementation Notes

*Derived from ADR-0015 Decision §2 and GDD F1:*

`FViewEnvelopeState` carries the view offset `v` (relative to body yaw), the target `r`, the push latch timestamp and the pushing flag. `FViewEnvelopeParams` is built from the story-001 keys: `LookYawLimitDeg` (SCL, 70), `ViewPitchDownLimitDeg`/`ViewPitchUpLimitDeg` (60/40), `CameraEdgePushDeg` (5; overshoot is 0 while own `IsConcealed`), `EdgePushMinRateDegPerSec`, `EdgePushLatchSec`, `CameraEdgeSoftMinGain`, `CameraEdgeSoftYawDeg`/`CameraEdgeSoftPitchDeg`, `EdgePushRelaxDegPerSec`. The disabled-by-default `ViewHeadLeadMaxDeg` clamp (CS-R3.5) lives inside the same function so enabling it later changes data, not code; it must have a test that it is a no-op when disabled.

Input is the frame's raw look delta after stick response (F6, story 003) and sensitivity, before any gain. The test harness uses a fixed dt of 1/60 and lets the rig converge 30 frames before asserting (GDD AC preamble). Keep the function free of `UObject`, the clock and statics so Crowd (ADR-0016) can call it from the server and so the unit tests need no world.

Prototype replaced: none. The prototype has no envelope; look is unclamped (`StealthCharacter.cpp`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-003-stick-look-response-no-aim-assist.md`: stick curve (F6) upstream of this function.
- `story-005-look-wiring-control-rotation.md`: `StepEnvelope`, `UpdateRotation` override, SCL body turn and replicated head pitch.
- Crowd's call site of `EvaluateViewEnvelope`: crowd-npc-behaviour epic (see story 024's Tell Matrix review).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthViewEnvelope.h`
- `Source/StealthGameplay/Private/Core/Camera/StealthViewEnvelope.cpp`
- `Source/StealthGameplay/Private/Tests/Camera/EnvelopeClampTest.cpp` (class `FCameraEnvelopeClampTest`, name `StealthGame.Camera.EnvelopeClamp`)
- `Source/StealthGameplay/Private/Tests/Camera/SoftEdgeNoDeadZoneTest.cpp` (class `FCameraSoftEdgeNoDeadZoneTest`)
- `Source/StealthGameplay/Private/Tests/Camera/MouseLatchedPushTest.cpp` (class `FCameraMouseLatchedPushTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC3 second clause: with 0.2 s gaps the push latch holds `pushing` for 0.15 s, so only 0.05 s of relax at 60 °/s remains and the target returns only to about 72°, never to 70°, before the next burst. "The target relaxes in each gap and the body does not turn" holds only if SCL's beyond-limit timer resets when the target dips. QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "with 0.2 s gaps the target falls at 60 °/s in the last 0.05 s of each gap and never exceeds 75°; whether SCL's beyond-limit timer resets is asserted in story 005". The author confirms against SCL R5.5.
- Scenario sketches: AC1 asserts |v| ≤ 70.0 and pitch within [−60, +40] every frame, target ≤ 75.0 and > 70 while input continues, and ≤ 70.0 within 15 frames after input stops before 0.5 s (run with `bConcealed` false; concealed is story 005 AC2). AC2 asserts gain 1.0, 0.5, 0.1 (±1e-3) at |v| = 60, 65, 70, the same-frame inward move from a held 75° target, and 70.0 reached within 15 frames of crossing 60° at 180 °/s (hand calculation: about 12 frames). AC3 first clause: `pushing` true on every frame and target above 70 for 0.08 s bursts at 200 °/s with 0.12 s gaps.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/Camera/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-001-camera-tuning-keys-and-invariants.md`; cross-epic: tuning-data-config module skeleton (`story-002-gameplay-presentation-module-skeletons.md`)
- Unlocks: `production/epics/camera-system/story-005-look-wiring-control-rotation.md`
