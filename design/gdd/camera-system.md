# Camera System

> **Status**: Reviewed - NEEDS REVISION 2026-10-24 (revisions applied; re-review pending); reconciled 2026-10-24; ADR-synced 2026-10-24
> **Author**: design-system pipeline (autonomous) + ux-designer, gameplay-programmer, creative-director
> **Design Review (2026-10-24, full)**: NEEDS REVISION. Revisions applied per creative-director rulings 1-10 (log: `design/gdd/reviews/camera-system-review-log.md`): one pitch envelope −60°/+40° for both modes; F1 edge push reads raw input with a 150 ms latch; F3 never draws a pawn without a line-of-sight result; `ConcealedBoomCm` 60; `ViewRelease` and `SetOwnerGrade` hooks; engine camera RPCs and host-side simulation guarded; acceptance criteria rebuilt.
> **Last Updated**: 2026-10-24
> **Last Verified**: 2026-10-24
> **Implements Pillar**: P2 Every Advantage Is a Tell (primary); P1 Hide in Plain Sight
> **Creative Director Review (CD-GDD-ALIGN)**: CONCERNS -> REVISED 2026-10-24. Four items applied:
> - the whole-pawn reveal at corners is stated as a residual, and OQ5 costs partial masking;
> - the `ViewHeadLeadMaxDeg` fallback and its trigger are written down;
> - a 1P/3P reading-parity playtest is added (AC-CS-26);
> - the narrow-aspect FOV gap is stated and accepted.
>
> Rulings: OQ1 strict mask confirmed; head lead, no server view check, concealed boom, FOV cap, default ThirdPerson and Player Fantasy approved.

## Summary

The Camera System gives each human player a first-person or third-person view of their own body. The toggle is local, and both modes have identical permissions. Its central rule is that the third-person camera can show nothing the body's own eyes could not see: no peeking around corners, over walls or behind the head. Camera mode is invisible to every other client, so the choice of camera is never a tell.

> **Quick reference** — Layer: `Core` · Priority: `MVP` · Key deps: `Input & Interaction Framework, Shared Character & Locomotion, Tuning Data & Config; dependents IIF (aim ray), Concealment, Assassination, NPC-Kill Penalty & Informants, HUD, Settings & Accessibility, Anti-Cheat`

## Overview

The Camera System is a client-local presentation layer that turns the owner's body (one Shared Pawn) into a view. It owns:

- the two view modes, the toggle and its blend;
- the third-person boom, its collision and the shoulder swap;
- the first-person eye camera and how the own body is presented;
- the **view envelope**: the view never leaves the head-look clamp;
- the **eye-origin visibility mask**, which makes the third-person view obey the body's line of sight;
- the aim ray that the IIF's Attack command and Strike hint consume;
- field-of-view and motion-comfort settings;
- the camera's behaviour during actions, traversal, concealment, elimination and round end.

It replicates nothing and adds nothing to what the server receives. The only view data that reaches the server is the control rotation every pawn already sends in its movement updates, and SCL clamps what other clients see of it. The engine's own camera-update RPCs are disabled (CS-R1). The server never knows which mode a player uses (SCL R9, IIF R14).

Without this system the game would have to choose between two failures. It could drop third person, which is the user-locked toggle. Or it could let third person become a free observation tool. "Watching safely from behind a corner" is exactly the advantage Pillar 2 says must cost something.

## Player Fantasy

**"You see the street through your own eyes, even when you stand behind yourself."**

- **Third person** lets you watch your own posture and check that you look like one more citizen.
- **First person** puts you at eye level among the faces, in the noon glare.

Either way, the only way to see around the corner is to walk to it, and the crowd can see you walking. The camera is the one control that says nothing out loud: nobody can tell which view you chose, and neither view is better for hunting. It must never feel like a scope, a drone or a security monitor. It should feel like being someone in the crowd.

Pillars served:
- **P2** ("Every Advantage Is a Tell"): watching from cover costs a body standing in the open.
- **P1**: camera mode is part of "network presentation", so it must not differ between players.
- **Anti-pillar "not a reflex game"**: no aim assist and no snapping; first person is not twitch play.

## Detailed Design

### Core Rules

**CS-R1. Local only, never observable.**
- **What is created where.** The camera rig is a custom view computed in the local player camera manager. No camera or boom component exists on the Shared Pawn class (IVP L17; -> ADR, OQ4). It is created only on the locally controlled client, for the pawn it possesses. It is never replicated and never exists for NPC or remote pawns.
- **What stays local.** Camera mode, boom state, FOV and shoulder side are client settings. No RPC, replicated property, component, pose, animation tier, footstep or sound depends on them (SCL R9, AC-SCL-30).
- **What reaches the server.** Only the standard control rotation in movement updates. It is the same in both modes, because the view envelope (CS-R3) is identical. The custom camera manager disables every engine path that sends the lens transform to the server: `ServerUpdateCamera` and client-side camera updates **[verify 5.8]**. AC-CS-27 traces the network and requires zero such calls.
- **View target and listener.** `bAutoManageActiveCameraTarget` is false, so possession never switches the view by itself. That flag alone does not stop every engine view-target change (seamless-travel world load, reset, unpossess), so the PlayerController's view-target setter accepts only targets the camera manager requests, and an unpossess started by the pawn factory (exile hand-over, body release) keeps the current view target (ADR-0015 §1, R-5; ADR-0018, ADR-0019 §4.4). The camera owns the cut and the fade (CS-R12). The audio listener is set through the listener-override path at the eye pivot (CS-R5d) **[verify]**.
- **Host.** The listen-server host follows the same rule for its own pawn. **The mask never reduces authoritative simulation.** On the host, every pawn ticks its pose and refreshes its bones whatever its render state, using always-tick. "Not rendered" for IIF R10, the Strike hint and SCL R11 is a flag the mask sets. It is never the engine's was-recently-rendered test (SCL change request; AC-CS-28).
- **What the camera reads.** Its own possessed pawn through the local controller only, plus Owner-audience client data: own binding presence, own Informant state, own `IsConcealed` and own action contact times. It never includes server query headers (Policy, Roster, Round, LSC, Penalty, Objectives). It never calls `GetPlayerState`, `GetController`, `IsPlayerControlled` or `GetOwner` on any other pawn (IVP AC-IVP-17; AC-CS-39).
- **Server.** The server has no camera-mode field and runs no view check in the MVP (CS-R5c).

**CS-R2. Two modes, one body.**
- **Modes.** `FirstPerson` and `ThirdPerson`. The default is `ThirdPerson`, because it lets a new player see their own posture against the crowd. The last chosen mode is remembered locally across rounds and matches.
- **What is identical.** Movement permissions, abilities, input contexts, reach, aim (CS-R8) and the view envelope, including the pitch range (CS-R3).
- **What differs, and only for the owner.** The own head, neck and hair are hidden from the owner's own camera in first person and in close third person (CS-R6). Their bones are scaled to 0 on the local pose only. The mesh stays rendered and ticking, so its animation tick, root motion and LOD do not change.

**CS-R3. View envelope (both modes; SCL R9, OQ8 ruling as adjusted by D6, extended to both modes by the creative director at design review).** The view rotation is measured relative to the body's facing: actor yaw, with the body facing its direction of travel (SCL R5.4).
1. **Clamp.**
   - **Yaw:** within ±`LookYawLimitDeg` (70°), SCL's head-look clamp, consumed here rather than duplicated (F1).
   - **Pitch:** from −`ViewPitchDownLimitDeg` (60°) to +`ViewPitchUpLimitDeg` (40°), local only, **in both modes**. A first-person-only range would let 1P strike a bush at the player's feet while 3P could not, which turns the camera choice into a gameplay choice (CS-R2, creative-director ruling; user to confirm, OQ9).
   - **What others see:** SCL clamps the replicated head pitch at ±`LookPitchLimitDeg` (20°) and holds it at the limit, so observers see an ordinary glance held at its limit (SCL R8.4, R9).
2. **Elastic edge push.** The control rotation is the head-look target that SCL filters. While the player pushes outward at the yaw limit, the target may sit up to `CameraEdgePushDeg` (5°) beyond the limit.
   - **Pushing** means a raw (unscaled) outward input of at least `EdgePushMinRateDegPerSec` (15 °/s) at some time in the last `EdgePushLatchSec` (0.15 s). The latch makes a mouse, whose hand lifts and re-centres, equivalent to a held stick.
   - **Why:** this gives SCL R5.5 a target beyond the head clamp, which turns the idle body in place after `TurnInPlaceDelaySec` (0.5 s).
   - **Release:** when pushing lapses, the overshoot relaxes to the limit at `EdgePushRelaxDegPerSec` (60 °/s).
   - **Reverse:** any inward input collapses the overshoot at once, so reversing has **no dead zone**.
   - **Stick drift** below the dead zone is zero input, so it cannot turn the body. Pitch has no edge push.
   - **While concealed** (own `IsConcealed`, Owner audience), the overshoot is 0, so the camera never asks for a turn-in-place. The hider's body yaw is locked (Concealment CON-R7). Look stays free within the clamp.
3. **While moving,** the envelope turns with the body (60 °/s at walk). Steering with Move turns the body and so turns the view. A wider look is always paid for through the body, which observers can see.
   - This is the one involuntary view rotation in the game. It is a playtest comfort watch item (OQ3).
   - Reduce camera motion does not remove it, because the body really turns.
4. **Soft edge.** The view decelerates into the edge instead of hitting a wall.
   - Outward Look input **to the view** is scaled by `g = max(g_min, 1 − smoothstep(Ylim − S_y, Ylim, |v|))` over the last `CameraEdgeSoftYawDeg` (10°). The floor `CameraEdgeSoftMinGain` (0.1) means the view reaches the limit in finite time.
   - Pitch uses the same ramp over `CameraEdgeSoftPitchDeg` (5°) at both pitch limits.
   - The gain shapes only the view response. The edge-push test reads the raw input, so the soft edge can never stop turn-in-place firing (F1).
   - Inward input is always at full gain.
   - This is a comfort feature tied to the envelope. It never depends on a pawn being under the view (CS-R9).
   - **Shared function.** F1 is exported as one pure function (`EvaluateViewEnvelope`). Crowd's virtual look layer (Crowd R3.4) runs NPC look targets through it, so an NPC's edge push and relax look like a human's (change request to Crowd).
5. **The camera leads the head.** Look applies to the view in the same frame (IIF feel target). The visible head follows at ≤ 140 °/s (SCL F6).
   - **Accepted residual for the MVP:** a flick-and-back glance shorter than 0.2 s shows observers a smaller head twitch than the view received.
   - It is measured in playtest telemetry (OQ2).
   - **Fallback, written now:** `ViewHeadLeadMaxDeg` caps how far the view may lead the visible head (SCL `HeadLook`). It ships **disabled** for the MVP.
   - **Trigger:** enable it (default when enabled: 35°) if the graybox telemetry shows more than 6 glances under 0.2 s per player-minute, or if a playtest shows flick-scanning beats sustained watching in correct reads.
   - The fallback is never "NPCs twitch too": that would erase the tell rather than restore it.
   - The envelope limits *where* the view can look, not *how fast*.

**CS-R4. Stable eye pivot and third-person boom.**
- **Eye pivot.** The **eye pivot** is the single origin for the camera, the mask, the aim ray and the audio listener.
  - Horizontal position: the capsule axis.
  - Height: the eye socket's height, low-passed over `CameraPivotLagSec` (0.10 s; 0.12 s while `IsInTraversal`), with vertical speed capped at `PivotMaxVertCmPerSec` (300) and the result clamped inside the capsule.
  - It never uses the head bone's rotation or horizontal position. Motion matching can push the head past a wall, and an animated head would put animation bob into the view.
- **Boom (over the shoulder).** The ideal camera point is the pivot moved:
  - back `BoomLengthCm` (180) along the view yaw;
  - sideways `ShoulderOffsetCm` (45) toward the current shoulder;
  - up `BoomHeightOffsetCm` (15).
- **No orbit.** The boom does not orbit with pitch: pitch rotates the camera in place. The camera therefore stays at eye + 15 cm and never rises over a wall (F2).
- **Own-pawn framing (Character Pool OQ-6: "the camera is the identifier").** In 3P at rest pitch (view pitch −20° to +5°) the own body sits in the same screen slot: lower third, offset to the side opposite the shoulder.
  - Outside that band the body may leave the frame: the torso is about 22° below the lens axis, and it leaves a 60° frame above about +8° pitch or below about −53°. This is a framing cost the creative director accepted for one envelope in both modes. It is not a fairness cost.
  - Spawn, takeover and toggle all start at full boom with pitch 0, so the slot is the first thing seen. No highlight, outline or tint is ever added. Playtest target: a new player finds their pawn within 2 s (AC-CS-25).

**CS-R5. The third-person line-of-sight rule (CD amendment, Pillar 2).** "The third-person camera can see nothing the body's line of sight cannot" is met by three layers. Only (b) is load-bearing.

