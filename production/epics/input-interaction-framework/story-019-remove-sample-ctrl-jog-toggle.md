# Story 019: D37: the sample's Ctrl jog toggle is removed; Ctrl only drives walk-slower

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: `IA_WalkSlower` (Left Ctrl 0.8, Left Alt 0.9) is the client-local keyboard walk-slower modifier in `IMC_Common`; gameplay input lives on `AStealthPlayerController` and the pawn's `SetupPlayerInputComponent` binds nothing gameplay-relevant, and the Game Animation Sample's character input bindings are removed when its Blueprint is reparented (ADR-0010 migration).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Input layer HIGH. The criterion is observable on `UEnhancedPlayerInput` (the active mappings for `EKeys::LeftControl`) and on the pawn's speed. The sample's Ctrl binding lives in its own Enhanced Input assets (candidates in the repo: `Content/Input/IA_Walk`, `Content/Input/IMC_Sandbox`; which asset carries the Ctrl toggle is not yet confirmed and must be found by read-only inspection, never by resaving). No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Gameplay input is bound on `AStealthPlayerController`; the pawn binds nothing gameplay-relevant — source: ADR-0011 Decision §9
- Required: `IA_WalkSlower` (`IMC_Common`; Left Ctrl 0.8, Left Alt 0.9) only scales the keyboard Move vector on the owning client before `ServerMove` — source: ADR-0011 (user decision 2026-10-25)
- Forbidden: Never send the walk-slower modifier to the server as a command or field, and never apply it to gamepad input — source: ADR-0011 (user decision 2026-10-25)
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn — source: ADR-0011
- Guardrail: Sample assets are never edited in place; duplicate or reparent with a `_snapshots/` copy first (ADR-0009 section 6, `production/never-resave-assets.md`)

MVP item 11 and decision D37. The Game Animation Sample's Ctrl key toggles a jog gait (the first-playable controls table notes "Shift (Ctrl toggles jog)"), which collides with the walk-slower modifier that needs Left Ctrl held to scale Move to 0.8. The epic's Build Order and Gates records it: "The walk-slower modifier collides with the sample's Ctrl jog toggle (first-playable controls) and Alt has OS and PIE side effects ...: remap the jog toggle at implementation." D37 is the user decision that resolves it: the toggle is removed and Ctrl only drives walk-slower. This story removes the sample's toggle from the live input path and proves it in PIE.

The removal is by not loading the sample's conflicting mapping context, not by editing and resaving it. It depends on 017 (the modifier scaling exists). It unlocks 093 (static scan and prototype retirement), which sweeps the remaining prototype and sample input. GDD relation: R15a, AC 76; ADR-0011 section 9 and ADR-0010 migration.

---

## Acceptance Criteria

*D37 (user decision 2026-10-25); criterion from the plan, phrased as one testable GIVEN/WHEN/THEN:*

- [ ] **GIVEN** a PIE character walking with W held **WHEN** Left Ctrl is pressed and released **THEN** the character's gait is not toggled between walk and jog (its Gait value and speed are unchanged before and after the tap), and Ctrl held with W gives the 0.8 pace (112 cm/s at defaults).

---

## Implementation Notes

*Derived from ADR-0011 Decision §9, Amendments (user decision 2026-10-25) and Migration Plan; ADR-0010 migration:*

**MVP item 11, decision D37.** "Sample's Ctrl jog toggle removed, Ctrl = walk-slower." ADR-0011 §9: "The pawn's `SetupPlayerInputComponent` binds nothing gameplay-relevant; the Game Animation Sample's character input bindings are removed when its Blueprint is reparented (ADR-0010 migration). Possession changes therefore never rebind gameplay input." Migration Plan: "the sample's own input assets stay untouched except the reparented character's bindings, removed under ADR-0010's migration".

