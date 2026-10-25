# Story 016: Predicates V-EMERGE and V-SOCK alley shape — witness sets

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-006`
*(At write time: TR-lsc-006 = Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Witness sets W(O) are computed once at Validate with physics line traces on the `ViewBlock` channel (eyes 1.6 m standing / 3.2 m climb tops; targets head 1.75 m and torso 1.0 m) and stored as cell sets for the runtime clear check; if the occlusion predicate cannot be met the layout fails, an effect is never added to hide it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Predicate logic runs against `ILayoutMeasureBackend`; these tests use analytic fixture alleys on the grid backend, so no navmesh is needed.

**Why this story**: An emergence socket is only valid if no witness can see its origin cell, which is the whole Pillar-1 basis for NPC appearance. Read first: R3 EmergenceSocket row, R5 item 2, V-EMERGE, AC-8.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every sight test uses the `ViewBlock` channel (ADR-0014, ADR-0015); every V-* predicate exists once (ADR-0014)
- Forbidden: Never fork predicate logic between backends (ADR-0014)
- Guardrail: witness sets precomputed at Validate; no run-time traces for the clear check (ADR-0014 7.2)

---

## Acceptance Criteria

*From GDD AC-8, scoped to this story:*

- [ ] GIVEN analytic fixture alleys with hand-computed witness sets, THEN W(O) equals the expected cell set; 37 cells passes, 38 fails (`Validate.EmergenceOcclusion`)
- [ ] GIVEN a 7 m Z leg, a 9 m straight alley, O 5 m deep, a 1.0 m origin, two sockets in one alley, sockets 11 m apart, and an ExitPoint 1.5 m or 7 m from O or 5 m from a bush, THEN each fails with its named rule (`Validate.AlleyShape`)

---

## Implementation Notes

- W(O) is every walkable cell within `EmergenceWitnessRangeCm` (32 m, never below `SightlineMaxCm`) with line of sight to O; `|W(O)|` at most `EmergenceWitnessCellsMaxCount` (37; hard ceiling 55); `OriginCell` at least 1.2 x 1.2 m.
- Shape rules: Z- or L-shaped alley with legs of at most 6 m preferred; a straight alley at least 10 m long with O at least 6 m deep; one socket per alley; sockets at least `EmergenceSpacingCm` (12 m) apart; ExitPoint on the navmesh 2-6 m walk from O and at least 6 m from any bush, court or traversal point.
- Boundary passes: 6 m leg, 10 m straight, 6 m deep, 1.2 m origin, 12 m spacing, ExitPoint at 2 m and 6 m, bush at 6 m. A climb-top eye (3.2 m) adds cells; the 32 m range boundary is tested.
- Each failure carries its named rule (V-EMERGE or V-SOCK per R3/R5.2): nine failing fixtures in total.
- The same eye/target samples are reused by respawn (story 033).

---

## Out of Scope

- Runtime clear check (story 030); respawn unseen test (story 033); V-SIGHT (story 017)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/Predicates/EmergencePredicates.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/EmergencePredicatesTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Validate.EmergenceOcclusion`
  - Given: analytic alley fixtures with hand-computed witness cell sets.
  - When: W(O) is computed (32 m range, eyes 1.6 m / 3.2 m climb top).
  - Then: W(O) equals the expected set; |W| = 37 passes, 38 fails.
  - Edge cases: a climb-top eye adds cells; range 32 m boundary.
- **AC-2** `Validate.AlleyShape`
  - Given: fixtures for a 7 m Z leg, 9 m straight alley, O 5 m deep, 1.0 m origin, two sockets in one alley, sockets 11 m apart, ExitPoint 1.5 m / 7 m from O, ExitPoint 5 m from a bush.
  - When: validated.
  - Then: each fails naming its own rule, 9 failures total.
  - Edge cases: boundary passes (6 m leg, 10 m straight, 6 m deep, 1.2 m origin, 12 m spacing, ExitPoint 2 m and 6 m, bush 6 m).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/EmergencePredicatesTest.cpp` — `StealthGame.Layout.Validate.EmergenceOcclusion`, `StealthGame.Layout.Validate.AlleyShape` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-014-socket-records-determinism-required-data.md`
- Unlocks: `production/epics/level-socket-contract/story-030-emergence-request-clear-check-uniform-pick.md`, `production/epics/level-socket-contract/story-033-respawn-pick-eligibility-unseen-draw.md`
