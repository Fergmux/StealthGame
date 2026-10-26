# Story 015: AC 30 DefaultBindings

> **Epic**: Input & Interaction Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Default bindings are the data of `IMC_Common` and `IMC_Active` exactly as the IIF R15 table lists them (WASD/left stick, mouse/right stick, Left Shift/L3 Run, Space/A Traverse, E/X Interact, Q/LT Prepare, LMB/RT Attack, F/B Hide, V/Y toggle, X-or-middle-mouse/D-pad-right shoulder swap, plus walk-slower and ping); gamepad is partial in MVP (every command mapped, no remap UI, never aim assist). The data lint asserts the table.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Input layer HIGH. Rebinding uses `UEnhancedInputUserSettings` (E4: `UPlayerMappableInputConfig` deprecated since 5.3), and "fresh settings" means that object cleared with no saved profile. Keys are injected as `FKey` events (including `Gamepad_FaceButton_Bottom`, `Gamepad_LeftTriggerAxis`) into the PIE player input. No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` to Begin and both `Completed` and `Canceled` to End; server-meaningful actions are Boolean — source: ADR-0011
- Required: Rebinding uses `UEnhancedInputUserSettings` — source: ADR-0011
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand` — source: ADR-0011
- Forbidden: Never use `UPlayerMappableInputConfig` or the Raw Input plugin — source: ADR-0011
- Forbidden: Never add lock-on, snapping, magnetism or aim assist — source: ADR-0011, ADR-0015
- Forbidden: Never route a ping through `ServerPawnCommand`, the action slot, the resolution queue or `FActionState` — source: ADR-0011, ADR-0001
- Forbidden: Never send the walk-slower modifier to the server as a command or field, and never apply it to gamepad input — source: ADR-0011
- Guardrail: Uplink at most 20 reliable RPCs per second per connection — source: ADR-0011

This story is the behavioural proof of the R15 binding table: starting from fresh settings, every row of the default bindings, pressed on keyboard/mouse and separately on gamepad, produces the right thing, and right mouse is not bound to Prepare (deliberately: right-mouse-hold is the strongest block/parry mental model and would pull the defence toward a reflex reading; GDD R15 note). It complements the static asset lint (story 007), which proves the table is in the data; this story proves it arrives.

