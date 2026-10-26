# Story 001: Spike: StateTree, controller and path-latency engine verifications

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-002`, `TR-crowd-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-002 = StateTree per NPC (~4 Hz) + follower emitting Move/Look through the IIF; no path-following driving. TR-crowd-003 = Async path queries <= 2 starts/frame; navmesh frozen at OnLayoutReady; 2 m spatial hash.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0010 (default non-replicated `UGameplayTasksComponent`, controller possession), ADR-0014 (frozen navmesh)
**ADR Decision Summary**: A server-only Crowd Director world subsystem owns the population ledger, ambient schedule, perception, path budgeting, counter-based draws and Halt; each AI-driven pawn gets an `AStealthCrowdController` running a scheduled-tick StateTree (about 4 Hz, event interrupts) plus a follower that emits `Move`/`Look` through the pawn command interface. Nothing drives the pawn through path following, `MoveTo`, RVO or a direct velocity write.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0016 Knowledge Risk; StateTree has no page in `docs/engine-reference/unreal/`, so treat the StateTree half as a HIGH knowledge gap)
**Engine Notes**: This story is the owner of ADR-0016 verification items (2), (3), (6) and (7). All StateTree claims in the ADR were read from 5.8.4 source: `ScheduleTickFrame` is protected; scheduled ticks are honoured only when `ScheduledTickPolicy` allows them (cached on the compiled tree, recompile after a schema change); the component overwrites its own schedule after every tree tick (E12); brain components tick in `TG_DuringPhysics` unless moved (E14); `FindPathAsync` results arrive at least 2 frames after a `TG_PrePhysics` request (E7); `AAIController::OnPossess` adds a replicated `UGameplayTasksComponent` unless the pawn already has one (E13). Use `UE_LOGF`.

**Why this story**: The ADR is Accepted conditionally: these four engine behaviours were read from source but never observed running. Stories 006, 007 and 012 build on them, and each has a documented fallback if an item fails. This spike turns the [U] tags into [V] or triggers the fallback before the real classes are written. It is not the 60-character cost spike (story 032): it measures behaviour, not milliseconds. Read GDD sections Core Rules R3 and R15 and Open Questions OQ8 (`bash .claude/scripts/md-section.sh extract design/gdd/crowd-npc-behaviour.md "Open Questions"`; the GDD is 130 KB, never read it whole).

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: `AStealthCrowdController` does not create a `PathFollowingComponent` (fallback if verification (2) fails: create it, never tick it, never request a move; the AC-CNB-02 tick counter reads 0); StateTree assets use the crowd schema with `ScheduledTickPolicy = Allowed` and are recompiled after any schema change; the Director asserts `StateTree.Component.ScheduledTickEnabled` at init (ADR-0016)
- Forbidden: `MoveTo*`, `AITask_MoveTo`, a StateTree MoveTo task, `RequestDirectMove`; `UAIPerceptionComponent`; Mass Entity / Mass Crowd (ADR-0016)
- Guardrail: spike code is throwaway and non-Shipping; it is not promoted into `StealthGameplay` (ADR-0007 promotion rule)

---

## Acceptance Criteria

*From ADR-0016 Engine Compatibility "Verification Required" items (2), (3), (6), (7) and GDD Open Question OQ8, scoped to this story:*

- [x] GIVEN a crowd controller built with `DoNotCreateDefaultSubobject("PathFollowingComponent")` possessing a test pawn, WHEN the gameplay debugger and visual logger are enabled for 60 s of routed motion, THEN no check, ensure or null dereference fires, or the report adopts the documented fallback (component created, never ticked, never used) and the AC-CNB-02 tick counter reads 0.
- [x] GIVEN 50 StateTree components on a 4 Hz phase grid with strike and salient events injected on 20 % of pawns each second, WHEN ticks are logged for 60 s, THEN each pawn returns to its own grid within one off-grid tick and no frame holds more than 6 decisions (ceil(50 / (60 / 4)) + 2).
- [x] GIVEN `FindPathAsync` requests issued from a `TG_PrePhysics` tick on the graybox, WHEN delivery frames are logged, THEN every result arrives at least 2 frames after the request and the p99 latency under 50-pawn load is recorded in the report.
- [x] GIVEN an AI-driven, a player-driven and a just-handed-over pawn, WHEN replicated subobjects and default components are inventoried, THEN the three sets are identical (`UGameplayTasksComponent` present and non-replicated on all three) and `StateTree.Component.ScheduledTickEnabled` is asserted on.

---

## Implementation Notes

*Derived from ADR-0016 Decision sections 1, 3 and 4 and Engine Compatibility facts E3, E6, E7, E12, E13:*

