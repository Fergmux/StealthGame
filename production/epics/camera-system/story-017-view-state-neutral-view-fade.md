# Story 017: View state, neutral view and published fade

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (with ADR-0021 §3 for the fade layer and ADR-0001 §4.5 for the neutral view)
**ADR Decision Summary**: `GetViewState()` returns `Live` or `Neutral`. The neutral view is a client-local, non-replicated `AStealthNeutralViewActor` set as the local view target; the PlayerController stays in `NAME_Playing`; no spectator pawn, spectator RPC or other participant's pawn is ever the view target; the world is not rendered. Cuts to a pawn happen on the client's own possession acknowledgement in the remembered mode at full boom and pitch 0, with the control rotation initialised from `HeadLook`, and fade in over `NoPawnFadeSec` from the manager's published screen fade. The manager computes the fade alpha itself and publishes it through `IStealthCameraView::GetScreenFade() -> FStealthScreenFade {Alpha, Color}`; the HUD fade layer (ZOrder 20) draws it. `StartCameraFade` is not used.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0015 verification item (5) (the `SetViewTarget` override holds the neutral view across seamless travel and round reset) is settled by this story's AC2 cases; the override itself is story 004. `StartCameraFade` darkens only the scene and its `FadeAmount` animates only while the scene fade is on, which is why the published alpha is used (ADR-0021 §3). Draw order is ADR-0021's: the fade sits above Teammate Markers and HUD world-anchored layers, below HUD plates and menus.

**Control Manifest Rules (this layer)**:
- Required: Pawnless viewers use the client-local `AStealthNeutralViewActor`; the camera keeps all fade timing and publishes the alpha through `GetScreenFade` — source: ADR-0015, ADR-0021
- Forbidden: Never use spectator pawns, spectator state or spectator RPCs; never make another participant's pawn the view target; never use `StartCameraFade` for neutral, cut or elimination fades — source: ADR-0001 §4.5, ADR-0015
- Guardrail: Fade alpha is one float per frame

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-20, AC-CS-37) and ADR-0015 §6 (derived item marked), scoped to this story:*

- [ ] **AC1** (AC-CS-20, [P] `NeutralView`): **GIVEN** `Preparing`, awaiting a pawn, and `Ended` without a pawn **THEN** no world scene is rendered (only HUD plates), `GetViewState()` returns `Neutral`, the PlayerController is in `NAME_Playing`, and the view target is a client-local, non-replicated camera actor (IVP R13).
- [ ] **AC2** (AC-CS-37, [P] `ViewStateAndLayer`): **GIVEN** a live view, a `Dying` hold and the neutral view **THEN** `GetViewState()` returns `Live`, `Live` and `Neutral`; during the fade a teammate marker drawn at the same screen position is covered by the black and a HUD plate is not; and after a seamless travel and after a round reset the first rendered frame is the neutral view (or the fade from it), never the default camera at the world origin or a stale pawn view.
- [ ] **AC3** (derived, ADR-0015 §6, ADR-0021 §3): **GIVEN** any fade **THEN** the manager computes the alpha itself each frame and publishes it through `GetScreenFade()`, `StartCameraFade` is never called for neutral-view, cut or elimination fades, and any audio fade uses the same published alpha.

---

## Implementation Notes

*Derived from ADR-0015 Decision §6 and GDD CS-R12.2-4, CS-R15:*

States per the GDD table: `NoPawn` (neutral view), `FirstPerson`, `ThirdPerson`, `Blend`, `Dying`; this story adds `NoPawn` and the publication of `Live`/`Neutral`. The actor `AStealthNeutralViewActor` spawns locally with `bReplicates = false`. Cut on `AcknowledgePossession`/`OnRep_Pawn` of `AStealthPlayerController`, in the remembered mode (story 008), boom full, pitch 0; the control rotation starts at the pawn's `HeadLook` (SCL R4.2) so the head does not move. `Preparing` shows the neutral view behind the briefing screen until the first binding. `Ended` with a pawn keeps the camera while the body brakes (SCL R6). Never blend across the map (it would draw a path). Seamless travel and round reset: the `SetViewTarget` override (story 004) plus the manager's own view state produce a neutral first frame; the test drives `NotifyLoadedWorld`/`Reset`.

HUD side: the fade layer, markers and plates ordering is HUD's and Teammate Markers' (`hud`, `teammate-markers` epics); this story asserts the observable layer order only with the real HUD present, so the PIE test depends on those stories.

Prototype replaced: the prototype's overhead `StealthDebugCam` (`StealthPlayerController.cpp:101-142`, `SetViewTarget(DebugCamera)`) is the only existing "no pawn" view; retired in story 025.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-018-dying-hold-and-view-release.md`: the `Dying` hold and when the fade is triggered.
- `story-004-camera-manager-shell-remote-pov.md`: the `SetViewTarget` override.
- hud / teammate-markers epics: fade layer rendering, marker layer.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthNeutralViewActor.h` and `Private/Core/Camera/StealthNeutralViewActor.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthCameraView.h` (`GetViewState`, `GetScreenFade`, `FStealthScreenFade`)
- `Source/StealthGameplay/Private/Core/Camera/StealthPlayerCameraManager.cpp` (state, fade computation, cut)
- `Source/StealthGameplay/Private/Tests/Camera/NeutralViewTest.cpp` (functional test, name `StealthGame.Camera.NeutralView`)
- `Source/StealthTestHarness/Private/Camera/ViewStateAndLayerTest.cpp` (CQTest, `StealthGame.Camera.PIE.ViewStateAndLayer.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC1: "no world scene is rendered" has no oracle. QA-lead proposed rewrite (unconfirmed): assert a render flag that disables world rendering (or published fade alpha 1, black), plus a retained UI screenshot per state showing HUD plates only.
- Scenario sketches: AC1 `GetViewState()` is `Neutral`, controller in `NAME_Playing`, the view target is `AStealthNeutralViewActor` with `bReplicates` false and absent on the server; another participant's pawn is never the view target. AC2 `GetViewState()` returns Live, Live (Dying hold) and Neutral; fade layer ZOrder 20 sits above the marker layer and below the plate layer (read the constants from ADR-0021), a screenshot shows the marker covered and the plate not; after seamless travel and after round reset the first rendered POV has the neutral actor as view target (not the world origin or a stale pawn) and the fade runs from the neutral state. AC3 `GetScreenFade` alpha is computed per frame and monotonic for the neutral, cut and elimination fades with colour black, `StartCameraFade` call count 0 and a grep of the module returns 0 hits.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `NeutralViewTest.cpp` and `ViewStateAndLayerTest.cpp` under the test roots above OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-004-camera-manager-shell-remote-pov.md`, `story-008-toggle-blend-reduce-motion-remembered-mode.md`; cross-epic: participant-state-roster (own binding presence), round-match-framework (`Preparing`, `Ended`), information-visibility-policy (IVP R13), hud (fade layer), teammate-markers (layer order)
- Unlocks: `production/epics/camera-system/story-018-dying-hold-and-view-release.md`, `story-019-grade-hooks.md`
