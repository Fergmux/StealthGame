# Story 020: One fall, one linger, one cut-at-occlusion removal, for every victim

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R17; VA-ASN-1 `Death_Fall`, VA-ASN-2, VA-ASN-6; AC-ASN-22)
**Requirement**: `TR-asn-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§5.1 fall, §6), with ADR-0013 (one `Death_Fall` montage in the shared AnimBP)
**ADR Decision Summary**: A valid `FDeathState` starts the one `Death_Fall` montage at `DeathServerTime` (seeked when late); pawn collision goes off client-locally at `DeathServerTime + DeathFallSec`; the body is hidden by VA-ASN-2's occlusion rule from `DeathServerTime + DeathFallSec + BodyLingerSec`; an actor first received after the linger is never shown.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (animation HIGH risk 5.4-5.8; the occlusion test is a project-owned visibility check)
**Engine Notes**: Inertialised entry (≤ 0.15 s), no ragdoll, no physics impulse (Pillar 1: nothing differs by kind). The occlusion test reads only geometry and time: bounds outside the viewer's frustum or fully hidden by world geometry (pawns are not occluders), applied with Concealment VA-4's flag set (mesh, hair, garments, props, shadow, reflections; `bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden`, `bRayTracingFarField` are class-default false, never toggled at runtime). If the viewer is still looking when `BodyRemovalGraceSec` runs out the body is cut in a single frame. Clips come from the shared Game Animation Sample / retargeted assets created in the editor; sample assets stay unmodified.

**Control Manifest Rules (this layer)**:
- Required: One `Death_Fall` clip for every victim, whether standing, walking, running, traversing, striking, bracing or countering; one removal method — source: ADR-0018, ADR-0013, GDD ASN-R17
- Forbidden: No particle, decal, blood, light, emissive or material change on the strike, fall or body; no per-kind MID, LOD or animation-rate difference — source: GDD VA-ASN-6, ADR-0013
- Required: Presentation hides and shows bodies only through the shared evaluate-hidden path from `EstimatedServerNow()`; clips seeked, never restarted — source: ADR-0017
- Guardrail: Body hide/show rebuilds the render proxy once per body

`Death_Fall`: 1.2 s, first 0.15 s a recoil that reads "struck", on the ground by about 0.8 s; a victim off the ground lands on the floor under its capsule within the clip. Linger: `BodyLingerSec` (6.0 s), a static final pose with no breathing, cloth or IK settle. Removal: from `DeathServerTime + DeathFallSec + BodyLingerSec` each client hides the body on the first frame it is out of that viewer's frustum or fully occluded; if still looking at `BodyRemovalGraceSec` expiry the body is cut in one frame with no fade, dissolve, sink or effect. A body blocks no walking pawn after the fall.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-22), scoped to this story:*

- [ ] **AC1** (AC-ASN-22, [Visual/BLOCKING] `StealthGame.Assassination.OneBodySequence`): **GIVEN** an NPC, a player and an informant victim **THEN** each plays `Death_Fall` (1.2 s), lingers 6.0 s and is removed by VA-ASN-2, with identical timings (± 1 frame); a body still in view at grace expiry is cut in one frame with no fade or effect.
- [ ] **AC2** (AC-ASN-22 continued, [Visual/BLOCKING]): **GIVEN** the same three bodies **THEN** no body blocks a walking pawn after the fall, and no particle, decal or material change occurs on any of them at any time; evidence `asn-22-*.png` (retained captures at contact, 0.8 s, 1.2 s, linger end and removal cut for each of the three kinds, same camera).

---

## Implementation Notes

*Derived from ADR-0018 §5.1, §6.1 and GDD VA-ASN-1, VA-ASN-2:*

Client side of the body sequence (the server timers are story 019): at `DeathServerTime + DeathFallSec` the client turns collision off pawn channels locally (matching the server's release); the hide evaluation runs per frame from `EstimatedServerNow()` while the body is within its removal window; the rule reads only geometry and time, so even a seen cut reveals nothing about identity. Occlusion queries must use a project-owned trace channel setup that does not leak into gameplay (reuse Camera's `ViewBlock` channel conventions, Camera story 007). Pawns are not occluders. A client that first receives the actor after the linger never shows it (story 022 covers the late-actor path; this story provides the evaluator it calls).

Evidence procedure: `.claude/docs/run-and-observe.md` for Unreal; a lead sign-off line goes in `production/qa/evidence/asn-22-evidence.md`. The timing comparison (± 1 frame) can be taken from the presentation log of story 017's hook rather than by eye.

Art status: the `Death_Fall` asset and its AnimBP state are asset work (see `/asset-spec system:assassination-defence-reversal` once the art bible is approved); until then the story uses a retargeted sample clip as the stand-in, and the timings are the contract.

**Prototype replaced:** `ApplyDeathPresentation` (ragdoll, physics impulse, one-second attacker hold) and the prototype's `BodyLifetime` destroy are replaced; known issue 5 in `production/first-playable-report.md` ("assassination presentation is a placeholder") is closed by this story together with story 018.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-server-body-sequence-and-reset.md`: server release, removal and `Reset`.
- `story-022-culled-victim-opens-falling-and-late-actors.md`: late and re-opened actors (BLOCKED).
- `story-023-concealed-body-stays-hidden.md`: killed-concealed bodies' permanent hide.
- `story-021-role-change-dispatch-and-transient-grade.md`: the own-pawn dying camera and grade.

