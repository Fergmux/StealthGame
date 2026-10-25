# Story 034: PickRespawnPawn — starvation widening and region membership

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

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (with ADR-0014 section 7.3)
**ADR Decision Summary**: With no eligible pawn the request retries every `EmergenceRetrySec`; after `RespawnStarvationSec` (10 s) the region widens to 2 x `InformantRespawnPathCm` (still cut by the nearer-anchor rule) and after another 10 s to any walkable cell not within `InformantRespawnPathCm` of an enemy-used anchor; the unseen test is never relaxed, a bound pawn is never taken and no pawn is spawned. The widening level is derived from elapsed server time since the **first** call of that `RespawnRequestSeq`, so retries never restart it; `CloseRespawnRequest` clears the record.
**ADR Version**: 2026-10-24 (ADR-0019); 2026-10-24 (ADR-0014)

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Injected clock; Penalty and Roster are stubs (they bind each returned pawn through Roster `BindPawn`).

**Why this story**: Starvation behaviour decides whether an informant can ever return when the crowd near its team is thin or watched. Read first: R5b items 5-6, R3 TeamSpawn row, AC-18, AC-53; ADR-0014 7.3.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Draw sites use `GetServerSubSeed(RespawnPick, RequestSeq)` (ADR-0014)
- Forbidden: Never relax the unseen test; never take a bound pawn; never spawn a pawn (GDD R5b.5)
- Guardrail: at most 1 pick per frame (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-18, AC-53 and ADR-0014 7.3, scoped to this story:*

- [ ] GIVEN no unseen eligible pawn, THEN the region widens to 2 x `InformantRespawnPathCm` (cut by the nearer-anchor rule) at 10 s and to the city (minus enemy-zone regions) at 20 s on the injected clock; seen pawns are never chosen; after both widenings the request waits and logs `RespawnStarved`; no pawn is created at any point (`Respawn.StarvationWidens`)
- [ ] GIVEN 100 `PickRespawnPawn(Team, RequestSeq, ExcludePawn = null)` calls without starvation, THEN 100% of returned pawns lie in the team's respawn region and Penalty binds each through Roster `BindPawn` (`Cross.Informants.RespawnRegion`)
- [ ] GIVEN retries of one `RequestSeq`, THEN widening elapsed time is measured from the first call and is not restarted; `CloseRespawnRequest` clears the record (`Respawn.WideningClockPerRequest`, proposed)

---

## Implementation Notes

- Widening record keyed by `RespawnRequestSeq`; two sequences keep independent clocks; the record is cleared by `CloseRespawnRequest` or at `Released`.
- A pawn becoming eligible at 9.9 s is served from the narrow region. After both widenings the request keeps waiting (the participant stays "awaiting pawn", Roster) and logs `RespawnStarved`.
- Region membership: path to own anchor at most 45 m **and** strictly nearer than the other used anchor (disjointness by construction, shown in story 035).
- Penalty and Roster are stubs: the stub binds each returned pawn; the real `RetireBinding`/`BindPawn` flow is Penalty's (`production/epics/npc-kill-penalty-informants/EPIC.md`).

---

## Out of Scope

- Story 033: eligibility, unseen test, draw, per-frame guard; story 035: the takeover integration

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/RespawnWidening.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/RespawnWideningTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Respawn.StarvationWidens`
  - Given: no unseen eligible pawn; injected clock.
  - When: the request retries every 1 s.
  - Then: region widens to 2 x `InformantRespawnPathCm` (nearer-anchor cut) at 10 s and to the city (minus enemy-zone regions) at 20 s; seen pawns are never chosen; after both widenings it waits and logs `RespawnStarved`; 0 pawns created.
  - Edge cases: a pawn becoming eligible at 9.9 s uses the narrow region.
- **AC-2** `Cross.Informants.RespawnRegion`
  - Given: 100 `PickRespawnPawn(Team, RequestSeq, ExcludePawn = null)` calls, no starvation.
  - When: resolved.
  - Then: 100% of returned pawns lie in the team's region; Penalty binds each through Roster `BindPawn`.
  - Edge cases: Penalty and Roster are stubs.
- **AC-3** `Respawn.WideningClockPerRequest` (proposed)
  - Given: retries of one RequestSeq.
  - When: retried across the widening points; then `CloseRespawnRequest`.
  - Then: elapsed time counts from the first call and is not restarted; `CloseRespawnRequest` clears the record.
  - Edge cases: two RequestSeqs keep independent clocks.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/RespawnWideningTest.cpp` — `StealthGame.Layout.Respawn.StarvationWidens`, `StealthGame.Layout.Cross.Informants.RespawnRegion`, `StealthGame.Layout.Respawn.WideningClockPerRequest` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-033-respawn-pick-eligibility-unseen-draw.md`
- Cross-epic: `production/epics/npc-kill-penalty-informants/EPIC.md`, `production/epics/participant-state-roster/EPIC.md` (stubs until they exist)
- Unlocks: `production/epics/level-socket-contract/story-035-respawn-takeover-integration.md`
