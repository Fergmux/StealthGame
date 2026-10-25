# Story 035: Informant respawn takeover — integration

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-006`
*(At write time: TR-lsc-006 = Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (with ADR-0014 section 7.3 and ADR-0010 for the factory takeover)
**ADR Decision Summary**: Penalty calls `PickRespawnPawn`, pre-checks `CheckTakeOverEligibility`, has Crowd release the NPC (`TryReleaseNpc`), then `UStealthPawnFactory::TakeOverCrowdPawn` binds it and hands it over in one call stack; the killed body is removed as a corpse and Crowd emerges a `Replacement` NPC whose card is `DrawReplacement()`. No pawn, actor or NetGUID is created; the total pawn count is constant.
**ADR Version**: 2026-10-24 (ADR-0019); 2026-10-24 (ADR-0014)

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (inherits ADR-0010's HIGH-risk possession parity: spikes S1, S14, H1)
**Engine Notes**: Takeover invisibility on the wire (no new actor, NetGUID, `AppearanceId`, net-config, velocity or `HeadLook` change) is ADR-0019 Verification 1 and inherits ADR-0010 spikes; this story asserts the actor/NetGUID deltas only. Penalty, Roster, Crowd and the factory are stubs until their epics land.

**Why this story**: It proves the whole respawn path leaves no trace on the wire or in the world. Read first: R5b items 1-4 and 6, AC-14, AC-17; ADR-0019 Decision (steps 2-5) and its flow diagram.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `TakeOverCrowdPawn` checks eligibility and calls `BindPawn` itself; no card move is committed unless it returned `Applied`; `TakeNpcCard` runs in the same call stack as its Roster counterpart (ADR-0010, ADR-0012)
- Forbidden: Never call `BindPawn` from a takeover caller; never destroy-and-respawn on takeover (ADR-0010)
- Guardrail: pawn count constant at each replacement slot (GDD R5b.4)

---

## Acceptance Criteria

*From GDD AC-14 and AC-17, scoped to this story:*

- [ ] GIVEN 12 NPCs (some inside team A's respawn region, some outside, one already bound) and A's informant killed, WHEN the respawn resolves, THEN the participant possesses a previously Unbound NPC whose path to A's anchor is at most `InformantRespawnPathCm` and shorter than its path to the other used anchor, its card is unchanged, the old body is removed as a corpse, and Crowd receives exactly one `Replacement` request, whose card is `DrawReplacement()` (membership of the vacant set when two or more vacancies; never equality to the victim's card); GIVEN two used anchors 40 m of path apart, THEN no cell is in both teams' regions (`Respawn.TakesOverUnboundNpcInTeamRegion`, `Respawn.RegionsDisjoint`)
- [ ] GIVEN 100 informant respawns and 20 rejoins after `Active`, THEN spawned-actor, replicated-actor and NetGUID deltas from them are 0, the player-pawn spawn-path counter is 0, the pawn count returns to its t0 value at each replacement slot, and a rejoining Active participant possesses its own previous pawn (`Respawn.CreatesNoActors`)

---

## Implementation Notes

*Derived from GDD R5b and ADR-0019:*

- Sequence per request: `PickRespawnPawn` -> factory `CheckTakeOverEligibility` (side-effect free; `NotEligible` adds the pawn to the request's exclusions and re-picks, at most 4) -> Crowd `TryReleaseNpc` (atomic Eligible re-check) -> factory `TakeOverCrowdPawn(R.Pawn, P, ExpectedEpoch)` (BindPawn, ReleaseControl, `RequestHandOver(Player)`). The killed body is removed like any NPC corpse; Crowd emerges the replacement through `RequestEmergence(Replacement)` (story 030) with the victim's card only when it is the only vacancy (Character Pool CP-12a).
- Rejoining Active participants get their own previous pawn back from crowd AI (Roster); `RequestEmergence` does not serve `Rejoin`.
- Counters: spawned-actor, replicated-actor, NetGUID deltas and the player-pawn spawn-path counter are read from the harness; a rejoin during a respawn is an edge case.
- With 2+ vacancies assert set membership for the replacement card (CP-12a draws uniformly over vacant cards).

---

## Out of Scope

- Stories 033/034: pick eligibility, draw and widening; Penalty's lifecycle (`production/epics/npc-kill-penalty-informants/EPIC.md`); the factory's possession mechanics (`production/epics/shared-character-locomotion/EPIC.md`)

---

## Files

- `Source/StealthTestHarness/Private/RespawnTakeoverTest.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/RespawnTakeoverStubTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Respawn.TakesOverUnboundNpcInTeamRegion`, `Respawn.RegionsDisjoint`
  - Given: 12 NPCs (some in team A's region, some outside, one bound); A's informant killed. Separately, two anchors 40 m of path apart.
  - When: the respawn resolves.
  - Then: the participant possesses a previously Unbound NPC with path to A's anchor <= `InformantRespawnPathCm` and shorter than to the other anchor; card unchanged; old body removed as a corpse; Crowd gets exactly 1 `Replacement` request whose card is in the vacant set; no cell is in both regions.
  - Edge cases: with 2+ vacancies assert membership, not equality to the victim's card.
- **AC-2** `Respawn.CreatesNoActors`
  - Given: 100 informant respawns and 20 rejoins after Active.
  - When: they resolve.
  - Then: spawned-actor, replicated-actor and NetGUID deltas = 0; player-pawn spawn-path counter = 0; pawn count returns to its t0 value at each replacement slot; a rejoining participant possesses its own previous pawn.
  - Edge cases: rejoin during a respawn.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/RespawnTakeoverTest.cpp` and `Source/StealthGameplay/Private/Tests/Layout/RespawnTakeoverStubTest.cpp` — `StealthGame.Layout.Respawn.TakesOverUnboundNpcInTeamRegion`, `.RegionsDisjoint`, `.CreatesNoActors` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-033-respawn-pick-eligibility-unseen-draw.md`, `production/epics/level-socket-contract/story-034-respawn-starvation-widening-and-region.md`, `production/epics/level-socket-contract/story-030-emergence-request-clear-check-uniform-pick.md`
- Cross-epic: `production/epics/npc-kill-penalty-informants/EPIC.md`, `production/epics/participant-state-roster/EPIC.md`, `production/epics/crowd-npc-behaviour/EPIC.md`, `production/epics/shared-character-locomotion/EPIC.md`, `production/epics/character-pool-appearance/EPIC.md` (stubs until they exist)
- Unlocks: `production/epics/level-socket-contract/story-036-crowd-and-objectives-contract-tests.md`