**Never-resave hazard (read `production/never-resave-assets.md` before any Content step):** the 5.8 resave of the sample's GameplayCameras assets broke the CameraDirector import on 2026-10-24. These must never be re-saved or locked for editing: `Content/Blueprints/Cameras/CameraAsset_SandboxCharacter.uasset`, `CameraDirector_SandboxCharacter.uasset`, `CameraRigPrefab_BasicThirdPersonBehavior.uasset`, `CameraRig_BaseDefaults.uasset`, `CameraRig_CollisionOffset.uasset`. Rule: "sample assets are never edited in place; duplicate or reparent with a `_snapshots/` copy first (active.md, ADR-0009 §6)". The sample's input assets (`Content/Input/IA_Walk`, `IA_Sprint`, `IA_Move`, `IA_Look`, `IMC_Sandbox` and the rest) and its character Blueprint are sample assets and are not edited or resaved either. The removal mechanism is therefore: the production controller does not add the sample's conflicting mapping context (and the reparented character's input setup is not called for gameplay), so the toggle is inactive because its context is not loaded, not because it was edited. If the toggle can only be removed by changing a sample Blueprint, take a `_snapshots/` copy first and change the copy or the reparented child, never the original.

Test design (qa-lead, to pin; QL-STORY-READY: GAPS because the criterion names no observable): "With W held, Left Ctrl pressed and released leaves the sample's Gait property unchanged from its pre-tap value and speed at the walk pace after release; the active mapping contexts contain exactly one mapping for LeftControl (to `IA_WalkSlower`); Ctrl held with W gives 0.8 pace (112 cm/s default)." Name the sample's gait variable at implementation by read-only inspection (it is unnamed in this repo). Edge cases: Ctrl while Run held has no effect; Ctrl pressed in a menu does nothing.

Prototype: the prototype note "Shift (Ctrl toggles jog)" is the sample control, superseded; no prototype source implements it. Tuning keys by name: `WalkSpeedCmPerSec` (140, SCL; 0.8 pace gives 112 cm/s).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-017-walk-slower-scaling.md`: the walk-slower scaling function (prerequisite).
- `story-018-walk-slower-two-client.md`: server-observed paces and RPC log for the modifier.
- `story-093-static-scan-and-prototype-retirement.md`: the wider removal of prototype and sample input paths and the static scan.
- `story-015-default-bindings.md`: the default key table.
- ADR-0010 migration (cross-epic, shared-character-locomotion, stories not yet written): reparenting the sample character Blueprint.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGame/Private/StealthPlayerController.cpp` (do not add the sample's conflicting mapping context; `IMC_Common` is the only owner of LeftControl)
- `Source/StealthTestHarness/Private/NoSampleCtrlToggleTest.cpp` (CQTest `NETWORK_TEST_CLASS(NoSampleCtrlToggle, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.NoSampleCtrlToggle.<Method>`)
- No sample `.uasset` is edited or resaved by this story.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "Gait is not toggled" names no observable; the sample's gait variable and the removal mechanism are unnamed, and removal cannot resave sample assets (never-resave).
- QA-lead proposed rewrite (unconfirmed; the plan criterion stays the authority): "With W held, Left Ctrl pressed and released leaves the sample's Gait property unchanged from its pre-tap value and speed at the walk pace after release; the active mapping contexts contain exactly one mapping for LeftControl (to IA_WalkSlower); Ctrl held with W gives 0.8 pace (112 cm/s default)."
- Scenario sketch from the qa file (`NoSampleCtrlToggle`, D37, P): PIE, character walking with W at pace 1.0; Left Ctrl tapped (press and release) 3 times at 1 s intervals; Gait value and speed (140 cm/s +/- 1) identical before and after each tap; `UEnhancedPlayerInput` active contexts hold exactly 1 mapping for `EKeys::LeftControl`, bound to `IA_WalkSlower`; Ctrl held with W settles at 0.8 pace. Edge cases: the sample's jog toggle must be inactive via not loading its IMC, not by resaving it (read `production/never-resave-assets.md`); Ctrl while Run held has no effect; Ctrl pressed in a menu does nothing.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/NoSampleCtrlToggleTest.cpp` — PIE test, or a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-017-walk-slower-scaling.md`; cross-epic: shared-character-locomotion (stories not yet written; reparented character and ADR-0010 migration)
- Unlocks: `production/epics/input-interaction-framework/story-093-static-scan-and-prototype-retirement.md`
