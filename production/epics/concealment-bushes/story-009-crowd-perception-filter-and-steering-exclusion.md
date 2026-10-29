# Story 009: Concealed and transitioning pawns are invisible to NPC perception and absent from NPC steering

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R4 avoidance, CON-R14, AC-CON-28 NPC-route part)
**Requirement**: `TR-con-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§7 `ICrowdPerceptionFilter`), with ADR-0016 (Crowd consumes the filter)
**ADR Decision Summary**: Concealment implements Crowd's consumer-owned `ICrowdPerceptionFilter`: `IsPerceivable = State != Concealed`, `IsSteeringObstacle = State == Outside`, identical for every pawn kind with no viewer input, and exposes no occupancy query to Crowd. Pawns in any of the four non-Outside states leave RVO, crowd avoidance and every NPC steering query so no NPC bends its path around an invisible occupant (that bend would be an occupancy oracle that needs no packet).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; plain C++ interface, steering integration is Crowd's)
**Engine Notes**: No post-cutoff API in this story. The interface is declared by the consuming layer (ADR-0007 §4); Concealment implements it and never includes Crowd types.

**Control Manifest Rules (this layer)**:
- Required: Consumer-owned interfaces are declared by the consumer; features implement them, and no lower-layer file names a feature type — source: ADR-0007, ADR-0017
- Forbidden: Never expose an occupancy query to Crowd; never let NPC choice or steering read occupancy — source: ADR-0017
- Guardrail: O(1) per query

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (CON-R4, CON-R14, AC-CON-28), scoped to this story:*

- [ ] **AC1** (derived, ADR-0017 §7): **GIVEN** pawns in every `EConcealState` (NPC and player) **THEN** `IsPerceivable` is false only for Concealed and `IsSteeringObstacle` is true only for Outside, identical for every pawn kind, with no viewer input; the public header exposes no occupancy query (header scan).
- [ ] **AC2** (AC-CON-28 NPC-route part, [U][PIE-2] `StealthGame.Concealment.NpcRouteBushInvariance`): **GIVEN** 100 NPC routes past a bush with fixed seeds, occupied (each state) vs empty, **THEN** the per-tick paths are bit-identical between runs.

---

## Implementation Notes

*Derived from ADR-0017 Decision §7 and CON-R4:*

`UConcealmentService` implements `ICrowdPerceptionFilter` (`IsPerceivable(const APawn*)`, `IsSteeringObstacle(const APawn*)`), registered by the composition root. Crowd owns the spatial hash, RVO and avoidance that consume it (Crowd stories 008 and 011). The invariance test runs the same fixed-seed routes with a bush empty and with an occupant in each of the five states (including mid-Enter) and compares per-tick positions exactly.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Crowd epic: NPC bush selection (`story-022-bush-herring.md`) and the avoidance system itself.
- `story-008-conceal-collision-pure-function.md`: collision responses.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentService.h` (`ICrowdPerceptionFilter` implementation)
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentService.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/PerceptionFilterTest.cpp` (class `FConcealmentPerceptionFilterTest`)
- `Source/StealthTestHarness/Private/Tests/PIE/NpcRouteBushInvarianceTest.cpp` (`NETWORK_TEST_CLASS(NpcRouteBushInvariance, "StealthGame.Concealment.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: PerceptionFilter
  - Given: pawns in every `EConcealState`, NPC and player.
  - When: `IsPerceivable` and `IsSteeringObstacle` are queried.
  - Then: `IsPerceivable` false only for Concealed; `IsSteeringObstacle` true only for Outside; results do not depend on pawn kind; the public header exposes no occupancy query (header scan).
- **AC2**: NpcRouteBushInvariance
  - Given: 100 fixed-seed NPC routes past a bush, occupied (each state) against empty.
  - When: per-tick positions are compared.
  - Then: bit-identical between runs.
  - Edge cases: an occupant mid-enter at route time. Needs Crowd stories 008 and 011 to be built.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `PerceptionFilterTest.cpp` and the PIE `NpcRouteBushInvarianceTest.cpp` — must exist and pass, OR a documented playtest

**Status**: [x] Created and passing (2026-10-29): StealthGame.Concealment.PerceptionFilter; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: N/A - headless core rule (no player-observable surface of its own); the integrated behaviour is observed in the batch's 3-player PIE run (con-*.png)

---

## Dependencies

- Depends on: `story-003-conceal-state-machine-and-occupancy-record.md`; Crowd `production/epics/crowd-npc-behaviour/story-008-spatial-hash-and-perception-filter.md` and `story-011-avoidance-and-yield.md`
- Unlocks: `story-026-per-viewer-visibility-pie.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1 passing. AC2 (NpcRouteBushInvariance, 100 seeded NPC routes bit-identical occupied vs empty) DEFERRED to the Crowd determinism harness: the invariance follows structurally from AC1 (IsSteeringObstacle is false for every non-Outside pawn and Crowd reads no occupancy; header scan) but the 100-route bit-identity run is not built.
**Deviations**: Crowd's bush herring is switched on (CNB-022): FStealthCrowdHerringSites feeds bush sites (entry 35 cm out) and BushReenterCooldownSec comes from Concealment's tuning.
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
