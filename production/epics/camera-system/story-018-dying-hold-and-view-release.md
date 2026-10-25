# Story 018: Dying hold and ViewRelease

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (with ADR-0018 for `OnRoleChangePresented` and ADR-0002 for `EstimatedServerNow()`)
**ADR Decision Summary**: `Dying` is entered from the client-local `OnRoleChangePresented` dispatch at the own local contact frame, with the concealed-input hold; the fade to black falls due on the client-local `ViewRelease(Reason, DelaySec)`: `Assassinated`/`ReversedAgainst` after `DeathFallSec`; `StrikeLimit` on HUD's card entrance or `ExileCardTimeoutSec`; `InformantKilled` at `DeathServerTime + DeathFallSec`. All timing comes from owner-audience data and `EstimatedServerNow()`; nothing is keyed to the server's binding retirement. A factory-initiated unpossess keeps the released body as the view target until the manager's own fade completes and it cuts to the neutral view.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; the logic is timers on top of stories 004 and 017)
**Engine Notes**: Relies on `AStealthPlayerController::OnUnPossess` skipping the engine's `SetViewTarget(this)` for factory unpossesses (story 004) and on `ReleaseBody` (ADR-0010 §3.2b). No ADR-0015 verification item gates it. AC4 depends on the takeover path of ADR-0010 and the head-look rule of SCL.

**Control Manifest Rules (this layer)**:
- Required: `ViewRelease` is computed on the owner's client from Owner-audience data and HUD's client-local card-entrance event; timing uses `EstimatedServerNow()` — source: ADR-0015, ADR-0002
- Forbidden: No death cam, kill cam, killer follow, replay or free camera; no visual effect keyed to the server's binding retirement — source: ADR-0015
- Guardrail: Timers are client-local; no RPC

The elimination timeline the player sees: the camera stays on the own falling body without control, then fades to black over `NoPawnFadeSec` (0.5 s), then the neutral view. The four cases differ only in when the fade falls due.

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-19, split by case), scoped to this story:*

- [ ] **AC1** (AC-CS-19 assassinated/reversed, [N] `EliminationHoldAndRelease`): **GIVEN** a victim in 3P **WHEN** they are assassinated **THEN** the view stays on the own body without control from the local contact frame until contact + `DeathFallSec` (± 1 frame), then fades to black in 0.5 s; at no frame is the view origin more than 2 m from the victim's pivot.
- [ ] **AC2** (AC-CS-19 exile): **GIVEN** a player is exiled **THEN** the view holds on the released body until HUD's card-entrance event (± 1 frame), then fades; **AND** with the card suppressed it fades `ExileCardTimeoutSec` after the owner's `StrikeLimit` state (Penalty AC-NKP-38).
- [ ] **AC3** (AC-CS-19 informant killed): **GIVEN** a player's informant body is killed **THEN** the view stays on the body and the fade starts at `DeathServerTime + DeathFallSec` (± 1 frame) with no removal card (Penalty CR-18).
- [ ] **AC4** (AC-CS-19 takeover): **GIVEN** a takeover **THEN** an observer sees 0° of head-yaw change on the adopted pawn at the switch.

---

## Implementation Notes

*Derived from ADR-0015 Decision §1 (factory-initiated unpossess) and §6 `Dying`, GDD CS-R12.1:*

Concealed input hold (cross-review S-W2): from the frame the strike on the own pawn is known locally until `Dying` ends, the camera keeps the own `IsConcealed` value it held before that strike, and a change received meanwhile is ignored; Concealment CON-R9 keeps the flag true until body removal for a victim killed in a bush. `ViewRelease` event sources: Assassination's `OnRoleChangePresented` and `DeathFallSec` (assassination-defence-reversal), Penalty's `ExileBeatSec`/`ExileCardTimeoutSec` (npc-kill-penalty-informants), HUD's card-entrance event (hud). Control is not given back during `Dying`; the transient grade (story 019) runs on the same contact frame. After the fade completes the manager cuts to the neutral view (story 017), which stays until the next binding. AC4: on takeover the camera cuts to the new pawn with control rotation from its `HeadLook`, so the adopted head does not turn; the observer-side check reads replicated head-look yaw at the switch frame.

Fixed 1/60 s frames; "± 1 frame" tolerances in the GDD apply. Tests take times from the injected clock, not wall time.

Prototype replaced: none (the prototype has no elimination camera).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-017-view-state-neutral-view-fade.md`: the neutral view, fade publication, the cut to a pawn.
- `story-019-grade-hooks.md`: the transient grade at the contact frame.
- assassination-defence-reversal / npc-kill-penalty-informants / hud epics: the events and delays they own.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthViewRelease.h` (`EViewReleaseReason`, `ViewRelease` event)
- `Source/StealthGameplay/Private/Core/Camera/StealthPlayerCameraManager.cpp` (`Dying` state, hold, release timers)
- `Source/StealthTestHarness/Private/Camera/EliminationHoldAndReleaseTest.cpp` (CQTest, `StealthGame.Camera.PIE.EliminationHoldAndRelease.*`, one method per AC)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: ADEQUATE

Test: AC1 `DyingHoldAssassinated` (AC-CS-19)
  Given: a victim in 3P and a fake local contact-frame dispatch.
  When: the assassination contact frame arrives.
  Then: input is ignored from the contact frame; the view stays on the own body until contact + `DeathFallSec` (±1 frame), then fades to black in 0.5 s; view origin is ≤ 2 m from the victim pivot on every frame.
  Edge cases: the reversed case uses the same timing.

Test: AC2 `DyingHoldExile`
  Given: an exile (StrikeLimit) with a fake HUD card-entrance event.
  When: the card event arrives; separately, the card is suppressed.
  Then: the hold lasts until the card-entrance event (±1 frame), then fades; with the card suppressed the fade starts at `StrikeLimit` + `ExileCardTimeoutSec`.
  Edge cases: the card event arriving before the timeout.

Test: AC3 `DyingHoldInformantKilled`
  Given: an informant body killed, with an injected clock offset between local and `EstimatedServerNow()`.
  When: the death arrives.
  Then: the fade starts at `DeathServerTime + DeathFallSec` (±1 frame) measured on `EstimatedServerNow()`; no removal card is requested.
  Edge cases: a nonzero server clock offset.

Test: AC4 `PIE.TakeoverNoHeadYawChange`
  Given: a takeover of a crowd pawn in 2-client PIE.
  When: the camera cuts to the adopted pawn.
  Then: client B sees 0° of head-yaw change on the adopted pawn at the switch; the control rotation was initialised from `HeadLook`.
  Edge cases: none.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `EliminationHoldAndReleaseTest.cpp` under `Source/StealthTestHarness/Private/Camera/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-014-concealed-and-informant-view.md`, `story-017-view-state-neutral-view-fade.md`; cross-epic: assassination-defence-reversal (`OnRoleChangePresented`, `DeathFallSec`), npc-kill-penalty-informants (`StrikeLimit`, `DeathServerTime`, informant death), hud (card-entrance event), shared-character-locomotion (`ReleaseBody`, `HeadLook`)
- Unlocks: `production/epics/camera-system/story-019-grade-hooks.md`
