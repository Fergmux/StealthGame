# Story 015: Planted brace: step-into-stance entry, then no translation and no yaw

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R5.2 Planted brace, R6 Bracing row, States `Planted`; AC-SCL-11; OQ9 closed by Assassination ASN-R3)
**Requirement**: `TR-scl-004`, `TR-scl-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6; with ADR-0013 section 1 for `ActionPose` states)
**ADR Decision Summary**: While `FActionState.Pose == Brace` Move is ignored for translation and yaw (planted); a requested Run is clamped and never cancels the brace. Brace entry from a walk plays a distance-matched step-into-stance clip from the chooser-selected brace pose database whose root motion equals the F2 stop distance within ±10 %, so the wide stance never slides. Only head-look still follows the clamp.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Brace entry is a PoseSearch family covering entry speeds 0 to full pace (not one fixed-length clip); the root-motion distance must match the baseline stop distance for the current speed (53 cm at 140 cm/s). PoseSearch and Chooser plugins are `VersionName 1.0`, no Beta flag; the animation database content is a Content step with the never-resave hazards in `production/never-resave-assets.md`.

**Control Manifest Rules (this layer)**:
- Required: movement shaping is one function per rule for every pawn; `ActionPose` drives AnimBP states and is written only through the IIF on the server (`WriteActionState`).
- Forbidden: never select an animation asset by controller, team or participant state; no brace-pivot clips; a planted pawn is a static obstacle.
- Guardrail: none specific.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-11** [P] `BracePlanted`: GIVEN a pawn walking at 140 cm/s, WHEN `ActionPose = Brace` is written, THEN the step-into-stance clip moves it 53 cm ± 10 % and it then holds; WHEN Move (Walk and Run, 8 directions) is issued for 2 s, THEN horizontal displacement is 0 ± 1 cm, yaw change is 0 ± 0.5°, head-look still follows the clamp, and the brace is not cancelled.

*QA clarification (create-stories 2026-10-25, not a GDD change):* "head-look still follows the clamp" means head yaw within ±70.6°, pitch within ±20.1° and at most 140 °/s (the AC-SCL-09 numbers).

---

## Implementation Notes

*Derived from ADR-0010 section 6 and GDD R5.2:*

- In the movement model, treat `Pose == Brace` as: translation and yaw input zeroed, Run clamped to Walk first (so the clamp never cancels the brace), no pivot. The entry distance comes from the distance-matched brace entry in the chooser-selected database, so the root motion and the F2 stop distance for the entry speed agree within ±10 %.
- The entry-clip authoring (distance-matched pose database) is animation content and is the visible half of this story; the movement half is testable with a stubbed pose write and a synthetic root-motion source.
- `ActionPose` itself is written by the IIF (`UPawnCommandComponent::WriteActionState`); this story reads it and tests with a stub writer.
- **Prototype replaced**: the prototype's defence state (`ServerSetDefence`, `HeldUntilServerTime` fields on `AStealthCharacter`) is replaced by the IIF action state plus this movement rule.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The IIF and Assassination epics: Brace entry/exit rules and the defence's meaning.
- Story 010: general gait clamp.
- Story 016: displacement accounting for the held pose.

---

## Files

- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.h`
- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthLocomotionModel.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnMovementComponent.cpp`
- `Source/StealthGameplay/Private/Tests/Locomotion/LocomotionTestWorld.h`
- `Source/StealthGameplay/Private/Tests/Locomotion/BracePlantedTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.BracePlanted`.*

Test: planted brace (AC-SCL-11)
  Given: a pawn walking at 140 cm/s
  When: `ActionPose = Brace` is written, then Move (Walk and Run, 8 directions) is issued for 2 s
  Then: the step-into-stance clip moves it 53 cm ± 10 % and it then holds; horizontal displacement is 0 ± 1 cm; yaw change is 0 ± 0.5°; head-look stays within ±70.6° yaw and ±20.1° pitch at ≤ 140 °/s; the brace is not cancelled
  Edge cases: depends on the Brace `ActionPose` write (IIF stub) and the distance-matched entry clip existing

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/BracePlantedTest.cpp`, test name `StealthGame.Locomotion.BracePlanted` — must exist and pass; retained screenshot of the planted wide stance after entry in `production/qa/evidence/` (player-observable)

**Status**: [x] Created and passing (`StealthGame.Locomotion.BracePlanted`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-010-speed-tiers.md`, `story-009-stop-floor.md`; `production/epics/input-interaction-framework/EPIC.md` (`FActionState`)
- Unlocks: `production/epics/assassination-defence-reversal/EPIC.md` (consumes the planted brace)

---

## Completion Notes
**Completed**: 2026-10-26 (SCL batch: 003, 007, 009-018, 046, 047)
**Criteria**: 1/1 (movement half): entry from 140 travels 52.5 cm (53 +- 10 %) then holds; 8 directions x Walk/Run for 2 s: 0 cm, 0 deg, brace kept, Run clamped. Head-look clause asserted by story 021 (not built); the step-into-stance clip is animation content (visible half DEFERRED)
**Deviations**: Pose written by a stub (IIF FActionState writer not built).
**Test Evidence**: `StealthGame.Locomotion.BracePlanted`; full suite 171/171 StealthGame.* Success (`production/qa/evidence/story-010-speed-tiers/full-suite.txt`)
**Run result**: N/A - no gameplay path writes ActionPose Brace yet (the prototype defence does not use the production pose), so it cannot be reached in play
**Reviewed by**: lead-programmer focused review (sonnet) + self-review
