# Story 046: Kit coping heights match the pawn's traversal capability heights

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R7.1 Types, R17.1; AC-SCL-34; LSC "coping means traversable, and only coping does")
**Requirement**: `TR-scl-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 7: traversal capability heights from tuning; heights provisional)
**ADR Decision Summary**: The capability heights (`TraversalVaultHeightCm` 80, `TraversalClimbHeightCm` 160, `TraversalMinHeightCm` 45, `TraversalHeightToleranceCm` 10) are tuning values the LSC kit must equal; only coping is traversable and the kit heights equal SCL's capability heights. The heights are marked provisional pending the traversal spike (story 045).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure data validation at `OnLayoutReady`; no engine API beyond the layout service's manifest. The validation logic does not depend on the spike's final numbers, but a changed capability height changes what passes, so keep fixtures parameterised from the tuning snapshot.

**Control Manifest Rules (this layer)**:
- Required: round and every feature reach layout only through `UStealthLayoutService`; validation uses the frozen tuning snapshot.
- Forbidden: never add a field to `FLayoutManifest` beyond the ADR-0014 section 8.1 shape.
- Guardrail: validation within the layout service's `Preparing` budgets.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-34** [U] `TraversalHeightsMatchKit`: GIVEN the tuning snapshot and the kit coping manifest, WHEN validated at `OnLayoutReady`, THEN every coping height is within `TraversalHeightToleranceCm` of a capability height (LSC cross-check).

*QA clarification (create-stories 2026-10-25, not a GDD change):* an out-of-tolerance coping makes the validator report the offending coping id and height and fail layout validation.

---

## Implementation Notes

*Derived from GDD R7.1 and LSC:*

- Add the cross-check to the layout validation: for each coping record, find a capability height (vault, climb) within `TraversalHeightToleranceCm`; otherwise report id and height and fail. Test with fixtures of copings inside and outside tolerance built from the tuning snapshot values (not hard-coded numbers).
- The LSC epic owns the coping manifest and the kit; this story owns the SCL side of the cross-check (LSC cross-check).
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002: the height-band classifier. Story 047: F5 eligibility for a given press.
- LSC kit authoring (`production/epics/level-socket-contract/EPIC.md`).

---

## Files

- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.h`
- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthLocomotionModel.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnMovementComponent.cpp`
- `Source/StealthGameplay/Private/Tests/Locomotion/TraversalHeightsMatchKitTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.TraversalHeightsMatchKit`.*

Test: traversal heights match the kit (AC-SCL-34)
  Given: the tuning snapshot, the kit coping manifest, and fixtures with copings inside and outside tolerance built from the tuning values
  When: validated at `OnLayoutReady`
  Then: every coping height is within `TraversalHeightToleranceCm` of a capability height (LSC cross-check); an out-of-tolerance coping makes the validator report the coping id and height and fail layout validation
  Edge cases: capability heights and tolerance are provisional pending spike 045, so fixtures read them from the snapshot rather than hard-coding

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/TraversalHeightsMatchKitTest.cpp`, test name `StealthGame.Locomotion.TraversalHeightsMatchKit` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Locomotion.TraversalHeightsMatchKit`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-002-body-constants-published.md`, `story-003-tuning-keys-registered.md`; `production/epics/level-socket-contract/EPIC.md` (coping manifest, `OnLayoutReady`)
- Unlocks: None

---

## Completion Notes
**Completed**: 2026-10-26 (SCL batch: 003, 007, 009-018, 046, 047)
**Criteria**: 1/1: kit copings at 80/120/160 (+- tolerance) validate; out-of-tolerance copings fail with id and height; bands follow the snapshot
**Deviations**: The LSC coping manifest and its OnLayoutReady call are the LSC epic's (not built): StealthLocomotion::ValidateKitCopingHeights is the SCL side it calls.
**Test Evidence**: `StealthGame.Locomotion.TraversalHeightsMatchKit`; full suite 171/171 StealthGame.* Success (`production/qa/evidence/story-010-speed-tiers/full-suite.txt`)
**Run result**: N/A - layout validation has no presentation
**Reviewed by**: lead-programmer focused review (sonnet) + self-review
