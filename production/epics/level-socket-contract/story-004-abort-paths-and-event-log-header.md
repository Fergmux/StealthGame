# Story 004: Abort paths and Event Log header

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-005`
*(At write time: TR-lsc-005 = Reseed and graybox fallback travel)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: When no attempt can be accepted the service aborts to Lobby with `LayoutFallbackLevel` 3 (graybox failure, Generated without fallback, no provider, `PlacementInfeasible`); attempt records, final disposition and the Selection sub-seed value go only into Round's server-only Event Log header, and `ServerSecret` never does.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API; the Event Log header store belongs to Round and Session (ADR-0005, ADR-0007 section 6: Event Log content never passes any log, message or trace path).

**Why this story**: It defines every way a round can fail to start and what is recorded about it. Read first: R8 items 3-5, States table (`Aborted`), AC-43, AC-44.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Draw sites use ADR-0006 shapes and stream tags (ADR-0014)
- Forbidden: Never log `FSiteAssignment`, zone -> team, respawn picks or any sub-seed value outside the server-only Event Log header (ADR-0014)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-43 and AC-44 (revised after QL-STORY-READY: thresholds and sentinel scan defined), scoped to this story:*

- [ ] GIVEN Generated exhausted, THEN Lobby with level 3; GIVEN graybox fails validation, THEN Lobby with level 3; GIVEN Generated with no provider registered, THEN immediate `NoProvider`; GIVEN fewer than `TeamSize` (5) free start cells in a used zone after `Published` (a start cell lost), THEN `PlacementInfeasible` and Lobby (`Fallback.AbortPaths`)
- [ ] GIVEN any round disposition (Accepted, FellBack, Aborted), THEN the Event Log header holds `LayoutId`, `Seed`, `LayoutAttempt`, `LayoutFallbackLevel`, the Selection sub-seed value, the `ObjectiveSites` and `PlayerPlacement` sub-seed values, the per-request stream roots and the disposition, and a sentinel `ServerSecret` (raw, hex and decimal forms) appears in 0 bytes of the serialised header, captured log output and any trace file the test writes (`Fallback.EventLogHeader`)

---

## Implementation Notes

- `LayoutFallbackLevel`: 0 first attempt accepted, 1 a reseed accepted, 2 hand-built fallback, 3 abort to Lobby; it is a different field from TDC's `FallbackLevel`. Disposition `Accepted` / `FellBack` / `Aborted`.
- Abort from any state on: graybox failed validation, Generated without fallback exhausted, `PlacementInfeasible`, `NpcCountTooLow`, listen-server host lost (host-loss test is story 005).
- `GeneratedWithFallback` exhausted goes to `FallbackTravel` (story 005), never straight to Lobby. Exactly `TeamSize` free cells does not abort.
- The header write goes through Round's server-only Event Log API (cross-epic); the attempt records come from `GetAttemptRecords_Server()`.
- R8.5 lists only the Selection sub-seed value while AC-44 says "every LSC sub-seed value"; this story records the superset (flagged to the GDD owner; the Selection and `ObjectiveSites` domains may be one domain, so assert the set per the fixture).

---

## Out of Scope

- Story 005: fallback travel and host-loss; story 003: reseed loop; story 025/029: placement itself

---

## Files

- `Source/StealthGameplay/Private/Core/LayoutServiceCore.cpp` (abort paths)
- `Source/StealthGameplay/Private/Core/LayoutEventLogHeader.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/AbortPathsTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Fallback.AbortPaths`
  - Given: four fixtures: Generated exhausted; graybox failing validation; Generated with no provider registered; after Published a used zone with 4 free start cells (TeamSize 5).
  - When: each is run.
  - Then: Lobby with level 3; Lobby with level 3; immediate `NoProvider`; `PlacementInfeasible` and Lobby.
  - Edge cases: exactly 5 free cells does not abort; `GeneratedWithFallback` exhausted goes to `FallbackTravel`, not Lobby.
- **AC-2** `Fallback.EventLogHeader`
  - Given: a round ending Accepted, FellBack and Aborted; a sentinel `ServerSecret`.
  - When: the Event Log header is serialised, log output captured, trace files written.
  - Then: header holds the fields above; the sentinel (raw, hex, decimal) appears in 0 bytes of header, logs and traces.
  - Edge cases: secret-derived values are server-only.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/AbortPathsTest.cpp` — `StealthGame.Layout.Fallback.AbortPaths`, `StealthGame.Layout.Fallback.EventLogHeader` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-002-service-state-machine-and-time-slicing.md`, `production/epics/level-socket-contract/story-003-attempt-loop-timeout-budget-reseed.md`
- Cross-epic: `production/epics/round-match-framework/EPIC.md` (Event Log header; stub until it exists)
- Unlocks: `production/epics/level-socket-contract/story-005-fallback-travel-and-host-loss.md`
