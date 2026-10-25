# Story 035: Navmesh frozen after hand-off

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The navmesh is built once per accepted attempt and frozen by a custom build lock at `OnLayoutReady`: from `HandedOff(k)` until `TearingDown(k)` no tile is rebuilt, no dirty area is processed and nothing the Generator spawns carries a dynamic nav modifier, so the navmesh Validate measured is the navmesh Crowd walks for the whole round (Crowd CR-18; the prototype's dynamic rebuild is not carried over).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014)
**Engine Notes**: Verified in 5.8.4 source (ADR-0014): `IsNavigationDirty()` does **not** report dirty areas queued while a build lock is held in game worlds; `GetNumDirtyAreas()` does, and queued areas are appended without a cap while locked. The freeze check therefore watches `GetNumDirtyAreas()` and the tile-build counter. The "navmesh data hash" at `Ended` versus `HandedOff` is a hash of the recast tile data bytes (tile order canonical).

**Control Manifest Rules (this layer)**:
- Required: Everything spawned after `OnLayoutReady` has `CanEverAffectNavigation` false on every component — source: ADR-0014
- Forbidden: Never let anything affect navigation after the freeze; no code other than the layout service adds or removes a navigation build lock; never use a fully dynamic navmesh — source: ADR-0014
- Guardrail: Layout (server, `Active`): no navmesh rebuild; runtime services <= 0.1 ms average — source: ADR-0014

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G36, R-G7 navmesh row), scoped to this story:*

- [ ] **AC1** (AC-G36, `Nav.FrozenAfterHandOff`): **GIVEN** a 2-client PIE on 3 golden seeds **WHEN** 60 s of `Active` play pass with pawns killed and NPCs emerging **THEN** the tile build counter and dirty-area count do not change after `OnLayoutReady`, and the navmesh data hash at `Ended` equals the hash at `HandedOff`.
- [ ] **AC2** (R-G7 / ADR-0014 4, `Nav.NothingSpawnedAffectsNav`): **GIVEN** anything the Generator spawns **THEN** no component carries a dynamic nav modifier or obstacle and every component spawned after `OnLayoutReady` has `CanEverAffectNavigation` false.

---

## Implementation Notes

*Derived from GDD R-G7 and ADR-0014 section 4.2:*

The lock lifecycle and counters belong to the layout service (`level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`); this story is the Generator-side proof over real play: record the tile build counter, `GetNumDirtyAreas()` and the nav data hash at `OnLayoutReady` and `HandedOff`, run 60 s of server time in `Active` with at least one pawn kill and one NPC emergence (preconditions asserted; the round length must exceed 60 s), then assert the counters are unchanged at 60 s and at `Ended` and the hash equals the `HandedOff` hash. The scope of AC2: kit components built **before** `OnLayoutReady` are `CanEverAffectNavigation` true on the server (story 026); the rule applies to what the Generator spawns **after** hand-off (decoration is client-side and never on the server navmesh) and to any dynamic modifier or obstacle at any time. T2 CQTest PIE in `StealthTestHarness`: `StealthGame.CityGen.PIE.NavFrozenAfterHandOff.SixtySecondsActive`.

Prototype replaced: the prototype rebuilt the navmesh dynamically whenever the city changed; that behaviour is explicitly not carried over (story 045 removes it).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`: the lock lifecycle itself.
- `story-028-navigation-integration-water-stairs-client-none.md`: water, stairs and client-none.
- `story-046-ten-connection-gauntlet-integration-pass.md`: the same freeze at 10 connections.
- Crowd's use of the frozen navmesh: `crowd-npc-behaviour`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/CityGen/NavFrozenAfterHandOffPieTest.cpp` (CQTest PIE)
- `Source/StealthGameplay/Private/Tests/CityGen/NothingSpawnedAffectsNavTest.cpp` (class `FCityGenNavNothingSpawnedAffectsNavTest`, name `StealthGame.CityGen.Nav.NothingSpawnedAffectsNav`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `StealthGame.CityGen.PIE.NavFrozenAfterHandOff.SixtySecondsActive`
  - Given: 2 clients on 3 golden seeds. Record tile build counter, dirty-area count and nav hash at `OnLayoutReady` and `HandedOff`.
  - When: a 60 s server-time Active window runs with pawns killed and NPCs emerging.
  - Then: counters are unchanged at 60 s and at `Ended`. The hash at `Ended` equals the hash at `HandedOff`.
  - Edge cases: preconditions are >= 1 kill and >= 1 emergence, and the round length must exceed 60 s.
- **AC2** `Nav.NothingSpawnedAffectsNav`
  - Given: all components after the server build and after Active.
  - When: they are enumerated.
  - Then: none has a dynamic nav modifier or obstacle. Every component spawned after `OnLayoutReady` has `CanEverAffectNavigation` false.
  - Edge cases: kit components built before `OnLayoutReady` are true per story 026; the scope is "Generator-spawned after hand-off".

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the T2 PIE test and the T1 component test OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-028-navigation-integration-water-stairs-client-none.md`, `story-031-golden-seeds-and-bump-rule.md`; cross-epic `level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`, `crowd-npc-behaviour/story-026-ambient-schedule-and-emergence.md`
- Unlocks: `story-046-ten-connection-gauntlet-integration-pass.md`
