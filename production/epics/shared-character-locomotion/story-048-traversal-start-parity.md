# Story 048: An NPC vault and a player vault look identical to an observer

> **Epic**: Shared Character & Locomotion
> **Status**: Ready — unblocked 2026-10-25: spike 045 reported (production/qa/spikes/2026-10-25_SCL045-net-emulation-traversal.md); E12 decision = proxies follow replicated movement (no proxy root-motion montage), movement mode during traversal = MOVE_Flying.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R7.4 Authority, R7.5 Anticipation, R7.6 During traversal; AC-SCL-13)
**Requirement**: `TR-scl-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 7), with ADR-0003 (forced-update scope `TraversalStateWrite`)
**ADR Decision Summary**: On success the server writes `TraversalState {Seq (uint16), TypeId (uint8), PointId (uint32)}` inside `FStealthForcedNetUpdate(TraversalStateWrite)`, sets the motion-warp targets from the coping record, switches to the traversal movement mode and plays the montage. Every client derives the warp targets locally from `PointId` and starts the same anticipation lead-in and clip from `OnRep_TraversalState`, offset by `EstimatedServerNow` so a late arrival starts at the elapsed position. The sample's `Traversal_ServerImplementation` and replicated `TraversalResult` are removed. The owner starts the lead-in locally at the press and cancels on the IIF's owner-only rejection; observers start it at the server's validation.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `UMotionWarpingComponent` is Beta in 5.8.4 (`GetOwnerAdapter()`; the pawn's `UStealthMotionWarpingComponent` disables `WarpTargets` replication so no warp target is ever on the wire). Proxy root motion either plays from `RepRootMotion` (E9) or follows replicated movement (E12): one choice for every pawn, from spike 045. Traversal puts the pawn at rate 1 on every client whatever its tier (ADR-0013 state override).

**Control Manifest Rules (this layer)**:
- Required: traversal starts only from a server-validated queue slot and writes `TraversalState` inside the `TraversalStateWrite` forced-update scope; one proxy root-motion policy applies to every pawn.
- Forbidden: never use the sample's `Traversal_ServerImplementation`/`TraversalResult` replication or a multicast for traversal; never replicate warp targets.
- Guardrail: `TraversalState` 7 bytes per change.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-13** [N] `TraversalStartParity` [SPIKE]: GIVEN an NPC and a player vaulting the same coping, WHEN an observer records onset (first anticipation frame) to landing, THEN clip, anticipation length, duration and trajectory match within 1 frame and 5 cm, and the observer sees no anticipation for a server-rejected owner press.

*QA clarification (create-stories 2026-10-25, not a GDD change):* a clean link is assumed (story 049 covers emulation).

---

## Implementation Notes

*Derived from ADR-0010 section 7 and GDD R7.4-R7.6:*

- Implement `TryStartTraversal` on the pawn (server): F5 (story 047), `TraversalState` write inside the forced-update scope, motion-warp targets from the coping record, traversal movement mode, montage start; `IsInTraversal` true from validation to landing, collision with other pawns kept, Move input buffered and applied on landing. Remove `Traversal_ServerImplementation` and `TraversalResult` from the reparented Blueprint (Content step, explicit path via MCP, snapshot first) and keep the sample's query and clip selection fed from the server's record.
- `OnRep_TraversalState` starts the same anticipation (`TraversalAnticipationSec`, 0.15 s) and clip on observers seeked by `EstimatedServerNow`; owner-side prediction starts the lead-in at the press and cancels on rejection.
- Final proxy policy comes from the spike: apply one policy to every pawn.
- **Prototype replaced**: the sample's Blueprint traversal replication (and prototype `AStealthCharacter` jump/traversal blocks) are replaced.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 047: F5 eligibility. Story 049: emulation behaviour. Hand-over wait during traversal: story 027's pending-request rule uses `IsInTraversal` from here.

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp` (`TryStartTraversal`, `OnRep_TraversalState`)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMovementComponent.cpp` (traversal mode, buffered input)
- `Source/StealthTestHarness/Private/Locomotion/TraversalStartParityTest.cpp` (planned)
- Content: reparented sample Blueprint edit (explicit path via MCP)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (BLOCKED on spike 045). Test `StealthGame.Locomotion.PIE.TraversalStartParity.NpcVsPlayer`.*

Test: traversal start parity (AC-SCL-13)
  Given: an NPC and a player vaulting the same coping on a clean link
  When: an observer records onset (first anticipation frame) to landing
  Then: clip, anticipation length, duration and trajectory match within 1 frame and 5 cm; the observer sees no anticipation for a server-rejected owner press
  Edge cases: emulation is covered by story 049

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/TraversalStartParityTest.cpp` — must exist and pass (multi-client PIE); retained screenshots of an NPC vault and a player vault from an observer in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-045-net-emulation-traversal-spike.md`, `story-047-traversal-eligibility-matrix.md`, `story-032-replicated-allowlist.md`; `production/epics/input-interaction-framework/EPIC.md` (queue slot, owner-only rejection reply)
- Unlocks: `production/epics/shared-character-locomotion/story-049-traversal-under-emulation.md`, `story-053-npc-tell-baseline-non-zero.md`
