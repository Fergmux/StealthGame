# Story 033: PickRespawnPawn — eligibility, unseen test and uniform draw

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

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (with ADR-0014 section 7.3 for the service side)
**ADR Decision Summary**: A killed informant's participant takes over an existing unseen crowd NPC near its team's TeamSpawn; the layout service's `PickRespawnPawn(Team, RequestSeq, Exclude)` pre-filters eligible pawns, tests that no non-team pawn can see the candidate (`ViewBlock` traces from 1.6 m eyes / 3.2 m climb tops with head and torso targets, dilated 2 m), draws uniformly by rejection sampling from `GetServerSubSeed(RespawnPick, RequestSeq)`, and re-runs the unseen test at the commit tick; a per-frame guard returns `Deferred` for a second call in the same frame. No pawn, actor or NetGUID is created.
**ADR Version**: 2026-10-24 (ADR-0019); 2026-10-24 (ADR-0014)

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (inherits ADR-0010's HIGH-risk possession parity through spikes S1, S14, H1 for the integration story 035)
**Engine Notes**: `ViewBlock` line traces; at most about 400 traces per pick and at most 1 pick per frame (ADR-0014 7.3). Crowd's `ITakeoverEligibilitySource::GetTakeoverEligibility` is applied in the pre-filter; a stub provides it until the Crowd epic lands.

**Why this story**: The draw must never pick a pawn anyone could see change, and must be fair among eligible pawns. Read first: R5b items 1-4, R5.2 (eye and target samples), AC-15, AC-16; ADR-0014 7.3; ADR-0019 sections on the pick.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Draw sites use `GetServerSubSeed(RespawnPick, RequestSeq)` counter-as-Attempt with tag `LSC_RSPN`; every sight test uses `ViewBlock` (ADR-0014, ADR-0015)
- Forbidden: `%` on a draw; never take a bound pawn; never spawn a pawn (GDD R5b.5)
- Guardrail: layout runtime services at most 0.5 ms in a respawn-pick frame; at most 1 pick per frame; about 400 `ViewBlock` traces per pick (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-15 and AC-16 and ADR-0014 7.3 (revised after QL-STORY-READY: trace threshold made exact), scoped to this story:*

- [ ] GIVEN pawns visible to a non-team pawn (active or informant) from 1.6 m eyes or a 3.2 m climb top, or within 2 m of such a line, WHEN 1,000 draws run, THEN none is selected; GIVEN a pawn that becomes visible between draw and commit, THEN it is dropped and the draw repeats in the same tick (`Respawn.ChosenPawnUnseen`)
- [ ] GIVEN exactly 8 eligible and 4 ineligible pawns (seen, outside the region, or bound), WHEN 8,000 pinned-sub-seed draws run, THEN ineligible pawns are chosen 0 times and chi-square against uniform is < 18.475 (df 7); shuffling actor order leaves every pick unchanged (canonical stable-id order); the draw uses rejection sampling and no `%` (`Respawn.UniformOverEligible`)
- [ ] GIVEN two `PickRespawnPawn` calls in one frame, THEN the second returns `Deferred` with 0 queue entries, and one evaluated pick issues at most 400 `ViewBlock` traces (instrumented counter, pinned reference fixture) (`Respawn.PerFrameGuard`)

---

## Implementation Notes

*Derived from GDD R5b and ADR-0014 7.3:*

- Eligible pawn for team T: an Unbound NPC pawn that is live, not in an interaction or tending pose, not in an emergence walk-in or pending exit, not claimed this tick, not in `Exclude`, inside `RespawnRegion(zone_T, zone_U)` (path to anchor_T at most `InformantRespawnPathCm` and strictly shorter than to anchor_U, no traversal), and **unseen**: no pawn of another team (active or informant) has line of sight to it from any point within `EmergenceWitnessDilationCm` (2 m), using the R5.2 samples within `EmergenceWitnessRangeCm` and no view cone. The only team reads are T's own zone, the other used zone and the set of non-team pawns.
- Draw: uniform over eligible pawns sorted by server-only stable pawn id, rejection-sampled; the same `RequestSeq` is reused on every retry so its stream continues. A pawn exactly 2.0 m from the line is dropped; one eligible pawn is an edge case.
- Result contract: `FRespawnPick { Pawn, Status ∈ {Picked, None, Deferred}, WideningLevel }`; `Exclude` is a `TConstArrayView<const AStealthPawn*>` (the participant's released exile body plus that request's short-term exclusions).
- `RespawnRegion` cell sets are precomputed once at `Published` on the nav-grid (story 015), cut by the nearer-anchor rule, server-only, never replicated or logged.

---

## Out of Scope

- Story 034: widening, starvation, region membership checks and request closure; story 035: possession, card and Penalty/Roster flows; Crowd's release (`production/epics/crowd-npc-behaviour/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/RespawnPick.cpp`, `RespawnPick.h`
- `Source/StealthGameplay/Private/Tests/Layout/RespawnPickTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Respawn.ChosenPawnUnseen`
  - Given: pawns visible to a non-team pawn from 1.6 m eyes or a 3.2 m climb top, or within 2 m of such a line.
  - When: 1,000 draws run; one pawn turns visible between draw and commit.
  - Then: no seen pawn is selected; the late-visible pawn is dropped and the draw repeats in the same tick.
  - Edge cases: pawn exactly 2.0 m from the line.
- **AC-2** `Respawn.UniformOverEligible`
  - Given: 8 eligible and 4 ineligible pawns.
  - When: 8,000 pinned draws; actor order shuffled.
  - Then: ineligible chosen 0 times; chi-square < 18.475 (df 7); shuffling changes no pick; rejection sampling, no `%`.
  - Edge cases: 1 eligible pawn.
- **AC-3** `Respawn.PerFrameGuard`
  - Given: the pinned reference fixture (graybox, NpcCount 50, 2 non-team viewers).
  - When: two `PickRespawnPawn` calls in one frame.
  - Then: the second returns `Deferred` with 0 queue entries; one evaluated pick issues <= 400 `ViewBlock` traces (counter).
  - Edge cases: the next frame serves the deferred call.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/RespawnPickTest.cpp` — `StealthGame.Layout.Respawn.ChosenPawnUnseen`, `.UniformOverEligible`, `.PerFrameGuard` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-016-predicates-emergence-witness-alley.md`, `production/epics/level-socket-contract/story-015-layout-measure-backend-world.md`, `production/epics/level-socket-contract/story-022-selection-secret-draw-streams.md`
- Cross-epic: `production/epics/crowd-npc-behaviour/EPIC.md` (`ITakeoverEligibilitySource`), `production/epics/participant-state-roster/EPIC.md` (Bound/Unbound; stubs until they exist)
- Unlocks: `production/epics/level-socket-contract/story-034-respawn-starvation-widening-and-region.md`, `production/epics/level-socket-contract/story-035-respawn-takeover-integration.md`
