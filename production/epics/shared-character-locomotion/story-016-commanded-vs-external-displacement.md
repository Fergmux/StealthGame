# Story 016: Commanded displacement is separate from external displacement

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R16 External displacement is not commanded displacement; AC-SCL-15)
**Requirement**: `TR-scl-012`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (sections 6 and 8)
**ADR Decision Summary**: The movement component accumulates horizontal displacement while the pawn's own `Move` input is non-zero (`CommandedDisplacementCm(Since)`), separately from displacement under zero input (depenetration, bases, root motion). The IIF reads only the commanded figure for R8 cancels. Pawn-pawn capsules block and physics push of pawns is off; a pawn in `Brace` or `InteractPose` is a static obstacle.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Depenetration and moving-base displacement happen inside the CMC step; the accumulator must separate them by input state, not by source guess. The value is server-side and unreplicated (owner-local copy for prompts only).

**Control Manifest Rules (this layer)**:
- Required: published state through `ISharedPawnStateView`; commanded displacement is read by the IIF for interaction cancel (`InteractMoveCancelDistCm`).
- Forbidden: never replicate `CommandedDisplacementCm`.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-15** [P] `CommandedVsExternalDisplacement`: GIVEN a tending pawn and 20 NPC brushes plus one depenetration push of 10 cm, WHEN `CommandedDisplacementCm` is read, THEN it is 0 and the tending pawn moved ≤ 1 cm from contact; a 20 cm stick nudge reads 20 ± 1.

*QA clarification (create-stories 2026-10-25, not a GDD change):* `CommandedDisplacementCm` is read cumulatively since the test start (confirm against R19 at `/story-readiness`).

---

## Implementation Notes

*Derived from ADR-0010 section 6 and GDD R16:*

- Accumulate per-step horizontal displacement into the commanded bucket only when the pawn's own `Move` input vector is non-zero; displacement under zero input goes to a separate external bucket. `CommandedDisplacementCm(Since)` returns the commanded total since a server timestamp.
- "Tending" means an `InteractPose` pawn (a static obstacle): NPC brushes (R5.8 contact reaction) must not move it more than 1 cm, so this test also covers the "contact never moves it" half of R16.
- **Prototype replaced**: none; the prototype cancels interactions by raw position change, which this replaces for the IIF epic.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The IIF's cancel logic (IIF epic).
- Story 020: the visual contact reaction.

---

## Files

- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.h`
- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthLocomotionModel.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnMovementComponent.cpp`
- `Source/StealthGameplay/Private/Tests/Locomotion/LocomotionTestWorld.h`
- `Source/StealthGameplay/Private/Tests/Locomotion/CommandedVsExternalDisplacementTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.CommandedVsExternalDisplacement`.*

Test: commanded versus external displacement (AC-SCL-15)
  Given: a tending pawn (`InteractPose`), 20 NPC brushes (fixed seed) and one depenetration push of 10 cm
  When: `CommandedDisplacementCm` is read, cumulative since test start
  Then: it is 0 and the tending pawn moved ≤ 1 cm from contact; a separate 20 cm stick nudge reads 20 ± 1
  Edge cases: confirm the read window against GDD R19 at `/story-readiness`

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/CommandedVsExternalDisplacementTest.cpp`, test name `StealthGame.Locomotion.CommandedVsExternalDisplacement` — must exist and pass (not directly player-observable, no screenshot)

**Status**: [x] Created and passing (`StealthGame.Locomotion.CommandedVsExternalDisplacement`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-010-speed-tiers.md`
- Unlocks: `production/epics/input-interaction-framework/EPIC.md` (interaction move-cancel)

---

## Completion Notes
**Completed**: 2026-10-26 (SCL batch: 003, 007, 009-018, 046, 047)
**Criteria**: 1/1: 20 brushes move the tending pawn 0.000 cm, commanded 0 after the brushes and a 10 cm push (the push lands in the external bucket), a nudge moved 19.4 cm and reads 19.4 (20 +- 1)
**Deviations**: Commanded includes the release stop that ends commanded motion; any capsule move between two moves is external (decision log). Read cumulatively since the test start.
**Test Evidence**: `StealthGame.Locomotion.CommandedVsExternalDisplacement`; full suite 171/171 StealthGame.* Success (`production/qa/evidence/story-010-speed-tiers/full-suite.txt`)
**Run result**: N/A - CommandedDisplacementCm is server state with no presentation
**Reviewed by**: lead-programmer focused review (sonnet) + self-review
