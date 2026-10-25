# ADR-0015: Camera system and LOS envelope

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed (awaiting user acceptance)

## Date
2026-10-24

## Last Verified
2026-10-24 (against UE 5.8.4 headers at `C:/Program Files/Epic Games/UE_5.8/Engine/Source`, the 2026-10-24 revised `camera-system.md`, and ADR-0001, 0003, 0007, 0010, 0011, 0014 as present on disk)

## Decision Makers
Design pipeline (autonomous run, user away) with technical-director (TD-ADR) and unreal-specialist validation; user acceptance pending.

**Review record (2026-10-24):** TD-ADR **CONCERNS** + engine check **MINOR NOTES** (one combined gate). Revised in place: (1) `bAutoManageActiveCameraTarget=false` does not stop every engine view-target change — `NotifyLoadedWorld` (seamless travel), `Reset`/`ClientReset`, `OnUnPossess` and `AGameMode::SetSeamlessTravelViewTarget` still call `SetViewTarget` locally — so the manager now overrides `SetViewTarget` and derives the view from its own state (§1, §6, R-5); (2) hiding at α = 0 removes the skinned proxy from the scene (re-created on show), so the stated reason against `CastShadow` toggling was corrected and the spike measures re-add cost and first-frame pose (§4.4, R-3); (3) hidden-primitive exclusion from RT/Lumen/shadow scenes and CPD on skinned proxies verified in source (R-1, R-4); (4) the server-side remote POV uses capsule eye height with a no-pawn fallback (§1); (5) the audio-listener path is the viewport's call to the virtual `GetAudioListenerPosition`, independent of the audio backend (§7.3); (6) architecture.md wording added to GDD Sync. The Look routing was aligned with ADR-0011 §9 (`IA_Look → UPawnCommandComponent::Look`) before the gate's report.

