# Story 008: NPC and player pawns have identical first bunches

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R2 last paragraph, R13; AC-SCL-04)
**Requirement**: `TR-scl-001`, `TR-scl-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 1 pawn net parity; with ADR-0010 section 3.1)
**ADR Decision Summary**: Uniform pawn net configuration for NPC and player: a pawn is spawned, appearance-applied and bound before `FinishSpawning`, so the first bunch any client receives already carries `AppearanceId`, the final controller-dependent fields and no binding, with the same property set, size and net tick for both kinds.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Legacy replication driver (Iris pinned off). `NetClientTicksPerSecond` is pinned to 2400 so the listen host considers every client each frame (ADR-0003 E15). Spike S1 (`production/qa/spikes/2026-10-25_S1-S2-S14.md`) found `RemoteViewPitch16` as a player-only property in the prototype: this test is the guard that catches that class of difference.

**Control Manifest Rules (this layer)**:
- Required: `AppearanceId` is written only by the factory via `ApplyAppearance(Id)` before `FinishSpawning`, replicated `COND_InitialOnly` with a RepNotify (ADR-0012); every pawn has identical replicated components and subobjects.
- Forbidden: no multicast RPC on the pawn; nothing replicated beyond the allowlist.
- Guardrail: `AppearanceId` about 100-150 B per client per round (ADR-0012).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-04** [N] `SpawnBunchParity`: GIVEN one NPC and one player pawn spawned by the factory in one frame, WHEN a third client captures each pawn's first received bunch, THEN both carry `AppearanceId` ≠ 255 and have identical replicated-property name sets, bunch sizes within 4 bytes of each other, and arrival within the same net tick (one 1/15 s update), over 20 spawn pairs with a fixed seed.

---

## Implementation Notes

*Derived from ADR-0003 section 1 and ADR-0008 (PIE network tests, raw-bunch capture):*

- This is a CQTest `NETWORK_TEST_CLASS` in `Source/StealthTestHarness/Private/` using the harness's raw-bunch capture on a third client (from the TDC epic's Exit A bootstrap). Use a fixed-seed RNG for the 20 spawn pairs and the NPC/player placement.
- The test fails first if anything makes the kinds differ: an NPC-only or player-only replicated property, a different dirty set at spawn, a different net tick of first send. Do not "fix" parity inside the test by masking properties: fix the pawn.
- Include the pitch path: `RemoteViewPitch16` must be written for both kinds (story 022 owns the write); until it lands the test may record that property as the known differentiator and the story is not Done.
- **Prototype replaced**: the prototype's player-only `RemoteViewPitch16` and the sample's `CharacterInputState` (SkipOwner) and `TraversalResult` (SimulatedOnly) are the measured differentiators (spike S1); story 032 removes them from the allowlist surface.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 030: replication settings dump (update frequency, priority, dormancy, relevancy flags).
- Story 032: allowlist and RPC enumeration.
- Story 022: the `RemoteViewPitch16` write.

---

## Files

- `Source/StealthTestHarness/Private/Locomotion/SpawnBunchParityTest.cpp` (planned; CQTest name `StealthGame.Locomotion.PIE.SpawnBunchParity.NpcVsPlayer`)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. Test `StealthGame.Locomotion.PIE.SpawnBunchParity.NpcVsPlayer`.*

Test: spawn bunch parity (AC-SCL-04)
  Given: one NPC and one player pawn spawned by the factory in one frame, over 20 spawn pairs with a fixed seed
  When: a third client captures each pawn's first received bunch
  Then: both carry `AppearanceId` ≠ 255; the replicated property-name sets are identical; bunch sizes are within 4 bytes of each other; arrival is within the same net tick (one 1/15 s update)
  Edge cases: the capture needs the harness raw-bunch hook; 255 is the unset sentinel

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/SpawnBunchParityTest.cpp` — must exist and pass (multi-client PIE network test, not player-observable, no screenshot)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-006-single-spawn-site.md`; `production/epics/tuning-data-config/EPIC.md` (Exit A: multi-client harness, raw-bunch capture)
- Unlocks: `production/epics/shared-character-locomotion/story-030-net-config-parity.md`
