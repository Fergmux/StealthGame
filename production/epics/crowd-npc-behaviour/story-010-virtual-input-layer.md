# Story 010: Virtual input layer: device profiles, virtual camera and recordable stream

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-011 = Follower drives Move/Look through a virtual input layer with device profiles; a recordable input stream is replayable through a bot client for parity tests (CNB-R3.4, F6, AC-CNB-27/28). Note: TR-crowd-011 is in the registry but not yet in the epic's requirements table; raise it at the next `/architecture-review`.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0003 (pawn parity), camera-system GDD (`EvaluateViewEnvelope`, Camera F1, CS-R3)
**ADR Decision Summary**: The follower does not emit the world direction it wants. It keeps a per-pawn virtual camera (yaw, pitch) and a device profile drawn once per pawn (keyboard and mouse: 8-way quantised move relative to camera yaw; gamepad: analog stick with dead zone and response curve). Every controller tick it moves the virtual camera through Camera's `EvaluateViewEnvelope`, expresses the travel direction as the input a human with that camera would press, converts it to the world `Move` direction as the human client builder does, and emits `Look(VirtualCameraRotation)`. Profiles are data in `UCrowdMotionProfileData`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff API; the layer is pure C++ over the shared view-envelope function and Enhanced Input's quantisation conventions. The risk is statistical (AC-CNB-27, AC-CNB-28), not engine behaviour. `FCrowdVirtualInputFrame` recording is compiled out of Shipping.

**Why this story**: Input emulation is the ADR's answer to the Open Question about motion parity (OQ11): NPCs are made indistinguishable at the input layer, not filtered afterwards. Everything downstream (gaze, parity gate, classifier gate) depends on the layer existing. Read GDD CNB-R3 item 4 and F6 (`md-section.sh extract ... "Formulas"`, F6 part).

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: the virtual camera, input quantisation and `Move`/`Look` emission run every controller tick; profiles come from `UCrowdMotionProfileData` via the registered key `CrowdMotionProfileAsset`; the device profile is drawn once per pawn (`ECrowdDecision::DeviceProfile`) (ADR-0016)
- Forbidden: replacing device-shaped input with a turn-smoothing filter (fallback only, Alternative 3); `FMath::Rand*` (ADR-0016)
- Guardrail: the per-tick cost of this layer (including `EvaluateViewEnvelope`) is not in the F5 `c_f` term and is measured by the spike (story 032)

---

## Acceptance Criteria

*From GDD CNB-R3.4 and ADR-0016 section 4 items 3, 5, 6, scoped to this story (the parity gate is story 034):*

- [ ] A per-pawn device profile is drawn once (`ECrowdDecision::DeviceProfile`) from the motion profile's device mix; keyboard and mouse emits 8-way quantised input relative to virtual camera yaw, gamepad emits analog input with the profile's dead zone and response curve.
- [ ] The virtual camera advances and `Look` is emitted every controller tick (not at `CrowdSteerPerSec`) through `EvaluateViewEnvelope`: with steering at 30 Hz and 60 fps, `Look` values change on consecutive frames during a pan.
- [ ] A scripted walking glance changes the emitted `Move` direction by the amount the profile's quantisation predicts (the walking glance bends the path as a human's does).
- [ ] `FCrowdVirtualInputFrame {ServerTime, CameraYaw, CameraPitch, DeviceInput, Gait}` is recorded in non-Shipping builds only (symbol absent from Shipping).

---

## Implementation Notes

*Derived from ADR-0016 section 4:*

- `UCrowdMotionProfileData` (ServerOnly data asset): device mix, per-device quantisation, view-envelope push statistics, turn-rate and glance-hold distributions. Provisional profiles from internal play until logged human telemetry exists (GDD OQ12); replacing them never changes code. Default asset `DA_CrowdMotionProfile_Default` under `Content/StealthGame/AI/`.
- Step (a) camera toward the gaze or travel target with push durations, frequencies and turn-rate distributions drawn from the profile, through `EvaluateViewEnvelope`; (b) desired direction as an 8-way key combination or stick vector, quantised as the device would, converted to the world 2D `Move` exactly as the human client builder does (share that conversion function with the IIF client builder rather than copying); (c) emit `Look(VirtualCameraRotation)`.
- Run every controller tick: a 30 Hz look stream would itself be a tell (humans change look every frame).
- `FCrowdVirtualInputFrame` is the recordable per-step output replayed by story 034; keep it POD and behind `#if !UE_BUILD_SHIPPING`.
- Pace magnitude is from {0.8, 0.9, 1.0} (R3.1); the layer carries it as `Gait`/magnitude.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype NPCs steered by acceleration-based paths with no input emulation. The `StealthBot` console driver is a separate test aid and is not promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 009: path shaping and stops (what direction is wanted)
- Story 016: which gaze targets exist
- Story 034: pipeline parity and nightly replay
- Story 035: the classifier gate on human data
- The camera epoch: `EvaluateViewEnvelope` itself

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdVirtualInput.h`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdMotionProfileData.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdVirtualInput.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdVirtualInputTest.cpp`
- `Content/StealthGame/AI/DA_CrowdMotionProfile_Default.uasset` (created in-editor by the implementer)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.DeviceProfileQuantisation`
  - Given: fixed-seed pawns with a device mix, and keyboard-and-mouse and gamepad fixtures
  - When: device profiles are drawn and scripted headings are emitted
  - Then: one DeviceProfile draw per pawn; K+M emits 8-way quantised input relative to camera yaw; gamepad emits analog input with the profile's dead zone and response curve
  - Edge cases: input inside the dead zone; the 22.5 degree quantisation boundary; non-zero camera yaw
- **AC-2**: `StealthGame.Crowd.LookEveryTick`
  - Given: steering at 30 Hz, a fixed 60 fps step and a scripted camera pan
  - When: `Look` is recorded every controller tick
  - Then: the camera advances every tick and `Look` changes on consecutive frames during the pan, via `EvaluateViewEnvelope`
  - Edge cases: `CrowdSteerPerSec` 20; a stationary camera leaves `Look` unchanged
- **AC-3**: `StealthGame.Crowd.WalkingGlanceBendsPath`
  - Given: a scripted walking glance of known angle theta, K+M and gamepad profiles
  - When: `Move` direction before and after the glance is compared
  - Then: the change equals the value the profile's quantisation predicts (K+M snapped to 45 degree steps; gamepad follows the curve)
  - Edge cases: theta smaller than one quantum gives 0 change
- **AC-4**: `StealthGame.Crowd.VirtualInputFrameNonShipping`
  - Given: Development and Shipping builds
  - When: the recording hook and the symbol are checked
  - Then: `FCrowdVirtualInputFrame` is recorded in non-Shipping only; the symbol is absent from Shipping
  - Edge cases: needs a Shipping build and a symbol check (deferred to the packaging check)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdVirtualInputTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-003-crowd-tuning-keys-and-profile-data.md`, `production/epics/crowd-npc-behaviour/story-009-follower-shaping-pace-and-stops.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`, `production/epics/crowd-npc-behaviour/story-016-gaze-catalogue.md`, `production/epics/crowd-npc-behaviour/story-032-spike-60-character-server-cost.md`, `production/epics/crowd-npc-behaviour/story-034-pipeline-parity.md`