## Summary
The third-person camera must show nothing the body's own eyes could not see, camera mode must be invisible to every other machine (the listen host included), and both modes must share one view envelope and one aim ray. This ADR fixes a custom `AStealthPlayerCameraManager` (no camera components on the pawn, engine camera-update RPCs off, Gameplay Cameras plugin not adopted), the pure `EvaluateViewEnvelope` function shared with Crowd, the eye-origin visibility mask (fixed eye cone, five capsule probes, async traces on a new `ViewBlock` channel, rate bands at every range, per-pawn dither through custom primitive data and a full hide at α = 0), the shared `GetAimRay`, the client-local neutral view without spectator state, and the listener at the eye.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Rendering (per-pawn hide and dither), Physics (trace channels, traces), Input (view rotation), Core (camera manager), Audio (listener) |
| **Layer** | Core (architecture.md layer map: Camera System #10, client-local; IIF and SCL consume its aim ray and mask flag) |
| **Knowledge Risk** | HIGH (Rendering: hide/shadow/ray-tracing/Lumen behaviour of hidden skinned meshes, dither on the shared character material, Substrate state unknown; architecture.md "Camera System — HIGH") |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/rendering.md`, `modules/physics.md`, `modules/input.md`, `modules/audio.md`, `plugins/gameplay-camera-system.md`, `PLUGINS.md` (Gameplay Cameras: Experimental, enabled by default), `breaking-changes.md`, `deprecated-apis.md`; engine headers listed under "Verified engine facts" |
| **Post-Cutoff APIs Used** | `USceneComponent::SetVisibility(bool, EVisibilityPropagation)` overload (the `bool` overload remains) [V]; `EFirstPersonPrimitiveType` exists (5.5+) but is **not** used (§7.3). Everything else is pre-cutoff API re-verified in 5.8.4 |
| **Verification Required** | Spike OQ5/OQ10 (camera mask spike): (1) hide/show cost: `SetVisibility(false)` removes the skinned proxy from the scene (no mesh, shadow, Lumen or RT presence — verified in source) and showing re-creates it; measure re-add cost, toggles/s in a crowded street, and first-frame pose after re-add; (2) custom-primitive-data dither on the shared character material with TSR, Lumen and VSM at 50 pawns (skinned path verified in source; Nanite skinned and Substrate unverified); (3) worst-case 245 async `ViewBlock` traces per frame: game-thread cost ≤ 0.5 ms; (4) `bUseClientSideCameraUpdates=false` yields zero `ServerUpdateCamera` calls (AC-CS-27) and the server-side camera update for remote controllers stays at the eye-only POV; (5) the `SetViewTarget` override holds the neutral view across seamless travel and round reset; (6) `GetAudioListenerPosition` override places the listener at the eye (viewport path, backend-independent) |

### Verified engine facts (5.8.4 source)

| Claim | Evidence |
|---|---|
| `APlayerCameraManager::bUseClientSideCameraUpdates` defaults true; when false the local client never sets `bShouldSendClientSideCameraUpdate`, so `ServerUpdateCamera` is never sent, and the server ignores a received `ServerUpdateCamera` | [V] `Camera/PlayerCameraManager.h:444-452`; `PlayerCameraManager.cpp:64, 816-863`; `PlayerController.cpp:1835-1840, 5668-5675` |
| With `bUseClientSideCameraUpdates=false`, `UpdateCamera` runs `DoUpdateCamera` on the **server for remote controllers** too | [V] `PlayerCameraManager.cpp:816` (`… \|\| !bUseClientSideCameraUpdates \|\| …`) |
| `APlayerController::bAutoManageActiveCameraTarget` (default true) drives automatic view-target changes on possession/restart | [V] `PlayerController.h:299`; `PlayerController.cpp:231, 603-605, 863-865, 921-923, 3608-3616` |
| `APlayerCameraManager::bClientSimulatingViewTarget`, `SetViewTarget`, `ProcessViewRotation`, `LimitViewPitch`, `UpdateViewTarget`, `DoUpdateCamera`, `StartCameraFade`, `AddCachedPPBlend` | [V] `PlayerCameraManager.h:442, 762, 771, 892, 751, 716, 865, 381` |
| `APlayerController::UpdateRotation` and `GetAudioListenerPosition` are virtual; `SetAudioListenerOverride` / `ClearAudioListenerOverride` exist | [V] `PlayerController.h:764, 1951, 1964-1970` |
| `UPrimitiveComponent::SetCustomPrimitiveDataFloat`, `bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden`, `bVisibleInRayTracing`, `bRenderInMainPass` | [V] `PrimitiveComponent.h:1185, 622, 560, 473, 481` |
| `USceneComponent::SetVisibility(bool, EVisibilityPropagation {NoPropagation, DirtyOnly, Propagate})` and `SetHiddenInGame` | [V] `SceneComponent.h:887-907, 917` |
| `UWorld::AsyncLineTraceByChannel(EAsyncTraceType, …, FTraceDelegate*, UserData)` and `LineTraceTestByChannel` | [V] `Engine/World.h:2522, 2128` |
| Gameplay Cameras plugin: `"IsExperimentalVersion": true`, `"EnabledByDefault": true` | [V] `Plugins/Cameras/GameplayCameras/GameplayCameras.uplugin:13-15` |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (identity invariants: spectator state banned, pawnless viewers use a client-local camera actor §4.5; tiering reads only screen height and this ADR's mask flag); ADR-0003 (no camera/boom/input component on the pawn; `RemoteViewPitch16` from `HeadLook`); ADR-0007 (module and `View/` placement, presentation include boundary); ADR-0010 (Shared Pawn: eye socket, capsule, always-tick pose on the server, the single per-pawn local presentation apply point); ADR-0011 (Look input path through the PlayerController and CMC saved moves; `GetAimRay` consumer; toggle lockout); ADR-0004 (client tuning snapshot for the camera keys). None is Accepted yet. |
| **Enables** | ADR-0013 (animation tiering reads `IsEyeMaskVisible`), ADR-0014 (uses the `ViewBlock` channel declared here for LSC sight predicates), ADR-0016 (Crowd's virtual look runs `EvaluateViewEnvelope`), ADR-0017 (concealed boom, hide arbitration with Concealment's client latch, bush foliage channel responses), ADR-0018 (grade hook timing at the local contact frame), ADR-0021 (`GetViewState`, fade layer order), ADR-0022 (dither material and mask cost lines) |
| **Blocks** | Camera epic (rig, envelope, mask, aim ray, neutral view); the camera-toggle milestone; IIF Attack payload stories (they need `GetAimRay`) |
| **Ordering Note** | Should-have before the camera-toggle milestone (architecture.md Required ADRs). OQ9 (pitch −60°/+40° in both modes) was confirmed by the user 2026-10-25; this ADR does not depend on the value (data keys). The mask spike (OQ5/OQ10) must report before Acceptance. |

## Context

### Problem Statement
Third person is user-locked, and Pillar 2 forbids it from becoming a peeking tool: a lens 180 cm back and 45 cm to the side sees past corners the eye cannot. Pillar 1 forbids camera mode from being observable: the engine's default camera path sends the lens transform to the server (`ServerUpdateCamera`), which would tell a listen host who plays in third person and where their lens is. The Game Animation Sample's camera stack (Gameplay Cameras plugin, Experimental in 5.8) puts camera components on the pawn and is driven by assets that broke on resave in 5.8 (active.md). Camera GDD OQ4 (rig), OQ5 (mask rendering), OQ10 (trace budget) and CS-R5c (server view check) are "-> ADR". IIF's Attack payload and SCL's animation tiering consume this system's outputs, so the interface must be fixed before those stories.

### Constraints
- **Local only** (CS-R1, IVP L17, ADR-0003): no camera, boom or input component on the Shared Pawn; nothing about camera mode replicates or is sent; the server never knows the mode.
- **Kind-blind** (P1, ADR-0001): the mask reads geometry only; NPCs, players, teammates and bodies go through one code path with identical timing.
- **Never more than the eye** (P2, CS-R5): no pawn is drawn in 3P without a passing eye-origin line-of-sight test, at any range.
- **Host parity** (CS-R1): the mask never reduces authoritative simulation on the listen host (always-tick pose, ADR-0010).
- **No spectator state** (ADR-0001 §4.5, IVP R13).
- **Budget:** mask ≤ 0.5 ms game thread target inside the 1.0 ms "camera eye-mask traces + misc" line (architecture.md CPU table).
- **Experimental plugins** are not adopted for MVP-critical paths (architecture.md, Gameplay Cameras).

### Requirements
- Two modes, one envelope (−60°/+40° pitch provisional, ±70° yaw relative to body), elastic edge push for turn-in-place, soft edge, view leads head (CS-R2, CS-R3, F1).
- Stable eye pivot; boom with no orbit, collision pull-in and recovery, shoulder squeeze and swap, close 3P, concealed boom 60 cm (CS-R4, CS-R6, CS-R11.3, F2).
- Eye-origin mask with cone, probes, hysteresis, fade, rate bands at every range, full hide incl. shadow and RT at α = 0, never toggling `CastShadow` (CS-R5b, F3).
- One `GetAimRay` for IIF Attack and the Strike hint, including `WorldPoint` into a `HideVolume` (CS-R8).
- Neutral view without spectator state; cuts and fades; transient grade at the local contact frame; informant grade from the cut frame; listener at the eye (CS-R5d, CS-R12, CS-R15).
- FOV rule with a horizontal cap (F5), stick response (F6), Reduce camera motion (CS-R13).

## Decision

### 1. The rig: a custom `APlayerCameraManager`, no pawn camera components

- **`AStealthPlayerCameraManager : APlayerCameraManager`** in `StealthGameplay` (`Core/` folder; header in `Public/View/Core/` because it is client code with no server facts, ADR-0007 §2). `AStealthPlayerController` (composition root, ADR-0007) sets `PlayerCameraManagerClass` to it.
- **Class defaults:** `bUseClientSideCameraUpdates = false` (no `ServerUpdateCamera` is ever sent and the server ignores a forged one [V]); `bClientSimulatingViewTarget = true` (the client owns its view target; the server never pushes one); `bAlwaysApplyModifiers = false`; no camera modifiers, no camera shakes, no lens effects registered.
- **`AStealthPlayerController` defaults:** `bAutoManageActiveCameraTarget = false` [V] (stops the automatic retarget on possession and `ClientRestart`/`PawnClientRestart` [V `Pawn.cpp:504, 633`]). That flag does **not** cover every engine path: `NotifyLoadedWorld` on seamless travel, `Reset`/`ClientReset`, `OnUnPossess` and `AGameMode::SetSeamlessTravelViewTarget` still call `SetViewTarget` locally [V `PlayerController.cpp:2022-2024, 1288, 1300, 1363-1366`; `GameMode.cpp:485`], and `bClientSimulatingViewTarget` blocks only the `ClientSetViewTarget` RPC [V `PlayerController.cpp:2834`]. Therefore **`AStealthPlayerCameraManager::SetViewTarget` (virtual [V]) is overridden to accept only targets the manager itself requests** (its own possessed pawn or its `AStealthNeutralViewActor`, through a private `RequestViewTarget`); any other call is ignored and counted (non-Shipping log), and `UpdateViewTarget` computes the view from the manager's own view state, never from whatever `VT.Target` an engine path set. The camera manager owns every cut (§6). **Factory-initiated unpossess** (amendment 2026-10-24, ADR-0018 item 6, ADR-0019 item 7): when the factory releases the body (`ReleaseBody`, ADR-0010 §3.2b) or performs the exile hand-over, the manager keeps the released body as its view target (the `Dying` state still references it) until its own fade completes and it cuts to the neutral view; `AStealthPlayerController::OnUnPossess` additionally skips the engine's `SetViewTarget(this)` for these unpossesses, and the `SetViewTarget` override ignores any engine retarget meanwhile.
- **Remote controllers on the server.** Because client-side updates are off, the engine also runs `DoUpdateCamera` on the server for remote controllers [V]. `AStealthPlayerCameraManager::UpdateViewTarget` therefore branches on `PCOwner->IsLocalController()`: for a **non-local** controller it produces only the **eye-only POV** — capsule axis at the pawn's base eye height (`BaseEyeHeight`-style capsule offset, never the eye socket, which only the local client reads, §3) + control rotation, or the controller's own location and rotation when it has no pawn — the same rule for every remote controller, so the host's own view of a connection's viewpoint never reveals a mode, and runs no boom, no collision, no mask and no trace. This POV is also the connection's relevancy viewer (`GetPlayerViewPoint` reads the camera cache, used by `FNetViewer` [V `PlayerController.cpp:1009-1013`; `NetDriver.cpp:5125, 5146`]); pawns have no cull distance (ADR-0003), so the viewer location changes nothing for them. The only other engine viewpoint uploads are spectator paths (`ServerSetSpectatorLocation`, banned by ADR-0001) and `ANoPawnPlayerController` (unused) [V `PlayerController.cpp:3020, 6845`]. The listen host's own local controller runs the full rig like any client.
- **Pawn class rule:** `AStealthPawn` and its Blueprints carry no `UCameraComponent`, `USpringArmComponent` or Gameplay Cameras component (CS-R1, ADR-0003 §6). An asset audit (ADR-0008 reflection audit) fails on any camera-type component in the pawn's class hierarchy. The sample's camera assets stay in the project unused and on the never-resave list (ADR-0009); the Gameplay Cameras plugin stays enabled by default (the sample content references it) but no production class references it.
- **Camera core is plain C++.** The rig's math (envelope F1, boom F2, mask F3 scheduling and hysteresis, toggle F4, FOV F5, stick F6) lives in `FStealthCameraCore` and `FEyeMaskCore` with injected trace and clock backends, so AC-CS-03/04/06/08/29/30/31/38 run headless (ADR-0007 §5, ADR-0008 T1). The manager is a thin adapter.

### 2. The view envelope: one pure function, shared with Crowd

- `EvaluateViewEnvelope` (F1) is a **pure, stateless-by-argument** function in `StealthGameplay/Public/View/Core/StealthViewEnvelope.h`: inputs = previous `FViewEnvelopeState` (view offset `v`, target `r`, push latch timestamp), raw Look delta this frame (post-sensitivity, pre-gain), `dt`, body yaw, own `bConcealed`, `FViewEnvelopeParams` (the tuning keys); outputs = new state, view rotation, control-rotation target. No `UObject`, no world; Crowd's virtual input layer (ADR-0016, server) calls the same function per NPC, so NPC edge push and relax match a human's (CS-R3.4).
- **Where it runs for the human** (aligned with ADR-0011 §9: `IA_Look → UPawnCommandComponent::Look`). The controller's `IA_Look` handler passes the frame's raw look delta (after stick response F6 and sensitivity, before any gain) to `AStealthPlayerCameraManager::StepEnvelope`, which calls `EvaluateViewEnvelope`, keeps the **view** rotation for the lens, and returns the **control target** `r`; the handler then calls `UPawnCommandComponent::Look(r)`, which writes the control rotation that CMC saved moves carry to the server (ADR-0011) and SCL filters into `HeadLook`. `AStealthPlayerController::UpdateRotation` (virtual [V]) is overridden so it **does not integrate `RotationInput`** (no `AddYawInput`/`AddPitchInput` path exists), making the envelope the only writer of a human's control rotation; `ProcessViewRotation`'s default limiting is bypassed, with `ViewPitchMin/Max` set to the hard ranges as a backstop only.
- **Values:** `LookYawLimitDeg` (SCL, 70), `ViewPitchDownLimitDeg`/`ViewPitchUpLimitDeg` (60/40 in both modes, **confirmed by the user 2026-10-25, camera OQ9**), soft widths, push rate and latch, overshoot 5° (0 while own `IsConcealed`), relax rate — all client-audience tuning keys from the frozen client snapshot (ADR-0004), identical for every client.
- **Head lead fallback** `ViewHeadLeadMaxDeg` is a disabled-by-default clamp applied inside the same function (CS-R3.5), so enabling it later changes data, not code.

### 3. Eye pivot, boom and collision

- **Eye pivot** (CS-R4): capsule axis x/y; height = eye-socket height (read from the pawn's mesh socket on the local client only), low-passed by `CameraPivotLagSec` (0.12 s while `IsInTraversal`), vertical speed ≤ `PivotMaxVertCmPerSec`, clamped inside the capsule; never the head bone's rotation or horizontal position. A network correction above SCL's `PawnSmoothSnapThresholdCm` resets the filter (snap with the body).
- **Boom** (F2): ideal point = pivot + R(view yaw)·(−L, s·O_eff, H); pitch rotates the lens in place (no orbit). **Collision**: per frame, one thin line trace (`CameraGuardRadiusCm` 2 cm → `LineTraceSingleByChannel` treated as a ray) and one sphere sweep (`CameraProbeRadiusCm` 14 cm) on the engine **`ECC_Camera`** channel, from the pivot to the ideal point; pull-in is immediate, recovery per F2 (hold, exponential, speed cap); shoulder squeeze; close 3P below 60 cm (head hidden locally, §7.4); mask off below 20 cm.
- **Concealed** (own `IsConcealed`, owner audience): L = `ConcealedBoomCm` (60), O = 0, H = 0, edge push 0 (CS-R11.3); bush foliage ignores `ECC_Camera` and `ViewBlock` (Concealment owns the foliage collision profile, ADR-0017), so the lens stays inside the dome and the mask from the eye inside the bush shows what a head there sees.
- **First person** (CS-R7): pivot + `FpForwardOffsetCm` along view yaw; near clip `CameraNearClipCm` (5) in both modes.

### 4. The eye-origin visibility mask

**4.1 Channels (project collision config, applied by the implementing story).**
- **`ViewBlock`** — a new custom **trace channel** (`ECC_GameTraceChannelN`, default response **Block**). Walls, doors, props, kit modules, court fixtures and water blockers' sight-relevant parts block; **pawns, bush foliage, glass, decoration and the water pawn-blocking volume ignore it**. It is the project's single definition of "what blocks sight": the mask, the aim ray's world test (§5) and the layout contract's sight predicates (ADR-0014 §5.3) all use it, so a wall that hides a pawn from the eye is the same wall that bounds LSC witness sets and sightlines.
- **`ECC_Camera`** (engine) — boom collision only; pawns and foliage ignore it.
- Collision profiles (pawn capsule and mesh, kit, foliage, glass, decoration) set their `ViewBlock` and `Camera` responses explicitly; an editor validator fails a kit or pawn asset whose responses differ from its profile (ADR-0008 asset audit).

**4.2 Targets.** Every `AStealthPawn` except the local player's own possessed pawn, and every **eye-masked element** (pawn attachments, bush rustle emitters at their emission point, objective-site transient effects) registered through `IEyeMaskTargetRegistry::Register(USceneComponent* Root, TConstArrayView<FVector> LocalProbes)`. Pawns register through a client-local **`UStealthPawnRegistry`** world subsystem (pawns add themselves at `BeginPlay`, remove at `EndPlay`; no kind, team or controller data; one registry shared with ADR-0013's tiering rather than two). Iteration order is the registry's insertion index (client-local, kind-blind).

**4.3 Test (F3).** Mask origin `P_m` = eye pivot with z = min(lagged pivot z, real eye-socket z). For each target due this frame:
1. **Cone first, no trace:** probes (pawns: head, chest, pelvis and two lateral points derived from the capsule, never bones) transformed into view space at `P_m`; a probe counts only inside the fixed eye cone ±`EyeConeHalfHDeg` (55°) × ±`EyeConeHalfVDeg` (38°), independent of the FOV slider.
2. **Traces:** for every in-cone probe, one **`AsyncLineTraceByChannel(EAsyncTraceType::Test, P_m → probe, ViewBlock)`** [V], all probes of a target submitted together; the result arrives the next frame (F3's "+1 frame" timing); `Pass` = any probe unblocked.
3. **Hysteresis and fade:** `Visible_t = Pass ∨ (Visible_{t−1} ∧ fails < EyeMaskHideFailCount (2))`; α moves toward 1/0 over `EyeMaskFadeSec` (0.10 s).
4. **Rate bands at every range:** `EyeMaskNearHz` 60 within 20 m, `EyeMaskFarHz` 15 to 60 m, `EyeMaskBeyondHz` 5 beyond — the test **always** runs; distance changes only the rate. A target with no probe in the cone is not visible and costs no trace.
5. **Per-frame cap:** at most `EyeMaskTracesPerFrameMax` (code constant, initial 160) traces are submitted per frame; due targets beyond the cap are deferred one frame in round-robin order of registry index (never by kind, team or distance-to-crosshair). Spike OQ10 sets the constant so the worst case (49 targets near, all probes in cone and blocked = 245) stays within one extra frame of delay.
6. **Fallback if the spike shows async submission or result handling over 0.5 ms GT:** synchronous `LineTraceTestByChannel` with early exit on the first clear probe for the near band only (GDD F3's original costing), async for the far and beyond bands.

**4.4 Applying α (presentation, client only).** The camera never touches a pawn's components directly. It writes `MaskAlpha` into the pawn's **client-local presentation state** and the Shared Pawn's single apply function (ADR-0010 owns it) combines it with Concealment's client latch (ADR-0017) and the local head-hide (§7.4):
- **0 < α < 1:** dither through **custom primitive data** slot `CPD_EyeMaskAlpha` (one project-wide index, reserved in the shared character material and every attachable pawn prop/VFX material) via `SetCustomPrimitiveDataFloat` [V] on every primitive of the pawn — on skinned meshes this updates GPU Scene data without re-creating the proxy [V `SkeletalMeshSceneProxy.cpp:363`; `RendererScene.cpp:1951-1963`]; Nanite skinned meshes and Substrate remain [U] — no per-pawn material instance, no material branch, the same parameter path for every pawn (CS-R5b).
- **α = 0:** `SetVisibility(false, EVisibilityPropagation::Propagate)` [V] on the pawn's mesh root, which hides mesh and attachments. With `bCastHiddenShadow = false` and `bAffectIndirectLightingWhileHidden = false` on every pawn primitive, a hidden component is **removed from the scene**: `OnVisibilityChanged` marks render state dirty and `ShouldComponentAddToScene` returns false [V `SceneComponent.cpp:3574-3576`; `PrimitiveComponent.cpp:2118-2121`], so the pawn has no mesh, shadow, Lumen or ray-traced presence [V `RayTracing.cpp:1368`; `RendererScene.cpp:6195`]. Showing it again re-creates the skinned proxy and mesh object. **`CastShadow` is still never toggled** (CS-R5b): a shadow-only toggle would leave a visible-but-shadowless or shadowed-but-hidden pawn — a tell — and costs a comparable render-state rebuild, so it buys nothing over a full hide. The cost of hide/show is bounded by hysteresis (2 failed tests) and the fade; the mask spike (OQ5) measures proxy re-add cost, toggles per second in a crowded street, and that the first frame after re-add shows the current pose (no one-frame pop).
- **α > 0 again:** `SetVisibility(true, Propagate)` first, then dither in.
- **Pose ticking is unaffected**: visibility never feeds `VisibilityBasedAnimTickOption` decisions on the server (always-tick pose, ADR-0010) and client tiering reads `IsEyeMaskVisible`, not the engine's was-recently-rendered test (ADR-0001 §3, CS-R1).

**4.5 Modes.** In **third person** (free boom ≥ 20 cm) the mask hides and dithers. In **first person** and close-3P below 20 cm the evaluator still runs in **flag-only** mode — it computes `IsEyeMaskVisible` with the same cone and probes but writes no visibility or dither — so IIF (Strike hint, `PawnRef`) and SCL tiering use one geometric definition of "rendered" in both modes (CS-R1 "the mask sets the flag"; the 108° horizontal FOV cap keeps every on-screen 1P pawn inside the 110° cone, so flag-only adds no hide the eye would not have). The cost is the same in both modes, which also keeps frame time from hinting at the mode in a shared capture.

**4.6 Server.** No mask, no view check, no camera-mode field in the MVP (CS-R5c). The mask is a consistency rule for honest clients, not a security boundary; a modified client gains only what replicated positions already give it (CS-R5 residual). Server-side view filtering is ADR-0025 (Alpha).

**4.7 Partial masking** (the whole-pawn-reveal residual, CS-R5b) is not built; the spike costs it. The extension point is the per-pawn α and the CPD slot: a per-section or screen-space clip would add a second CPD value, not a new path.

### 5. Aim ray: `GetAimRay()`

One function on the camera's client view facade, used by IIF's Attack payload builder and the Strike hint (ADR-0011):
- **1P:** the camera-centre ray from the lens.
- **3P:** (1) camera-centre ray, starting from the point on it nearest the eye pivot (nothing between lens and body is targetable), `LineTraceSingleByChannel(ViewBlock)` to find the world aim point P, then a pawn query along the same segment (`LineTraceSingleByObjectType(Pawn)` limited to P); (2) re-trace from the **pivot** to P + 10 cm with the same two queries; the first blocking result on this eye ray is the aim result.
- **Targetable pawn** only if `IsEyeMaskVisible(Pawn)` (α > 0, in either mode, §4.5) and the eye ray reaches it; otherwise the result is a **`TargetSpot`** (CS-R8, IIF R10). The camera adds no lock-on, snapping, magnetism or slowdown (CS-R9).
- **`WorldPoint`:** if the eye-side ray enters a `HideVolume` on Concealment's bush-spot query channel (ADR-0017) within range, the entry point projected to the ground; otherwise the ground point under the ray, clamped to range. The query is identical for occupied and empty bushes.
- Returns `FAimRay { FVector Origin; FVector Direction; FVector WorldPoint; TWeakObjectPtr<const AStealthPawn> TargetPawn; bool bTargetSpot; }`. Reach is the IIF's, measured from the pawn.

### 6. View states, cuts, fades and the neutral view

- **States** exactly as the GDD table: `NoPawn` (neutral view), `FirstPerson`, `ThirdPerson` (normal/squeezed/close/concealed), `Blend`, `Dying`. `GetViewState()` returns `Live` or `Neutral` (CS-R15).
- **Neutral view** (IVP R13, ADR-0001 §4.5): a client-local, non-replicated `AStealthNeutralViewActor` (spawned locally; `bReplicates = false`) set as the local view target by the client's camera manager; the PlayerController stays in `NAME_Playing`; no spectator pawn, no spectator RPC, never another participant's pawn as view target; the world is not rendered (black, HUD plates only).
- **Cut to a pawn** on the client's own possession acknowledgement (`AcknowledgePossession` / `OnRep_Pawn` path on `AStealthPlayerController`), in the remembered mode, at full boom and pitch 0, control rotation initialised from the pawn's `HeadLook` (SCL R4.2); fade-in over `NoPawnFadeSec` from the manager's published screen fade (below). Never a blend across the map.
- **Dying and `ViewRelease`:** entered from the client-local `OnRoleChangePresented` dispatch at the own local contact frame (ASN-R23); concealed-input hold (CS-R12.1); the fade-to-black falls due on the client-local `ViewRelease(Reason, DelaySec)` (`Assassinated`/`ReversedAgainst`: `DeathFallSec`; `StrikeLimit`: HUD card entrance or `ExileCardTimeoutSec`; `InformantKilled`: `DeathServerTime + DeathFallSec`). All timing from owner-audience data and ADR-0002's `EstimatedServerNow()`; nothing keyed to the server's binding retirement.
- **Grades** (CS-R11.1, CS-R15): `ApplyTransientGrade()` and `SetOwnerGrade(None|Informant)` are post-process blends the camera manager adds with `AddCachedPPBlend` [V] — local to the viewer, never a world post-process volume, never driven by another pawn's state. The transient grade runs **at the local contact frame** only on participating players' cameras (Assassination owns when, VA-ASN-4); the informant grade is on from the cut frame.
- **Fade layer order:** the camera's fade draws above Teammate Markers and HUD world-anchored layers, below HUD plates and menus (CS-R15; mechanism with ADR-0021).
- **Published screen fade** (amendment 2026-10-24, ADR-0021 §3): the camera manager keeps all fade timing (when, how fast, colour) and computes the fade alpha itself each frame; it publishes it through `IStealthCameraView::GetScreenFade() -> FStealthScreenFade {Alpha, Color}` and the HUD fade layer (ZOrder 20) renders it. `StartCameraFade` is **not** used for the neutral-view, cut or elimination fades (it darkens only the scene and its `FadeAmount` animates only while the scene fade is on); the transient and informant grades stay post-process blends. Any audio fade this ADR drives uses the same published alpha.
- **No spectator, death, kill or free camera** in the MVP; the post-round reveal camera is VS (Post-Round Reveal).

### 7. Toggle, FOV, listener, own-body presentation

- **7.1 Toggle and shoulder swap** (CS-R10, F4): local; blend clamped below IIF's `CameraToggleLockoutSec` minus one frame; mask applies from t = 0 in both directions; shoulder swap mirrors over 0.25 s with one buffered press. Mode is remembered in local settings (`UGameUserSettings`-style local config, never tuning, TDC CR-14).
- **7.2 FOV** (F5): Hor+ with `FovHMaxDeg` (108°) cap for every aspect; per-aspect slider top; eases over 0.15 s (instant under Reduce camera motion); the eye cone never follows FOV.
- **7.3 Listener at the eye** (CS-R5d): `AStealthPlayerController::GetAudioListenerPosition` (virtual [V]) is overridden to return the eye pivot and the view rotation in both modes (fallback: `SetAudioListenerOverride` per frame [V]). `EFirstPersonPrimitiveType` (5.5+) is not used: it is a first-person *rendering* feature (separate FOV and world-space representation) and would add a mode-dependent primitive path.
- **7.4 Own head hide** (CS-R2, CS-R6): in 1P and close-3P the own head, neck and hair bones are scaled to 0 **on the local pose only** (a local-only AnimInstance flag read by the shared AnimBP's final pose, ADR-0013); the mesh stays rendered and ticking; observers and the server never see it.
- **7.5 Stick response and Reduce camera motion** (F6, CS-R13) are local settings applied before `EvaluateViewEnvelope`.

### Architecture Diagram

```
 LOCAL CLIENT (and the listen host's own local controller)                     SERVER (remote controllers)
 Enhanced Input IA_Look ─► camera StepEnvelope (UpdateRotation override integrates nothing)
      raw ΔLook ─► EvaluateViewEnvelope(F1) ─► UPawnCommandComponent::Look(target r) ─► CMC ServerMove ─► SCL HeadLook
                                 └─► view rotation v ─► AStealthPlayerCameraManager            AStealthPlayerCameraManager
 AStealthPlayerCameraManager (bUseClientSideCameraUpdates=false, bClientSimulatingViewTarget)   (non-local PC): eye-only POV,
   ├─ FStealthCameraCore: eye pivot (F4 lag) → 1P | 3P boom (F2: ray + sweep on ECC_Camera)     no boom, no mask, no traces
   ├─ FEyeMaskCore: registry targets ─ cone ─ AsyncLineTrace(ViewBlock) ─ hysteresis ─ α        (ServerUpdateCamera never sent,
   │      └─► pawn local presentation state ─► Shared Pawn apply (CPD dither | SetVisibility)    ignored if forged)
   │      └─► IsEyeMaskVisible(Pawn) ─► IIF Strike hint / PawnRef, SCL tiering (ADR-0013)
   ├─ GetAimRay(): centre ray → P (ViewBlock + Pawn query) → pivot→P re-trace → FAimRay
   ├─ view states: NoPawn(AStealthNeutralViewActor) / 1P / 3P / Blend / Dying ─► fades, PP grades
   └─ GetViewState() ─► HUD, Markers (View facade)
 AStealthPlayerController::GetAudioListenerPosition ─► eye pivot + view rotation
```

### Key Interfaces

```cpp
// ---- StealthGameplay/Public/View/Core/StealthViewEnvelope.h (client-safe; also used by Crowd on the server) ----
struct FViewEnvelopeParams { float YawLimitDeg, PitchDownDeg, PitchUpDeg, SoftYawDeg, SoftPitchDeg, SoftMinGain,
    EdgePushDeg, PushMinRateDegPerSec, PushLatchSec, RelaxDegPerSec; TOptional<float> HeadLeadMaxDeg; };
struct FViewEnvelopeState { float ViewYawOffset = 0, ViewPitch = 0, TargetYawOffset = 0; double LastPushTime = -1; };
struct FViewEnvelopeResult { FViewEnvelopeState State; FRotator ViewRotation; FRotator ControlTarget; };
FViewEnvelopeResult EvaluateViewEnvelope(const FViewEnvelopeState& Prev, FVector2f RawLookDeltaDeg, float Dt,
    double Now, float BodyYawDeg, bool bConcealed, const FViewEnvelopeParams& Params);   // pure (F1)

// ---- StealthGameplay/Public/View/Core/StealthCameraView.h (client view facade, ADR-0007) ----
enum class EStealthViewState : uint8 { Live, Neutral };
enum class EViewReleaseReason : uint8 { Assassinated, ReversedAgainst, StrikeLimit, InformantKilled };
enum class EOwnerGrade : uint8 { None, Informant };
struct FAimRay { FVector Origin; FVector Direction; FVector WorldPoint;
                 TWeakObjectPtr<const AStealthPawn> TargetPawn; bool bTargetSpot = true; };
struct FStealthScreenFade { float Alpha = 0.f; FLinearColor Color = FLinearColor::Black; };
class IStealthCameraView {
public:
    virtual FAimRay GetAimRay() const = 0;                          // IIF Attack + Strike hint (one function)
    virtual EStealthViewState GetViewState() const = 0;             // HUD, Markers
    virtual bool IsEyeMaskVisible(const AStealthPawn* Pawn) const = 0;  // α > 0; geometry only; both modes
    virtual void ApplyTransientGrade() = 0;                         // Assassination decides when
    virtual void SetOwnerGrade(EOwnerGrade Grade) = 0;              // own Informant state only
    virtual void RaiseViewRelease(EViewReleaseReason Reason, float DelaySec) = 0;
    virtual FSimpleMulticastDelegate& OnViewStateChanged() = 0;
    virtual FStealthScreenFade GetScreenFade() const = 0;          // §6; HUD fade layer renders it (ADR-0021)
};

// ---- StealthGameplay/Public/View/Core/StealthPlayerCameraManager.h ----
UCLASS() class AStealthPlayerCameraManager : public APlayerCameraManager, public IStealthCameraView {
    // ctor: bUseClientSideCameraUpdates = false; bClientSimulatingViewTarget = true; bAlwaysApplyModifiers = false
    virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override; // local: full rig; remote: eye-only POV
    FRotator StepEnvelope(FVector2f RawLookDeltaDeg, float Dt);                  // IA_Look handler; returns control target r
};

// ---- StealthGameplay/Public/View/Core/StealthEyeMask.h ----
class IEyeMaskTargetRegistry {
public:
    virtual FEyeMaskHandle Register(USceneComponent* Root, TConstArrayView<FVector> LocalProbes) = 0; // attachments, rustle, site FX
    virtual void Unregister(FEyeMaskHandle Handle) = 0;
};
UCLASS() class UStealthPawnRegistry : public UWorldSubsystem {   // client-local pawn set (all net modes); kind-blind
public:
    void Add(AStealthPawn* Pawn);  void Remove(AStealthPawn* Pawn);
    TConstArrayView<TWeakObjectPtr<AStealthPawn>> GetPawns() const;  // insertion order
};
```

### Implementation Guidelines
1. The Shared Pawn class and its Blueprints **must never** contain a `UCameraComponent`, `USpringArmComponent` or any Gameplay Cameras component; the camera **must** be computed only in `AStealthPlayerCameraManager`.
2. `bUseClientSideCameraUpdates` **must** be false and **must never** be re-enabled; no code **may** call `ServerUpdateCamera`; `bAutoManageActiveCameraTarget` **must** be false on `AStealthPlayerController`.
3. On a non-local controller the camera manager **must** compute only the eye-only POV and **must never** run the boom, collision, mask or any trace.
4. The mask **must** read only geometry (positions, capsules, `ViewBlock` traces); it **must never** read team, binding, participant state, controller kind or any server-only header (AC-CS-07, AC-CS-39).
5. No pawn **may** be drawn in third person without a passing eye-origin trace result; distance **may** change only the test rate, never skip the test.
6. α = 0 **must** hide via `SetVisibility(false, Propagate)`; code **must never** toggle `CastShadow` per frame; pawn primitives **must** keep `bCastHiddenShadow` and `bAffectIndirectLightingWhileHidden` false.
7. The dither **must** use the single `CPD_EyeMaskAlpha` custom-primitive-data slot; per-pawn material instances and material branches by pawn kind are forbidden.
8. The camera **must never** call `SetVisibility` on a pawn directly; it writes `MaskAlpha` to the pawn's local presentation state and the Shared Pawn's apply function arbitrates with Concealment's latch.
9. The aim ray **must** be the only source of the Attack payload's `WorldPoint`/`TargetPawn` (ADR-0011); a pawn with α = 0 or blocked from the eye **must** produce a `TargetSpot`.
10. Pawnless viewers **must** use the client-local `AStealthNeutralViewActor`; spectator pawns, spectator state and spectator RPCs are forbidden (ADR-0001 §4.5).
11. Every sight test in the project (camera mask, aim world test, LSC sight predicates) **must** use the `ViewBlock` channel; a second "sight" channel **must not** be introduced.
12. Camera code **must never** add aim assist, snapping, magnetism, slowdown, zoom, shake or FOV punches (CS-R9).

## Alternatives Considered

### Alternative 1: Gameplay Cameras plugin (the Game Animation Sample's camera stack)
- **Description**: Use `UGameplayCameraComponent` / camera assets and rigs as the sample does.
- **Pros**: Data-driven rigs, blending, already present in the sample content; less hand-written camera math.
- **Cons**: Experimental in 5.8 [V uplugin]; its component lives on the pawn (violates CS-R1 / ADR-0003 §6 on every pawn, including NPCs); the sample's camera assets broke on resave in 5.8 (active.md, ADR-0009 never-resave list); the mask, envelope and eye-only server POV would still be custom code fighting the plugin's evaluation order; post-cutoff API with little reference material.
- **Rejection Reason**: Experimental, pawn-component based, and does not remove the custom parts that matter; architecture.md already recommends against it.

### Alternative 2: Spring arm with its origin at the eye (`USpringArmComponent` + `TargetOffset`) — GDD option B
- **Description**: A spring arm on the pawn whose origin is moved to the eye, with probe collision.
- **Pros**: Familiar engine component; built-in lag and collision.
- **Cons**: A component on the Shared Pawn (CS-R1); spring-arm collision probes from the arm origin with its own lag model, not the GDD's thin-ray guarantee + sweep; orbit-with-pitch behaviour must be disabled piecemeal; exists on remote and NPC pawns unless stripped per instance, which is a kind/ownership-dependent component set.
- **Rejection Reason**: Puts camera state on the pawn and duplicates logic the custom manager needs anyway.

### Alternative 3: Stock spring arm with `SocketOffset` — GDD option A
- **Description**: The default third-person template rig.
- **Pros**: Zero code.
- **Cons**: The lens leaves through walls sideways (the offset is applied after the probe), and the boom orbits over the head with pitch — both are peeking tools.
- **Rejection Reason**: Violates CS-R5a; rejected by the GDD.

### Alternative 4: Hide masked pawns with per-pawn material instances (opacity) instead of CPD + visibility
- **Description**: A dynamic material instance per pawn drives a dither/opacity parameter all the way to 0.
- **Pros**: No visibility toggling; one mechanism for fade and hide.
- **Cons**: A dithered-to-zero pawn still casts its shadow and contributes to Lumen and ray tracing unless shadows are also toggled (CS-R5b forbids `CastShadow` toggling for VSM); per-pawn MIDs break batching and multiply PSO/material state across 50 pawns; a kind-blind guarantee then depends on every pawn having been given its MID identically.
- **Rejection Reason**: Does not remove shadow/GI/RT presence and costs more; CPD for the fade plus `SetVisibility` at zero does both.

### Alternative 5: GPU occlusion queries or scene-depth tests instead of physics traces
- **Description**: Use hardware occlusion results or a depth-buffer sample from the eye.
- **Pros**: No trace cost; scales with pawn count.
- **Cons**: The depth/occlusion data is from the lens, not the eye; an eye-origin depth pass would be a second scene render; occlusion queries lag 1-2 frames and treat foliage and glass by material, not by the GDD's "what blocks sight" rule; results differ by GPU and settings.
- **Rejection Reason**: Wrong origin and non-deterministic semantics; kept as GDD OQ10's last-resort option only if traces cannot meet the budget.

### Alternative 6: Disable the mask in first person entirely (no flag)
- **Description**: 1P uses the engine's render state for "rendered".
- **Pros**: Saves trace cost in 1P.
- **Cons**: Two definitions of "rendered" for IIF and SCL tiering by mode (CS-R1 forbids the engine's was-recently-rendered test); frame-time difference by mode.
- **Rejection Reason**: One geometric definition in both modes is simpler to test and keeps mode parity.

### Alternative 7: Server-side view check / per-viewer pawn filtering (CS-R5c)
- **Description**: Server culls pawns not visible from each viewer's eye.
- **Pros**: A real security boundary against wall hacks.
- **Cons**: Must filter all 50 pawns identically for every viewer to avoid a P1 leak; ~10 × 50 eye traces per server tick on the listen host; interacts with ADR-0003's cull path.
- **Rejection Reason**: Out of MVP scope; deferred to ADR-0025 (Alpha) as the GDD states.

## Consequences

### Positive
- Camera mode leaves no trace on the wire or in the host's process state for remote players (no `ServerUpdateCamera`, eye-only server POV for every remote controller).
- One envelope function for humans and NPCs; one aim ray; one "rendered" flag; one sight channel shared with the layout contract.
- The third-person view provably shows no pawn without an eye-origin line-of-sight result, at every distance.
- The camera is testable headless (pure cores with injected trace/clock backends).
- No dependency on an Experimental plugin or on sample camera assets that break on resave.

### Negative
- More hand-written camera code than a plugin or spring-arm rig (boom, collision, recovery, blends, fades).
- Mask trace cost exists in both modes (flag-only in 1P).
- One frame of extra reveal latency from async traces (accepted by F3).
- The whole-pawn reveal at corners remains (stated residual; partial masking deferred to the spike's costing).
- The hide arbitration couples the camera to a Shared Pawn presentation apply point (ADR-0010) and Concealment's latch (ADR-0017); all three must agree on the apply order.

## Risks
- **R-1 `SetVisibility(false)` leaves a ray-traced or Lumen footprint** — verified in source that it does not (the hidden component leaves the scene, §4.4) [V]; the spike capture with RT and Lumen on is kept only as confirmation.
- **R-2 Mask trace cost over 0.5 ms GT at 49 near pawns** [U]. *Mitigation:* async traces, per-frame cap with round-robin deferral, near-band sync early-exit fallback (§4.3.6); GDD OQ10's remaining options (fewer probes for distant pawns) are a tuning change with the CD's limit that 3P reveal must not be noticeably worse than 1P culling.
- **R-3 Visibility toggling churns render state**: every hide removes the skinned proxy from the scene and every show re-creates it [V], with VSM invalidation around the pawn. *Mitigation:* hysteresis (2 fails) and fade bound toggles; the spike measures re-add cost, toggles/s in a crowded street and the first-frame pose after re-add; fallback is a CPD-only fade that stays at α = 0 for short hides (proxy kept, shadow and GI still suppressed only if the spike finds a cheaper flag path) [U].
- **R-4 CPD dither unsupported or costly on the chosen character path**: skinned meshes verified [V]; Nanite skinned and Substrate [U]. *Mitigation:* ADR-0022 owns the shared character material; spike tests skinned and Nanite skinned; fallback per-pawn MID is rejected for Pillar reasons, so the fallback is a hard cut at α thresholds (no fade).
- **R-5 Engine paths change the view target locally** (`NotifyLoadedWorld` on seamless travel, `Reset`/`ClientReset`, `OnUnPossess`, `SetSeamlessTravelViewTarget` [V]) or send camera RPCs. *Mitigation:* the `SetViewTarget` override accepts only manager-requested targets and the view is computed from the manager's own state (§1); `bClientSimulatingViewTarget`; server code never calls `SetViewTarget` for a player; AC-CS-37 gains a seamless-travel test and a round-reset test (neutral view stays black, no world frame from the controller's location); AC-CS-27 traces RPCs in a 10-minute session.
- **R-6 Listener override not honoured** [U, low]: the viewport sets the listener from the virtual `GetAudioListenerPosition` before any audio backend is involved [V `GameViewportClient.cpp:1883`]. *Mitigation:* AC test with a scripted source behind a corner; fallback `SetAudioListenerOverride`.
- **R-7 Pitch range (OQ9)** — confirmed 2026-10-25; any later change is data only.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| camera-system.md | CS-R1 local only; no pawn camera components; engine camera RPCs disabled; listener; host parity; reads only own/owner data | §1 (manager, `bUseClientSideCameraUpdates=false`, eye-only server POV, pawn rule), §4.4 (no pose effect), §7.3, Guidelines 1-4 |
| camera-system.md | CS-R2 two modes, identical permissions; own head hidden locally | §2, §7.4 |
| camera-system.md | CS-R3 / F1 view envelope both modes, edge push, soft edge, head lead fallback | §2 (`EvaluateViewEnvelope`, `UpdateRotation` override) |
| camera-system.md | CS-R4, CS-R6 / F2 eye pivot, boom, collision, squeeze, close 3P | §3 |
| camera-system.md | CS-R5 (a)(b) / F3 eye-origin mask: cone, probes, `ViewBlock`, rate bands at every range, full hide incl. shadow/RT, no `CastShadow` toggling, dither per pawn, kind-blind | §4 |
| camera-system.md | CS-R5 (c) no server view check in MVP | §4.6; ADR-0025 |
| camera-system.md | CS-R5 (d) listener at the eye | §7.3 |
| camera-system.md | CS-R7 first person | §3 |
| camera-system.md | CS-R8 aim ray, `TargetSpot`, `WorldPoint` into `HideVolume` | §5 |
| camera-system.md | CS-R9 no identification aid | Guideline 12 |
| camera-system.md | CS-R10 / F4 toggle and shoulder swap | §7.1 |
| camera-system.md | CS-R11 actions, traversal, concealed boom 60 cm, informants same rules, transient grade at contact | §3, §6 |
| camera-system.md | CS-R12 no pawn: Dying, ViewRelease, neutral view without spectator state, Ended | §6 |
| camera-system.md | CS-R13 / F5 FOV cap; CS-R14 / F6 stick | §7.2, §7.5 |
| camera-system.md | CS-R15 hooks, `GetViewState`, fade layer | §6, Key Interfaces |
| camera-system.md | OQ4 rig, OQ5 mask rendering, OQ10 trace budget | §1; §4.4 + spike; §4.3 |
| shared-character-locomotion.md | R11 tiering reads "rendered" = mask flag (via ADR-0001) | §4.5 `IsEyeMaskVisible` |
| input-interaction-framework.md | R10 Attack payload from one aim ray; R14 toggle local | §5, §7.1 |

## Performance Implications
- **CPU (client)**: mask ≤ 0.5 ms game thread target (async submission + result handling; ≤ 160 traces per frame cap; typical ≈ 75); boom 2 traces/frame; aim ray ≤ 4 traces per query (on Attack and Strike-hint refresh). Inside architecture.md's "camera eye-mask traces + misc 1.0 ms" line. Physics trace work on task threads ≤ ~1.2 ms worst case [U spike].
- **CPU (server)**: none beyond the eye-only POV per remote controller (a few vector ops); no mask, no traces.
- **GPU**: dithered pawns use the shared masked material path (ADR-0022); hidden pawns cost nothing.
- **Memory**: negligible (per-target mask state ~64 bytes × ≤ 60 targets).
- **Network**: zero camera traffic (no `ServerUpdateCamera`, no camera RPC, nothing replicated).

## Migration Plan
1. The prototype's debug camera (`StealthDebugCam` exec), the sample `CBP_SandboxCharacter` camera setup and its Gameplay Cameras assets are not promoted (ADR-0007 §8); the production pawn (ADR-0010) is created without camera components.
2. Build order: (a) `EvaluateViewEnvelope` + `FStealthCameraCore` with headless tests (AC-CS-03/04/38/29/30/31); (b) camera manager with 1P/3P, boom, collision, fades, neutral view (AC-CS-01/02/27, 06, 37); (c) `ViewBlock` channel + collision profile audit; (d) mask spike (OQ5/OQ10) on the graybox with 50 pawns, then `FEyeMaskCore` + apply path (AC-CS-05/07/08/09/36/28); (e) aim ray with IIF (AC-CS-34 and IIF AC); (f) grades and listener.
3. Config (implementing story): `ViewBlock` trace channel and per-profile responses in `DefaultEngine.ini` `[/Script/Engine.CollisionProfile]`.

## Validation Criteria
- Camera ACs pass under ADR-0008's harness (`StealthGame.Camera.*`, tests under `Source/StealthGameplay/Private/Tests/Camera/`): AC-CS-01, 02, 27 (mode not observable; zero `ServerUpdateCamera`/camera RPCs in a traced session), 28 (host masked simulation parity), 05 (LOS multi-position vs the 25-point oracle, ≤ 5 % false hides), 07 (mask parity and inputs), 08 (timing), 09 (masked casts nothing, RT/Lumen included), 36 (beyond-range LOS), 34 (`WorldPoint` into bush), 37 (view state and layer), 21 (informant same rules), 39 (presentation include check).
- Spike OQ5/OQ10 report: GT ≤ 0.5 ms at 49 near pawns on the reference machine; hidden pawn has no shadow, Lumen or RT presence; dither works on the chosen character mesh path.
- Playtests: AC-CS-25 (own pawn found within 2 s), AC-CS-26 (1P/3P read parity; > 15 % gap goes to the user, OQ11).

## Related
- Depends on: ADR-0001, ADR-0003, ADR-0004, ADR-0007, ADR-0010, ADR-0011.
- Enables: ADR-0013, ADR-0014 (`ViewBlock`), ADR-0016 (`EvaluateViewEnvelope`), ADR-0017, ADR-0018, ADR-0021, ADR-0022; ADR-0025 (server view check, Alpha).
- Design: `design/gdd/camera-system.md` (CS-R1..CS-R15, F1-F6, OQ4, OQ5, OQ9, OQ10), `design/gdd/input-interaction-framework.md` (R10, R14), `design/gdd/shared-character-locomotion.md` (R9, R11), `docs/architecture/architecture.md` (Required ADRs row ADR-015; TR-camera-001..008).

### GDD Sync (not applied by this ADR; for the GDD owners)
1. `camera-system.md` CS-R5b "First person: the mask is off" → "off for hiding; it still computes the rendered flag in flag-only mode" (§4.5) — a clarification, no behaviour change for the viewer.
2. `camera-system.md` F3 "early exit on the first clear trace" → async traces submit all in-cone probes of a due target together (no early exit; same Pass result); sync early-exit is the near-band fallback (§4.3).
3. `camera-system.md` CS-R5b fade mechanism "[verify]" → custom primitive data slot `CPD_EyeMaskAlpha` (§4.4); "skeletal meshes do not take per-instance custom data" is true for per-instance data but per-primitive CPD applies (spike confirms).
4. `camera-system.md` Acceptance Criteria test root/prefix follow ADR-0007/0008 (`Source/StealthGameplay/Private/Tests/Camera/`, `StealthGame.Camera.*`); AC-CS-37 adds a seamless-travel and a round-reset neutral-view case (§1, R-5).
5. `docs/architecture/architecture.md` (architecture sync, not a GDD): the Core-layer Camera row still lists "per-pawn MID" as a dither option, and the Required ADRs row ADR-015 says "time-sliced beyond 60 m" / "per-pawn parameter"; this ADR chose the CPD slot and rate bands at every range (§4.3-§4.4). Update when the ADR is accepted.

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §6, Key Interfaces: `GetScreenFade()` published (Camera computes the alpha; audio fade uses it); `StartCameraFade` no longer used for neutral/cut/elimination fades — requested by ADR-0021 (§3).
- §1: view target kept on factory-initiated unpossess (`ReleaseBody`, exile hand-over) until the manager's own fade; `OnUnPossess` skip — requested by ADR-0018 (item 6) and ADR-0019 (item 7). `bAutoManageActiveCameraTarget = false` was already decided here (no edit).
- Config applied 2026-10-25 (phase 2, user-authorised): `ViewBlock` declared as `ECC_GameTraceChannel2` (`Traversable` already holds channel 1), trace type, default **Block**, in `Config/DefaultEngine.ini [/Script/Engine.CollisionProfile]`. Explicit per-profile responses: **Ignore** on `Pawn`, `CharacterMesh`, `Spectator`, `Ragdoll`, `NoCollision`, `InvisibleWall`, `InvisibleWallDynamic`, `Trigger`, `OverlapOnlyPawn`, `WaterBodyCollision`; **Overlap** on `OverlapAll`, `OverlapAllDynamic`, `UI` (overlap never blocks a `Test` trace); every blocking profile (`BlockAll`, `BlockAllDynamic`, `PhysicsActor`, `Destructible`, `Vehicle`, `IgnoreOnlyPawn`) takes the channel default Block. Bush foliage, glass and decoration profiles do not exist yet; their owners (ADR-0017, kit stories) add them with `ViewBlock=Ignore` and explicit `Camera` responses. `Camera` responses on existing profiles unchanged.

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §2 values — user decision 2026-10-25: view/aim pitch −60°/+40° in both modes confirmed (camera OQ9).
- `EViewReleaseReason::InformantKilled` — user decision 2026-10-25: confirmed (Penalty CR-18 / OQ-NKP-12).
- Settings — user decision 2026-10-25: tap/hold camera toggle and a left-handed default shoulder are not in MVP; later as Vertical Slice settings (camera OQ12).
- Key renames — user decision 2026-10-25 (TDC CR-10): `EyeMaskHideFailCount`, `StickDeadZoneFrac`, `StickOuterZoneFrac`.
