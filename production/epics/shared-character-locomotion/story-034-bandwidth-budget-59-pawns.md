# Story 034: Pawn replication stays within 36 KB/s per client with 59 pawns walking

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R12 Performance contract, F9 Pawn network budget per client; AC-SCL-19)
**Requirement**: `TR-scl-008` (at most 36 KB/s pawn replication per client, working value since `NpcCount` 50, user decision 2026-10-25)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0022: Rendering configuration and performance budgets (cross-cutting budget table), with ADR-0003 (section 4 fixed-width serialisers) and ADR-0010 (guardrail: `TraversalState` 7 bytes per change)
**ADR Decision Summary**: The pawn replication line is at most 36 KB/s per client of the 50 KB/s total budget at the worst case of 59 relevant moving pawns; fixed-width serialisers keep per-pawn cost identical for every kind and event.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Measured with Network Insights on one client. The 36 KB/s figure is a working value (re-verify in spike R3 per GDD R12). Legacy replication driver (Iris pinned off).

**Control Manifest Rules (this layer)**:
- Required: pawn network within the 36 KB/s line (SCL F9); `TraversalState` 7 bytes per change (ADR-0010).
- Forbidden: no extra replicated property on the pawn beyond the allowlist.
- Guardrail: pawn replication at most 36 KB/s per client at `NpcCount` 50; typical half-idle crowd at most 22 KB/s.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-19** [N] `BandwidthBudget59Pawns`: GIVEN 59 relevant pawns all walking (`NpcCount` 50), WHEN Network Insights measures one client for 60 s, THEN pawn replication averages ≤ 36 KB/s; with a typical half-idle crowd ≤ 22 KB/s.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the measurement boundary is pawn actor channels only (excluding PlayerState and GameState); half-idle means 50 percent of NPCs idle; no network emulation is assumed; the test needs a full 59-pawn scene, so it is a deferred (scene-dependent) test.

---

## Implementation Notes

*Derived from GDD F9 and ADR-0003:*

- Run the harness with 10 players and 50 NPCs (60 pawns, 59 relevant to one observing client), capture with Network Insights for 60 s after warm-up, and sum the bytes of pawn actor channels. Two runs: all walking, and half the NPCs idle.
- A result above 36 KB/s is a finding for the animation/replication budget (spike R3 and ADR-0022 re-verification), not something to hide by changing the serialisers per kind.
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 044: animation CPU budget.
- Story 033: delay. Story 055: the combined net-emulation pass.

---

## Files

- `Source/StealthTestHarness/Private/Locomotion/BandwidthBudget59PawnsTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; deferred to a full 59-pawn scene). Test `StealthGame.Locomotion.PIE.BandwidthBudget59Pawns.AllWalking`.*

Test: pawn bandwidth budget (AC-SCL-19)
  Given: 59 relevant pawns all walking (`NpcCount` 50), then a half-idle crowd (50 % of NPCs idle); no network emulation
  When: Network Insights measures one client for 60 s (pawn actor channels only, excluding PlayerState and GameState)
  Then: pawn replication averages ≤ 36 KB/s all walking; ≤ 22 KB/s with the half-idle crowd
  Edge cases: 59 is likely 60 minus the observer's own pawn (confirm); the 36 KB/s line is a working value re-verified in spike R3

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/BandwidthBudget59PawnsTest.cpp` — must exist and pass, with the Network Insights capture summary stored in `production/qa/evidence/`; not player-observable

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-030-net-config-parity.md`, `story-032-replicated-allowlist.md`; `production/epics/crowd-npc-behaviour/EPIC.md` (50 NPCs walking)
- Unlocks: `production/epics/shared-character-locomotion/story-044-anim-budget-50.md` (shares the reference scene)
