# Story 017: Density model (F1) and assignment-blind start placement

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-004`, `TR-crowd-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-004 = Identity blindness: no crowd code reads player/binding/team/state. TR-crowd-005 = Counter-based draws from the crowd server sub-seed.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0006 (`NpcRoutes` domain 5 for start placement), ADR-0014 (`RequestStartPlacement`, `CrowdRoute` graph)
**ADR Decision Summary**: Destination and start-cell weights depend only on area type and geometry and on observed NPC density from public positions, never on the site assignment, zone roles or player positions. Start placement draws from `NpcRoutes` inside the layout service's `RequestStartPlacement`; per-pawn behaviour after spawn uses `CrowdBehaviour` only.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Pure data and math over the layout manifest; the assignment-blindness assert is a server-only `check` that fires if any Crowd code calls the site-assignment query.

**Why this story**: Where NPCs stand and walk must carry no information about objective sites (Pillar 2 and the LSC layout rules). Density also decides the F1 targets that the ambient schedule and the whole tell budget are calibrated against. Read GDD CNB-R13, F1 and AC-CNB-05, AC-CNB-06.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: start placement uses `NpcRoutes`; behavioural draws use `FCrowdDraws` on `CrowdBehaviour`; rates and weights from the frozen tuning snapshot (ADR-0016)
- Forbidden: reading `FSiteAssignment`, zone roles or player positions; `FMath::Rand*`; including Objectives-assignment headers (ADR-0016, ADR-0014)
- Guardrail: weights are computed once at round start and on a coarse refresh, not per frame

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-05, AC-CNB-06, CNB-R13 and F1, scoped to this story:*

- [x] (AC-CNB-05) GIVEN 50 fixed layouts each run with two different `FSiteAssignment`s, WHEN start placement and the first 120 s of destination weights are computed, THEN the outputs are bit-identical, and an instrumented assignment query from Crowd fires the server assert.
- [x] (AC-CNB-06, ADVISORY) GIVEN the reference scene after 120 s, WHEN density per area type is sampled every 10 s for 10 min, THEN each type's mean is within +-20 % of F1's target and no zone disc exceeds `ZoneDensityMaxMult` x the map mean for more than 30 s.
- [x] Start placement draws from `NpcRoutes` inside `RequestStartPlacement`, and route-node capacity counts pawns of any kind.

---

## Implementation Notes

*Derived from GDD CNB-R13, F1 and ADR-0016 section 9:*

- Weights: area-type multipliers `CrowdAreaWeightCausewayMult` / `MarketMult` / `TempleMult` / `ResidentialMult` (1.4 / 1.6 / 0.8 / 0.6) over geometry; a court inherits its host area's type. Observed density feeds back from public pawn positions only.
- Node capacity: a person does not walk into an occupied spot, counting pawns of any kind, so player and NPC occupy identically.
- Start placement happens inside LSC `RequestStartPlacement` (Round spawns `NpcCount` NPCs from its single shuffled spawn list, Round P6, placed by CNB's density function); the draw uses the `NpcRoutes` domain, not `CrowdBehaviour`.
- The server assert uses a sentinel: any call to the site-assignment query from a Crowd translation unit trips it; the test injects one deliberately to prove it fires.
- AC-CNB-06 is advisory (a miss is reported with the F1 target table, not failed), per the GDD.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces `FindCrowdPoint` and the initial placement in `SpawnCrowd` in `Source/StealthGame/Private/StealthGameMode.cpp`. Placement is rewritten on the layout manifest.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 026: mid-round emergence sockets
- The layout-service epoch (`production/epics/level-socket-contract/EPIC.md`): `RequestStartPlacement` itself
- Story 009: leg walking

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdDensity.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdDensity.cpp`
- `Source/StealthGame/Private/StealthGameMode.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdDensityTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.DensityBlindToAssignment`
  - Given: 50 fixed layouts, each run with two different `FSiteAssignment`s, and an instrumented assignment query
  - When: start placement and the first 120 s of destination weights are computed
  - Then: outputs are bit-identical across assignments; the assignment query from Crowd fires the server assert
  - Edge cases: the assert test uses an expected-error declaration
- **AC-2**: `StealthGame.Crowd.AreaDensityTargets` (advisory)
  - Given: the reference scene after 120 s
  - When: per-area-type density is sampled every 10 s for 10 min
  - Then: each type's mean within +-20 % of the F1 target; no zone disc exceeds `ZoneDensityMaxMult` x the map mean for more than 30 s
  - Edge cases: advisory, so a miss is reported, not failed
- **AC-3**: `StealthGame.Crowd.StartPlacementDomainAndCapacity`
  - Given: `NpcRoutes` with capacity-limited nodes and a mix of pawn kinds
  - When: `RequestStartPlacement` runs
  - Then: placement draws from `NpcRoutes` inside that call only; node capacity counts pawns of any kind
  - Edge cases: a full node occupied by a player pawn

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdDensityTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Density.AssignmentBlind`, `StealthGame.Crowd.Density.StartPlacement`, `StealthGame.Crowd.Density.TargetsAdvisory`; 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-026-ambient-schedule-and-emergence.md`

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 3/3 passing
**Deviations**: Area types are graybox stand-ins (plaza = Market); the density targets are reported advisory (one type missed its target by 1.6 NPCs in the 275-test run, not gated). The game mode takes NPC start transforms from RequestStartPlacement at round start.
**Test Evidence**: `StealthGame.Crowd.Density.AssignmentBlind`, `StealthGame.Crowd.Density.StartPlacement`, `StealthGame.Crowd.Density.TargetsAdvisory`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
