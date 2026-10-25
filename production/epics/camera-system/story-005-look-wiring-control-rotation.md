# Story 005: Look wiring into control rotation

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (with ADR-0011 §9 for the Look routing)
**ADR Decision Summary**: The controller's `IA_Look` handler passes the frame's raw look delta (after stick response and sensitivity, before gain) to `AStealthPlayerCameraManager::StepEnvelope`, which calls `EvaluateViewEnvelope`, keeps the view rotation for the lens and returns the control target; the handler then calls `UPawnCommandComponent::Look(r)`. `AStealthPlayerController::UpdateRotation` is overridden so it does not integrate `RotationInput`, making the envelope the only writer of a human's control rotation.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `APlayerController::UpdateRotation` is virtual and `ProcessViewRotation`'s default limiting is bypassed, with `ViewPitchMin/Max` set to the hard ranges as a backstop only (ADR-0015 §2, verified in `PlayerController.h`). Enhanced Input and Common UI were unified in 5.8: verify the `IA_Look` handler signature against 5.8.4 headers. No ADR-0015 verification item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove` (Look is `UPawnCommandComponent::Look`, never a queue item); the envelope is the only writer of a human's control rotation — source: ADR-0011, ADR-0015
- Forbidden: Never add a Look RPC or a Look event; no `AddYawInput`/`AddPitchInput` path — source: ADR-0011, ADR-0015
- Guardrail: Envelope and look wiring add no per-frame allocation

This story connects the pure envelope (002) and stick response (003) to the live controller, and closes the SCL-dependent half of AC-CS-03 (body turn after 0.5 s, replicated head pitch seen by an observer) plus the cases where the body turns without the view following (concealment, strike warp).

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-03 SCL-dependent bullets, AC-CS-35, AC-CS-40) and ADR-0015 §2 (derived), scoped to this story:*

- [ ] **AC1** (AC-CS-03, SCL-dependent bullets): **GIVEN** idle body facing 0° and continuous outward yaw input at 90 °/s **THEN** SCL turns the body after 0.5 s beyond the limit; **AND** the replicated head pitch observed on a second client stays within ±21.5°; **AND** when input stops before 0.5 s the body does not turn.
- [ ] **AC2** (AC-CS-35, [P] `ConcealedNoTurnInPlace`): **GIVEN** a concealed owner **WHEN** AC-CS-03's yaw input is injected **THEN** the target never exceeds 70.0° and server body yaw changes by 0°.
- [ ] **AC3** (AC-CS-40, [U] `StrikeWarpViewHold`): **GIVEN** view offset v = 0 and then v = +50°, Look idle, and a Strike whose root motion or warp turns the body by `StrikeWarpYawLimitDeg` (30°) **THEN** for v = 0 the view's world yaw is unchanged (±0.01°) on every frame, and for v = 50° it is unchanged until the body-relative offset would pass 70°, then follows the body with no step above one frame of body rotation; **AND** no camera shake, zoom or cut occurs.
- [ ] **AC4** (derived, ADR-0015 §2): **GIVEN** the production controller **WHEN** Look input arrives **THEN** `UpdateRotation` does not integrate `RotationInput` and no `AddYawInput`/`AddPitchInput` path writes control rotation; the envelope's returned target is the only value passed to `UPawnCommandComponent::Look`.

---

## Implementation Notes

*Derived from ADR-0015 Decision §2 and ADR-0011 §9:*

`StepEnvelope` is on the camera manager (client view code); the controller handler is in the composition-root controller. `ViewPitchMin/Max` are set to the hard ranges (-75/+60 outer limits from the keys' hard ranges) as a backstop only. After possession the control rotation is initialised from the pawn's `HeadLook` world rotation (manifest rule, ADR-0010), and the envelope state is reset from it so the view does not jump. AC1's SCL-dependent half uses SCL's `TurnInPlaceDelaySec` (0.5 s) and the replicated `RemoteViewPitch` clamp; it needs the body-turn and head-look stories in the SCL epic (`shared-character-locomotion/story-017-idle-turn-in-place.md`, `story-021-look-envelope-700-deg-spin.md`, `story-022-remote-view-pitch-clamped.md`) to be built, so the PIE test is written against their published interface.

The concealed case reads own `IsConcealed` (Owner audience) and sets overshoot and edge push to 0; the Concealment epic supplies the flag. The strike-warp case reads `StrikeWarpYawLimitDeg` (Assassination key) and holds the view's world yaw while root motion rotates the body: the view stores world yaw, and the body-relative offset `v` is recomputed each frame as `view − body`, clamped to the envelope edge.

Prototype replaced: unclamped look in `StealthCharacter.cpp` / `StealthPlayerController.cpp`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-002-view-envelope-pure-function.md`: the function and its numeric assertions.
- SCL body turn and head-look interpolation: shared-character-locomotion epic (this story only asserts the observable result).
- Concealed boom length and lens position: `story-014-concealed-and-informant-view.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/Camera/StealthPlayerCameraManager.cpp` (`StepEnvelope`)
- `Source/StealthGame/Private/StealthPlayerController.cpp` (`UpdateRotation` override, `IA_Look` handler)
- `Source/StealthTestHarness/Private/Camera/EnvelopeBodyTurnTest.cpp` (CQTest, `StealthGame.Camera.PIE.EnvelopeBodyTurn.*`)
- `Source/StealthGameplay/Private/Tests/Camera/ConcealedNoTurnInPlaceTest.cpp` (class `FCameraConcealedNoTurnInPlaceTest`, name `StealthGame.Camera.ConcealedNoTurnInPlace`)
- `Source/StealthGameplay/Private/Tests/Camera/StrikeWarpViewHoldTest.cpp` (class `FCameraStrikeWarpViewHoldTest`, name `StealthGame.Camera.StrikeWarpViewHold`)
- `Source/StealthGameplay/Private/Tests/Camera/NoRotationInputIntegrationTest.cpp` (class `FCameraNoRotationInputIntegrationTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: ADEQUATE

Test: AC1 `LookWiringBodyTurn` (AC-CS-03 SCL-dependent bullets)
  Given: 2-client PIE, body 0°, 90 °/s outward yaw for 2 s, and a pitch sweep of 60 °/s down then up.
  When: the input runs; separately, input stops before 0.5 s beyond the limit.
  Then: the server body turns only after 0.5 s beyond the limit; if input stops before 0.5 s, body yaw change is 0 ± 0.01°; head pitch seen on client B stays within ±21.5°.
  Edge cases: stop at 0.45 s and at 0.55 s.

Test: AC2 `ConcealedNoTurnInPlace` (AC-CS-35)
  Given: a concealed owner with the same yaw input.
  When: the input is injected.
  Then: the target never exceeds 70.0° and server body yaw changes by 0°.
  Edge cases: concealment starting mid-sweep (the spec is silent; the author states the expected target).

Test: AC3 `StrikeWarpViewHold` (AC-CS-40)
  Given: a strike warp of 30° (`StrikeWarpYawLimitDeg`) with view offset v = 0, then v = +50°.
  When: the strike runs with Look idle.
  Then: for v = 0 world view yaw is unchanged (±0.01°) on every frame; for v = 50° with the body turning away, view yaw holds until the offset would pass 70° (after 20° of body turn), then follows with a per-frame step no larger than one frame of body rotation; no shake, FOV change or cut.
  Edge cases: the GDD gives no warp direction, so test both; turning toward the view holds throughout.

Test: AC4 `UpdateRotationIgnoresRotationInput`
  Given: `AddYawInput(90)` and `AddPitchInput(30)` called on the controller.
  When: `UpdateRotation` runs.
  Then: control rotation is unchanged; only the `StepEnvelope` to `Look(r)` path changes it; `ViewPitchMin/Max` equal −60/+40.
  Edge cases: NPC controllers are unaffected.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the PIE and unit tests above under `Source/StealthTestHarness/Private/Camera/` and `Source/StealthGameplay/Private/Tests/Camera/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-002-view-envelope-pure-function.md`, `story-003-stick-look-response-no-aim-assist.md`, `story-004-camera-manager-shell-remote-pov.md`; cross-epic: input-interaction-framework (`UPawnCommandComponent::Look`, `IA_Look` binding), shared-character-locomotion (body turn, head look), concealment-bushes (`IsConcealed`), assassination-defence-reversal (`StrikeWarpYawLimitDeg`)
- Unlocks: `production/epics/camera-system/story-006-eye-pivot-boom-collision.md`, `story-021-camera-mode-not-observable.md`