---

## Files

- `Source/StealthGameplay/Private/Features/Assassination/EncounterPresentation.cpp` (fall branch of `Refresh`; collision-off timing; shared with story 018)
- `Source/StealthGameplay/Private/Features/Assassination/BodyRemovalEvaluator.h` and `.cpp` (cut-at-occlusion rule, plain C++ core plus a trace adapter)
- `Source/StealthGameplay/Private/Tests/Assassination/BodyRemovalEvaluatorTest.cpp` (class `FAssassinationBodyRemovalEvaluatorTest`: timing and one-frame-cut logic, headless)
- `production/qa/evidence/asn-22-*.png` and `production/qa/evidence/asn-22-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "± 1 frame" has no frame rate, `BodyRemovalGraceSec` has no value, "cut in one frame" is undefined, and "on the ground by about 0.8 s" has no tolerance; proposed rewrite (unconfirmed): at a fixed 60 fps capture, for each kind `Death_Fall` starts at `DeathServerTime` and runs 1.2 s, the body is static for `BodyLingerSec` (6.0 s), and removal starts at `DeathServerTime + 1.2 s + 6.0 s`; all three kinds are identical ± 1 frame (16.7 ms), read from story 017's presentation log; a body still in view at grace expiry (value from TDC) goes from fully visible to fully hidden between two consecutive frames with no intermediate opacity, material parameter or particle; the body is on the ground by 0.8 s ± 0.1 s.
- AC2: "blocks no walking pawn" and "no particle, decal or material change" have no check method; proposed rewrite (unconfirmed): a test pawn walking at default speed through the body's capsule volume after 1.2 s is displaced by its full intended path (± 1 cm) with no blocking hit; over the whole sequence the body has zero spawned particle or decal components and no MID or material-slot change versus frame 0.

- **AC-1**: One body sequence
  - Setup: three victims (NPC, player, informant) killed under the same camera at a fixed 60 fps with the presentation-log hook; an out-of-frustum run and a still-looking run
  - Verify: log timestamps for fall start, collision-off, linger end and hide for each kind; capture contact, 0.8 s, 1.2 s, linger end and removal cut
  - Pass condition: timings identical across kinds ± 1 frame; fall 1.2 s, linger 6.0 s; the still-looking body vanishes in one frame with no fade, dissolve, sink or effect; an out-of-view body is removed on the first frame it is out of frustum or occluded
- **AC-2**: No blocking, no effects
  - Setup: the same three bodies; a walking test pawn routed through each body after the fall; a component and material audit hook
  - Verify: the pawn's path is unobstructed; the audit lists particle, decal and MID components at each capture point
  - Pass condition: zero blocking hits; zero particle, decal or material changes at any time; `asn-22-*.png` retained for the five moments for each of the three kinds from the same camera; lead sign-off line present in `asn-22-evidence.md`

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures `production/qa/evidence/asn-22-*.png` and a sign-off in `production/qa/evidence/asn-22-evidence.md` — must exist (AC-ASN-22 is `[Visual/BLOCKING]`)
- Supporting: `BodyRemovalEvaluatorTest.cpp` (timing and cut logic)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-013-death-write-staging-and-after-latch.md`, `story-018-strike-clip-variants-warp-and-contact-scheduling.md`, `story-019-server-body-sequence-and-reset.md`; cross-epic `production/epics/shared-character-locomotion/story-041-body-and-anim-set-parity.md`; `production/epics/camera-system/story-007-viewblock-channel-collision-profiles.md`
- Unlocks: `story-022-culled-victim-opens-falling-and-late-actors.md`, `story-023-concealed-body-stays-hidden.md`, `story-025-reversal-readability-and-brace-tell-playtest.md`