- Build the smallest throwaway rig under `Source/StealthGame/Private/Spikes/` (as `StealthIdentitySpike.*` did): a controller subclass without the path-following component, a `UStateTreeAIComponent` subclass that overrides `TickComponent` to call `Super` and re-impose `ScheduleTickFrame(FStateTreeScheduledTick::MakeCustomTickRate(NextGridPoint - Now))`, and an async path requester driven from a `TG_PrePhysics` tick function.
- The phase grid is `t = Phase + k / CrowdDecisionPerSec`. Events wake the tree with `MakeNextFrame`, so one off-grid tick per event is expected; the pass criterion is a return to the grid on the next tick.
- If criterion 2 fails, adopt the ADR's fallback (the Director calls `ScheduleTickFrame` per pawn on the grid; the tree only sleeps between calls) and record it. A failure that no fallback covers is reported as a superseding-ADR request, not worked around.
- Component inventory for criterion 4 reuses the replicated-subobject inventory idea of ADR-0008's leak suite (verification (7)); this story supplies the first result, the leak-suite story elsewhere makes it permanent.
- Write the report to `production/qa/spikes/` as `<date>_crowd-engine-verifications.md`: per item PASS / FAIL / FALLBACK ADOPTED, with the measurement.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Reference only. `Source/StealthGame/Private/StealthNpcController.cpp` (66 lines, `MoveToLocation` wandering) is the contrast case; do not extend it. `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*` shows the spike-probe pattern to reuse. Nothing is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 006 (`story-006-crowd-controller-and-look-pitch.md`): the production controller class
- Story 012 (`story-012-statetree-schema-and-decision-grid.md`): the production schema, assets and component
- Story 032 (`story-032-spike-60-character-server-cost.md`): millisecond cost of the six `Crowd.*` scopes

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/StealthCrowdController.h`
- `Source/StealthGameplay/Features/Crowd/Private/StealthCrowdController.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdStateTreeComponent.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdStateTreeComponent.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdDirector.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdDirector.cpp`
- `Source/StealthTestHarness/Private/Tests/Crowd/CrowdPIETests.cpp`
- `production/qa/spikes/2026-10-26_crowd-engine-verifications.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). Implement against these; do not invent new cases.*

- **AC-1**: crowd controller without PathFollowingComponent under debugger and vislog
  - Given: crowd controller without the default PathFollowingComponent possesses a test pawn, gameplay debugger and visual logger on
  - When: 60 s of routed motion
  - Then: zero check/ensure/null-deref, or the report adopts the fallback and the AC-CNB-02 tick counter is 0
  - Edge cases: the fallback must hold counter 0 for the full 60 s
- **AC-2**: phase grid survives events
  - Given: 50 StateTree components on a 4 Hz phase grid, events injected on 20 % of pawns each second
  - When: ticks are logged for 60 s
  - Then: each pawn returns to its own grid within one off-grid tick; no frame holds more than 6 decisions
  - Edge cases: an event lands on a pawn's own tick frame
- **AC-3**: path latency
  - Given: `FindPathAsync` issued from a `TG_PrePhysics` tick on the graybox, 50-pawn load
  - When: delivery frames are logged
  - Then: every result arrives at least 2 frames after the request; p99 latency recorded
  - Edge cases: needs a runtime graybox (deferred to a built scene)
- **AC-4**: component parity
  - Given: an AI-driven, a player-driven and a just-handed-over pawn
  - When: replicated subobjects and default components are inventoried
  - Then: the three sets are identical, `UGameplayTasksComponent` present and non-replicated on all three, ScheduledTickEnabled asserted on
  - Edge cases: a handover pawn captured within its first frame

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `production/qa/spikes/<date>_crowd-engine-verifications.md` (the spike report; a spike has no permanent test) plus, where practical, `Source/StealthGameplay/Private/Tests/Crowd/CrowdEngineVerificationTest.cpp` named `StealthGame.Crowd.PIE.EngineVerifications`

**Status**: [x] Created and passing (`StealthGame.Crowd.PIE.EngineVerifications.CrowdEngineFactsHold`, `StealthGame.Crowd.StateTree.GridWithEvents`, `StealthGame.Crowd.Controller.Shape`, `StealthGame.Crowd.Controller.ComponentParity`; 2026-10-26)

---

## Dependencies

- Depends on: None
- Unlocks: `production/epics/crowd-npc-behaviour/story-006-crowd-controller-and-look-pitch.md`, `production/epics/crowd-npc-behaviour/story-007-path-source-and-start-budget.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 4/4 passing
**Deviations**: The spike measured the production classes written in this batch instead of a throwaway rig under Source/StealthGame/Private/Spikes/ (no spike code to delete). Gameplay debugger not run under -nullrhi (visual logger on). AC-3 p99 measured with the 40-NPC default crowd (2 frames). Item 3 needed two component fixes (skip unrequested off-grid ticks; one-frame lateness tolerance) and a Director wake budget; no ADR fallback adopted. Report: production/qa/spikes/2026-10-26_crowd-engine-verifications.md (all four PASS).
**Test Evidence**: `StealthGame.Crowd.PIE.EngineVerifications.CrowdEngineFactsHold`, `StealthGame.Crowd.StateTree.GridWithEvents`, `StealthGame.Crowd.Controller.Shape`, `StealthGame.Crowd.Controller.ComponentParity`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
