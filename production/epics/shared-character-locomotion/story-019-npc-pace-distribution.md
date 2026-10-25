# Story 019: NPC pace distribution matches the quantised player pace set

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R5.1 Full-pace rule; AC-SCL-36 with Crowd)
**Requirement**: `TR-scl-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6, "Gaits and pace"; with ADR-0016 input emulation)
**ADR Decision Summary**: One quantiser maps analog magnitude to {0.8, 0.9, 1.0} for every pawn, so an analog stick produces only the same three values Crowd uses; Crowd keeps at least 50 % of NPC walking time at 1.0 and changes pace only at a stop or route-leg boundary.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No engine API specific to this story; it is an observation test over the Crowd reference scene.

**Control Manifest Rules (this layer)**:
- Required: pace quantiser applied where the input is consumed; the keyboard walk-slower modifier gives 0.8 and 0.9 on keys (ADR-0011).
- Forbidden: no kind-specific pace set; Crowd owns no pace-set key (decision D2).
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-36** [P] `NpcPaceDistribution` (with Crowd): GIVEN the Crowd reference scene for 10 minutes, WHEN NPC walk scale is logged, THEN only {0.8, 0.9, 1.0} occur, ≥ 50 % of NPC walking time is at 1.0, every pace change happens at a stop or a route-leg boundary, and a player's analog stick produces only the same three values.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the criterion is deferred to the Crowd reference scene (it cannot pass in this epic alone) and runs as an accelerated fixed-step simulation of 10 minutes with a fixed seed.

---

## Implementation Notes

*Derived from GDD R5.1 and ADR-0016:*

- This is an observation test with a logging hook on the movement component's effective pace (test-only, not shipped). It needs Crowd's reference scene and follower; the pawn-side quantiser is story 010. Drive the "player analog stick" half with a scripted analog input sweep through the saved-move path.
- Log pace at every `Move` consumption and at every stop/route-leg boundary event the Crowd reports; assert the three-value set, the 50 % share and the boundary-only pace change.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010: the quantiser.
- Crowd's pace-selection behaviour (`production/epics/crowd-npc-behaviour/EPIC.md`).

---

## Files

- `Source/StealthGameplay/Private/Tests/Locomotion/NpcPaceDistributionTest.cpp` (planned; 10-minute PIE run, may be tagged slow)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; deferred to the Crowd reference scene). Test `StealthGame.Locomotion.NpcPaceDistribution`.*

Test: NPC pace distribution (AC-SCL-36)
  Given: the Crowd reference scene for 10 simulated minutes (accelerated fixed-step run, fixed seed)
  When: NPC walk scale is logged
  Then: only {0.8, 0.9, 1.0} occur; ≥ 50 % of NPC walking time is at 1.0; every pace change happens at a stop or a route-leg boundary; a player's analog stick produces only the same three values
  Edge cases: needs the Crowd epic's reference scene and pace assignment; cannot pass in this epic alone

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/NpcPaceDistributionTest.cpp`, test name `StealthGame.Locomotion.NpcPaceDistribution` — must exist and pass, or a documented playtest log in `production/qa/evidence/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-010-speed-tiers.md`, `story-018-ai-path-follow-routes-through-move.md`; `production/epics/crowd-npc-behaviour/EPIC.md` (Crowd reference scene)
- Unlocks: None
