# Story 007: IA_* actions and IMC contexts exist and the trigger lint passes

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Config/Data
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-003`, `TR-iif-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: Gameplay input is a set of Boolean `IA_*` actions (axis actions for Move and Look, Axis1D for walk-slower) in `IMC_Common`, `IMC_Active` and `IMC_Informant`, with the Hold, HoldAndRelease, Tap, Pulse and Chorded-action triggers banned on any action with server meaning, checked by an editor data lint that fails the build. ADR-0021 adds that `IMC_Menu` maps only UI navigation actions and never an `IA_*` gameplay action.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Highest of ADR-0011 (HIGH) and ADR-0021 (HIGH). Relevant facts: E4 `UPlayerMappableInputConfig` is deprecated since 5.3 in favour of `UEnhancedInputUserSettings`; E12 `bIgnoreAllPressedKeysUntilRelease` covers Boolean input actions only, which is why server-meaning actions must be Boolean; E5 Enhanced Input and Common UI are unified in 5.8 (menus use one `IMC_Menu`, ADR-0021 §7). Verification items V1 (story 008) and V5 (story 011) test these contexts at runtime; this story only authors and lints the data.

**Control Manifest Rules (this layer)**:
- Required: Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` to Begin and both `Completed` and `Canceled` to End; server-meaningful actions are Boolean — source: ADR-0011
- Required: Rebinding uses `UEnhancedInputUserSettings` — source: ADR-0011
- Forbidden: Never use Hold/HoldAndRelease/Tap/Pulse/Chorded triggers for server-meaningful actions — the client would own hold timing — source: ADR-0011
- Forbidden: Never use `UPlayerMappableInputConfig` or the Raw Input plugin; never make a gameplay command a Common UI action — source: ADR-0011
- Forbidden: Never send the walk-slower modifier to the server as a command or field, and never apply it to gamepad input — source: ADR-0011 (user decision 2026-10-25)
- Forbidden: Never bind `ShowTeamNames` in the MVP — source: ADR-0011, ADR-0021
- Guardrail: Input mapping contexts and actions are small data assets loaded with the controller class (Load Time) — source: ADR-0011

This story authors the project's gameplay input data and the lint that protects it. It creates the `IA_*` actions and the three mapping contexts under `Content/StealthGame/Input` and builds the editor data lint that ADR-0011 section 9 says "fails the build": every server-meaning action is Boolean with no banned trigger, `IA_Move` and `IA_Look` are axis actions, `IA_WalkSlower` is Axis1D, no `UPlayerMappableInputConfig` exists, and the membership of each context matches the GDD R13 and R15 tables. It is the single data prerequisite of the input stories: edge handling (012), the spikes V1 and V5 (008, 011), camera toggle (014), default bindings (015), no-toggle (016), walk-slower (017) and the informant plate (072), plus the controller skeleton (094).

It sits as a root story (no story dependency) so the spikes can start early; the V1 and V5 spike stories exercise these exact assets. The lint is built here and is scoped to `Content/StealthGame/Input` only: the Game Animation Sample's own input assets (`Content/Input/IA_*`, `IMC_Sandbox`) are never scanned and never resaved. GDD relation: R4 (edges only, Hold trigger never carries server meaning), R13 (contexts), R15 (default bindings), R15a (walk-slower). MVP item 11 starts here: `IA_WalkSlower` is created now, behaviour in stories 017 and 018.

---

## Acceptance Criteria

*ADR-derived (no numbered GDD AC). Source: ADR-0011 Decision §9 ("Edges only"; "an editor data lint in ADR-0008 checks both and fails the build"; IMC membership) and Validation Criteria static scan ("no Hold/Tap/Pulse/HoldAndRelease/Chorded trigger on an `IA_*` action with server meaning; no `UPlayerMappableInputConfig` use"):*

- [ ] **GIVEN** the assets under `Content/StealthGame/Input` **WHEN** the editor data lint runs **THEN** `IA_Move` and `IA_Look` are Axis2D, `IA_WalkSlower` is Axis1D and `IA_Run`, `IA_Traverse`, `IA_Interact`, `IA_Prepare`, `IA_Attack` and `IA_Hide` are Boolean with no Hold, HoldAndRelease, Tap, Pulse or Chorded-action trigger on the action or on any mapping of it; **AND** `IMC_Common` contains Move, Look, CameraToggle, ShoulderSwap, WalkSlower, Ping and menu, `IMC_Active` contains Run, Traverse, Interact, Prepare, Attack and Hide, and `IMC_Informant` contains no gameplay action; **AND** no `UPlayerMappableInputConfig` asset or reference exists.

---

## Implementation Notes

*Derived from ADR-0011 Decision §9, Amendments 2026-10-25 and Validation Criteria; ADR-0021 Decision §7:*

"Held commands bind `ETriggerEvent::Started` to `Begin` and **both** `Completed` and `Canceled` to `End` (E1); instant commands bind `Started`. Actions with server meaning are **Boolean** input actions (E12) with only the default (Down/Pressed) trigger: the **Hold, HoldAndRelease, Tap, Pulse and Chorded-action triggers are banned** on them. `IA_Move` and `IA_Look` are axis actions and carry no edges. No command auto-repeats." The reasoning is that the server owns every hold timer; a client Hold trigger would give the client authority over timing. `IMC_Common` (always): Move, Look, Camera toggle, Shoulder swap, Walk slower, Ping, menu. `IMC_Active`: Run, Traverse, Interact, Prepare, Attack, Hide. `IMC_Informant`: no gameplay actions (its presses are caught locally for the plate, story 072). Default bindings are "the data of `IMC_Common`/`IMC_Active` exactly as the IIF table lists them"; the lint asserts the table (R15 lines 195-213; behavioural check is story 015). `IA_Ping` is a Boolean press in `IMC_Common` (G / D-pad Up) and is a controller RPC, never a pawn command. `ShowTeamNames` is not bound in MVP. `IMC_Menu` (ADR-0021 §7) is a separate Presentation asset; this lint additionally fails if any menu context maps an `IA_*` gameplay action (the lint may share code with the HUD epic's check; the check owned here is the gameplay side).

**QL-STORY-READY gaps resolved by the author at implementation:** (1) the table of action, value type and allowed triggers is not tabulated by the ADR (IA_Run, IA_CameraToggle, IA_ShoulderSwap, IA_Ping unspecified); the qa-lead's proposal is: Move/Look Axis2D (no trigger), WalkSlower Axis1D, every other action Boolean, "default trigger only" meaning an empty trigger list or the single confirmed default (Down/Pressed) and nothing else; confirm and record the table in the lint source. (2) Triggers set at the *mapping* level, not on the action, must also fail. (3) The lint needs a seeded negative (a Hold trigger on `IA_Interact`) that must fail the build. (4) The lint reads assets without resaving them.

**Content and never-resave hazard:** read `production/never-resave-assets.md` before touching any Content. New assets are created under `Content/StealthGame/Input` only. The sample's `Content/Input/*` (IA_Move, IA_Look, IA_Sprint, IA_Walk, IMC_Sandbox and the rest) and its `Content/Blueprints/Cameras/*` assets are never edited in place and never resaved (Camera assets are explicitly listed as never-resave: `CameraAsset_SandboxCharacter`, `CameraDirector_SandboxCharacter`, `CameraRigPrefab_BasicThirdPersonBehavior`, `CameraRig_BaseDefaults`, `CameraRig_CollisionOffset`). The lint must not resave any asset it loads.

Prototype: new code and data, no prototype counterpart (the prototype binds keys in `StealthCharacter::SetupPlayerInputComponent`, replaced). Tuning keys: none (bindings are client-local, not tuning, TDC CR-14).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-008-spike-v1-enhanced-input-edges.md` and `story-011-spike-v5-commonui-menu-end.md`: runtime behaviour of these contexts under edge cases.
- `story-012-edges-not-repeat.md`: the Begin/End edge handling.
- `story-015-default-bindings.md`: the behavioural check that each default binding reaches the server.
- `story-017-walk-slower-scaling.md`: `IA_WalkSlower` behaviour and the modifier-scaling logic.
- `story-072-informant-plate.md`: `IMC_Informant` press catching and the plate.
- `story-094-controller-transport-skeleton.md`: binding the actions in `SetupInputComponent`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Content/StealthGame/Input/IA_Move`, `IA_Look`, `IA_Run`, `IA_Traverse`, `IA_Interact`, `IA_Prepare`, `IA_Attack`, `IA_Hide`, `IA_CameraToggle`, `IA_ShoulderSwap`, `IA_WalkSlower`, `IA_Ping` (new assets, `.uasset`)
- `Content/StealthGame/Input/IMC_Common`, `IMC_Active`, `IMC_Informant` (new assets)
- `Source/StealthGameEditor/Private/Input/InputAssetLint.cpp`, `Source/StealthGameEditor/Public/Input/InputAssetLint.h`
- `Source/StealthGameEditor/Private/Tests/InputAssetsAndTriggerLintTest.cpp` (class `FInputInputAssetsAndTriggerLintTest`, name `StealthGame.Input.InputAssetsAndTriggerLint`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- The set of "server-meaning" actions and each action's value type are not tabulated (IA_Run, IA_CameraToggle, IA_ShoulderSwap, IA_Ping unspecified), "default trigger only" is ambiguous (empty list vs Down vs Pressed), and no story owns building the lint ADR 9 says "fails the build".
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): add a table (action, value type, allowed triggers) to the story; state that the lint is built here and scoped to Content/StealthGame/Input only (the sample's own IAs are never scanned).
- Scenario sketch from the qa file (Config/Data; evidence smoke check `production/qa/smoke-[date].md` plus the lint run): IA_Move and IA_Look Axis2D; IA_WalkSlower Axis1D; IA_Run, Traverse, Interact, Prepare, Attack, Hide Boolean with an empty trigger list (or the single confirmed default); no UInputTriggerHold/HoldAndRelease/Tap/Pulse/ChordAction on any such IA or its IMC mapping; IMC_Common = Move, Look, CameraToggle, ShoulderSwap, WalkSlower, Ping, menu; IMC_Active = Run, Traverse, Interact, Prepare, Attack, Hide; IMC_Informant = no gameplay action; 0 `UPlayerMappableInputConfig` assets or references (asset registry plus grep). Edge cases: mapping-level trigger also fails; seeded negative (Hold on IA_Interact) fails the build; lint never resaves assets; smoke check passes (BLOCKING for smoke-check by default).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass (`production/qa/smoke-<date>.md`, written when `/smoke-check` runs) plus the editor lint test `Source/StealthGameEditor/Private/Tests/InputAssetsAndTriggerLintTest.cpp`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (cross-epic: tuning-data-config module skeleton and StealthGameEditor module, stories not yet written)
- Unlocks: `production/epics/input-interaction-framework/story-008-spike-v1-enhanced-input-edges.md`, `production/epics/input-interaction-framework/story-011-spike-v5-commonui-menu-end.md`, `production/epics/input-interaction-framework/story-012-edges-not-repeat.md`, `production/epics/input-interaction-framework/story-014-camera-toggle-local.md`, `production/epics/input-interaction-framework/story-015-default-bindings.md`, `production/epics/input-interaction-framework/story-016-no-toggle-setting-mvp.md`, `production/epics/input-interaction-framework/story-017-walk-slower-scaling.md`, `production/epics/input-interaction-framework/story-072-informant-plate.md`, `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`