**(a) Geometry.** These keep the lens on the eye's side of every surface:
- The lens is never on the far side of a surface from the eye (F2: a thin-ray check from the pivot plus a sphere sweep).
- Height is capped at eye + `BoomHeightOffsetCm`.
- The boom never orbits over the head.
- The view envelope applies (CS-R3).

Geometry alone does not stop parallax peeking. A lens 180 cm back and 45 cm to the side can see past a pillar edge that the eye cannot.

**(b) Eye-origin visibility mask (load-bearing).** In third person, every **eye-masked** element is drawn only while it is visible from the eye pivot. The visibility test (F3) has two parts:
1. A probe point lies inside a **fixed physical eye cone** around the view direction: ±`EyeConeHalfHDeg` (55°) horizontal and ±`EyeConeHalfVDeg` (38°) vertical. The cone does not follow the FOV slider, and the largest slider setting stays inside it.
2. At least one of five capsule probe points has an unobstructed line from the **mask origin** on the dedicated `ViewBlock` trace channel. The probes are head, chest, pelvis and two lateral points. On that channel walls, doors and props block; pawns, bush foliage and glass do not.
   - **Mask origin:** the eye pivot, with its height set to the lower of the lagged pivot height and the real eye-socket height. On a descent the lagged pivot can sit up to 30 cm above the real eye for about 0.1 s, so the lower of the two never sees over a wall the eye cannot.
   - **Evaluation order:** the cone test runs first and costs no trace. Traces then run for the in-cone probes only: asynchronous traces submit all in-cone probes of a due target together (no early exit; the same `Pass` result), and the synchronous near-band fallback exits early on the first clear probe (ADR-0015 §4.3).
   - **At every range:** no pawn is ever drawn in 3P without a line-of-sight result. Beyond `EyeMaskRangeCm` (60 m) the same traces run, time-sliced at `EyeMaskBeyondHz` (5 Hz) with the same hysteresis (F3; creative-director ruling).

Rules for the mask:
- **What is masked:**
  - every Shared Pawn except the owner's own: players, NPCs, informants, teammates and bodies being removed;
  - everything attached to a pawn: props, VFX, decals;
  - bush entry and exit rustle effects, at their emission point;
  - objective-site transient effects.
- **What is not masked:** static geometry and bushes themselves. They are public.
- **Inputs:** the mask reads **only geometry**. It never reads team, binding, participant state or controller kind. It hides an NPC and a player through one code path, with identical timing (P1).
- **Masked means fully hidden.** A masked element is drawn with no mesh, no shadow and no ray-traced reflection or GI. A shadow kept on visible ground would give away the pawn. The 3P view therefore sees slightly *less* than 1P, which is acceptable: 3P must not see *more*.
  - **Mechanism [verify]:** at α = 0 the mask hides the pawn's primitives (`SetVisibility(false)`), which removes the mesh, its shadow and its ray-tracing presence together.
  - It never toggles `CastShadow` frame by frame, because that invalidates virtual shadow map pages.
