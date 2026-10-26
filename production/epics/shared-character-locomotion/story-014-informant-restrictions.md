# Story 014: Informants walk only, cannot traverse, and SCL refuses the traversal mode

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R5.2, R6 Informant row; AC-SCL-10)
**Requirement**: `TR-scl-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6, "Effective-gait clamps")
**ADR Decision Summary**: Informants and planted pawns may not enter the traversal movement mode: `SetMovementMode` refuses it as a backstop (the IIF already rejects the command with `NotNow`). The gait clamp reads participant state only through `IGaitLegalitySource`. SCL adds no informant-only movement (no different top speed, no pass-through), because each would be a visible tell.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `SetMovementMode` is a CMC override point (E8). The traversal mode in the sample is `MOVE_Flying` with root motion and warping, but the refusal is keyed on the traversal mode switch, not the mode value alone.

**Control Manifest Rules (this layer)**:
- Required: the gait clamp's participant-state input comes only from `IGaitLegalitySource` (ADR-0010).
- Forbidden: never read a PlayerState for the gait clamp; no informant-only top speed or pass-through.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-10** [U] `InformantRestrictions`: GIVEN an informant pawn, WHEN Run and Traverse are issued and the traversal movement mode is forced directly, THEN speed stays ≤ 140, Traverse is rejected `NotNow` by the IIF, and SCL refuses the mode switch (backstop).

*QA clarification (create-stories 2026-10-25, not a GDD change):* the forced traversal mode is a stub custom-mode id and the IIF `NotNow` reply is stubbed until spike 045 reports the sample's real traversal mode; re-run with the real mode then.

---

## Implementation Notes

*Derived from ADR-0010 section 6:*

- Inject a stub `IGaitLegalitySource` returning `Informant` and a stub IIF `Traverse` entry returning `NotNow`; the SCL half asserts the Run clamp and that `SetMovementMode` into the traversal mode is refused for an `Informant`-class pawn (and for a planted `Brace` pawn).
- The IIF's own rejection is owned by the IIF epic; this test asserts SCL's backstop and the observed speed, and records the IIF result as an integration check once that epic's `Traverse` entry exists.
- **Prototype replaced**: the prototype's informant speed cap and traversal block (code existed, never exercised in play, first-playable known issue 4) are replaced by this clamp.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010: general gait clamp and quantiser.
- IIF `Traverse` reply codes (IIF epic).
- Story 047: F5 traversal eligibility itself.

---

## Files

- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.h`
- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthLocomotionModel.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnMovementComponent.cpp`
- `Source/StealthGame/Private/StealthMovementComponent.cpp`
- `Source/StealthFoundation/Public/Server/StealthFaultSink.h`
- `Source/StealthGameplay/Private/Tests/Locomotion/LocomotionTestWorld.h`
- `Source/StealthGameplay/Private/Tests/Locomotion/InformantRestrictionsTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.InformantRestrictions`.*

Test: informant restrictions (AC-SCL-10)
  Given: an informant pawn (stub `IGaitLegalitySource` returning `Informant`)
  When: Run and Traverse are issued and the traversal custom movement mode (stub id) is forced directly
  Then: speed stays ≤ 140; Traverse is rejected `NotNow` by the IIF (stub); SCL refuses the mode switch (backstop)
  Edge cases: re-run with the sample's real traversal mode once spike 045 reports it; a braced pawn is also refused

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/InformantRestrictionsTest.cpp`, test name `StealthGame.Locomotion.InformantRestrictions` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Locomotion.InformantRestrictions`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-010-speed-tiers.md`; `production/epics/input-interaction-framework/EPIC.md` (`Traverse` entry, `NotNow` reply)
- Unlocks: None

---

## Completion Notes
**Completed**: 2026-10-26 (SCL batch: 003, 007, 009-018, 046, 047)
**Criteria**: 1/1 (SCL half): informant Run peaks at 140, effective gait Walk; MOVE_Flying refused for an informant and a braced pawn (MovementMisuse/RefusedTraversalMode to the fault sink); an Active pawn may enter it. The IIF NotNow reply to Traverse is DEFERRED to the IIF epic (no Traverse legality predicate exists yet), as the story allows
**Deviations**: EStealthFault::MovementMisuse = 19 added.
**Test Evidence**: `StealthGame.Locomotion.InformantRestrictions`; full suite 171/171 StealthGame.* Success (`production/qa/evidence/story-010-speed-tiers/full-suite.txt`)
**Run result**: OBSERVED - production/qa/evidence/story-010-speed-tiers/ (WalkSlower informant phases: no Run above walk)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review