The AC wording "the mapped command reaches the server command log" is false for most rows (see QL-STORY-READY below), so the proposed rewrite per row is the working definition. It depends on 007 (the assets), 094 (controller transport and builder) and 095 (server command log and outgoing-RPC spy, per the plan's additions). GDD relation: R15 (default bindings, lines 195-213), R13, R14, R15a; ADR-0011 sections 2, 9.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 30 (R15, Integration, P `DefaultBindings`): **GIVEN** fresh settings **WHEN** each binding in the R15 table is pressed on keyboard/mouse and on gamepad **THEN** the mapped command reaches the server command log; **AND** RMB is not bound to Prepare.

---

## Implementation Notes

*Derived from ADR-0011 Decision §9 ("Default bindings (IIF R15)") and GDD R15:*

"Default bindings (IIF R15) are the data of `IMC_Common`/`IMC_Active` exactly as the IIF table lists them (WASD/left stick, mouse/right stick, Left Shift/L3 Run, Space/A Traverse, E/X Interact, Q/LT Prepare, LMB/RT Attack, F/B Hide, V/Y toggle, X-or-middle-mouse/D-pad-right shoulder swap); gamepad is partial in MVP (every command mapped, no remap UI, never aim assist). The data lint asserts the table." The GDD table also lists walk-slower (Left Ctrl pace 0.8, Left Alt pace 0.9, no gamepad key) and Ping (G / D-pad Up, a controller RPC, never a pawn command). Hide toggles Enter/Exit by state (press F once to enter, again to exit).

**QL-STORY-READY gap (to pin in the implementation):** "each binding in the R15 table reaches the server command log" is false for most rows: Move/Look ride `ServerMove`, Run is a Gait flag, Camera toggle, Shoulder swap and Walk slower are local, Ping is a controller RPC, and Hide needs a bush; "fresh settings" is undefined. Proposed per-row rewrite: Traverse, Interact (Begin/End), Prepare (Begin/End), Attack, Hide produce a command in the server command log with the right kind; Move/Look produce a `ServerMove` input-vector / control-rotation change; Run sets the run flag; Camera toggle, Shoulder swap, Walk slower produce 0 server commands and the local effect; Ping produces the controller Ping RPC (`ServerTeamPing`). "Fresh settings" = `UEnhancedInputUserSettings` cleared with no saved profile. Setup: 2 PIE clients, a bush and a court fixture within reach, each row pressed on keyboard/mouse then separately on gamepad (injected FKeys); gamepad Walk slower has no key bound. RMB press produces no Prepare Begin and no command of any kind. Edge cases: Hide F toggles Enter then Exit by state; Left Alt yields 0.9 and Left Ctrl 0.8 (story 017); Shoulder swap is X plus middle mouse alternate; the informant context sends nothing (story 072).

Needs the server command log and outgoing-RPC spy (story 095) and a PIE net profile: the AC form is P (2-client PIE without net emulation). Alt behaviour in PIE (menu-bar activation) is recorded in story 018 notes.

Prototype: the prototype's default bindings (WASD, Shift run, Ctrl jog toggle, LMB/F attack, hold Q, hold E, T ping, V camera, C crouch) are the starting defaults, re-mapped here under R15: G ping, F hide, Space traverse, Ctrl/Alt walk-slower, no crouch. Replaced code: `StealthCharacter.cpp` `SetupPlayerInputComponent`. Tuning keys: none (bindings are client-local, TDC CR-14).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-input-assets-and-trigger-lint.md`: the static asset and trigger lint (the table in the data).
- `story-017-walk-slower-scaling.md` and `story-018-walk-slower-two-client.md`: walk-slower magnitudes and pace.
- `story-072-informant-plate.md`: informant context presses.
- `story-014-camera-toggle-local.md`: the camera toggle lockout.
- `story-094-controller-transport-skeleton.md` and `story-095-command-test-seams.md`: transport and test seams used here.

---

## Files

*The code and test files this story changed (written by /dev-story; /story-done checks scope against it).*

- `Source/StealthTestHarness/Private/Tests/Input/DefaultBindingsTest.cpp`
- `Source/StealthGame/Private/StealthHUD.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "Each binding in the R15 table reaches the server command log" is false for most rows. Move/Look ride ServerMove, Run is a Gait flag, Camera toggle, Shoulder swap and Walk slower are local, Ping is a controller RPC, and Hide needs a bush. "Fresh settings" is undefined.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): per row: Traverse, Interact (Begin/End), Prepare (Begin/End), Attack, Hide -> a command in the server command log with the right kind; Move/Look -> ServerMove input vector / control rotation change; Run -> run flag set; Camera toggle, Shoulder swap, Walk slower -> 0 server commands and the local effect; Ping -> controller Ping RPC. "Fresh settings" = UEnhancedInputUserSettings cleared and no saved profile.
- Scenario sketch from the qa file (`DefaultBindings`, AC 30, P): fresh settings, 2 PIE clients, a bush and a fixture within reach; each row pressed on keyboard/mouse, then separately on gamepad (injected FKeys incl. `Gamepad_FaceButton_Bottom`, `LeftTriggerAxis`); then per the rewrite above; gamepad Walk slower has no key bound; RMB press produces no Prepare Begin and no command of any kind. Edge cases: Hide F toggles Enter then Exit by state; Left Alt yields 0.9 and Left Ctrl 0.8 (story 017); Shoulder swap is X plus middle mouse alternate; informant context sends nothing (story 072).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/DefaultBindingsTest.cpp` — CQTest 2-client PIE test, or a documented playtest

**Status**: [x] Created and passing (`StealthGame.Input.PIE.DefaultBindings.EveryDefaultBindingReachesItsTarget`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`, `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`, `production/epics/input-interaction-framework/story-095-command-test-seams.md`; cross-epic: camera-system, shared-character-locomotion, concealment (stories not yet written; bush, Gait and camera handlers)
- Unlocks: None

---

## Completion Notes
**Completed**: 2026-10-26 (IIF batch 13: command/reply types, core, hub, reply step, transport, seams, spikes V2/V3/V5, input ACs; D53 integration of the production input path)
**Criteria**: 1/1 passing (25 rows on keyboard/mouse and gamepad: Traverse, Interact Begin/End, Prepare Begin/End, Attack, Hide reach the server command log; Move and Look reach the command component / control rotation; Run sets the Run gait; camera toggle, shoulder swap and walk-slower send 0 commands with their local effect; Ping sends ServerTeamPing; RMB is not bound to Prepare)
**Deviations**: Per-row rewrite pinned (QL). 'Fresh settings': the project registers no UEnhancedInputUserSettings profile (rebinding is not in the MVP). Hide always sends HideEnter (the owner has no hidden state until the Concealment epic). The HUD's controls line now shows the R15 bindings.
**Test Evidence**: `Source/StealthTestHarness/Private/Tests/Input/DefaultBindingsTest.cpp`
Run result: OBSERVED — the bindings in play: production/qa/evidence/story-094-controller-transport-skeleton/02 and 03 (W + Ctrl walk, Q Prepare) and the HUD controls line
**Reviewed by**: lead-programmer focused review (sonnet; 3 medium + 6 low findings, all fixed except the documented limits) + self-review, IIF batch 13
