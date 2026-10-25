# Story 007: ViewBlock channel and collision profiles

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-003`, `TR-camera-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope
**ADR Decision Summary**: `ViewBlock` is a new custom trace channel (`ECC_GameTraceChannelN`, default response Block) and is the project's single definition of "what blocks sight": the mask, the aim ray's world test and the layout contract's sight predicates all use it. Walls, doors, props, kit modules, court fixtures and sight-relevant water blockers block it; pawns, bush foliage, glass, decoration and the water pawn-blocking volume ignore it. The engine `ECC_Camera` is for boom collision only; pawns and foliage ignore it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; this story is project collision config, pre-cutoff)
**Engine Notes**: Config goes in `Config/DefaultEngine.ini` under `[/Script/Engine.CollisionProfile]` (ADR-0015 Migration Plan 3; the "config batch" item "Add a `ViewBlock` trace channel" in `production/pending-decisions-2026-10-25.md` A4, authorised for this run). The mapping from channel name to `ECC_GameTraceChannelN` is fixed by the first free slot; record the index chosen in the ADR-0015 follow-up note, because code refers to it through one project constant. No ADR-0015 verification item gates this story. Do not open or save any Content asset: profile definitions are config, and pawn capsule/mesh profile names are set by the Shared Pawn story in C++.

**Control Manifest Rules (this layer)**:
- Required: Every sight test (camera mask, aim world test, LSC sight predicates) uses the `ViewBlock` channel; collision profiles set their `ViewBlock` and `Camera` responses explicitly — source: ADR-0015
- Forbidden: Never introduce a second "sight" channel — source: ADR-0015
- Guardrail: Config change only; no per-frame cost

This story is the shared precondition for the mask oracle (AC-CS-05), the aim ray and the layout sight predicates. The GDD has no acceptance criterion for the channel itself, so the criteria below are taken from ADR-0015 §4.1 and its Migration Plan, and are marked derived.

---

## Acceptance Criteria

*Derived from ADR-0015 §4.1 and Migration Plan 3 (no GDD criterion; AC-CS-05's oracle presupposes the channel):*

- [ ] **AC1** (derived): **GIVEN** `Config/DefaultEngine.ini` **WHEN** the collision profile section is parsed **THEN** exactly one `+DefaultChannelResponses` entry named `ViewBlock` exists with `bTraceType=True` and `DefaultResponse=ECR_Block`, and exactly one project constant equals its `ECC_GameTraceChannelN` index. *(Revised at create-stories: the channel is already defined in the ini as `ECC_GameTraceChannel2`, line 164, with `Traversable` on channel 1. This criterion is now a regression guard plus the C++ constant, not new config.)*
- [ ] **AC2** (derived): **GIVEN** the project collision profiles **WHEN** their responses are listed against a table of profile name to expected `ViewBlock` and `Camera` response (this story writes the table; ADR-0017 supplies the foliage row) **THEN** walls, doors, props, kit modules and court fixtures Block `ViewBlock`, while pawns, bush foliage, glass, decoration and the water pawn-blocking volume Ignore it, and pawns and bush foliage Ignore the engine `Camera` channel, each set explicitly. *(The stock `Pawn` profile in the ini sets `Visibility` and `ViewBlock` but not `Camera`; the table decides whether the production pawn profile adds it.)*
- [ ] **AC3** (derived, ADR-0008 asset audit): **GIVEN** a kit or pawn asset whose `ViewBlock` or `Camera` response differs from its profile **WHEN** the editor validator runs **THEN** validation fails and names the asset and the response.

---

## Implementation Notes

*Derived from ADR-0015 Decision §4.1:*

Profiles (pawn capsule and mesh, kit, foliage, glass, decoration) set their `ViewBlock` and `Camera` responses explicitly. Foliage ignoring both channels is what lets the lens and the mask eye sit inside the bush dome (Concealment owns the foliage collision profile, ADR-0017; coordinate with `concealment-bushes`). The editor validator belongs to the ADR-0008 asset audit in `StealthGameEditor` (a `UEditorValidatorBase` subclass); it reads responses from asset data and never loads-and-saves an asset. AC1 and AC2 are checked mechanically by a Config/Data smoke check: a test (or script) parses the ini and asserts the listed responses, and `tests/smoke` lists it.

Prototype replaced: the prototype uses ad hoc `Visibility`/`Camera` traces (`StealthCharacter.cpp`); none is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-013-mask-live-world-is-eye-mask-visible.md` and `story-016-aim-ray.md`: the code that traces `ViewBlock`.
- level-socket-contract epic: sight predicates (consumers of the channel).
- concealment-bushes epic: foliage collision profile contents.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Config/DefaultEngine.ini` (`[/Script/Engine.CollisionProfile]`: `+DefaultChannelResponses` for `ViewBlock`, profile responses)
- `Source/StealthGameplay/Public/View/Core/StealthCollisionChannels.h` (the single `ViewBlock` constant)
- `Source/StealthGameEditor/Private/Validators/StealthCollisionProfileValidator.cpp`
- `Source/StealthGameEditor/Private/Tests/CollisionProfileValidatorTest.cpp` (class `FCameraCollisionProfileValidatorTest`, name `StealthGame.Camera.CollisionProfileValidator`)
- `tests/smoke/` entry for the ini check

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS (reviewed before the AC1/AC2 revision above; the revision answers the first two lines)
- AC2: the story named no collision profiles; it now says this story writes a profile-to-expected-response table. ADR-0017 owns the foliage row.
- AC2: the stock `Pawn` profile in `DefaultEngine.ini` customises `Visibility` and `ViewBlock` only, so `Camera` is not explicit, while ADR-0015 §4.1 says explicit; the table decides.
- AC3: assets on a Custom preset need a rule from the author.
- Mechanical checks: AC1 is a grep of the `+DefaultChannelResponses` line plus a runtime `UCollisionProfile` lookup that equals the C++ constant; AC2 asserts `GetResponseToChannel` per profile against the table; AC3 runs the validator against fixture assets (a kit mesh with `ViewBlock` overridden, a pawn mesh with `Camera` overridden, two clean ones) through the ADR-0008 audit commandlet and expects the two bad ones to fail with paths.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass (`production/qa/smoke-*.md`) covering the ini assertions, plus the validator test

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic: tuning-data-config `story-011-t0-lint-framework-and-gate.md` (the ADR-0008 audit framework that hosts the validator) and `story-002-gameplay-presentation-module-skeletons.md`
- Unlocks: `production/epics/camera-system/story-006-eye-pivot-boom-collision.md` (Camera responses at runtime), `story-013-mask-live-world-is-eye-mask-visible.md`, `story-016-aim-ray.md`
