# Story 005: Fallback travel to the graybox and host-loss abort

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-005`
*(At write time: TR-lsc-005 = Reseed and graybox fallback travel)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: On exhaustion in `GeneratedWithFallback` the service calls Session's `RequestFallbackTravel(GrayboxMapId)` (a second seamless travel; level streaming rejected), enters `FallbackTravel`, and the new world's service starts `Idle` with `ProviderKind = Graybox`; the round-scoped Event Log header and attempt records ride Round's match store across it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Seamless travel in 5.8.4 PIE may not be exercisable; if not, use the multi-process harness (ADR-0008) and mark the criterion DEFERRED until it exists. Host-loss bounds come from Session F6 (graceful quit about 0.55 s plus one frame; process kill about 12.3 s).

**Why this story**: It proves the permanent fallback works across a world change and that a lost host aborts cleanly. Read first: R8 item 3, States (`FallbackTravel`, `Aborted`), AC-35, AC-42; ADR-0014 8.5.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Round and every feature reach layout only through `UStealthLayoutService` (ADR-0014)
- Forbidden: Never log sub-seeds or `FSiteAssignment` outside the Event Log header (ADR-0014)
- Guardrail: <= 45 s worst case including reseeds (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-42 and AC-35 (revised after QL-STORY-READY: preserved fields and host-lost observable named), scoped to this story:*

- [ ] GIVEN `GeneratedWithFallback` with the always-failing stub in a 2-client PIE (or a multi-process harness if 5.8.4 PIE cannot seamless-travel), THEN one seamless travel to the graybox preserves the match-scoped fields (`Seed`, `FRoundParams`, roster, round-scoped Event Log header and attempt records), Round re-enters `Preparing` with `ProviderKind = Graybox`, and the log header shows level 2 and the attempt records (`Fallback.TravelToGraybox`)
- [ ] GIVEN the listen-server host quits in `Preparing` or in `Active`, THEN the service is `Aborted` with `LayoutFallbackLevel` 3, and each client receives a host-lost event and is on the menu map within a bounded time on the injected clock (graceful quit 0.55 s plus one frame, process kill 12.3 s, per Session F6) (`Network.HostLeaveAborts`)

---

## Implementation Notes

- Fallback uses Session rule 11.9 `RequestFallbackTravel`; the service enters `FallbackTravel` and does not itself spawn anything in the old world.
- Exactly one travel (assert the travel counter); a client joining during travel follows Session's rules.
- Host loss: `Aborted` from any state; `LayoutFallbackLevel` 3 logged; clients return to the menu.
- The Generated stub is the same always-failing stub of story 003; the real Generator is a later epic.

---

## Out of Scope

- Story 003: the reseed loop; story 004: the abort dispositions without travel
- Session's travel implementation (`production/epics/session-travel/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Private/Core/LayoutFallbackTravel.cpp`
- `Source/StealthTestHarness/Private/LayoutFallbackTravelTest.cpp`
- `Source/StealthTestHarness/Private/HostLeaveAbortsTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Fallback.TravelToGraybox`
  - Given: GeneratedWithFallback, always-failing stub, 2-client PIE (multi-process harness if needed).
  - When: all 6 attempts fail.
  - Then: exactly 1 seamless travel to the graybox; Seed, FRoundParams, roster and the round-scoped log header and attempt records are preserved; Round re-enters Preparing with ProviderKind Graybox; header shows level 2 and 6 attempt records.
  - Edge cases: a client joining during travel.
- **AC-2** `Network.HostLeaveAborts`
  - Given: a 2-client session in Preparing, then in Active.
  - When: the host quits; separately the host process is killed.
  - Then: service Aborted with LayoutFallbackLevel 3; each client sees a host-lost event and is on the menu map within 0.55 s + 1 frame (quit) or 12.3 s (kill), per Session F6.
  - Edge cases: quit during travel.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/LayoutFallbackTravelTest.cpp` and `HostLeaveAbortsTest.cpp` — `StealthGame.Layout.Fallback.TravelToGraybox`, `StealthGame.Layout.Network.HostLeaveAborts` (CQTest PIE naming per ADR-0008 if used) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-003-attempt-loop-timeout-budget-reseed.md`, `production/epics/level-socket-contract/story-004-abort-paths-and-event-log-header.md`
- Cross-epic: `production/epics/session-travel/EPIC.md` (`RequestFallbackTravel`, match-scoped store, host-loss F6), `production/epics/round-match-framework/EPIC.md` (stub until it exists)
- Unlocks: Generator epic fallback path (`production/epics/procedural-city-generator/EPIC.md`)
