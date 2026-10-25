# Story 020: Audio listener at the eye

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope
**ADR Decision Summary**: `AStealthPlayerController::GetAudioListenerPosition` (virtual) is overridden to return the eye pivot and the view rotation in both modes, so what the player hears matches what the body hears, not the lens at the end of the boom. Fallback if the override is not honoured: `SetAudioListenerOverride` per frame. `EFirstPersonPrimitiveType` is not used.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; this path is pre-cutoff and independent of the audio backend)
**Engine Notes**: Verified in 5.8.4: the viewport reads the virtual `GetAudioListenerPosition` before any audio backend (`GameViewportClient.cpp:1883`), `PlayerController.h:1951, 1964-1970` for `SetAudioListenerOverride`/`ClearAudioListenerOverride`. 5.8 changed the default Windows audio backend to WASAPI; the override sits above the backend, but confirm with ADR-0015 Risk R-6's scripted test. ADR-0015 verification item (6) is settled here.

**Control Manifest Rules (this layer)**:
- Required: The listener is at the eye pivot with the view rotation in both modes — source: ADR-0015 (CS-R5d)
- Forbidden: None specific
- Guardrail: Zero per-frame cost beyond one vector return

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-12) and ADR-0015 §7.3 / R-6 (derived item marked), scoped to this story:*

- [ ] **AC1** (AC-CS-12, [P] `AudioListenerAtEye`): **GIVEN** 3P with the lens at 186 cm **WHEN** the listener transform is read **THEN** its position equals the pivot within 1 cm and its rotation equals the view rotation.
- [ ] **AC2** (derived, ADR-0015 §7.3, R-6): **GIVEN** a scripted sound source behind a corner **THEN** it is attenuated as heard from the eye pivot and not from the lens, in both 1P and 3P.

---

## Implementation Notes

*Derived from ADR-0015 Decision §7.3, Risk R-6:*

Override `AStealthPlayerController::GetAudioListenerPosition(FVector& Location, FVector& FrontDir, FVector& RightDir)` to return the manager's pivot and view rotation. Fallback only if AC2 shows the override is ignored: `SetAudioListenerOverride(SceneComponent, Location, Rotation)` each frame. The scripted source for AC2 uses a stock attenuation with occlusion enabled (or a distance-based assertion with a known wall position), and the test reads the listener transform the audio device receives (`FAudioDevice::GetListeners`), not just the controller's return value.

Prototype replaced: none (the prototype listener is the camera lens).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-006-eye-pivot-boom-collision.md`: the pivot value.
- Audio fades (use the published fade alpha, story 017): any audio-fade implementation belongs to audio work.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Private/StealthPlayerController.cpp` (`GetAudioListenerPosition` override; production controller per composition-root story)
- `Source/StealthGameplay/Private/Tests/Camera/AudioListenerAtEyeTest.cpp` (functional test, name `StealthGame.Camera.AudioListenerAtEye`)
- `Source/StealthGameplay/Private/Tests/Camera/ListenerOcclusionTest.cpp` (functional test, class `FCameraListenerOcclusionTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC2: "attenuated as heard from the eye pivot and not from the lens" has no measure, and a tester cannot judge it by ear. QA-lead proposed rewrite (unconfirmed): assert the listener transform used by the occlusion check equals the pivot; a source behind a corner that is blocked from the pivot but clear from the lens must report the occluded attenuation state, and the reverse case must report clear. This needs a real audio device, not `-nosound`.
- AC1 sketch: 3P with the lens at 186 cm, listener position within 1 cm of the pivot and rotation equal to the view rotation; also 1P, a squeezed boom (52 cm) and mid-toggle blend.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two functional tests above under `Source/StealthGameplay/Private/Tests/Camera/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-004-camera-manager-shell-remote-pov.md`, `story-006-eye-pivot-boom-collision.md`
- Unlocks: None
