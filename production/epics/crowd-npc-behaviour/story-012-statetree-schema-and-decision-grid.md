# Story 012: StateTree schema, assets, decision grid and the per-frame due-time list

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-002 = StateTree per NPC (~4 Hz) + follower emitting Move/Look through the IIF; no path-following driving.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0004 (asset keys `CrowdFullStateTreeAsset` / `CrowdRestrictedStateTreeAsset`), ADR-0002 section on AI timing (decision timing is input generation, not a gameplay timer)
**ADR Decision Summary**: Two StateTree assets on one schema (a `UStateTreeAIComponentSchema` subclass with `ScheduledTickPolicy = Allowed` and `AIControllerClass = AStealthCrowdController`): `ST_CrowdFull` (the GDD States table) and `ST_CrowdRestricted` (Routing, Pausing, Halted). Each pawn has a decision grid `t = Phase + k / CrowdDecisionPerSec` re-imposed after every tree tick; behaviour clocks run on a per-frame due-time list in the controller, so timing statistics do not depend on the decision rate. Event interrupts: Strike, Salient, Path.Blocked, Path.Failed, Converse.Faced; priority Halted > Reacting > the rest.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (StateTree is post-cutoff; treat as HIGH knowledge gap)
**Engine Notes**: Facts E5, E6, E12, E14, E15 of the ADR: `UCrowdStateTreeComponent : UStateTreeAIComponent` so it can call the protected `ScheduleTickFrame` and override `TickComponent`; the `ScheduledTickPolicy` is cached on the compiled tree (recompile after schema change); `StateTree.Component.ScheduledTickEnabled` off forces every-frame ticks; the component re-schedules itself after every tick and events wake it with `MakeNextFrame`; brain components tick in `TG_DuringPhysics` unless moved to `TG_PrePhysics`; `SendStateTreeEvent` to a stopped tree logs and does nothing. The `StateTree` and `GameplayStateTree` plugins are already enabled in `StealthGame.uproject`; add the `StateTreeModule`/`GameplayStateTreeModule` module dependencies to `StealthGameplay.Build.cs` in this story.

**Why this story**: This is the decision layer for every behaviour story that follows, and the only place where the 4 Hz cadence, phase spread and event wake-ups interact. Read GDD States and Transitions and ADR-0016 section 3 in full.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: StateTree assets use the crowd schema with `ScheduledTickPolicy = Allowed` and are recompiled after any schema change; the Director asserts `StateTree.Component.ScheduledTickEnabled`; the component ticks in `TG_PrePhysics` after the Director and before the controller; decision cadence is one global `CrowdDecisionPerSec` re-imposed after every tree tick; reaction and behaviour delays live in the controller's per-frame due-time list (ADR-0016)
- Forbidden: BT/StateTree with stock `MoveTo`; never time crowd delays in decision-tick counts or the engine timer manager; never rely on a StateTree event alone for Halt or handover (ADR-0016)
- Guardrail: StateTree work sits in the `Crowd.StateTree` scope; tasks are thin and allocation-free per tick

---

## Acceptance Criteria

*From ADR-0016 section 3, GDD States and Transitions and AC-CNB-33, scoped to this story:*

- [x] `ST_CrowdFull` and `ST_CrowdRestricted` use the crowd schema subclass (`ScheduledTickPolicy = Allowed`, `AIControllerClass = AStealthCrowdController`); a boot test fails on a stale compiled asset or `ScheduledTickEnabled` off.
- [x] `UCrowdStateTreeComponent` ticks in `TG_PrePhysics` after the Director and before the controller and re-imposes the phase grid after every tree tick and event; the per-frame decision bound of story 004 holds with events injected.
- [x] A due-time entry (pause end, glance hold end, red-herring due, reaction start) acts in the controller tick in which it falls due (error at most 1 frame) at `CrowdDecisionPerSec` 4 and 3.
- [x] `Crowd.Stimulus.Strike`, `Crowd.Stimulus.Salient`, `Crowd.Path.Blocked`, `Crowd.Path.Failed` and `Crowd.Converse.Faced` interrupt with priority Halted > Reacting > the rest.

---

## Implementation Notes

*Derived from ADR-0016 section 3:*

