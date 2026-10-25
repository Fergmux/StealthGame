# Story 036: Crowd and Objectives contract tests

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-006`, `TR-lsc-009`
*(At write time: TR-lsc-006 = Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn; TR-lsc-009 = Server-only query API for site assignment and team spawns)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0019 for the Crowd release interface)
**ADR Decision Summary**: Crowd's every spawn and absorption of a living NPC goes through `RequestEmergence`; Objectives reads `FSiteAssignment` only through the server query API and, if it has court actors, one per court with identical class and net settings spawned in SocketId order.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: These are cross-system contract tests: ADVISORY (DEFERRED) until the real Crowd, Objectives and the Crowd GDD matrix exist; the static dependency test can run now.

**Why this story**: It pins the two neighbouring systems to the contract so neither can quietly bypass it. Read first: R5 item 6 (ambient baseline), R4 item 7, Dependencies (Downstream), AC-50, AC-51, AC-54.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Round and every feature reach layout only through `UStealthLayoutService`; any future per-court or per-bush replicated object exists on every court or bush with identical class and net settings, spawned in SocketId order (ADR-0014)
- Forbidden: Never let anything but the service read the assignment outside its server API (ADR-0014)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-50, AC-51 and AC-54 (revised after QL-STORY-READY: rate wording and DEFERRED markers), scoped to this story:*

- [ ] GIVEN a 10-minute graybox simulation with the real Crowd, THEN 100% of living-NPC spawns and absorptions after t0 (corpse removals excluded) go through `RequestEmergence` (spawn-path counter), and the scheduler's configured entry and exit rates are each at least `AmbientEmergencePerSocketMinPerMin` x N_E per minute (DEFERRED until the Crowd epic exists) (`Cross.Crowd.EmergenceApiAndFloor`)
- [ ] GIVEN a static dependency test, THEN Objectives reads `FSiteAssignment` only through the server query API; if Objectives actors exist, every court has one with identical class and net settings, spawned in SocketId order (actor parity ADVISORY until Objectives exists) (`Cross.Objectives.AssignmentServerOnly`)
- [ ] GIVEN the Crowd GDD, THEN its Baseline and Tell Matrix has the rows "emerging from or leaving by an alley" (deviation none; NPCs do it: yes; owners LSC + Crowd) and "takeover of a crowd pawn" (deviation none; owner Crowd + Shared Character) (document review, ADVISORY until the Crowd GDD matrix exists)

---

## Implementation Notes

- Spawn-path counter: every living-NPC spawn and absorption after t0 increments a counter at the emergence entry; the test asserts counter == observed spawns+absorptions (corpse removal excluded).
- Rates are read from the configured scheduler values (not noisy observed counts) against `AmbientEmergencePerSocketMinPerMin` (0.5) x N_E (13 -> 6.5 per minute each for entries and exits).
- Static dependency test: scan Objectives sources/Build.cs for any access to `FSiteAssignment` other than `GetSiteAssignment_Server()`; direct struct access fails.
- Court-actor parity (if Objectives actors exist): same class and net settings on every court, spawned in SocketId order (ADR-0014 7.4 / Objectives OC-8).
- The matrix review is a document check recorded in the evidence sheet; ADVISORY until the Crowd GDD matrix is written.

---

## Out of Scope

- Story 030/031: the emergence implementation; Objectives and Crowd implementations (`production/epics/objectives-chain/EPIC.md`, `production/epics/crowd-npc-behaviour/EPIC.md`)

---

## Files

- `Source/StealthTestHarness/Private/CrowdEmergenceContractTest.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/ObjectivesAssignmentDependencyTest.cpp`
- `production/qa/evidence/level-socket/crowd-matrix-review.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE; AC-1 and AC-3 DEFERRED until Crowd / the Crowd GDD matrix exist).*

- **AC-1** `Cross.Crowd.EmergenceApiAndFloor` (DEFERRED until the real Crowd exists)
  - Given: a 10-minute graybox simulation with the real Crowd.
  - When: spawns and absorptions after t0 are counted (corpse removals excluded).
  - Then: 100% go through `RequestEmergence` (spawn-path counter); configured entry and exit rates each >= 0.5 x N_E per minute.
  - Edge cases: rates read from configuration, not observed counts.
- **AC-2** `Cross.Objectives.AssignmentServerOnly`
  - Given: Objectives sources.
  - When: a static dependency scan runs; if Objectives actors exist, courts are enumerated.
  - Then: `FSiteAssignment` is read only through the server query API; every court has an actor with identical class and net settings, spawned in SocketId order (parity ADVISORY).
  - Edge cases: direct struct access fails.
- **AC-3** document review (DEFERRED until the Crowd GDD matrix exists)
  - Setup: open the Crowd GDD Baseline and Tell Matrix.
  - Verify: rows "emerging from or leaving by an alley" (deviation none; NPCs do it yes; owners LSC + Crowd) and "takeover of a crowd pawn" (deviation none; owner Crowd + Shared Character).
  - Pass condition: both rows present as stated.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CrowdEmergenceContractTest.cpp` and `Source/StealthGameplay/Private/Tests/Layout/ObjectivesAssignmentDependencyTest.cpp` — `StealthGame.Layout.Cross.Crowd.EmergenceApiAndFloor`, `StealthGame.Layout.Cross.Objectives.AssignmentServerOnly`, plus the matrix review sheet

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-023-server-query-api-and-leak-blindness.md`, `production/epics/level-socket-contract/story-031-emergence-kill-decoupling-and-rate-bound.md`, `production/epics/level-socket-contract/story-035-respawn-takeover-integration.md`
- Cross-epic: `production/epics/crowd-npc-behaviour/EPIC.md`, `production/epics/objectives-chain/EPIC.md`
- Unlocks: None
