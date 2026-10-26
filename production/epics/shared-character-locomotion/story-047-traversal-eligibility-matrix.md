# Story 047: Traversal eligibility matrix (F5) from body facing

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R7 Traversal set, F5 Traversal eligibility; AC-SCL-12)
**Requirement**: `TR-scl-004` (replicated traversal set, spike)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 7)
**ADR Decision Summary**: `Traverse(Hint?)` is a discrete command resolved in a queue slot; the resolver calls `TryStartTraversal`, which re-runs F5 with the server's own query (never trusting the hint, body facing only) after the server has consumed every saved move with a timestamp at or before the command's client timestamp (AI commands validate immediately). `TraversalState` is the one replicated start record.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Provisional pending the traversal spike (heights, movement mode, proxy root-motion policy, E12). F5 evaluates against the server's `CurrentClientTimestamp` consumption (`ServerData->CurrentClientTimeStamp`); a forced position update advances it, bounding the re-submit wait.

**Control Manifest Rules (this layer)**:
- Required: traversal starts only from a server-validated queue slot; eligibility uses body facing, never the camera.
- Forbidden: never trust the client's traversal hint; never use the sample's `Traversal_ServerImplementation`; informants, braced pawns and `Strike`/`HideTransition` pawns cannot traverse.
- Guardrail: traversal traces run only on a `Traverse` press (no per-frame traces).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-12** [U] `TraversalEligibilityMatrix` [SPIKE]: GIVEN coping at 45, 75, 83, 100, 120, 155, 165 and 175 cm, WHEN an idle Active pawn presses Traverse at 70 cm and 20° off facing, THEN results are none, vault, vault, none, hurdle, climb, climb, none (row 120 updated 2026-10-26: the 1.2 m hurdle band made final by spike SCL-045; it read none before the spike); AND with the camera turned 180° but the body unchanged the results are identical; AND with a pawn on the landing cell, rejected.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the landing-cell rejection is tested at the 80 cm coping with reply code `NotNow`; confirm `NotNow` is the IIF's intended code for an SCL-side rejection (AC-10 uses it for informants).

---

## Implementation Notes

*Derived from ADR-0010 section 7 and GDD F5/R7:*

- Implement F5 as a pure function over (coping height, horizontal distance capsule edge to coping at most `TraversalReachCm` (100), angle between body facing and coping normal within `TraversalFacingHalfAngleDeg` (45), landing cell clear for `TraversalLandingClearCm`, pawn on the landing cell) reusing story 002's `ClassifyCopingHeight`. The camera never enters.
- Heights beyond the bands block (45 and 100 and 120 and 175 return none: at or below `TraversalMinHeightCm` it is a step, not a traversal; between vault and climb bands blocks). Re-read the final heights from the spike report before building (a 1.2 m hurdle may be added).
- **Prototype replaced**: prototype traversal is the sample's Blueprint query; its clip selection and chooser are kept, its replication replaced (stories 048/049).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 048: start parity on the wire. Story 049: behaviour under emulation. Story 014: informant backstop.

---

## Files

- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.h`
- `Source/StealthGameplay/Private/Core/Movement/StealthSavedMove.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthLocomotionModel.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthLocomotionModel.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnMovementComponent.cpp`
- `Source/StealthGameplay/Private/Tests/Locomotion/TraversalEligibilityMatrixTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; BLOCKED on spike 045). Test `StealthGame.Locomotion.TraversalEligibilityMatrix`.*

Test: traversal eligibility matrix (AC-SCL-12)
  Given: coping at 45, 75, 83, 100, 120, 155, 165 and 175 cm
  When: an idle Active pawn presses Traverse at 70 cm and 20° off facing
  Then: results are none, vault, vault, none, none, climb, climb, none; with the camera turned 180° and the body unchanged the results are identical; with a pawn on the landing cell the press is rejected (tested at the 80 cm coping, reply `NotNow`)
  Edge cases: confirm `NotNow` is the IIF's intended code for an SCL-side rejection; re-read the final heights from the spike report first

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/TraversalEligibilityMatrixTest.cpp`, test name `StealthGame.Locomotion.TraversalEligibilityMatrix` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Locomotion.TraversalEligibilityMatrix`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-045-net-emulation-traversal-spike.md`, `story-002-body-constants-published.md`, `story-014-informant-restrictions.md`; `production/epics/input-interaction-framework/EPIC.md` (`Traverse`), `production/epics/level-socket-contract/EPIC.md` (coping records)
- Unlocks: `production/epics/shared-character-locomotion/story-048-traversal-start-parity.md`

---

## Completion Notes
**Completed**: 2026-10-26 (SCL batch: 003, 007, 009-018, 046, 047)
**Criteria**: 1/1: 45 none, 75 vault, 83 vault, 100 none, 120 hurdle, 155 climb, 165 climb, 175 none; camera not an input; landing cell occupied -> refused (NotNow at the IIF); legality, slot, grounded, reach, cone, clearance and the selection order
**Deviations**: AC row 120 reads hurdle (the 1.2 m band made final by spike SCL-045, decision log); before the spike the AC read none. TraversalLandingClearCm (LSC, 100) is a pending owner constant. The F5 function is not yet called from a Traverse resolver (story 048).
**Test Evidence**: `StealthGame.Locomotion.TraversalEligibilityMatrix`; full suite 171/171 StealthGame.* Success (`production/qa/evidence/story-010-speed-tiers/full-suite.txt`)
**Run result**: N/A - eligibility function; the traversal start is story 048
**Reviewed by**: lead-programmer focused review (sonnet) + self-review
