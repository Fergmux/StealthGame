# Smoke record: gameplay input assets and the trigger lint (story iif-007)

> **Story:** `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md` · **Date:** 2026-10-25 · **Tool:** the editor data lint `StealthInputLint::LintProjectInputAssets` (`Source/StealthGameEditor/Private/Input/InputAssetLint.cpp`), run by the gate test `StealthGame.Input.InputAssetsAndTriggerLint` and by the console command `Stealth.Input.Lint`
> **Result:** **PASS** — the 13 actions and 3 contexts under `/Game/StealthGame/Input` match the table recorded in `Source/StealthGameEditor/Private/Input/InputAssetSpec.h`; every seeded negative is reported; no `UPlayerMappableInputConfig` asset or source reference exists.

## The assets (created by `Stealth.Input.CreateAssets`, saved by explicit path only)

| Action | Value type | Context | Keys (keyboard/mouse; gamepad) |
|---|---|---|---|
| IA_Move | Axis2D | IMC_Common | W (swizzle), S (swizzle+negate), A (negate), D; Gamepad_Left2D |
| IA_Look | Axis2D | IMC_Common | Mouse2D; Gamepad_Right2D |
| IA_CameraToggle | Boolean | IMC_Common | V; Gamepad_FaceButton_Top |
| IA_ShoulderSwap | Boolean | IMC_Common | X, MiddleMouseButton; Gamepad_DPad_Right |
| IA_WalkSlower | Axis1D | IMC_Common | LeftControl (scalar 0.2), LeftAlt (scalar 0.1); none |
| IA_Ping | Boolean | IMC_Common | G; Gamepad_DPad_Up |
| IA_Menu | Boolean | IMC_Common | Escape; Gamepad_Special_Right |
| IA_Run | Boolean | IMC_Active | LeftShift; Gamepad_LeftThumbstick |
| IA_Traverse | Boolean | IMC_Active | SpaceBar; Gamepad_FaceButton_Bottom |
| IA_Interact | Boolean | IMC_Active | E; Gamepad_FaceButton_Left |
| IA_Prepare | Boolean | IMC_Active | Q; Gamepad_LeftTrigger |
| IA_Attack | Boolean | IMC_Active | LeftMouseButton; Gamepad_RightTrigger |
| IA_Hide | Boolean | IMC_Active | F; Gamepad_FaceButton_Right |
| — | — | IMC_Informant | no mapping |

No action and no mapping carries a trigger (every one uses the implicit default). Left Ctrl is walk-slower (decision D37);
the Game Animation Sample's `IMC_Sandbox` (Ctrl jog toggle) and its `Content/Input/*` assets were not opened, scanned or saved.

## Checks

| Check | Observed | Result |
|---|---|---|
| Real assets lint clean | `STEALTH INPUT LINT: PASS (0 finding(s))` after creation (`production/qa/evidence/story-007-input-assets-and-trigger-lint/create-assets-run.txt`); the gate test reports zero findings | PASS |
| Lint never dirties what it loads | every IA_/IMC_ package `IsDirty() == false` after the lint (gate test) | PASS |
| Seeded Hold on IA_Interact fails | reported `IA_Interact: banned trigger Hold` | PASS |
| HoldAndRelease, Tap, Pulse, Chorded action on an action | each reported | PASS |
| Trigger at the mapping level (Tap on `E -> IA_Interact` in IMC_Active) | reported | PASS |
| Wrong value type (IA_Attack as Axis1D) | reported | PASS |
| Gameplay action in IMC_Informant; menu context mapping IA_Interact | each reported | PASS |
| IA_Ping removed from IMC_Common; IA_Move missing | each reported | PASS |
| No `UPlayerMappableInputConfig` | 0 assets in the asset registry; no project source names it outside the lint and its test | PASS |

Run: `tools/dev/cycle.ps1 -Tests -NoLaunch` — `Result: Succeeded`, `tests exit=0`, **45/45 Success**
(`production/qa/evidence/story-007-input-assets-and-trigger-lint/tests-45.txt`).