- Schema: subclass of `UStateTreeAIComponentSchema`; `ScheduledTickPolicy` is a protected editable property inherited from `UStateTreeComponentSchema` (set it in the subclass default). Do not rely on the cvar default (`StateTree.Component.DefaultScheduledTickAllowed`). Assets are authored in-editor under `Content/StealthGame/AI/` and referenced by soft path through the tuning keys of story 003.
- `UCrowdStateTreeComponent::TickComponent`: `Super`, then `ScheduleTickFrame(FStateTreeScheduledTick::MakeCustomTickRate(NextGridPoint - Now))` unless the tree asked to sleep or tick every frame. If this proves fragile (story 001 criterion 2), use the fallback: the Director drives `ScheduleTickFrame` per pawn.
- States: `Dormant`, `Emerging`, `Routing`, `Pausing`, `Conversing`, `Tending`, `Bracing`, `Hiding`, `Exiting`, `Reacting`, `Halted` (full tree); `Routing`, `Pausing`, `Halted` with C1, C2, C5, C6, G1-G3, G5, G7 only (restricted). This story creates the skeleton, schema, grid and interrupts; each behaviour story fills its state's tasks.
- Activity mirror: each state's enter/exit task writes `ECrowdActivity` into the Director ledger; takeover eligibility reads authoritative sources first and the mirror only for `Reacting` and `Conversing` (story 030).
- Due-time list: small per-controller array checked each controller tick against `ServerNow()`; a due entry acts through the follower or sink at once and, when a decision is needed, wakes the tree with `ScheduleTickFrame(MakeNextFrame)`.
- Tasks choose (destination, pause length, glance target, red-herring due, bush, court) and hand intents to the follower or sink; they never move the pawn.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the `FTimerManager` pause and wander timers in `Source/StealthGame/Private/StealthNpcController.cpp`. The prototype has no StateTree; this is all new.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014-016, 019-022, 024, 026-029: the behaviours inside each state
- Story 031: Halt and handover as direct calls
- Story 004: the Director and the decision-spread bound

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdStateTreeTypes.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdStateTreeTypes.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdStateTreeSchema.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdStateTreeSchema.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdStateTreeComponent.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdStateTreeComponent.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdDueTimeList.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdDueTimeList.cpp`
- `Source/StealthGameEditor/Private/Crowd/CrowdAssetAuthoring.cpp`
- `Content/StealthGame/AI/ST_CrowdFull.uasset`
- `Content/StealthGame/AI/ST_CrowdRestricted.uasset`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdStateTreeTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.StateTreeBootCheck`
  - Given: the two StateTree assets, plus a stale compiled asset and a run with `ScheduledTickEnabled` off
  - When: the boot test loads and inspects them
  - Then: the crowd schema subclass has `ScheduledTickPolicy` Allowed and `AIControllerClass` `AStealthCrowdController`; the stale asset or the CVar off fails the boot test
  - Edge cases: wrong schema class
- **AC-2**: `StealthGame.Crowd.DecisionGridUnderEvents`
  - Given: 50 StateTree components with events injected on 20 % of pawns each second
  - When: a 60 s fixed-step run logs tick order
  - Then: the component ticks in `TG_PrePhysics` after the Director and before the controller; the grid is re-imposed after every tree tick and event; decisions per frame at most `ceil(N / (FPS / Hz)) + 2`
  - Edge cases: an event on a tick frame
- **AC-3**: `StealthGame.Crowd.DueTimeIndependentOfDecisionRate`
  - Given: due-time entries for pause end, glance hold end, red-herring due and reaction start
  - When: run at `CrowdDecisionPerSec` 4 and 3 at 60 fps
  - Then: each entry acts in the controller tick in which it falls due, error at most 1 frame
  - Edge cases: due between decision ticks; a hitch frame
- **AC-4**: `StealthGame.Crowd.EventInterruptPriority`
  - Given: a pawn receiving Strike, Salient, Path.Blocked, Path.Failed and Converse.Faced events
  - When: they arrive in the same frame, in combinations
  - Then: Halted preempts Reacting, which preempts the rest; a lower-priority event never interrupts a higher one
  - Edge cases: Halt latched during a reaction; duplicate events

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdStateTreeTest.cpp` (automation plus PIE for tick order) — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.StateTree.AssetsBootCheck`, `StealthGame.Crowd.StateTree.GridWithEvents`, `StealthGame.Crowd.StateTree.DueTimeAccuracy`, `StealthGame.Crowd.StateTree.DueTimeList`; 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-001-spike-engine-verifications.md`, `production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md`, `production/epics/crowd-npc-behaviour/story-006-crowd-controller-and-look-pitch.md`, `production/epics/crowd-npc-behaviour/story-010-virtual-input-layer.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-013-identity-and-command-path-gate.md`, `production/epics/crowd-npc-behaviour/story-014-pauses-and-stall-cover.md`, `production/epics/crowd-npc-behaviour/story-015-conversation-and-turn-in-place.md`, `production/epics/crowd-npc-behaviour/story-016-gaze-catalogue.md`, `production/epics/crowd-npc-behaviour/story-018-tell-sampler-and-calibration-counters.md`, `production/epics/crowd-npc-behaviour/story-020-decoy-tending-herring.md`, `production/epics/crowd-npc-behaviour/story-021-brace-and-startle-herring.md`, `production/epics/crowd-npc-behaviour/story-024-strike-stimulus-and-reactions.md`, `production/epics/crowd-npc-behaviour/story-028-bound-pawn-restricted-driver.md`, `production/epics/crowd-npc-behaviour/story-030-takeover-eligibility-and-atomic-release.md`, `production/epics/crowd-npc-behaviour/story-031-halt-and-handover-epoch.md`, `production/epics/crowd-npc-behaviour/story-032-spike-60-character-server-cost.md`, `production/epics/crowd-npc-behaviour/story-039-retire-prototype-crowd.md`

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 4/4 passing
**Deviations**: Trees are authored by Stealth.Crowd.BuildAssets (not by hand in the editor). The component skips off-grid ticks nobody requested and UCrowdDirector::AdmitWakeDecision admits one wake decision a frame so the story 004 bound holds with events (headless max 5, bound 6); events go through UCrowdStateTreeComponent::SendCrowdEvent.
**Test Evidence**: `StealthGame.Crowd.StateTree.AssetsBootCheck`, `StealthGame.Crowd.StateTree.GridWithEvents`, `StealthGame.Crowd.StateTree.DueTimeAccuracy`, `StealthGame.Crowd.StateTree.DueTimeList`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
