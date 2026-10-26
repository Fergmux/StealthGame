# Story 038: The placement share invariant passes at 5 players/5 NPCs and aborts PlacementInvariant at 5/4; GlobalShare matches F5

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-011` (Single shuffled spawn list in one frame, server-secret order); GDD AC 56; formula F5 (`ZoneShare_z`, `GlobalShare`), rules R6 P6, R23.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams; ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: ADR-0006: 128-bit `ServerSecret`, in-project SipHash-2-4 as `H64`, counter-mode secret streams for hidden draws, PCG32 for the public layout stream; domain registry append-only. ADR-0014: `UStealthLayoutService` world subsystem with provider registry, integer `FLayoutManifest` with `LayoutHash`, server navmesh built once and frozen at `OnLayoutReady`. This story applies ADR-0014 section 7.1 (start placement): `RequestStartPlacement` returns transforms only, player cells come from `GetServerSubSeed(PlayerPlacement, LayoutAttempt)`, and Round owns the spawn order and the F5 zone-share assertion, which aborts `PlacementInvariant` before `Active`.
**ADR Version**: ADR-0006 2026-10-24; ADR-0014 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0006; ADR-0014 is MEDIUM)
**Engine Notes**: None touched. The invariant is integer and floating arithmetic over a placement result supplied by a fake Level Socket gateway (story 003); no navigation, replication or post-cutoff API is exercised (ADR-0014's navmesh build lock and `AddInstances` items belong to the Level Socket Contract epic).

**Control Manifest Rules (this layer)** (Core Layer Rules, Layout service):
- Required: Round and every feature reach layout only through `UStealthLayoutService` (ADR-0014); Round reads the placement result only through `RequestStartPlacement`.
- Forbidden: Never let Round and the Generator include each other's headers (ADR-0014).
- Guardrail: Layout (server, `Preparing`): game-thread frames at most 33 ms, at most 45 s worst case including reseeds (ADR-0014); the F5 check is one pass over the placement result.

**Prototype disposition**: none (new Round Core code; the zone-share invariant is Round's assertion over Level Socket Contract output).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 56, scoped to this story:*

- [ ] GIVEN placement fixtures, THEN 5 players with 5 NPCs in a zone (share 0.5) passes and 5 with 4 aborts `PlacementInvariant` before Active; an empty zone has no share; `GlobalShare` is 0.167 at 5v5/50 and 0.074 at 2v2/50.

---

## Implementation Notes

*Derived from GDD F5 and R6 P6, ADR-0002 section 8.6 and ADR-0014 section 7.1:*

- **Formulas (GDD F5).** `ZoneShare_z = P_z / (P_z + N_z)`, defined only for zones with `P_z > 0` (an empty zone has no share); required `ZoneShare_z <= 0.5`, asserted by Round (guaranteed by Level Socket Contract R6.2/D6). `GlobalShare = P / (P + NpcCount)` with `P` the participants this round.
- **Worked values.** 5 players with 5 NPCs: `5 / (5 + 5) = 0.5`, passes. 5 players with 4 NPCs: `5 / (5 + 4) = 0.556`, aborts `PlacementInvariant` (the GDD's example reads 0.56 for 5 with 4). `GlobalShare` at 5v5 with 50 NPCs: `10 / (10 + 50) = 0.167`; at 2v2 with 50 NPCs: `4 / (4 + 50) = 0.074`.
- **Abort timing.** The check runs at P6 after `RequestStartPlacement` returns and before any pawn is spawned, so the abort happens in `Preparing` and `Active` is never entered. The abort path itself (return to Lobby, archive `PreparingFailed(PlacementInvariant)`) is story 034; this story only asserts that the invariant raises that code through the abort entry. `PlacementInfeasible` and `NpcCountTooLow` returned by the Level Socket Contract abort with their own codes (GDD R6 P6) and are not asserted here.
- **Tolerance.** Compare `GlobalShare` with a tolerance of half the last printed digit (0.0005 for 0.167, 0.0005 for 0.074).
- **Inputs from fixtures.** Placement fixtures are fake Level Socket gateway results (zone, `P_z`, `N_z`); the constants live in the Round test constants files (story 005), not inline.
- **Spawn order is separate.** `sigma` (stories 036 and 037) is independent of the placement share; this story does not touch it.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 003 (`production/epics/round-match-framework/story-003-test-seam-fake-gateways.md`): the fake Level Socket gateway (this story's dependency).
- Story 036 (`production/epics/round-match-framework/story-036-spawn-order-uniform.md`): spawn-order uniformity (dependency).
- Story 039 (`production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md`): the one-frame spawn and the Connecting gate (this story unlocks it).
- Story 034 (`production/epics/round-match-framework/story-034-preparing-aborts.md`): the abort-to-Lobby cleanup for `PlacementInvariant`.
- Level Socket Contract internals (placement, zone fill, I-LSC-1, `PlacementInfeasible`, `NpcCountTooLow`) and Crowd NPC placement.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Start/PlacementInvariant.h`
- `Source/StealthGameplay/Private/Round/Start/PlacementInvariant.cpp`
- `Source/StealthGameplay/Private/Tests/Round/PlacementInvariantTest.cpp` (class `FRoundPlacementInvariantTest`, test name `StealthGame.Round.PlacementInvariant`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: The placement share invariant passes at 5 players/5 NPCs and aborts PlacementInvariant at 5/4, and GlobalShare matches F5.
  - Given: placement fixtures.
  - When: each zone fixture is evaluated.
  - Then: 5 players with 5 NPCs (share 0.5) passes.
  - Then: 5 players with 4 NPCs aborts `PlacementInvariant` before Active.
  - Then: an empty zone has no share.
  - Then: `GlobalShare` is 0.167 at 5v5/50 and 0.074 at 2v2/50.
  - Edge cases: use a tolerance of half the last printed digit; the numbers are consistent with players / (players + NPCs); the abort happens in Preparing, so Active is never entered.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/PlacementInvariantTest.cpp` (automation test `StealthGame.Round.PlacementInvariant`, class `FRoundPlacementInvariantTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.PlacementInvariant`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-003-test-seam-fake-gateways.md`, `production/epics/round-match-framework/story-036-spawn-order-uniform.md`
- Unlocks: `production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md`
- Cross-epic: `production/epics/level-socket-contract` (`RequestStartPlacement`, zone results, `PlacementInfeasible`, `NpcCountTooLow`; stories not yet created), `production/epics/tuning-data-config` (`NpcCount`, module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (5/5 passes, 5/4 aborts PlacementInvariant before any spawn and before Active, empty zone has no share, GlobalShare 0.167 and 0.074)
**Deviations**: The Level Socket Contract is not built: the prototype layout reports no zones (GetPlacementZones empty), so the invariant has nothing to assert in L_Prototype yet.
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/PlacementInvariantTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: N/A - the prototype provider reports no zones
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
