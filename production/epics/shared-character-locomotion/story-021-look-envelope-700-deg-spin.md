# Story 021: One look path: HeadLook stays inside the envelope under a 700 deg/s spin

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R8 One look path, F6 Head-look rate limit; AC-SCL-09)
**Requirement**: `TR-scl-005` (one look path; `RemoteViewPitch16` written from `Look` for every pawn)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (one look path, identity blindness) with ADR-0003 section 3 (one pitch path)
**ADR Decision Summary**: `Look(Rotation)` sets control rotation for every pawn (human through the saved-move path, AI by the crowd controller writing it directly); control rotation has exactly one consumer, the F6 filter, which turns it into the server-computed `HeadLook` state (torso-relative yaw and pitch clamped to `±LookYawLimitDeg` / `±LookPitchLimitDeg`, rate-limited at fixed 1/60 s sub-steps, in the pawn's own tick, never inside ServerMove processing).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `HeadLook` yaw replicates as one byte over ±`LookYawLimitDeg` (0.55° at 70°); pitch goes through the engine's `RemoteViewPitch16` (16-bit, ≈ 0.0055°, verified 5.8.4). A ServerMove-stepped pawn and a tick-stepped AI pawn must produce `HeadLook` at the same cadence, which is why the filter runs on the pawn tick, not in `MoveAutonomous`.

**Control Manifest Rules (this layer)**:
- Required: one control-rotation path for every pawn; the pawn's F6 filter is the only consumer; owner runs the same pure filter locally for prediction.
- Forbidden: no Look event; no presentation code reads control rotation or `GetBaseAimRotation()`; never set control rotation from the actor rotation after possession; never add a Move/Look RPC.
- Guardrail: filter cost counts inside the pawn replication and CMC lines (trivial per pawn).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-09** [N] `LookEnvelope700DegSpin`: GIVEN one scripted target sequence (700 °/s spin for 5 s, then a 180° snap, then a 90° diagonal) driven once through the human input path and once through the AI focus path, 20 runs each with a fixed seed, WHEN an opponent client logs every received `HeadLook` yaw and `RemoteViewPitch` sample, THEN yaw stays within ±70.6°, pitch within ±20.1° (as AC-SCL-18), per-sample yaw step ≤ 140/15 + 0.55 = 9.9° and pitch step ≤ 9.4° (as AC-SCL-18), the time to reach 90 % of 140 °/s is 0.12-0.18 s, the diagonal's vector rate ≤ 140 °/s + 5 %, actor yaw rate ≤ 120 °/s when idle, and the human and AI step-size distributions are equivalent (two-sample KS D ≤ 0.15, n ≥ 200 samples per path).

---

## Implementation Notes

*Derived from ADR-0001, ADR-0003 section 3 and GDD R8.1-R8.8, F6:*

- Build the F6 filter as a plain-C++ pure function (2D rate-limited, `LookMaxRateDegPerSec` 140, `LookEaseSec` 0.15 for eased onset and stop) fixed at 1/60 s sub-steps, in the pawn's tick. The filter's output is `HeadLook` (yaw as the fixed-width byte, pitch as the clamped value); the owner runs the same function locally and simulated proxies run it toward the latest received `HeadLook` (story 023).
- Human `Look` travels only in `ServerMove` (saved move); AI `Look` is the crowd controller writing control rotation including pitch. Both end as control rotation consumed once by the filter.
- Body yaw is never driven by control rotation (`bUseControllerRotationYaw = false`), so a 700 deg/s mouse spin is visible nowhere: not in `HeadLook`, not in actor rotation, not in pitch.
- This is a CQTest `NETWORK_TEST_CLASS` in `Source/StealthTestHarness/Private/` with an opponent-client logger and a fixed-seed RNG for the script variations; KS statistics with n ≥ 200 samples per path.
- **Prototype replaced**: the prototype replicates raw view rotation and the player-only `RemoteViewPitch16` (spike S1 measured it as a differentiator); no filter existed.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 022: `RemoteViewPitch16` write in `PreReplication` and its clamp test.
- Story 023: observer-side interpolation.
- Stories 024 and 025: the idle gaze layer and its deadzone.
- Story 017: idle body turn-to-look.

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/HeadLookFilter.cpp` (plain C++, planned)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp` (tick integration, `HeadLook` state)
- `Source/StealthTestHarness/Private/Locomotion/LookEnvelope700DegSpinTest.cpp` (planned; CQTest `StealthGame.Locomotion.PIE.LookEnvelope700DegSpin.*`)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. Test `StealthGame.Locomotion.PIE.LookEnvelope700DegSpin.HumanVsAi`.*

Test: look envelope under a 700 °/s spin (AC-SCL-09)
  Given: one scripted target sequence (700 °/s spin for 5 s, a 180° snap, then a 90° diagonal), driven once via the human input path and once via the AI focus path, 20 runs each (fixed seed)
  When: an opponent client logs every received `HeadLook` yaw and `RemoteViewPitch` sample
  Then: yaw stays within ±70.6° and pitch within ±20.1°; per-sample yaw step ≤ 9.9° and pitch step ≤ 9.4°; time to 90 % of 140 °/s is 0.12-0.18 s; the diagonal's vector rate ≤ 140 °/s + 5 %; actor yaw rate ≤ 120 °/s when idle; the human and AI step-size distributions are equivalent (two-sample KS D ≤ 0.15, n ≥ 200 per path)
  Edge cases: fixed-seed RNG for any script variation

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/LookEnvelope700DegSpinTest.cpp` — must exist and pass (multi-client PIE); retained screenshot of the head tracking under the spin in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md`, `story-003-tuning-keys-registered.md`; `production/epics/input-interaction-framework/EPIC.md` (`Look`), `production/epics/tuning-data-config/EPIC.md` (Exit A harness)
- Unlocks: `production/epics/shared-character-locomotion/story-017-idle-turn-in-place.md`, `story-022-remote-view-pitch-clamped.md`, `story-023-head-look-observer-interpolation.md`, `story-024-idle-gaze-parity.md`, `production/epics/camera-system/EPIC.md` (`GetHeadLookWorldRotation`)
