# Story 010: Speed tiers, pace quantisation and effective-gait clamps

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R5.1 Gaits, R5.2 Effective-gait clamps, R6, F1 Target speed; AC-SCL-05)
**Requirement**: `TR-scl-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6, "Gaits and pace", "Effective-gait clamps")
**ADR Decision Summary**: One quantiser maps analog magnitude to the shared pace set {0.8, 0.9, 1.0} with ±0.03 hysteresis where the input is consumed (`ScaleInputAcceleration` on the owning client and on the server for AI); human moves snap on the server in `MoveAutonomous`. Run becomes Walk when `IGaitLegalitySource` reports `Informant` or `FActionState.Pose == Brace`. The owner applies the 0.2 dead zone before sending; the server applies none.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `MoveAutonomous` does not call `ScaleInputAcceleration` (engine gate), so the server snaps a human move's received acceleration magnitude to the nearest pace-set value itself. `GetMaxSpeed` returns `WalkSpeedCmPerSec × pace` or `RunSpeedCmPerSec`. The gait travels in the saved move as `FLAG_Custom_0` (`WantsRun`).

**Control Manifest Rules (this layer)**:
- Required: the gait clamp's participant-state input comes only from `IGaitLegalitySource`; `IA_WalkSlower` (Left Ctrl 0.8, Left Alt 0.9) only scales the keyboard Move vector on the owning client, and SCL quantises the received magnitude to the shared pace set (ADR-0011, user decision 2026-10-25).
- Forbidden: never read a PlayerState for the gait clamp; never send the walk-slower modifier to the server as a command or field, and never apply it to gamepad input.
- Guardrail: movement shaping well under 5 % of the 3.0 ms CMC line.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-05** [P] `SpeedTiers`: GIVEN default tuning in a minimal test world, WHEN Move is issued with (m = 1, Walk), (0.9, Walk), (0.5, Walk), (1, Run), (1, Run) as an informant, (0, Run) and (1, Walk) while braced, THEN steady speeds are 140, 126, 112, 450, 140, 0 and 0 cm/s (± 1); AND a client stick reading of 0.19 is sent as 0.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the stick dead zone is 0.2 (GDD R5.1), applied by the owning client's input mapping; the quantiser is tested separately with direct `Move` calls; add a probe above 0.2 asserting only non-zero (not exactly 0.2, whose strictness the GDD does not state).

---

## Implementation Notes

*Derived from ADR-0010 section 6 and GDD R5.1/R5.2:*

- Add the pace quantiser and the effective-gait clamp to `FStealthLocomotionModel`; the movement component calls them from `ScaleInputAcceleration` (owner and server AI) and `MoveAutonomous` (server human snap). Walk speed is `WalkSpeedCmPerSec × clamp(|Direction|, WalkSpeedScaleMinMult, 1)` quantised to {0.8, 0.9, 1.0}; Run is `RunSpeedCmPerSec` with no scale; there is no sprint, crouch, jump, strafe, back-pedal or dodge.
- `Direction` is clamped to length 1. The 0.2 stick dead zone is applied by the owning client before sending (0.19 is sent as 0); the server applies no dead zone and treats length 0 as no input for every pawn.
- `IGaitLegalitySource` is injected by the composition root: server calls Roster `GetLegalityClass(Pawn)` (Unbound maps to `Active`), the owner reads its own C-TeamView part, other clients never call it. In tests, inject a stub returning `Informant`.
- While `ActionPose = Brace` a requested Run is clamped and never cancels the brace; the zero-speed result for "(1, Walk) while braced" is the planted brace of story 015, so this story asserts only the clamp and that Move produces no speed while the pose is `Brace`.
- **Prototype replaced**: the prototype `IsInformant()` read from the replicated PlayerState and its informant speed cap (`InformantMaxSpeedCmPerSec`, retired key) are replaced, not ported. The informant clamp and traversal block were never exercised in play in the prototype (known issue 4, `production/first-playable-report.md`); this story is the first proof.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 009: braking and the stop floor.
- Story 014: informant Run/Traverse/mode-switch restrictions end to end.
- Story 015: brace entry clip and planted behaviour.
- Story 019: Crowd's use of the pace set (distribution).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp` (pace quantiser, gait clamp)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMovementComponent.cpp` (`ScaleInputAcceleration`, `MoveAutonomous`, `GetMaxSpeed`)
- `Source/StealthGameplay/Private/Tests/Locomotion/SpeedTiersTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.SpeedTiers`.*

Test: speed tiers (AC-SCL-05)
  Given: default tuning in a minimal test world
  When: Move is issued for (m = 1, Walk), (0.9, Walk), (0.5, Walk), (1, Run), (1, Run) as an informant, (0, Run) and (1, Walk) while braced
  Then: steady speeds are 140, 126, 112, 450, 140, 0 and 0 cm/s (± 1); a client stick reading of 0.19 is sent as 0 (owning client's input mapping, dead zone 0.2); a probe above 0.2 asserts only non-zero
  Edge cases: 0.5 maps to 112 (values below the minimum walk scale clamp to it); the quantiser is also tested separately with direct Move calls

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/SpeedTiersTest.cpp`, test name `StealthGame.Locomotion.SpeedTiers` (PIE minimal test world) — must exist and pass; plus a retained screenshot of a walking and a running pawn in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-009-stop-floor.md`; `production/epics/input-interaction-framework/EPIC.md` (`UPawnCommandComponent::Move`, `IA_WalkSlower`)
- Unlocks: `production/epics/shared-character-locomotion/story-011-release-baseline-stop.md`, `story-014-informant-restrictions.md`, `story-015-brace-planted.md`, `story-019-npc-pace-distribution.md`
