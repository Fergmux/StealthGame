# Story 053: NPCs actually run, vault and climb in the Crowd reference scene

> **Epic**: Shared Character & Locomotion
> **Status**: Ready — unblocked 2026-10-25: spike 045 reported (production/qa/spikes/2026-10-25_SCL045-net-emulation-traversal.md); still depends on the traversal stories 048/049 (now Ready) being built first.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R7.3 pillar constraint; AC-SCL-37 with Crowd; OQ7)
**Requirement**: `TR-scl-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (Pillar 1), with ADR-0016 (Crowd issues `Traverse` and run commands through the IIF)
**ADR Decision Summary**: Every SCL tell must have a non-zero NPC baseline set by Crowd & NPC Behaviour; Crowd decides the rate, not whether there is one. NPCs reach traversal through navigation links via the same `Traverse` entry as players.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Counts are read from the published server state (`IsRunning`, `IsInTraversal`, story 013/048), not from animation.

**Control Manifest Rules (this layer)**:
- Required: AI traversal enters through the IIF `Traverse` entry like any pawn.
- Forbidden: no NPC-only movement path; gameplay never reads animation.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-37** [P] `NpcTellBaselineNonZero` (with Crowd): GIVEN 10 minutes of the Crowd reference scene, WHEN NPC runs, vaults and climbs are counted, THEN each count is > 0.

---

## Implementation Notes

*Derived from the GDD:*

- Count transitions of `IsRunning` and starts of `IsInTraversal` for NPC-driven pawns over a 10-minute accelerated fixed-step run with a fixed seed. The threshold is deliberately only "> 0": Crowd's own rates (CNB-R16: 0.148 runs, 0.042 vaults, 0.018 climbs per NPC per minute) are asserted by Crowd's criteria.
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Crowd's rate calibration (Crowd epic). Story 052: matrix rows.

---

## Files

- `Source/StealthGameplay/Private/Tests/Locomotion/NpcTellBaselineNonZeroTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (BLOCKED on the traversal stories). Test `StealthGame.Locomotion.NpcTellBaselineNonZero`.*

Test: NPC tell baseline is non-zero (AC-SCL-37)
  Given: 10 minutes of the Crowd reference scene (accelerated fixed-step run, fixed seed)
  When: NPC runs, vaults and climbs are counted from the published `IsRunning` and `IsInTraversal` state
  Then: each count is > 0
  Edge cases: needs the Crowd epic and traversal; the "> 0" threshold is weak next to CNB-R16's rates (0.148 runs, 0.042 vaults, 0.018 climbs per NPC per minute) and Crowd's own criteria assert the rates

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/NpcTellBaselineNonZeroTest.cpp`, test name `StealthGame.Locomotion.NpcTellBaselineNonZero` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-048-traversal-start-parity.md`, `story-013-isrunning-hysteresis.md`; `production/epics/crowd-npc-behaviour/EPIC.md` (reference scene, rates)
- Unlocks: None