- **Fade.** The fade uses the shared character material's dither. It is driven by a per-pawn value: the custom primitive data slot `CPD_EyeMaskAlpha` (one project-wide index reserved in the shared character material and every attachable pawn material; ADR-0015 §4.4). Skeletal meshes do not take per-*instance* custom data, but per-*primitive* custom data applies (spike confirms). Every pawn uses the same parameter path.
- **"Visible" and "not rendered".** A pawn is mask-visible while α > 0. A pawn with α = 0 counts as not rendered for IIF R10, the Strike hint and SCL R11, through the flag the mask sets (CS-R1). It is the same as an occlusion-culled pawn in 1P.
- **False-hide budget.** 3P may hide a pawn that a 1P eye sees only as a sliver. Hides where a dense 25-point oracle sees at least 5 points are capped at 5 % of samples (AC-CS-05). Above that, the mask is too strict and fails review.
- **First person:** the mask is off for hiding, because the eye is the camera; the evaluator still runs in **flag-only** mode and computes the rendered flag (`IsEyeMaskVisible`) with the same cone and probes, which targeting and animation tiering read (ADR-0015 §4.5; no behaviour change for the viewer). It is also off in 3P when the free boom length is below 20 cm (CS-R6).
- **Hearing is not masked.** A masked pawn's sound still plays. Hearing is a mechanic and is equal in both modes.
- **Stated residual: whole-pawn reveal (CD-GDD-ALIGN).** The mask draws the whole pawn once any one probe is clear. At a corner the eye may see only a head, while the lens 45 cm to the side sees the full gait, and gait is the reading skill. Accepted for the MVP. The OQ5 spike also costs **partial masking** (per-section or screen-space clipping to the eye's visible region) as the fix if playtests show 3P reading gait at corners earlier than 1P (AC-CS-26).

**(c) Server-side view check: none in the MVP** (-> ADR, Anti-Cheat & Info-Leak Security, Alpha; Iris dynamic filters to be evaluated **[verify]**).

**Residual risk, stated honestly.** Layer (b) is a client-side rule that keeps the view consistent. It is **not a security boundary**:
- Every client already receives every pawn's position, because crowd parity requires it.
- A modified client that disables the mask gains only what reading replicated positions already gives it.
- The mask stops honest 3P players from out-observing 1P players.

A server-side fog-of-war would have to filter all 50 pawns identically to avoid a P1 leak. Its cost goes in the Alpha ADR.

**(d) Audio.** The listener sits at the eye pivot with the view rotation, in both modes. A listener at the camera would hear around corners.

**CS-R6. Collision (3P).**
- **Two checks.** Both run from the pivot toward the ideal point on the `Camera` channel, which pawns and foliage ignore:
  - a thin ray of radius `CameraGuardRadiusCm` (2): the guarantee;
  - a sphere sweep of radius `CameraProbeRadiusCm` (14): the comfort margin, and larger than the near-plane corner diagonal.
- **Pull-in** is instant: the lens is never inside a wall for a frame.
- **Recovery** waits `BoomRecoverHoldSec` (0.15 s), then eases out exponentially (τ `BoomRecoverTauSec` 0.30 s), capped at `BoomRecoverCmPerSec` (300).
- **Shoulder squeeze.** Against a wall on the shoulder side, the shoulder offset slides from 45 to 0 cm over `ShoulderSqueezeInSec` (0.15 s) and returns over `ShoulderSqueezeOutSec` (0.40 s). Shoulders never flip automatically.
- **Close third person.** If the free length falls below `BoomCloseCm` (60), the own head, neck and hair are hidden as in 1P, with 15 cm of hysteresis. Below 20 cm the camera is effectively at the eye and the mask is off.
- **Pivot overlap.** If the sweep starts overlapping geometry, the free length is 0.
- **Near clip** is `CameraNearClipCm` (5) in both modes.

**CS-R7. First person.**
- **Placement.** The camera sits at the pivot plus `FpForwardOffsetCm` (8) along the view yaw.
- **Rotation.** The view rotation only: no head-bone rotation, no roll, no bob in the MVP.
- **Own body.** The head is hidden (CS-R2) and the rest of the body is visible when looking down. At pitch −20° and 60° vertical FOV, the ground is visible from about 1.4 m ahead. At the −60° limit the player sees their own feet and a bush at their feet, while observers see the head held at −20° (SCL R9).
- **Not in the MVP:** a separate first-person arms mesh, because it would need a second animation path.

**CS-R8. Aim ray (consumed by IIF R10 and the Strike hint through one shared function).** `GetAimRay()`:
- **First person:** the camera-centre ray.
- **Third person:**
  1. Trace the camera-centre ray from the point on it nearest the eye pivot, so nothing between the lens and the body can be targeted. This finds the aim point P.
  2. Re-trace from the **pivot to P**, extended by 10 cm.
  3. The first blocking hit on that second ray is the aim result. It may be a nearer pawn than the one under the crosshair point, and that nearer pawn is then the target.
- **No targeting what the eye cannot see.** A pawn the eye cannot see, or a masked pawn, is never the `TargetPawn`. The result is a `TargetSpot`, which blind attacks on a spot already allow (IIF R10).
- **`WorldPoint`** (carried by every Attack, IIF R10) is computed in this order:
  1. If the eye-side ray (step 2 in 3P, the centre ray in 1P) enters a `HideVolume` on the bush-spot query channel within range, `WorldPoint` is that entry point projected to the ground. A strike aimed into the leaves therefore lands in Concealment's spot zone (CON-R12, F4).
  2. Otherwise, it is the ground point under the ray, clamped to range.
  - The query is the same for an occupied and an empty bush, and it adds no hint or presentation (Assassination AC-ASN-40).
- **Reach** is measured by the IIF from the pawn, not from the camera.
- **Server validation.** The server validates against its own state, with slack for the head lag (IIF).

**CS-R9. No identification aid, ever.** The camera has none of the following:
- aim assist, target snapping, reticle magnetism, or aim slowdown or friction over pawns;
- rotation toward pawns, or camera reactions to nearby pawns;
- zoom, FOV punches, or camera shake.

The camera's inputs are own input, own pawn state (body facing, eye pivot, own `IsConcealed`, own action), own settings and static geometry. The only thing it reads about other pawns is their geometry, for the mask (CS-R5b). Screen centre stays empty (art bible 7.1).

**CS-R10. Toggle and shoulder swap.**
- **Toggle.** V on keyboard, Y on gamepad (IIF R14). Behaviour:
  - It blends over `CameraToggleBlendSec` (0.20 s), shorter than the IIF lockout of 0.30 s (F4). At runtime the blend is clamped to at most the lockout minus one frame, whatever the two keys say (I-CS-1).
  - The mask applies from t = 0 in both directions.
  - The head hides at u ≥ 0.85 going into 1P, and shows at u ≤ 0.15 coming out.
  - It works whenever a pawn is possessed: mid-hold, mid-move, during montages, in a bush and as an informant.
- **Shoulder swap** (3P only). `X` on keyboard (middle mouse as alternate), **D-pad Right** on gamepad. Both are free in the IIF table (IIF change request). R3 click was rejected because it misfires during stick look. Behaviour:
  - It mirrors the offset over `ShoulderSwapBlendSec` (0.25 s).
  - It buffers one press.
  - The collision checks apply throughout the blend.

**CS-R11. Actions, traversal, concealment.**
1. **Own Strike, reversal, Brace, Tend.** Mode and rules are unchanged, and Look stays live within the envelope. If the body turns by root motion or warping, the view keeps its world yaw until the envelope edge carries it along. Assassination caps strike warp rotation at `StrikeWarpYawLimitDeg` (30°, ASN-R16; OQ8 closed), so the envelope edge does not drag the view.
   - **Transient grade.** The art bible 2.7 transient grade (1.1 s) runs through a local post-process hook the camera owns, `ApplyTransientGrade` (CS-R15). Assassination owns when it is called (VA-ASN-4). The grade's look belongs to the art bible.
     - It runs at the **local contact frame**, and only on participating players' cameras: the attacker on a kill or reversal; a player victim before its CS-R12 fade; a bound defender on a reversal.
     - It never runs before contact on a victim's or defender's client (ASN-R6). It never runs on a whiff, a brace, a bystander or an ally.
   - No shake, slow motion, zoom or cut.
2. **Traversal (vault, climb).** The pivot uses the traversal lag and the vertical speed cap (CS-R4), and Look stays live. In 1P the camera never inherits the animation's head rotation.
3. **Concealment in a bush.** The camera stays at the body.
   - While the owner's own `IsConcealed` is true (Owner audience), the 3P ideal point is the pivot moved back `ConcealedBoomCm` (60) along the view yaw, with shoulder offset 0 and height offset 0. The free length is therefore at most 60 cm from the pivot, which sits on the bush anchor's axis.
     - This keeps the lens inside the 1.8 m leaf dome (Concealment CON-R7, change request 12; invariant I-CS-4). The hider looks out through the leaves in both modes (art bible 2.8, "the city still visible through leaves") instead of surveying from behind the bush.
     - The old default of 100 cm put the lens about 10 cm outside the leaves, which made the bush a lookout post.
   - **Body yaw is locked** while concealed (Concealment CON-R7, answering OQ7). Look is free within the CS-R3 clamp, and the edge push is off (CS-R3.2).
   - Bush foliage ignores both the `Camera` and `ViewBlock` channels, so the mask measured from the eye inside the bush shows exactly what a head in that bush sees.
   - The leaf-edge vignette is the art bible's.
4. **Informants** get the same two modes, envelope, mask and aim ray. There is no free camera, no detached view and no x-ray: concealed pawns are not rendered for them either (SCL R6).

**CS-R12. No pawn: elimination, respawn, round end.**
1. **Own elimination** (Assassinated, ReversedAgainst, StrikeLimit, InformantKilled). **Concealed input hold** (cross-review S-W2 2026-10-24): from the frame the strike on the own pawn is known locally until `Dying` ends, the camera keeps the own `IsConcealed` value it held before that strike (concealed boom, overshoot 0, leaf-edge vignette), and a change received meanwhile is ignored; Concealment CON-R9 keeps the flag true until body removal for a victim killed in a bush, so the camera never pulls out of the leaves before contact. The camera enters `Dying` at the own local contact frame (for an exile, the contact frame of the strike that drew the third strike). Entry into `Dying` and `ViewRelease` are driven by the client-local `OnRoleChangePresented` dispatch, which fires at the own pawn's `DeathServerTime` (or on arrival of `FDeathState` if later), never by the arrival of replicated Roster state (Assassination ASN-R23, its change request 19). It applies the transient grade (CS-R11.1) and stays on the body in its current mode, with no control.
   - It fades to black over `NoPawnFadeSec` (0.5 s) when the client-local **`ViewRelease(Reason, DelaySec)`** event falls due (counted from that contact frame unless stated otherwise):
     - **Assassinated or ReversedAgainst:** after `DeathFallSec` (1.2 s, Assassination). Possession stays through the fall (ASN-R22).
     - **StrikeLimit (exile):** the view holds on the released body without control until **HUD's client-local card-entrance event** (the plate's show time + `ExileBeatSec` (0.8 s), HUD-R9) raises `ViewRelease(StrikeLimit)`, then fades. If no card entrance occurs within `ExileCardTimeoutSec` (2.0 s, Penalty) of the owner's `StrikeLimit` state, it fades anyway (PEN-R16, Penalty CR-10 as amended at Penalty's design review; replaces the earlier timer counted from the contact frame).
     - **InformantKilled** (an informant's own body is killed; Penalty CR-18): the view stays on the own falling body and fades over `NoPawnFadeSec` from `DeathServerTime + DeathFallSec`, keyed on the own pawn's death state, not on any binding change. HUD shows no removal card for it; the awaiting countdown appears after the fade. *(Contract change also tracked as Penalty OQ-NKP-12; a client-local event reason, nothing replicated.)*
   - `ViewRelease` is computed on the owner's client from Owner-audience data (own Roster state, own contact time) and, for an exile, HUD's client-local card-entrance event. Camera owns the event; Assassination and Penalty own the delays; HUD owns the card entrance. No visual effect is keyed to the server's binding retirement.
   - There is no death cam, kill cam, killer follow or replay.
2. **Awaiting a pawn** (informant respawn, rejoin). The **neutral view** is a black screen with only the HUD's plates, including Penalty's Owner-only "returning" countdown (PEN-R8, R16). It renders no world.
   - **Mechanism (IVP R13):** a client-local, non-replicated camera actor set through the owner-only view-target path. The PlayerController stays in `NAME_Playing`; the view target is never another participant's pawn, PlayerState or controller; no spectator state or RPC is used.
   - On a new binding (decided rule C1 takeover, or rejoin), the camera cuts to the new pawn in the remembered mode and fades in over `NoPawnFadeSec`. An informant's grade (art bible 2.9) is on from the cut frame and never eased in (`SetOwnerGrade(Informant)`, CS-R15; Penalty VA-NKP-4).
   - The control rotation starts at the pawn's current `HeadLook` (SCL R4.2), so the new head does not move.
   - The camera never blends across the map, which would draw a path.
3. **`Preparing`:** the neutral view, behind the briefing screen, until the first binding.
4. **`Ended`:**
   - A participant with a pawn keeps its camera and rules while the body brakes to idle (SCL R6).
   - A participant without a pawn gets the neutral view (Round: "view only, neutral camera").
   - There is no spectator or free camera in the MVP. The post-round reveal camera belongs to Post-Round Reveal & Results (VS).

**CS-R13. Field of view and comfort (MVP options, local).**
- **Default FOV:** 60° vertical in both modes (`FpFovVDeg`, `TpFovVDeg`). This is the art bible's reference camera, so its LOD and thumbnail tables hold. It is about 91.5° horizontal at 16:9.
- **Sliders:** one per mode, `FovVMinDeg`-`FovVMaxDeg` (55-75°), step 1, shown to the player as horizontal degrees.
  - **Per-aspect clamp.** The slider's top is limited to the vertical FOV at which the horizontal cap is reached for the current aspect (F5). At 21:9 that is about 61°, so no slider position does nothing.
- **Horizontal cap.** Horizontal FOV is Hor+ (it widens with aspect) but is capped at `FovHMaxDeg` (108°) for every aspect. An ultrawide monitor loses vertical FOV instead of gaining periphery. Every player can pick any setting, and no setting or display gives more than 108° horizontal or 75° vertical.
  - **Known, accepted gap (CD-GDD-ALIGN):** displays narrower than 16:9 see less horizontally at the same slider value. At 75° vertical, 16:10 reaches about 101.7° and 4:3 about 91.4°, against 107.5° at 16:9.
  - The MVP platform is PC at 16:9 or wider, so no vertical boost is added for narrow aspects. The accessibility guard holds in its intended sense: nothing gives more than the best setting every player can choose. FOV never changes with gait, traversal or strike.
- **Other MVP options:**
  - look sensitivity, split by X/Y and by mouse/stick;
  - invert Y;
  - stick dead zone and curve preset;
  - **Reduce camera motion**: the toggle becomes a 0.08 s crossfade with no dolly; there is no traversal pivot lag (only the speed cap); the shoulder swap and the boom recovery are cuts; FOV changes apply instantly. The body-driven view turn while steering (CS-R3.3) stays, because the body really turns. The setting changes no permission and nothing that is sent.
- **Settings & Accessibility (VS)** adds an optional view bob (default 0), separate blend options and the wider Reduce motion / Reduce screen effects settings (art bible 7.6, C8).

**CS-R14. Look input.**
- **Mouse:** raw input × sensitivity. No smoothing and no acceleration.
- **Right stick:**
  - radial dead zone `StickDeadZone` (0.15), outer dead zone `StickOuterZone` (0.98);
  - response `((|x| − dz)/(outer − dz))^StickCurveExp`, with exponent 1.5 (presets 1.0 / 1.5 / 2.0, F6);
  - full-deflection rates `StickYawRateDegPerSec` 180 °/s and `StickPitchRateDegPerSec` 120 °/s;
  - no acceleration ramp, no filtering, no aim assist of any kind (CS-R9, IIF R15);
  - the dead zone is **scaled radial**: the response is computed on the stick's magnitude and applied along its direction, so diagonals behave the same in every implementation (F6).

**CS-R15. Owner presentation hooks and view state (client-local).**
- **`ApplyTransientGrade()`** runs the art bible 2.7 grade once. Callers and timing are Assassination's (CS-R11.1).
- **`SetOwnerGrade(Id)`** sets a persistent per-viewer grade: `None` or `Informant` (art bible 2.9). It is driven only by the owner's own Informant state, and it is on from the cut frame (CS-R12.2).
- **Leaf-edge vignette** (art bible 2.8) is driven by own `IsConcealed` with Concealment's ease times (VA-5).
- **`GetViewState()`** returns `Live` (the world is rendered from a possessed pawn, including `Dying` and `Ended` with a pawn) or `Neutral`. HUD and Teammate Markers read it (Teammate Markers TM-R8, CR-3).
- **Published fade (ADR-0021 §3, ADR-0015 §6):** Camera keeps all fade timing (when, how fast, colour) and computes the fade alpha itself each frame; it publishes it through `GetScreenFade()`, and the **HUD's fade layer renders it**. The engine camera fade (`StartCameraFade`) is not used for the neutral, cut or elimination fades; the audio fade is driven from the same published value.
- **Layer order:** the fade-to-black sits above the marker layer, the HUD's world-anchored layers (rings, strike hint anchors) and the world overlays, and below HUD plates, cards and menus, so markers go out with the black and the neutral view's plates stay readable (HUD-R4, CR-HUD-10; matches Teammate Markers CR-3).
- **Settings:** Reduce screen effects scales the grades as the art bible specifies. No hook is ever driven by another pawn's state.

### States and Transitions

| State | Entered when | Camera | Mask | Exits to |
|---|---|---|---|---|
| `NoPawn` (neutral view) | no possessed pawn (Preparing, awaiting a pawn, Ended without a pawn) | client-local camera actor (IVP R13): black + HUD plates, no world render; `GetViewState() = Neutral` | n/a | `FirstPerson`/`ThirdPerson` on binding (cut + fade in) |
| `FirstPerson` | remembered mode or toggle | eye camera (CS-R7) | off | `Blend` on toggle; `Dying`; `NoPawn` on binding loss |
| `ThirdPerson` | remembered mode or toggle | boom; sub-states normal / squeezed / close (CS-R6) / concealed (CS-R11.3) | on (off below 20 cm) | `Blend`; `Dying`; `NoPawn` |
| `Blend` | toggle accepted (outside the lockout) | interpolates position and FOV over `CameraToggleBlendSec` | on | target mode at blend end |
| `Dying` | own local contact frame of an elimination (kill, reversal, or the exile strike) | current mode, no control, transient grade | as mode | fade over `NoPawnFadeSec` when `ViewRelease` falls due (`DeathFallSec` after contact; for an exile, HUD's card entrance or the `ExileCardTimeoutSec` fallback; for `InformantKilled`, `DeathServerTime + DeathFallSec`), then `NoPawn` |

### Interactions with Other Systems

| System | In | Out | Interface owner |
|---|---|---|---|
| Input & Interaction Framework | Look axis, toggle press and lockout (R14), shoulder-swap binding | `GetAimRay()` for Attack and the Strike hint (R10); masked = not rendered | IIF (bindings), Camera (aim ray) |
| Shared Character & Locomotion | body facing, eye socket height, capsule, `IsInTraversal`, look clamps, `TurnInPlaceDelaySec`, R9 owner-local allowances | control rotation (clamped target, CS-R3); masked = not rendered for R11 | SCL |
| Concealment (Bushes) | own `IsConcealed` (Owner); foliage channel responses; rustle effects tagged eye-masked; `HideVolume` bush-spot channel | CS-R11.3 concealed boom (60 cm), edge push off; `WorldPoint` into the leaves (CS-R8) | Concealment |
| Assassination & Defence Reversal | own action state and local contact frame (CS-R11.1); `DeathFallSec` | `ApplyTransientGrade` hook; `Dying` hold and `ViewRelease` | Assassination (when), Camera (hook) |
| NPC-Kill Penalty & Informants | own Informant state; `ExileBeatSec`, `ExileCardTimeoutSec`; "returning" countdown plate (HUD) | exile hold (CR-10); neutral view while awaiting a pawn; `SetOwnerGrade(Informant)` from the cut frame | Penalty (when), Camera (hooks) |
| Participant State & Roster / Round | own binding, owner-audience state, phase (Owner-audience client data only, CS-R1) | none | Roster, Round |
| Teammate Markers / HUD | view transform and FOV for constant-pixel overlays (art bible 7.1) | `GetViewState()` (Live / Neutral); fade layer above markers, below plates (CS-R15) | Camera (view state), HUD |
| Crowd & NPC Behaviour | none | `EvaluateViewEnvelope` (F1) for NPC virtual look; Tell Matrix row "camera mode: no visible deviation" | Crowd (use), Camera (function) |
| Settings & Accessibility (VS) | settings values | CS-R13 hooks | Settings |

## Formulas

### F1. View envelope

The view_envelope formula is defined as:

Per frame, with raw Look input `Δ` (degrees this frame, before any gain) and `v` the view offset from the body:
- **View yaw:** outward `|v| ← min(Ylim, |v| + g · |Δ|)` with `g = max(g_min, 1 − smoothstep(Ylim − S_y, Ylim, |v|))`; inward `v ← v + Δ` (gain 1). `ViewYaw = BodyYaw + v`.
- **Target (control yaw, the head-look target):** `r = v` while `|v| < Ylim`. At `|v| = Ylim`: if `pushing`, `|r| ← min(|r| + |Δ|, Ylim + E_eff)`; else `|r| ← max(Ylim, |r| − ρ_relax · dt)`. Any inward input first sets `|r| ← min(|r|, Ylim)`.
- **Pushing:** `pushing = ∃ t' ∈ [t − T_latch, t] : outward |Δ(t')| / dt ≥ ρ_min` (raw rate; the gain g is never applied here).
- **Overshoot:** `E_eff = E`, or 0 while own `IsConcealed`.
- **View pitch:** the same gain ramp over `S_p` at both limits, with `ViewPitch ∈ [−P_down, +P_up]` and no push. The replicated head pitch is SCL's clamp `±Plim` (SCL R8.4), not this range.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| ControlYaw, ControlPitch | float (°) | any | calculated (Look input) | control rotation (head-look target) |
| BodyYaw | float (°) | any | calculated (SCL actor yaw) | body facing |
| Δ | float (°) | any | calculated (Look input after sensitivity, F6) | raw input this frame |
| Ylim, Plim | float (°) | 30-90 / 10-45 | data file (SCL `LookYawLimitDeg` 70, `LookPitchLimitDeg` 20) | head clamps, consumed (Plim is replication only) |
| P_down, P_up | float (°) | 20-75 / 20-60 | data file (`ViewPitchDownLimitDeg` 60, `ViewPitchUpLimitDeg` 40) | local view pitch limits, both modes |
| S_y, S_p | float (°) | 0-15 / 0-10 | data file (`CameraEdgeSoftYawDeg` 10, `CameraEdgeSoftPitchDeg` 5) | soft-edge widths |
| g_min | float | 0.05-0.3 | data file (`CameraEdgeSoftMinGain` 0.1) | gain floor, so the limit is reached in finite time |
| E | float (°) | 2-10 | data file (`CameraEdgePushDeg` 5) | maximum overshoot of the target |
| ρ_min | float (°/s) | 5-40 | data file (`EdgePushMinRateDegPerSec` 15) | outward raw rate that counts as pushing |
| T_latch | float (s) | 0.05-0.3 | data file (`EdgePushLatchSec` 0.15) | how long one outward input keeps "pushing" true |
| ρ_relax | float (°/s) | 20-200 | data file (`EdgePushRelaxDegPerSec` 60) | overshoot relaxation |

**Output Range:**
- View yaw is within body ±70°, and view pitch within −60° to +40°. The target yaw is within ±75° (±70° while concealed).
- The gain at |v| = 60, 65 and 70 is 1, 0.5 and 0.1 (the floor).
- Reversal responds on the first frame.
- At `g_min` 0.05 and 180 °/s, the last degree takes about 0.11 s. With `E` at its hard minimum of 2°, the target still crosses the limit, so SCL R5.5 can always fire (I-CS-5).

**Example:** the body faces north (0°) and the player swings the mouse right at 180 °/s for 1 s.
- The view reaches 60° in 0.33 s, then slows through the soft band and reaches 70° about 0.2 s later.
- The raw input keeps "pushing" true, so the target climbs to 75° and holds while input continues, including a mouse re-centre gap under 0.15 s.
- If idle: after 0.5 s beyond the limit SCL pivots the body right, and the view turns with it.
- If the player lets go instead: "pushing" lapses after 0.15 s, the target relaxes to 70° in 0.08 s, and the body does not turn.

### F2. Boom position and collision

The boom_position formula is defined as:

`P_ideal = P + R(ViewYaw) · (−L, s · O_eff, H)` ; `d_max = |P_ideal − P|` ; `d_tgt = max(0, min(t_ray · d_max, t_s · d_max))` ; `d_t = d_tgt` if `d_tgt < d_{t−1}`, else after `t_hold`: `d_t = d_{t−1} + min((d_tgt − d_{t−1}) · (1 − e^(−dt/τ)), V_rec · dt)` ; `C = P + û · d_t`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| P | vector | world | calculated (CS-R4 pivot) | stable eye pivot |
| L | float (cm) | 100-250 | data file (`BoomLengthCm` 180) | boom length |
| O_eff | float (cm) | 0-60 | data file (`ShoulderOffsetCm` 45), reduced by the squeeze | effective shoulder offset |
| s | int | −1, +1 | calculated | shoulder side |
| H | float (cm) | 0-25 | data file (`BoomHeightOffsetCm` 15; 0 while concealed) | height above the eye |
| L_c | float (cm) | 0-70 | data file (`ConcealedBoomCm` 60) | while concealed: replaces L, with O_eff = 0 and H = 0, so d_max = L_c |
| t_ray, t_s | float | 0-1 | calculated | hit fractions of the 2 cm ray and the 14 cm sweep |
| t_hold, τ, V_rec | s, s, cm/s | 0-0.5, 0.1-0.6, 100-800 | data file (`BoomRecoverHoldSec` 0.15, `BoomRecoverTauSec` 0.30, `BoomRecoverCmPerSec` 300) | recovery |

**Output Range:** the free length is 0 to about 186 cm (√(180² + 45² + 15²)), about 258 cm at the hard maximums, and at most 60 cm while concealed. The camera is at eye height + 0-15 cm. Pull-in happens in the same frame. Recovery reaches 95 % of the gap about 1.1 s after the obstacle clears: 0.15 s hold, then 0.15 s at the 300 cm/s cap, then exponential. It never moves faster than 300 cm/s.

**Example:** walking into a 1.2 m alley with the right shoulder near the wall.
- The squeeze slides the offset to 0 over 0.15 s.
- A pillar at the back gives `t_s · d_max` = 52 cm, and the camera is at 52 cm that frame.
- Stepping clear: 0.15 s hold, then about 1 s back to 186 cm.
- Backed against a wall (free length 18 cm): close 3P with the head hidden, and the mask off.

### F3. Eye-origin visibility mask

The eye_mask formula is defined as:

`P_m = (P.x, P.y, min(P.z, z_eye))` (mask origin) ; `InCone(q) = x_q > 0 ∧ |atan2(y_q, x_q)| ≤ C_h ∧ |atan2(z_q, x_q)| ≤ C_v` (q in the view space at P_m) ; `Pass(T) = ∃ q ∈ Probes(T): InCone(q) ∧ ¬Blocked_ViewBlock(P_m → q)` (cone first, traces only for in-cone probes, early exit on the first clear trace; **at every distance**) ; `Visible_t = Pass_t ∨ (Visible_{t−1} ∧ fails < N_h)` ; `α_t = moveToward(α_{t−1}, Visible_t, dt / T_f)` ; mask-visible ⇔ `α > 0` ; test rate `f = f_near` if `dist ≤ D_near`, `f_far` if `dist ≤ D_max`, else `f_beyond` ; cost per frame `C = Σ_T n_T · f_T / f_frame`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| C_h, C_v | float (°) | 40-70 / 25-50 | data file (`EyeConeHalfHDeg` 55, `EyeConeHalfVDeg` 38) | fixed physical eye cone |
| Probes(T) | set | 5 | constant | head, chest, pelvis and two lateral capsule points |
| N_h | int | 1-4 | data file (`EyeMaskHideFails` 2) | consecutive failed tests before hiding |
| T_f | float (s) | 0.05-0.3 | data file (`EyeMaskFadeSec` 0.10) | fade time, in and out |
| P_m, z_eye | vector, float (cm) | world | calculated (CS-R4 pivot; real eye-socket height) | mask origin: never above the real eye |
| D_near, D_max | float (cm) | 1000-4000 / 3000-10000 | data file (`EyeMaskNearCm` 2000, `EyeMaskRangeCm` 6000) | rate bands (never a cutoff) |
| f_near, f_far, f_beyond | float (Hz) | 30-60 / 10-30 / 2-15 | data file (`EyeMaskNearHz` 60, `EyeMaskFarHz` 15, `EyeMaskBeyondHz` 5) | test rate per pawn by band |
| n_T | int | 0-5 | calculated | traces actually cast (0 when no probe is in the cone; early exit on the first clear one) |

**Output Range:**
- Each pawn is either shown or hidden, with α between 0 and 1. No pawn is drawn without a passing line-of-sight test at any distance.
- Reveal starts on the first passing test. With async traces, the result lands one frame later, so the delay is ≤ 1/f + 1 frame + T_f: ≤ 0.14 s near, ≤ 0.19 s far and ≤ 0.32 s beyond 60 m. It is never earlier than 1P.
- Hide starts after 2 failing tests: ≤ 50 ms near.
- **Worst case cost:** all 49 pawns near, inside the cone and hidden, so every probe is traced. That is 49 × 5 = 245 traces per frame at 60 Hz.
  - Typical: the cone prefilter removes about 70 % of pawns, and visible pawns exit early, so about 75 or fewer per frame.
  - Budget: target ≤ 0.5 ms game thread, async traces allowed. At about 5 µs per trace the worst case is about 1.2 ms **[verify, profile]**.
  - If the worst case exceeds 0.5 ms, technical-director decides among time-slicing the near band, fewer probes for distant pawns, or GPU queries (OQ10). The creative limit is that the 3P reveal delay must not be noticeably worse than 1P occlusion culling.

**Example:** the player in 3P stands 40 cm behind a wall corner, and an NPC walks down the cross street 8 m away.
- The lens, 45 cm to the side and 180 cm back, sees the street, but all five pivot→NPC traces hit the wall, so the NPC is not drawn and casts no shadow.
- The player steps 50 cm forward and the head probe clears. Within ≤ 17 ms + 1 frame + 0.10 s the NPC fades in: about when a 1P player at that spot would see it, and never earlier.
- The same NPC 80 m down a long street, visible from the eye: tested at 5 Hz, it fades in within 0.32 s. Behind a wall at 80 m, it stays hidden.

### F4. Toggle blend

The toggle_blend formula is defined as:

`u = smoothstep(clamp(t / T_b, 0, 1))` ; `C = lerp(C_from, C_to, u)` ; `FOV_v = lerp(FOV_from, FOV_to, u)` ; head hidden when into-1P `u ≥ 0.85` or out-of-1P `u ≤ 0.15` ; constraint `T_b < T_lock`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t | float (s) | 0-T_b | calculated | time since the press |
| T_b | float (s) | 0-0.30 | data file (`CameraToggleBlendSec` 0.20; 0.08 crossfade under Reduce camera motion) | blend time |
| T_lock | float (s) | 0.2-0.5 | data file (IIF `CameraToggleLockoutSec` 0.30) | lockout, consumed |
| C_from, C_to | vector | | calculated (F2 / CS-R7, re-evaluated every frame) | endpoints |

**Output Range:** the blend ends 0.10 s before the next toggle can be accepted. The peak lens speed is about 1.5 · 186 / 0.20 ≈ 14 m/s, which is the smoothstep peak.

**Example:** 3P → 1P at 60 fps takes 12 frames. The lens moves 186 cm along a smoothstep. u reaches 0.85 at t ≈ 0.151 s, so the head is hidden from frame 10 onward, when the lens is still about 28 cm from the pivot. The lens never sees the inside of the head.

### F5. Field of view

The fov formula is defined as:

`HFOV = min(2 · atan(tan(VFOV / 2) · A), H_max)` ; if capped, `VFOV_eff = 2 · atan(tan(H_max / 2) / A)` ; slider top for aspect A: `VFOV_top(A) = min(FovVMaxDeg, 2 · atan(tan(H_max / 2) / A))`, floored at `FovVMinDeg`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| VFOV | float (°) | 55-75 | data file (`FpFovVDeg` 60, `TpFovVDeg` 60; slider `FovVMinDeg` 55-`FovVMaxDeg` 75) | chosen vertical FOV |
| A | float | 1.25-3.6 | calculated | aspect ratio |
| H_max | float (°) | 90-120 | data file (`FovHMaxDeg` 108) | horizontal cap, any aspect |

**Output Range:** at 16:9, 55-75° vertical gives 86-107.5° horizontal, all under the cap and inside the 110° eye cone.

**Example:** at 32:9 (A = 3.56) and 60° vertical, the uncapped width would be 128°. It is capped at 108°, which gives VFOV_eff ≈ 42.3°. A super-ultrawide player never sees more periphery than a 16:9 player at 75°. At 21:9 (A = 2.33) the slider top is about 61.1°; at 32:9 every slider position is capped, so the slider is shown disabled and the view is a fixed 42.3° vertical.

### F6. Stick look response

The stick_look formula is defined as:

`m = clamp((|x| − dz) / (outer − dz), 0, 1)` on the stick's radial magnitude `|x|` ; `d = x / |x|` (direction preserved) ; `ω_yaw = m^k · d_x · Ω_yaw · sens_x`, `ω_pitch = m^k · d_y · Ω_pitch · sens_y`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| x | float | 0-1 | calculated | stick deflection (radial) |
| dz, outer | float | 0.05-0.40 / 0.9-1.0 | data file (`StickDeadZone` 0.15, `StickOuterZone` 0.98) | dead zones |
| k | float | 1.0-2.0 | data file (`StickCurveExp` 1.5) | curve exponent |
| Ω | float (°/s) | 60-360 | data file (`StickYawRateDegPerSec` 180, `StickPitchRateDegPerSec` 120) | full-deflection rate |
| sens | float | 0.33-2 | user setting (default 1) | sensitivity multiplier |

**Output Range:** 0 up to Ω · sens per axis. The 140° yaw envelope takes about 0.9 s from edge to edge at full deflection (0.78 s plus the soft edge, F1).

**Example:** at a pure-right deflection of 0.565, m = (0.565 − 0.15)/0.83 = 0.5, so ω = 0.5^1.5 · 180 = 63.6 °/s yaw. A diagonal of the same magnitude (d = (0.707, 0.707)) gives 45.0 °/s yaw and 30.0 °/s pitch.

## Edge Cases

**Toggle and blend**
- **If the player toggles inside the 0.30 s lockout**: the press is ignored (IIF R14), and a blend already running completes.
- **If the player toggles mid-hold (Interact, Prepare) or mid-montage**: the hold or montage continues, no RPC is sent, and the HUD does not change (IIF AC 29).

**Boom and collision**
- **If the 3P sweep starts inside geometry** (for example, the pivot is pressed against a wall during a climb): the free length is 0 and the view is at the eye with the mask off. The lens never renders from inside a wall, because the check starts from the pivot and the near clip is 5 cm.
- **If a dense crowd fills an alley**: pawns are ignored by the sweep, so only walls pull the lens in. The 15 cm hysteresis prevents close-3P flicker.

**The eye mask**
- **If a pawn is partly visible at a corner or behind a fence**: it is shown if any of its five probes passes. It is hidden only after 2 consecutive failed tests, which stops thin-pole flicker. It may be hidden when a 1P eye could see a sliver, which is allowed: 3P may see less, never more.
- **If a masked pawn is beside or behind the body but inside the 3P frame**: it is not drawn (strict mask, confirmed by the CD; OQ1 closed).
- **If a pawn walks between the lens and the own body**: it is behind the eye and outside the cone, so it is masked. It never blocks the own-pawn slot, and it is as invisible as it would be to a 1P eye. A pawn approaching from behind is therefore not seen in either mode; it may be heard.
- **If a masked pawn performs an assassination behind a wall**: it stays masked. An NPC kill is exposed only to viewers with line of sight, as in 1P.
- **If the pawn under the 3P crosshair is masked or unseen from the eye**: the aim ray gives a `TargetSpot`, and no Strike hint appears.
- **If a pawn stands behind glass or foliage**: it is not blocked, because the 1P eye sees through those too, so it is visible.
- **If a pawn crosses the 60 m band boundary**: only its test rate changes (15 Hz to 5 Hz). The line-of-sight test still runs, so a pawn behind a wall at any distance stays hidden. The bands are the same for every pawn.
- **If the player drops off a ledge in 3P**: the lagged pivot trails above the real eye for about 0.1 s, but the mask origin uses the lower of the two, so nothing above the wall line is revealed early.

**Concealment**
- **If a concealed enemy is in a bush in front of the player**: Concealment does not render it, whatever the mask says. The aim ray at that spot gives a `TargetSpot`, so a blind attack is possible.
- **If the owner is concealed and toggles to 3P**: the boom is at most 60 cm straight back from the pivot, with shoulder and height offsets of 0, and the view stays in the leaves.
- **If a concealed owner pushes Look against the yaw limit**: the view stops at 70° and the target never overshoots, so the locked body never turns (CON-R7).
- **If the owner aims into a bush**: `WorldPoint` is the ray's entry into the `HideVolume`, projected to the ground, whether the bush is occupied or empty. No hint differs (CS-R8).

**Body movement and network**
- **If the body turns by root motion during a Strike**: the view keeps its world yaw until the envelope edge carries it. There is no snap.
- **If the owner receives a network correction (SCL F10)**:
  - Below `PawnSmoothSnapThresholdCm`, the pivot low-pass smooths it.
  - Above that threshold, the camera snaps with the body. It never shows a smoothed false position across a teleport.
- **If stick drift sits at the edge**: drift under the dead zone is zero input. Drift above it but below 15 °/s cannot hold the overshoot, so the body never turns by itself.

**Elimination and respawn**
- **If the player is eliminated in 3P**: the camera stays in 3P on the dying body through the 1.2 s fall, then fades to black. The remembered mode carries over to the informant body.
- **If the player is exiled (third NPC strike)**: the view holds on their released body without control until HUD's card-entrance event (plate show time + `ExileBeatSec`), or `ExileCardTimeoutSec` (2.0 s) after the owner's `StrikeLimit` state if no card enters, then fades (PEN-R16, Penalty CR-10 amended).
- **If the player's informant body is killed**: the view stays on the falling body and fades from `DeathServerTime + DeathFallSec`; no removal card; the neutral view and countdown follow (Penalty CR-18).
- **If a strike whiffs**: no transient grade on any camera; the attacker's `NoContact` plate appears on the same frame (VA-ASN-4).
- **If the host masks a pawn in 3P**: its authoritative pose, root motion and notifies tick exactly as for an unmasked pawn (always-tick; CS-R1, AC-CS-28).
- **If an informant respawn takes over a pawn facing another way**: the camera cuts and fades in. The control rotation starts from the new pawn's `HeadLook` (SCL R4.2), so observers see no head move.

**Host, session and rejoin**
- **If the listen-server host plays in 1P**: only the host's local view hides the head. The server's simulation of the host pawn (pose tick, root motion, notifies, LOD) does not depend on local visibility (SCL change request). AC-CS-02 checks this from a client.
- **If the host leaves (listen server ends)**: the session ends (Session & Travel), and the camera goes to `NoPawn` and shows the session message. No camera state migrates.
- **If a client disconnects and rejoins**: camera mode is a local setting, so it is unchanged. The view is `NoPawn` until the client repossesses a pawn.

**Settings and display**
- **If the FOV setting changes mid-round**: it eases in over 0.15 s. The mask is unaffected (the cone is fixed), and nothing is sent.
- **If the aspect ratio exceeds the cap (ultrawide, triple screen)**: F5 caps the horizontal FOV at 108° and lowers the vertical FOV. The slider top is clamped per aspect, so no position is a no-op.
- **If a first-person player looks down 60° at a bush at their feet**: the view and aim pitch reach −60° in both modes; observers see the head held at −20° (SCL R9).
- **If the display is narrower than 16:9 (16:10, 4:3)**: the horizontal FOV is lower (about 101.7° and 91.4° at 75° vertical). This is an accepted gap (CS-R13).

## Dependencies

Source: `design/gdd/systems-index.md` row 10 (Depends On: Input & Interaction Framework, Shared Character & Locomotion). Tuning Data & Config is added as an upstream dependency because it holds every key. Index rows 12, 13, 14, 17 and 18 list Camera as a (mostly soft) dependency (verified 2026-10-24, cross-review C-W11); the dependents below follow the interfaces. Statuses were re-checked on 2026-10-24 at design review.

### Upstream

| System (#) | Interface | Hard/Soft | Status |
|---|---|---|---|
| Tuning Data & Config (1) | Camera keys (Tuning Knobs, CR-15) via `GetClientTuning()`; SCL and IIF keys read from the same snapshot | Hard | designed (add Camera to its dependents) |
| Input & Interaction Framework (3) | Look axis, toggle (R14, `CameraToggleLockoutSec`), `IMC_Common`, shoulder-swap binding (change request) | Hard | in review |
| Shared Character & Locomotion (9) | body facing, eye socket height, capsule, `IsInTraversal`, look clamps, `TurnInPlaceDelaySec`, R9 (incl. D6 pitch), `PawnSmoothSnapThresholdCm`, always-tick | Hard | in review |
| Participant State & Roster (2) | own binding and owner-audience state (pawn present, Informant) for CS-R12 | Soft | designed |
| Round & Match Framework (6) | phase, for the `Preparing`/`Ended` neutral view | Soft | designed |
| Information Visibility Policy (7) | L17 (local-only components), R13 (neutral view without spectator state), AC-IVP-17 (presentation include check), own `IsConcealed` (Owner audience) | Soft | in review |
| Concealment (12) | own `IsConcealed`; foliage channel responses; `HideVolume` bush-spot channel; bush dome size (I-CS-4) | Soft | in review |
| Assassination & Defence Reversal (13) | local contact frame and `DeathFallSec` (CS-R12.1); grade timing (VA-ASN-4) | Soft | designed (pending review) |
| NPC-Kill Penalty & Informants (14) | `ExileBeatSec`, `ExileCardTimeoutSec`; own Informant state for `SetOwnerGrade`; `InformantKilled` view reason (CR-18) | Soft | in review |

### Downstream

| Dependent (#) | What it takes | Hard/Soft | Status |
|---|---|---|---|
| Input & Interaction Framework (3) | `GetAimRay()` and `WorldPoint`; masked = not rendered (R10, Strike hint) | Hard | in review (change request) |
| Shared Character & Locomotion (9) | masked = not rendered for R11 (mask flag); the clamped control-rotation target | Soft | in review |
| Concealment (12) | CS-R11.3 (60 cm boom, edge push off); foliage ignores `Camera`/`ViewBlock`; rustle effects tagged eye-masked; `WorldPoint` into `HideVolume` | Soft | in review |
| Assassination & Defence Reversal (13) | `ApplyTransientGrade`; aim ray; `Dying` hold and own-elimination fade (CS-R12.1) | Soft | designed (pending review) |
| NPC-Kill Penalty & Informants (14) | informant camera follows the same rules; `NoPawn` while awaiting a pawn; exile hold (CR-10); `SetOwnerGrade(Informant)` | Soft | in design |
| Crowd & NPC Behaviour (11) | `EvaluateViewEnvelope` (F1) for NPC virtual look; Baseline and Tell Matrix row: camera mode has no visible deviation | Soft | in review |
| Teammate Markers (17) | view transform and FOV; `GetViewState()`; fade layer order (CR-3) | Hard (for Markers) | designed (pending review) |
| HUD (18) | view transform and FOV; `GetViewState()`; the one-time 3P hint; neutral-view plates | Soft | designed (hud.md) |
| Settings & Accessibility (24, VS) | CS-R13 hooks | Soft | not written |
| Post-Round Reveal (21, VS) | owns its own reveal camera; Camera hands over in `Ended` | Soft | not written |
| Anti-Cheat & Info-Leak Security (27, Alpha) | the CS-R5c residual risk and the server-side view-check ADR | Soft | not written |

## Tuning Knobs

The table uses the CR-15 columns. Every key is owner-local presentation with one value for every client, and no key has an NPC or player variant. Audience `Client`, Latch `Use` unless noted. Keys marked * are user settings, clamped to the hard range.

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|---|---|---|
| `ViewHeadLeadMaxDeg` | float | Camera | disabled (35 when enabled) | 15-70 | 25-45 | Client | Round | Deg | Fallback for OQ2: the view may lead the visible head by at most this. Low: the view feels tethered and laggy (comfort). Off: glance asymmetry |
| `ViewPitchDownLimitDeg` / `ViewPitchUpLimitDeg` | float | Camera | 60 / 40 | 20-75 / 20-60 | 50-65 / 30-45 | Client | Round | Deg | Local view and aim pitch, both modes (CS-R3.1; SCL D6). Low: cannot aim at a bush at the feet. High: own body leaves the 3P frame more often; replicated head pitch is unaffected (SCL ±20°) |
| `CameraEdgePushDeg` | float | Camera | 5 | 2-10 | 3-8 | Client | Use | Deg | Below 2: SCL R5.5 may never see the target cross the limit (I-CS-5). High: no effect on feel (it is elastic) but a longer body pivot |
| `EdgePushMinRateDegPerSec` | float | Camera | 15 | 5-40 | 10-25 | Client | Use | DegPerSec | Raw outward rate. Low: drift turns the body. High: hard to trigger turn-in-place |
| `EdgePushLatchSec` | float | Camera | 0.15 | 0.05-0.3 | 0.1-0.2 | Client | Use | s | Low: mouse users cannot hold a push across a re-centre. High: the body keeps turning after the player stops |
| `CameraEdgeSoftMinGain` | float | Camera | 0.1 | 0.05-0.3 | 0.08-0.15 | Client | Use | Mult | Low: the last degree before the limit crawls. High: the edge feels like a wall |
| `EdgePushRelaxDegPerSec` | float | Camera | 60 | 20-200 | 40-120 | Client | Use | DegPerSec | Low: the body keeps turning after release |
| `CameraEdgeSoftYawDeg` / `CameraEdgeSoftPitchDeg` | float | Camera | 10 / 5 | 0-15 / 0-10 | 6-12 / 3-6 | Client | Use | Deg | 0: hard wall (sickness). High: the edge feels mushy |
| `CameraPivotLagSec` | float | Camera | 0.10 (0.12 in traversal) | 0-0.25 | 0.06-0.14 | Client | Use | s | High: the view trails the body. 0: animation jitter |
| `PivotMaxVertCmPerSec` | float | Camera | 300 | 100-600 | 200-400 | Client | Use | CmPerSec | Low: the lens lags a climb |
| `BoomLengthCm` | float | Camera | 180 | 100-250 | 150-200 | Client | Use | Cm | High: more parallax for the mask to hide. Low: the body fills the screen |
| `ShoulderOffsetCm` | float | Camera | 45 | 0-60 | 35-50 | Client | Use | Cm | High: larger peek geometry. 0: the body blocks the centre |
| `BoomHeightOffsetCm` | float | Camera | 15 | 0-25 | 5-20 | Client | Use | Cm | High: over-wall views of rooftops (I-CS-2) |
| `CameraGuardRadiusCm` / `CameraProbeRadiusCm` | float | Camera | 2 / 14 | 1-5 / 10-20 | 2 / 12-16 | Client | Use | Cm | Probe below 12: the near plane clips walls |
| `BoomRecoverHoldSec` / `BoomRecoverTauSec` / `BoomRecoverCmPerSec` | float | Camera | 0.15 / 0.30 / 300 | 0-0.5 / 0.1-0.6 / 100-800 | 0.1-0.2 / 0.25-0.4 / 200-450 | Client | Use | s / s / CmPerSec | Fast: the camera pumps in alleys. Slow: it stays compressed |
| `ShoulderSqueezeInSec` / `ShoulderSqueezeOutSec` | float | Camera | 0.15 / 0.40 | 0.05-0.5 / 0.1-1 | 0.1-0.2 / 0.3-0.5 | Client | Use | s | Presentation |
| `BoomCloseCm` | float | Camera | 60 | 30-100 | 50-80 | Client | Use | Cm | Low: the lens is inside the head. High: frequent headless 3P |
| `ConcealedBoomCm` | float | Camera | 60 | 0-70 | 40-60 | Client | Use | Cm | Straight back, no shoulder or height offset. High: the lens leaves the leaf dome and the hider surveys from behind the bush (P2; I-CS-4). Low: the leaf framing fills the screen |
| `FpForwardOffsetCm` | float | Camera | 8 | 0-20 | 5-12 | Client | Use | Cm | Low: the neck is visible. High: the lens passes a wall the face touches |
| `CameraNearClipCm` | float | Camera | 5 | 1-15 | 3-8 | Client | Use | Cm | High: own hands and close walls clip |
| `EyeConeHalfHDeg` / `EyeConeHalfVDeg` | float | Camera | 55 / 38 | 40-70 / 25-50 | 52-60 / 36-42 | Client | Round | Deg | High: 3P sees beside the body. Must stay ≥ the FOV slider maximum (I-CS-3) |
| `EyeMaskHideFails` | int | Camera | 2 | 1-4 | 2-3 | Client | Use | Count | High: 3P keeps seeing briefly after the eye loses sight |
| `EyeMaskFadeSec` | float | Camera | 0.10 | 0.05-0.30 | 0.08-0.12 | Client | Use | s | High: 3P is later than 1P. Low: hard pops |
| `EyeMaskNearCm` / `EyeMaskRangeCm` | float | Camera | 2000 / 6000 | 1000-4000 / 3000-10000 | — | Client | Use | Cm | Trace-cost bands |
| `EyeMaskNearHz` / `EyeMaskFarHz` / `EyeMaskBeyondHz` | float | Camera | 60 / 15 / 5 | 30-60 / 10-30 / 2-15 | 45-60 / 12-20 / 4-8 | Client | Use | Hz | Low: late reveal. High: trace cost. Beyond `EyeMaskRangeCm` the line-of-sight test still runs (F3) |
| `CameraToggleBlendSec` | float | Camera | 0.20 | 0-0.29 | 0.15-0.25 | Client | Use | s | Must be < `CameraToggleLockoutSec` (I-CS-1) |
| `ShoulderSwapBlendSec` | float | Camera | 0.25 | 0-0.5 | 0.2-0.35 | Client | Use | s | Low: jarring. High: sluggish |
| `NoPawnFadeSec` | float | Camera | 0.5 | 0-1.5 | 0.3-0.8 | Client | Use | s | Presentation |
| `FpFovVDeg` * / `TpFovVDeg` * | float | Camera | 60 / 60 | 55-75 | 55-70 | Client | Use | Deg | 60 is the art bible reference camera |
| `FovVMinDeg` / `FovVMaxDeg` | float | Camera | 55 / 75 | 45-90 | 55 / 75 | Client | Round | Deg | The maximum raises everyone's periphery equally |
| `FovHMaxDeg` | float | Camera | 108 | 90-120 | 100-110 | Client | Round | Deg | Cap for any aspect (fairness) |
| `StickDeadZone` * / `StickOuterZone` | float | Camera | 0.15 / 0.98 | 0.05-0.40 / 0.9-1 | 0.1-0.2 / 0.95-0.99 | Client | Use | Mult | Low: drift. High: a dead stick |
| `StickCurveExp` * | float | Camera | 1.5 | 1.0-2.0 | 1.0-2.0 | Client | Use | Mult | Precision versus turn speed |
| `StickYawRateDegPerSec` * / `StickPitchRateDegPerSec` * | float | Camera | 180 / 120 | 60-360 | 140-240 / 90-150 | Client | Use | DegPerSec | Low: cannot turn toward a sound. High: twitchy |

Invariants:
- **I-CS-1**: `CameraToggleBlendSec` ≤ `CameraToggleLockoutSec` − 1 frame. The lint enforces it, and the runtime also clamps the blend, because the IIF's lockout hard range (0-1 s) reaches below the blend.
- **I-CS-2**: `BoomHeightOffsetCm` ≤ 25.
- **I-CS-3**: `EyeConeHalfHDeg` ≥ ½ · `FovHMaxDeg`, and `EyeConeHalfVDeg` ≥ ½ · `FovVMaxDeg`.
- **I-CS-4**: `ConcealedBoomCm` ≤ bush dome inner radius − 20 cm (90 − 20 = 70 cm for the 1.8 m dome of Concealment CON-R7), and ≤ `BoomLengthCm`.
- **I-CS-5**: `CameraEdgePushDeg` ≥ 2°, so the target crosses the yaw limit that SCL R5.5 tests.

Consumed, not owned: `LookYawLimitDeg`, `LookPitchLimitDeg`, `TurnInPlaceDelaySec`, `PawnSmoothSnapThresholdCm` (SCL); `CameraToggleLockoutSec` (IIF); `DeathFallSec`, `StrikeWarpYawLimitDeg` (Assassination); `ExileBeatSec`, `ExileCardTimeoutSec` (Penalty); bush dome size (Concealment).

## Visual/Audio Requirements

*art-director not consulted (orchestrator budget of two specialists). These requirements are derived from art bible sections 2.7, 2.8, 5.2, 7.1 and 8.5.*

- **FOV and the art bible tables.** The default 60° vertical FOV in both modes confirms the art bible's reference camera. The 8.5 LOD and thumbnail tables therefore hold, and the "FOV unconfirmed (4.8)" note can be closed. A player who raises the FOV makes figures smaller on screen; LODs switch by screen height, so no rule changes.
- **Own body in 3P.** No self-outline, ring or marker on the own body (art bible 5.2, 7.1).
- **Mask fade.** One shared character material with a dithered opacity driven by a per-pawn value (custom primitive data or a per-pawn material instance; CS-R5b). At α = 0 the primitives are hidden, which removes shadow and ray-tracing presence. Every pawn uses the same parameter path **[verify cost on the 50-pawn case, and TSR/Lumen/MegaLights behaviour with dither, OQ5]**. No colour, outline, silhouette or ghost is shown while fading.
- **Post-process hooks (CS-R15).** The leaf-edge vignette (2.8), the transient grade (2.7, at the local contact frame only) and the persistent informant grade (2.9, on from the cut frame) are camera post-process hooks; their look is the art bible's.
- **Audio.** The listener sits at the eye pivot in both modes, and the toggle and shoulder swap make no sound.

## Game Feel

| Feel target | Target | Why |
|---|---|---|
| Look input → view | same frame, no smoothing (IIF F5) | the eyes never lag |
| Edge of the envelope | 10° yaw / 5° pitch smoothstep ease (gain floor 0.1), reversal within 1 frame, no dead zone; turn-in-place fires from a held push on mouse and stick alike | the clamp reads as the neck's limit, not a wall |
| Toggle | 0.20 s eased dolly, peak about 14 m/s, inside the 0.30 s lockout, no sound | quick without disorienting |
| Boom pull-in / recovery | instant in; 0.15 s hold then τ 0.30 s out | never inside a wall; no pumping |
| Mask reveal compared with 1P | ≤ 0.14 s later within 20 m, ≤ 0.19 s to 60 m, ≤ 0.32 s beyond, and never earlier | 3P is never the better scouting mode |
| 1P stability | no bob, no roll, pivot low-pass 0.10 s | comfort in a slow walking game |

**What it must never feel like:** a sniper scope, a drone above the crowd, or an aim-assisted shooter.

## UI Requirements

The camera has no HUD element and no reticle (art bible 7.1). It supplies the view transform and FOV to HUD and Teammate Markers.

The MVP options listed in CS-R13 go on a minimal options panel: default camera mode, FOV per mode, sensitivity, invert Y, stick dead zone and curve, and Reduce camera motion. The full screen belongs to Settings & Accessibility (VS).

**Teaching the mask.** The mask is taught by the world, not by a popup:
- **Primary:** the first-play graybox map opens with a scripted corner. A walker passes behind a wall the player approaches in 3P. It appears only when the player's eye clears the corner. The scenario uses a scripted dummy, never a real masking event in a live round (owner: Onboarding & Practice Mode, Alpha; until then the graybox test map, AC-CS-24).
- **Supporting:** on the first **live** 3P frame (never during the briefing), an owner-only hint appears once for 4 s: "Third person shows only what your character can see." It is a local flag, and the same line appears in the controls help and a loading tip.

**Neutral view.** The camera renders only black and the HUD's plates. The plates include Penalty's Owner-only "returning" countdown while the player awaits a pawn (PEN-R8). The countdown is HUD's widget.

**No hint is ever triggered by masking itself.** "The first time a pawn is masked" would tell the player that someone is behind the wall.

> **📌 UX Flag — Camera System**: this system has UI requirements (the options entries and the one-time hint). Run `/ux-design` for the options panel and HUD hint before writing epics.

## Cross-References

| This GDD references | Target doc | Element | Status |
|---|---|---|---|
| Head clamps, eye socket, R4.2, R5.5 turn-in-place, R9 (incl. D6 pitch), R11, OQ8 ruling | design/gdd/shared-character-locomotion.md | R4, R5, R8, R9, R11, F6 | in review |
| Toggle R14 and lockout, Attack ray R10 and `WorldPoint`, Strike hint, bindings R15 | design/gdd/input-interaction-framework.md | R10, R14, R15 | in review |
| L17 local-only components; `IsConcealed` audience | design/gdd/information-visibility-policy.md | leak table, fact table | in review |
| Neutral camera in `Ended` and during rejoin; informant respawn takeover (C1) | design/gdd/round-match-framework.md | R11, edge cases | designed |
| Reference camera 60°, LOD by screen height, HUD camera-mode independence, 2.7 and 2.8 grades | design/art/art-bible.md | 2.7, 2.8, 5.2, 7.1, 8.5 | approved |
| Own-pawn finding by camera framing (OQ-6) | design/gdd/character-pool-appearance.md | Game Feel, OQ-6 | designed |
| Concealed boom 60 cm, body yaw locked (OQ7), foliage channels, rustle tag, `HideVolume` spot channel | design/gdd/concealment-bushes.md | CON-R7, CON-R12, VA-5, CR 12 | in review |
| Strike warp cap 30° (OQ8), transient grade timing, possession through the fall, no warning to the victim | design/gdd/assassination-defence-reversal.md | ASN-R6, ASN-R16, ASN-R22, VA-ASN-4, AC-ASN-23 | designed (pending review) |
| Exile hold, neutral-view countdown, informant grade from the cut frame | design/gdd/npc-kill-penalty-informants.md | PEN-R8, PEN-R16, VA-NKP-4, CR-10 | designed |
| View gate and fade layer order | design/gdd/teammate-markers.md | TM-R8, TM-R9, CR-3 | designed (pending review) |
| NPC virtual look (motion parity), Baseline and Tell Matrix | design/gdd/crowd-npc-behaviour.md | R3.4, OQ11 | in review |
| Neutral view without spectator state; presentation include check | design/gdd/information-visibility-policy.md | R13, L17, L30, AC-IVP-17 | in review |
| Informant grade | design/art/art-bible.md | 2.9 | approved |
| Gameplay Cameras plugin (Experimental in 5.8) | docs/engine-reference/unreal/plugins/gameplay-camera-system.md | 5.8 changes | reference |

**Change requests to other GDDs** (the orchestrator applies them):
1. **IIF:**
   - Add Shoulder swap (X / middle mouse, **D-pad Right**; R3 rejected at design review) to `IMC_Common`, effective in 3P only.
   - R10 and the Strike hint call Camera's `GetAimRay()`; R10's `WorldPoint` is Camera's (CS-R8), including the `HideVolume` entry point.
   - "Rendered" means the mask's α > 0 flag, never the engine's was-recently-rendered test.
2. **SCL:**
   - Camera's pivot is capsule-derived and uses only the eye socket's height.
   - Extend the R9 owner-local allowance to hiding the head, neck and hair in close 3P (lens within 60 cm).
   - The R5.5 trigger accepts the elastic 5° overshoot (minimum 2°, I-CS-5).
   - Server-side pose tick, bone refresh, root motion and notifies must not depend on local render state: always-tick for every pawn on the listen-server host. R11 "rendered" reads the mask flag (AC-CS-28).
   - **R9 / OQ8:** record that the −60°/+40° view pitch applies in **both** modes (creative-director ruling at Camera design review, extending D6; user to confirm, OQ9).
   - Review raising `PivotTurnRateDegPerSec` from 120 to ≥ 150 °/s (OQ3).
3. **Concealment:** items 1-2 (foliage channels, rustle tag) confirmed by Concealment; OQ7 answered and applied (CS-R11.3); change request 12 applied (`ConcealedBoomCm` 60).
   - **New:** Concealment's note "safe range capped at 80" becomes 70 under I-CS-4 (dome inner radius 90 cm − 20 cm margin). Concealment should publish the dome inner radius as a key or a fixed asset fact that Camera's lint can read.
4. **Assassination:** applied (`StrikeWarpYawLimitDeg` 30, `ApplyTransientGrade` at the local contact frame, never on a whiff). No further request.
5. **Crowd:**
   - Add a Baseline and Tell Matrix row: "camera mode / toggle: no visible deviation; not an advantage; owner Camera".
   - **New:** Crowd R3.4's virtual look layer calls Camera's `EvaluateViewEnvelope` (F1), so NPC edge pushes, latches and relaxes follow the same function. Push durations are drawn from human telemetry, because matching the code alone does not give motion parity.
6. **HUD:**
   - Add the one-time 3P hint on the first live 3P frame; never add a mask-triggered hint.
   - Read `GetViewState()`.
   - The neutral view shows Penalty's "returning" countdown plate.
7. **Art bible:** close the FOV-unconfirmed note in 4.8 and 8.5 (60° confirmed). OI-13 still owns the density rule.
8. **Tuning Data & Config:** add Camera as a dependent. Register the new keys `ViewPitchDownLimitDeg`, `ViewPitchUpLimitDeg`, `EdgePushLatchSec`, `CameraEdgeSoftMinGain` and `EyeMaskBeyondHz`, plus invariants I-CS-4 and I-CS-5.
9. **NPC-Kill Penalty:** CR-10 applied (CS-R12.1 exile hold through `ViewRelease`). VA-NKP-4's informant grade uses `SetOwnerGrade(Informant)` (CS-R15).
10. **Teammate Markers:** CR-3 applied (`GetViewState()`, fade layer above markers and below plates, CS-R15).
11. **Onboarding & Practice Mode (Alpha; index row 25):** own the scripted mask-teaching corner (UI Requirements).

**Change requests received (reconciliation 2026-10-24; requests newer than this GDD's review)**

| From | Request | Disposition |
|---|---|---|
| Assassination CR 19 | `Dying` entry and `ViewRelease` driven by `OnRoleChangePresented` (ASN-R23) | APPLIED (CS-R12.1) |
| Assassination CR 6 | `StrikeWarpYawLimitDeg` 30; Camera lists Assassination as soft dependent | ALREADY DONE |
| Penalty CR-10 (amended at Penalty review) | Exile `ViewRelease(StrikeLimit)` raised by HUD's card-entrance event, `ExileCardTimeoutSec` fallback | APPLIED (CS-R12.1, States, Edge Cases, consumed keys, AC); replaces the earlier contact-frame timer |
| Penalty CR-18 | Own-view reason `InformantKilled` (fade from `DeathServerTime + DeathFallSec`, no card) | APPLIED (CS-R12.1, States, Edge Cases, AC). Judged not a schema change (client-local event reason, nothing replicated or stored); Penalty OQ-NKP-12 keeps the user confirmation open |
| HUD CR-HUD-10 | Expose live/neutral view state and the fade layer (plates above the black; fade above HUD world-anchored layers, below cards) | APPLIED (CS-R15 layer order); `GetViewState()` ALREADY DONE |
| Concealment CR 12 | `ConcealedBoomCm` 60, safe 40-80; OQ7 answered | ALREADY DONE |
| Teammate Markers CR-3 | `GetViewState()`, fade layer above markers and below plates | ALREADY DONE |
| SCL CR 6 | `GetHeadLookWorldRotation()` and eye socket; mode changes nothing replicated; ±70° yaw; pitch −60/+40 | ALREADY DONE (CS-R3, CS-R4) |
| Cross-review 2026-10-24 S-W2 | Hold own `IsConcealed` into the death presentation of a victim killed in a bush | APPLIED (cross-review fixes 2026-10-24): CS-R12.1 concealed input hold |
| Cross-review 2026-10-24 C-W11 | Stale index and status claims | APPLIED (cross-review fixes 2026-10-24): Dependencies source note (index rows listing Camera), HUD and Penalty statuses |
| ADR-0015 sync 2026-10-24 | CS-R5b "First person: the mask is off" → off for hiding; still computes the rendered flag in flag-only mode | APPLIED (CS-R5b; no behaviour change for the viewer) |
| ADR-0015 sync 2026-10-24 | F3 "early exit on the first clear trace" → async traces submit all in-cone probes together (same `Pass`); sync early-exit is the near-band fallback | APPLIED (CS-R5 evaluation order); F3 formula and `n_T` row keep "early exit" as the synchronous-path statement |
| ADR-0015 sync 2026-10-24 | CS-R5b fade mechanism "[verify]" → custom primitive data slot `CPD_EyeMaskAlpha` | APPLIED (CS-R5b Fade) |
| ADR-0015 sync 2026-10-24 | AC test root/prefix per ADR-0007/0008; AC-CS-37 adds seamless-travel and round-reset neutral-view cases | APPLIED (AC preamble, AC-CS-37) |
| ADR-0021 sync 2026-10-24 | CS-R15: the fade is rendered by the HUD fade layer from Camera's published fade (`GetScreenFade()`); no `StartCameraFade` | APPLIED (CS-R15) |
| ADR-0015 / ADR-0018 / ADR-0019 sync 2026-10-24 | View-target setter accepts only manager-requested targets; factory-initiated unpossess keeps the view target | APPLIED (CS-R1 view target bullet) |

## Acceptance Criteria

Tags:
- **[U]** automation or unit test (runs `-nullrhi` and asserts on state);
- **[P]** PIE or functional test map;
- **[N]** 2-client PIE (listen server + 1 client unless stated);
- **[E]** needs a retained screenshot in `production/qa/evidence/`;
- **[PT]** playtest with the stated sample and analysis.

Test names take the form `StealthGame.Camera.<Scenario>`; tests live in `Source/StealthGameplay/Private/Tests/Camera/`, multi-client tests in `Source/StealthTestHarness/Private/` (ADR-0007/0008). Every criterion uses a fixed dt of 1/60 and lets the rig converge for 30 frames before asserting. Input sequences are injected through the Look axis after sensitivity. *qa-lead reviewed these criteria at design review (2026-10-24).*

**A. Never observable (CS-R1, CS-R2)**
- **AC-CS-01** [N] `ModeNotObservable`:
  - **Given** two clients, a fixed `RoundSeed` and NPC stream, and a scripted 60 s input track for client A (walk, look, strike a dummy).
  - **When** the track is run once in 1P and once in 3P, with 20 toggles, 10 shoulder swaps and 3 FOV changes added in the 3P run.
  - **Then:**
    - The `ServerMove` payload bytes from A are identical between the runs, frame for frame.
    - Client B's recorded replicated properties, pose, animation tier and step events for A's pawn are identical.
    - Zero RPCs other than movement are sent by A's camera code.
- **AC-CS-02** [N] `NoCameraComponentsHostParity`:
  - **Given** the listen-server host in 1P and a client.
  - **When** the client compares component lists on the server, owner and simulated proxy.
  - **Then** all are identical, and the Shared Pawn class has no camera or boom component (IVP L17).
  - **And** the host pawn's head is visible on the client, with root-motion end positions within 1 cm of a 3P-host run.
- **AC-CS-27** [N] `NoServerCameraTraffic`:
  - **Given** a client in 3P that toggles, swaps shoulder and changes FOV for 60 s.
  - **When** the network is traced on both ends.
  - **Then** zero `ServerUpdateCamera` (or equivalent lens-transform) calls are sent, and the server-side PlayerCameraManager holds no lens transform from the client **[verify 5.8 names]**.
- **AC-CS-28** [N] `HostMaskedSimParity`:
  - **Given** the listen-server host in 3P, with NPC dummy A masked behind a wall and mirrored dummy B in view, both on the same scripted path with a strike montage and footsteps.
  - **Then** on the server, A and B have identical pose-tick counts, root-motion displacement (within 0.1 cm) and notify sequences (footsteps, strike windows).
  - **And** a client observing both receives identical data.
  - **And** "not rendered" for IIF R10 and SCL R11 equals the mask's α = 0 flag on every frame.

**B. View envelope (CS-R3, F1)**
- **AC-CS-03** [U] `EnvelopeClamp`: idle body facing 0°, run in 1P and in 3P.
  - **Given** continuous outward yaw input at 90 °/s for 2 s, and continuous pitch input at 60 °/s for 2 s down, then 2 s up.
  - **Then** view yaw ≤ 70.0°, and view pitch stays within −60.0° and +40.0°.
  - **And** the target yaw is ≤ 75.0° and exceeds 70° while input continues, and SCL turns the body after 0.5 s beyond the limit.
  - **And** the replicated head pitch observed on a second client stays within ±21.5°.
  - **And** when input stops before 0.5 s, the target relaxes to 70° within 0.15 s + 0.09 s and the body does not turn.
- **AC-CS-04** [U] `SoftEdgeNoDeadZone`:
  - **Given** the F1 gain.
  - **When** it is evaluated at |v| = 60, 65 and 70.
  - **Then** the gain is 1.0, 0.5 and 0.1.
  - **And** from a held 75° target, one frame of inward input moves the view inward on that same frame.
  - **And** at 180 °/s raw outward input, the view reaches 70° within 0.25 s of crossing 60°.
- **AC-CS-38** [U] `MouseLatchedPush`:
  - **Given** a mouse-style input at the yaw limit: 0.08 s bursts at 200 °/s separated by 0.12 s gaps, for 1.5 s.
  - **Then** `pushing` stays true throughout, the target stays above 70°, and SCL turns the body after 0.5 s.
  - **And** with 0.2 s gaps, the target relaxes in each gap and the body does not turn.
- **AC-CS-35** [P] `ConcealedNoTurnInPlace`:
  - **Given** a concealed owner.
  - **When** AC-CS-03's yaw input is injected.
  - **Then** the target never exceeds 70.0°, and server body yaw changes by 0°.

**C. The LOS rule (CS-R5, CS-R6, F2, F3)**
- **AC-CS-05** [P] `LOSMultiPosition` (the core test):
  - **Map:** a functional test map with an L-corner, a T-junction, a pillar, a door frame, a 100 cm wall, a wall whose top lies between eye height and eye + 25 cm, a 120 cm alley, a dead end, a 5 cm fence, a window slit and a bush.
  - **Dummies:** 24 dummy pawns (12 NPC-controlled, 12 bound to fake participants) on rings and behind each occluder.
  - **Sampling:** the player stands at ≥ 40 positions (corner-adjacent, 5 cm from walls, alley centres) and sweeps yaw in 15° steps across the envelope at pitch −60, −20, 0, +20 and +40, in 3P on both shoulders. Each sample is taken after the rig and mask settle (30 frames, past the N_h hysteresis).
  - **Oracle:** 25 points per dummy, a superset of the 5 mask probes, traced from the real eye socket on `ViewBlock` and restricted to the eye cone.
  - **Assertions at every sample:**
    1. The 2 cm guard ray from the pivot to the lens is unblocked.
    2. The 14 cm sphere does not overlap geometry.
    3. For each dummy, mask-visible (α > 0) implies that the oracle sees at least one point: **zero false reveals**.
    4. False hides (α = 0 while the oracle sees ≥ 5 of 25 points) are ≤ 5 % of samples.
- **AC-CS-06** [U] `BoomRecovery`:
  - **Given** F2 with the default keys.
  - **When** the free length drops from 186 to 52 cm and the obstacle is then removed.
  - **Then** the lens is at 52 cm in the same frame, holds for 0.15 s, reaches ≥ 179.3 cm (95 % of the gap) by 1.15 s, and never exceeds 300 cm/s.
- **AC-CS-07** [U] `MaskParityAndInputs`:
  - **Given** an NPC-controlled dummy and a participant-bound dummy mirrored behind the same wall.
  - **When** the player walks past the corner.
  - **Then** their α curves are identical frame for frame.
  - **And** a static scan finds no read of team, binding, Roster state or controller class in the mask or rig code.
- **AC-CS-08** [U] `MaskTiming`: given a near dummy, with async trace results landing one frame after the test.
  - **When** its probe first passes, **then** the fade-in starts on the frame the result lands and completes 0.10 s later.
  - **When** it fails, **then** the fade-out starts after the 2nd consecutive failed result (≤ 50 ms).
- **AC-CS-09** [P][E] `MaskedCastsNothing`:
  - **Given** a masked dummy in sunlight whose shadow would fall into the 3P frame, a reflective floor in frame, and TSR jitter off.
  - **When** the frame is captured, and captured again with the dummy deleted.
  - **Then** the maximum per-channel difference between the two captures is ≤ 2/255.
  - Both captures are retained.
- **AC-CS-11** [P] `ConcealedView`:
  - **Given** the owner concealed in a bush, in 3P.
  - **Then** the free length is ≤ 60 cm with shoulder and height offsets of 0, and the lens is inside the dome at every sample of a full Look sweep.
  - **And** the set of mask-visible dummies equals the AC-CS-05 oracle's set for the same pivot (at most the 5 % false-hide budget).
- **AC-CS-12** [P] `AudioListenerAtEye`: given 3P with the lens at 186 cm, when the listener transform is read, then its position equals the pivot within 1 cm and its rotation equals the view rotation.
- **AC-CS-13** [P] `MaskPerf`:
  - **Given** 49 dummies within 20 m, all inside the eye cone and behind walls (5 traces each), on a named RTX 3060-class reference machine (CPU recorded in the evidence file) at 1080p.
  - **When** the mask runs for 60 s.
  - **Then** its mean game-thread cost is ≤ 0.5 ms and its p99 per-frame cost is ≤ 1.0 ms.
  - A failure goes to technical-director (OQ10). It is never fixed by dropping the line-of-sight test.
- **AC-CS-36** [P] `BeyondRangeLOS`:
  - **Given** dummies at 70 m and 90 m, one visible from the eye and one behind a wall.
  - **Then** the visible one fades in within 0.32 s of becoming visible. The occluded one is never drawn over 60 s of yaw sweeps.
- **AC-CS-29** [U] `PivotCapAndCorrection`:
  - **Given** a 150 cm drop and a 120 cm climb.
  - **Then** the pivot's vertical speed never exceeds 300 cm/s, and the mask origin is never above the real eye socket.
  - **And** for an injected owner correction below `PawnSmoothSnapThresholdCm`, the pivot moves smoothly (no step > 5 cm per frame). Above the threshold, it snaps with the body on the same frame.
- **AC-CS-30** [U] `SqueezeAndSwap`:
  - **Given** a wall 30 cm from the right shoulder: the offset reaches 0 within 0.15 s and returns within 0.40 s after clearing, with no automatic flip.
  - **And** a shoulder swap mirrors the offset over 0.25 s. A second press during the blend is buffered and runs once. The 2 cm guard ray stays clear on every frame of the blend.

**D. No identification aid (CS-R9, F6)**
- **AC-CS-14** [U] `NoAimAssist`: given an identical injected Look input sequence, when it is replayed once with a dummy under screen centre and once with none, then the view rotation trajectories are identical to 0.01°.
- **AC-CS-15** [U] `StickCurve`: given F6 with the defaults:
  - at pure-right deflection 0.565, 0.10 and 1.0, the yaw rate is 63.6 ± 0.5 °/s, 0 and 180 °/s;
  - at a diagonal of magnitude 0.565, the rates are 45.0 ± 0.5 °/s yaw and 30.0 ± 0.5 °/s pitch.
- **AC-CS-10** [P] `AimRayCorner`:
  - **Given** a dummy within 2 m, visible from the 3P lens but not from the pivot.
  - **When** Attack is pressed with the crosshair on it.
  - **Then** the command carries `TargetSpot` and no `PawnRef`, and no Strike hint shows.
  - **And** in 1P at the same spot the result is identical.
- **AC-CS-34** [P] `WorldPointIntoBush`:
  - **Given** the crosshair on an occupied bush and on an empty bush, in 1P and in 3P.
  - **Then** the Attack's `WorldPoint` is the ray's `HideVolume` entry point projected to the ground, within 5 cm, in all four cases.
  - **And** the client presentation is identical between occupied and empty.

**E. Modes, toggle, FOV (CS-R7, CS-R10, CS-R13, F4, F5)**
- **AC-CS-16** [U] `ToggleBlend`:
  - **Given** 3P → 1P, when V is pressed, then the blend completes in 0.20 s (12 frames at 60 fps).
  - The head is hidden from frame 10 (u ≥ 0.85), and at no frame does the near plane intersect the own head bounds.
  - A second press at 0.25 s is ignored; one at 0.31 s is accepted.
  - With `CameraToggleLockoutSec` set to 0.15, the blend is clamped to ≤ 0.133 s.
- **AC-CS-17** [P][E] `FirstPersonHeadHidden`:
  - **Given** 1P looking down at −20° and at −60°.
  - **When** captured locally and on an observer.
  - **Then** the owner sees the body without a head (feet visible at −60°). The observer sees the full model with the head pitched no lower than −21.5°.
  - All screenshots are retained.
- **AC-CS-18** [U] `FovCap`:
  - **Given** aspects 16:9, 21:9 and 32:9 at slider values of 75° and 60°, when F5 runs, then the horizontal FOV is ≤ 108° in every case.
  - At 32:9 and 60°, VFOV_eff is 42.3° ± 0.2.
  - The slider top is 75° at 16:9 and 61.1° ± 0.2 at 21:9, and the slider is disabled at 32:9.
- **AC-CS-31** [U] `FovEase`: given a mid-round FOV change from 60° to 70°, then the FOV eases over 0.15 s (instantly under Reduce camera motion), the mask cone is unchanged, and zero RPCs are sent.
- **AC-CS-32** [P] `ReduceMotion`:
  - **Given** Reduce camera motion on.
  - **Then** the toggle is a 0.08 s crossfade with no lens travel, there is no traversal pivot lag, and the shoulder swap and boom recovery are cuts.
  - **And** AC-CS-01 still passes with the setting on.

**F. No pawn, grades, informants (CS-R11, CS-R12, CS-R15)**
- **AC-CS-19** [N] `EliminationHoldAndRelease`:
  - **Given** a victim in 3P.
  - **When** they are assassinated, **then** the view stays on the own body without control from the local contact frame until contact + `DeathFallSec` (± 1 frame), then fades to black in 0.5 s. At no frame is the view origin more than 2 m from the victim's pivot.
  - **When** a player is exiled, **then** the view holds on the released body until HUD's card-entrance event (± 1 frame), then fades; **and** with the card suppressed, it fades `ExileCardTimeoutSec` after the owner's `StrikeLimit` state (Penalty AC-NKP-38).
  - **When** a player's informant body is killed, **then** the view stays on the body and the fade starts at `DeathServerTime + DeathFallSec` (± 1 frame) with no removal card (Penalty CR-18).
  - **And** on takeover, an observer sees 0° of head-yaw change on the adopted pawn at the switch.
- **AC-CS-33** [N] `GradeHooks`:
  - **Given** a kill, a reversal and a whiff.
  - **Then** `ApplyTransientGrade` runs on the participating players' cameras on their local contact frames only, never earlier, never on a bystander or ally, and never for the whiff (Assassination AC-ASN-23).
  - **And** on an informant takeover, `SetOwnerGrade(Informant)` is active on the cut frame at full strength (no ease-in).
- **AC-CS-20** [P] `NeutralView`:
  - **Given** `Preparing`, awaiting a pawn, and `Ended` without a pawn.
  - **Then** no world scene is rendered, only HUD plates, and `GetViewState()` returns `Neutral`.
  - **And** the PlayerController is in `NAME_Playing`, and the view target is a client-local, non-replicated camera actor (IVP R13).
- **AC-CS-37** [P] `ViewStateAndLayer`: given a live view, a `Dying` hold and the neutral view, then `GetViewState()` returns `Live`, `Live` and `Neutral`. During the fade, a teammate marker drawn at the same screen position is covered by the black, and a HUD plate is not. Two further cases (ADR-0015 §1, R-5): after a seamless travel and after a round reset, the first rendered frame is the neutral view (or the fade from it), never the default camera at the world origin or a stale pawn view.
- **AC-CS-21** [P] `InformantSameRules`: given an informant, when AC-CS-03 and AC-CS-05 are re-run, then the results are identical to an Active player's, and no free-camera input exists.

**G. Data, code and pillar checks**
- **AC-CS-22** [U] `KeysRegistered`: given the TDC schema, when the CR-15 lint runs, then every Camera key exists with its audience and latch, and invariants I-CS-1 to I-CS-5 are enforced.
- **AC-CS-39** [U] `PresentationIncludeCheck` (IVP AC-IVP-17):
  - **Given** the camera module, when the include and grep check runs, then it includes no server query header.
  - It calls `GetPlayerState`, `GetController`, `IsPlayerControlled` or `GetOwner` only on the local controller's own possessed pawn.
- **AC-CS-23** [P] `TellMatrixRow` (with Crowd):
  - **Given** the Baseline and Tell Matrix.
  - **When** it is reviewed.
  - **Then** it has a "camera mode / toggle" row reading "no visible deviation", pointing to Camera.
  - **And** Crowd's virtual look calls `EvaluateViewEnvelope`.
- **AC-CS-24** [PT] `ScoutParity`:
  - **Given** the graybox corner scenario, with 12 or more testers each playing both modes in counterbalanced order (paired design).
  - **When** they report a walker approaching from behind a corner.
  - **Then** the paired difference in detection time (3P minus 1P) is not significantly negative (one-sided Wilcoxon signed-rank, α = 0.05).
  - With fewer than 12 testers the result is advisory only.
- **AC-CS-26** [PT] `ReadParity` (two-sided):
  - **Given** 12 testers split between 1P and 3P and swapped after one session, playing the graybox 5v5 for at least 6 rounds per mode.
  - **When** correct identifications of enemy players and strikes on wrong targets (NPC kills plus reversed strikes) per round are compared.
  - **Then** the two modes differ by ≤ 15 % relative on both measures, **in either direction**.
  - **If 3P is ahead by more**, the OQ2 fallback and the OQ5 partial-masking fix are triggered.
  - **If 1P is ahead by more**, the mask's false-hide budget and neighbour visibility are reviewed, and changing the default mode goes to the user (OQ11).
  - The run also logs the number of visible neighbours within 3 m in the 3P frame versus 1P.
- **AC-CS-25** [PT] `OwnPawnFinding` (Character Pool OQ-6):
  - **Given** 6 or more new players in 3P in a crowd of same-card pawns, over at least 20 events in total (spawn, takeover, leaving an occluder, and returning from a look beyond +10° pitch).
  - **When** each player locates their own pawn by pointing on screen.
  - **Then** the 90th-percentile time is ≤ 3 s and the median is ≤ 2 s, and no own-pawn decoration exists.

## Open Questions

| # | Question | Owner | Resolve by |
|---|---|---|---|
| OQ1 | **Closed (CD-GDD-ALIGN 2026-10-24):** the strict mask is confirmed. A peripheral allowance would let 3P see more than 1P. | creative-director | closed |
| OQ2 | Glance asymmetry (CS-R3.5): the camera leads the head, so a glance under 0.2 s shows observers only a small twitch. Accepted for the MVP (CD ruling). Log glance counts under 0.2 s in the graybox playtest. The fallback `ViewHeadLeadMaxDeg` and its trigger are written in CS-R3.5. | game-designer | graybox playtest |
| OQ3 | Turn cost and comfort: a walking 180° look takes about 1.8 s and an idle look-back 1.1-1.4 s. ux-designer asks for an SCL `PivotTurnRateDegPerSec` ≥ 150 °/s (currently 120; the run turn rate of 180 °/s already meets the ≥ 90 request). Add telemetry on edge-push events per minute. Also watch the body-driven view turn while steering (CS-R3.3) for comfort. | SCL owner + creative-director | graybox playtest |
| OQ4 | -> ADR: the rig. Options: (C, recommended) a custom PlayerCameraManager with no pawn components, engine camera-update RPCs disabled and manual view-target management; (B) SpringArm with its origin at the eye and `TargetOffset`; (A) the stock SpringArm with `SocketOffset`, rejected because it leaves through walls sideways; or the Gameplay Cameras plugin used by the Game Animation Sample (Experimental in 5.8). | technical-director | before the first camera story |
| OQ5 | -> ADR / spike: per-viewer mask rendering. Covers per-pawn dither cost with masked blend on 50 pawns (custom primitive data or per-pawn material instance), `SetVisibility` at α = 0 versus shadow and ray-tracing state, virtual shadow map invalidation, and TSR, Lumen and MegaLights behaviour with dither **[verify]**. Also costs partial masking for the whole-pawn-reveal residual (CS-R5b). | technical-artist + gameplay-programmer | before the mask story |
| OQ6 | Server-side view check or an Iris dynamic filter applied identically to all pawns (CS-R5c). | Anti-Cheat GDD / technical-director | Alpha |
| OQ7 | **Closed (Concealment CON-R7, 2026-10-24):** a concealed hider's body yaw is locked; Look is free within the head clamp; the camera's edge push is off while concealed (CS-R3.2, CS-R11.3). | Concealment GDD | closed |
| OQ8 | **Closed (Assassination ASN-R16, 2026-10-24):** `StrikeWarpYawLimitDeg` is 30°. | Assassination GDD | closed |
| OQ9 | **User to confirm:** one view and aim pitch range of −60°/+40° in **both** modes (creative-director ruling at design review, extending SCL D6 from first person only). Applied provisionally. The alternative of −60°/+40° in 1P with ±20° in 3P was rejected because it makes the camera choice a gameplay choice (CS-R2). | user (creative-director recommends) | before the first camera story |
| OQ10 | Mask trace budget: worst case 245 traces per frame, about 1.2 ms at 5 µs per trace, against the 0.5 ms target (F3, AC-CS-13). The options are time-slicing the near band, fewer probes at distance, or GPU queries. The line-of-sight test is never dropped, and the 3P reveal must not be noticeably worse than 1P occlusion culling. | technical-director | mask spike (OQ5) |
| OQ11 | **User decision, only if triggered:** if AC-CS-26 shows 1P reading better than 3P by more than 15 %, should the default mode change from `ThirdPerson` to `FirstPerson`? | user | after the graybox read-parity playtest |
| OQ12 | **Pending user decision (scope):** a tap or hold toggle option, and a left-handed default shoulder. Recommended by ux-designer and allowed by the creative director as settings, but **not applied** because they add options to the MVP (always-ask). | user | Settings & Accessibility (VS) or earlier |
