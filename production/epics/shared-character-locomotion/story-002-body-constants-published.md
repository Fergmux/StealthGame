# Story 002: Body constants published: step height, floor angle, capsule, kerb and block heights

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R17 Body and collision, R17.1 body constants published for the kit; AC-SCL-48)
**Requirement**: `TR-scl-013` (one capsule, collision profile and physics asset)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: One capsule size, one collision profile, one physics asset for every pawn; `MaxStepHeight = TraversalMinHeightCm` (one key); walkable floor angle at the engine default; capsule radius and half-height are the reparented sample character's values, read once and published to LSC and the Generator from the class default object.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `ACharacter` defaults are capsule 34 / 88 cm; the sample character's real values are `[verify in the 5.8.4 project]` (GDD R17.1). Walkable floor angle default is 44.765 degrees (the 40 degree temple stair passes with about 4.8 degrees margin; do not lower it). A 0.4 m kerb is below the step height and is stepped onto whether or not it carries coping.

**Control Manifest Rules (this layer)**:
- Required: one capsule size, collision profile and physics asset on every pawn; `MaxStepHeight = TraversalMinHeightCm`; the body values come from the class defaults and are the same for every pawn (ADR-0010).
- Forbidden: no per-instance tuning or component for NPCs or players; never read controller kind in a movement rule.
- Guardrail: none specific (no per-tick cost).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-48** [U] `BodyConstantsPublished` (R17.1): GIVEN the Shared Pawn class defaults, WHEN read, THEN `MaxStepHeight` equals `TraversalMinHeightCm` (45), the walkable floor angle is 44.765° (the engine default), capsule radius and half-height equal the values published to the Generator (read from the reparented sample character, R17.1 [verify]); AND in a test map a 40 cm kerb is stepped onto, a 46 cm and a 69 cm block (not traversable) and a coping at 80 cm is a vault.

*QA clarification (create-stories 2026-10-25, not a GDD change):* capsule literals are committed as test constants once R17.1 is verified (until then that assertion is gated); the kerb and block map walk is part of this test; the 80 cm vault clause is asserted here through the pure `ClassifyCopingHeight` classifier and in the world by story 047.

---

## Implementation Notes

*Derived from ADR-0010 Decision section 2 (Body) and GDD R17.1:*

- Set capsule radius and half-height, collision profile and physics asset once in the `AStealthPawn` class defaults; read the sample character's actual capsule in the graybox, record it in the class defaults, and publish the values to LSC and the Generator from the class default object (Procedural City Generator change request 4). Record the verified numbers in the story's completion notes: the GDD still marks them `[verify]`.
- `MaxStepHeight` is not an independent value: it is assigned from the `TraversalMinHeightCm` tuning key (one key, two uses). The key is a `Client`-audience TDC value latched per round, so assign it where the movement component reads the frozen snapshot, not in the constructor.
- A kerb or edge meant to stop a pawn must be 46-69 cm or carry a pawn-blocking volume (routed back to the Generator): the test map therefore uses 46 and 69 cm blocks as the "blocks, not traversable" cases.
- The "coping at 80 cm is a vault" clause is asserted through a pure height-band classifier introduced here (`ClassifyCopingHeight(heightCm)` returning Step, Vault, Climb or Block from `TraversalMinHeightCm`, `TraversalVaultHeightCm`, `TraversalClimbHeightCm` and `TraversalHeightToleranceCm`). Story 047 (F5 eligibility, Blocked on the net-emulation/traversal spike) reuses this classifier and adds position, facing and landing rules. The tuning heights are marked provisional pending that spike: a changed value must fail this test loudly, never silently pass.
- **Prototype replaced**: the prototype `AStealthCharacter` inherits the engine capsule and sample reparent values with no recorded constants; nothing is ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 047: full F5 traversal eligibility (reach, facing, landing cell, pawn on the landing).
- Story 046: validation of kit coping heights against capability heights at `OnLayoutReady`.
- Story 041: RVO flag, AnimBP class and database parity (the other half of body parity).

---

## Files


- Source/StealthGameplay/Public/View/Core/StealthLocomotionModel.h, Private/Core/SharedPawn/StealthLocomotionModel.cpp (new: FStealthTraversalHeights, ClassifyCopingHeight, FStealthBodyConstants)
- Source/StealthGameplay/Public/View/Core/StealthPawn.h, Private/Core/SharedPawn/StealthPawn.cpp (capsule 30/86 recorded from CBP_SandboxCharacter; GetPublishedBodyConstants; MotionWarping member renamed SharedMotionWarping, subobject optional)
- Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h, Private/Core/SharedPawn/StealthPawnMovementComponent.cpp (MaxStepHeight = TraversalMinHeightCm via ApplyTraversalHeights)
- Source/StealthGame/Public/StealthCharacter.h, Private/StealthCharacter.cpp, Public/StealthMovementComponent.h (prototype adapter re-based onto AStealthPawn / UStealthMovementComponent)
- Source/StealthGameplay/Private/Tests/Locomotion/BodyConstantsPublishedTest.cpp (new)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.BodyConstantsPublished`.*

Test: body constants published (AC-SCL-48)
  Given: the `AStealthPawn` class defaults and a test map
  When: they are read, the publish function the Generator consumes is called, and the pawn walks onto a 40, 46 and 69 cm obstacle
  Then: `MaxStepHeight` equals `TraversalMinHeightCm` (45); the walkable floor angle is 44.765°; capsule radius and half-height equal the values recorded in the class defaults and published to the Generator; the 40 cm kerb is stepped onto and the 46 and 69 cm blocks are not; `ClassifyCopingHeight` returns vault at 80 cm and none (step or block) at 45, 46 and 69 cm
  Edge cases: exactly 45 cm is a boundary the AC does not state, so it is not asserted; the capsule-literal assertions are gated until GDD R17.1 is verified in the 5.8.4 project (that part is BLOCKED until then); the in-world 80 cm vault is asserted by story 047

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/BodyConstantsPublishedTest.cpp`, test name `StealthGame.Locomotion.BodyConstantsPublished` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Locomotion.BodyConstantsPublished`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md`; `production/epics/shared-character-locomotion/story-003-tuning-keys-registered.md` (registers `TraversalMinHeightCm`); TDC pipeline (`production/epics/tuning-data-config/EPIC.md`, Exit B)
- Unlocks: `production/epics/shared-character-locomotion/story-047-traversal-eligibility-matrix.md`, `production/epics/level-socket-contract/EPIC.md` (consumes the published constants)

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 1/1 passing. Verified R17.1 numbers: CBP_SandboxCharacter capsule radius 30.0 cm, half-height 86.0 cm (UE 5.8.4 project); MaxStepHeight 45; walkable floor angle 44.765
**Deviations**: Depends on SCL-003 (Ready): TraversalMinHeightCm and the vault/climb/tolerance keys are not in the TDC schema yet; FStealthTraversalHeights carries the GDD canonical defaults and ApplyTraversalHeights is the hook the frozen snapshot will call. 'Reparent CBP_SandboxCharacter onto AStealthPawn' was realised by re-basing the prototype AStealthCharacter (the CBP's parent) onto AStealthPawn: no .uasset edit, no redirect needed (decision logged). The sample Blueprint keeps its own MotionWarping component until ADR-0010's Blueprint cleanup.
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Locomotion/BodyConstantsPublishedTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: N/A for the constants; the pawns in integration-spine-2026-10-26 screenshots are the re-based Shared Pawns
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
