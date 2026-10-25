# Story 023: Server-only query API and leak blindness

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-009`
*(At write time: TR-lsc-009 = Server-only query API for site assignment and team spawns)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (with ADR-0003 for the replication audit)
**ADR Decision Summary**: `GetSiteAssignment_Server()`, `GetTeamSpawn_Server(Team)` and `GetSocketView_Server()` live only in `Public/Server/`; Crowd's density function receives a filtered view with no assignment access (an attempted read fires the server misuse branch); the assignment is `ServerOnly` until the Visibility Policy releases it with the Selection sub-seed to `PostRound` in `Ended`.
**ADR Version**: 2026-10-24 (ADR-0014); 2026-10-24 (ADR-0003)

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0003 networking)
**Engine Notes**: The leak test scans replicated property values and RPC payloads on every client with the ADR-0008 multi-client harness and raw-bunch capture. Use an ensure/custom-assert hook (not `check`) so the misuse-branch test survives.

**Why this story**: The role assignment is the secret the whole game rests on; this story proves no client can see it and that the placement density cannot read it. Read first: R1 data classes, R4 item 7, R9 item 5, Interactions with Other Systems, AC-37, AC-39.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Round and every feature reach layout only through `UStealthLayoutService` (ADR-0014)
- Forbidden: Never log `FSiteAssignment`, zone -> team, respawn picks or any sub-seed value outside the server-only Event Log header (ADR-0014); never replicate any property outside the ADR-0003 allowlist
- Guardrail: Layout runtime services at most 0.1 ms average in `Active` (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-37, AC-39 and R4.7, scoped to this story:*

- [ ] GIVEN a 2-client PIE through `Preparing` and `Active`, THEN no replicated property or RPC payload on any client holds `FSiteAssignment`, used zones, zone -> team, roles, respawn regions or sub-seeds; in `Ended` the assignment and Selection sub-seed are released to `PostRound` (`Leak.NoAssignmentOnClient`)
- [ ] GIVEN one seed and two different `FSiteAssignment`s, THEN an instrumented assignment query from Crowd fires the server assert, and Crowd's NPC placement and density output are identical (identical-output half ADVISORY until the real Crowd exists) (`Leak.DensityBlindToAssignment`)
- [ ] THEN `GetSiteAssignment_Server`, `GetTeamSpawn_Server` and `GetSocketView_Server` are declared only in `Public/Server/` headers and a presentation-module include of them fails the build guard (`Leak.ServerApiHeaderGuard`, proposed; the negative-compile check is DEFERRED to CI)

---

## Implementation Notes

- `FSiteAssignment`, zone records and respawn regions live in the service's server memory only; the only serialized forms are the server-only Event Log header (story 004) and the `PostRound` release the Visibility Policy performs in `Ended`.
- Leak scan: plant a sentinel byte pattern for the assignment, zones, roles and sub-seeds; scan GameState, PlayerState, pawns and sub-objects on every client plus RPC payloads; include a late-joining client.
- Crowd gets a filtered `FLayoutSocketView` through a Crowd-facing interface; Crowd's density function may not read `FSiteAssignment`, zone roles or player positions (server assert); the two-assignment test uses a Crowd stub until the real Crowd exists.
- Header placement follows ADR-0007: server-only headers carry the compile-time guard and a presentation module has no dependency on them (lint on declarations and Build.cs).
- The host can read every secret from its own memory (accepted; Open Question 10, covered in story 045).

---

## Out of Scope

- Story 022: the draw; story 037: BushSite; Visibility Policy release mechanics (`production/epics/information-visibility-policy/EPIC.md`); the real Crowd (`production/epics/crowd-npc-behaviour/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Public/Server/Core/StealthLayoutService.h` (query API)
- `Source/StealthGameplay/Public/Server/Core/StealthLayoutSocketView.h`
- `Source/StealthTestHarness/Private/LayoutLeakTest.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/DensityBlindTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Leak.NoAssignmentOnClient`
  - Given: a 2-client PIE with a sentinel byte-pattern assignment, zones, roles and sub-seeds.
  - When: Preparing and Active run; then Ended.
  - Then: the sentinel appears in 0 replicated property values and 0 RPC payloads on any client; in Ended the assignment and Selection sub-seed are released to PostRound.
  - Edge cases: scan GameState, PlayerState, pawns and subobjects; late-joining client.
- **AC-2** `Leak.DensityBlindToAssignment`
  - Given: one seed and two different `FSiteAssignment`s.
  - When: an instrumented Crowd query reads the assignment; Crowd density runs for both.
  - Then: the server assert fires; NPC placement and density output are identical (ADVISORY until the real Crowd exists).
  - Edge cases: use an ensure/custom-assert hook, not `check`.
- **AC-3** `Leak.ServerApiHeaderGuard` (proposed)
  - Given: the Public/Server headers and presentation-module Build.cs.
  - When: declarations and include paths are linted.
  - Then: the three server queries are declared only there, and the presentation module has no dependency on it.
  - Edge cases: the negative-compile build-guard check is DEFERRED to CI.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/LayoutLeakTest.cpp` and `Source/StealthGameplay/Private/Tests/Layout/DensityBlindTest.cpp` — `StealthGame.Layout.Leak.NoAssignmentOnClient`, `.DensityBlindToAssignment`, `.ServerApiHeaderGuard` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-020-crowd-route-prebaked-paths.md`, `production/epics/level-socket-contract/story-022-selection-secret-draw-streams.md`
- Cross-epic: `production/epics/information-visibility-policy/EPIC.md` (`PostRound` release), `production/epics/tuning-data-config/EPIC.md` (harness)
- Unlocks: `production/epics/level-socket-contract/story-036-crowd-and-objectives-contract-tests.md`, `production/epics/level-socket-contract/story-038-batch-audits-pillar1.md`
