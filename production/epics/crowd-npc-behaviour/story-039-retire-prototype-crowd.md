# Story 039: Retire the prototype crowd: swap to the Director and one pawn class

> **Epic**: Crowd & NPC Behaviour
> **Status**: Blocked — BLOCKED (2026-10-27): the prototype ScheduleNpcReplacement / FindCrowdPoint path is still the only mid-round refill in L_Prototype, because the Level Socket Contract emergence sockets (and CPA DrawReplacement) the Director's ambient schedule needs are not built (story 026 built the schedule against seams); removing it would leave killed NPCs unreplaced, and the AC's 5-minute PIE needs ambient exit and entry, which need sockets. Round's spawn list already places the round's NPCs through the Director (AStealthGameMode::SpawnStartPawn -> SpawnNpc -> RequestStartPlacement) and no AStealthNpcController is possessed (D53, batch 18); SpawnCrowd has no caller
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-001`, `TR-crowd-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-001 = Server-only Crowd Director subsystem. TR-crowd-008 = Ambient emergence and replacement via shared sockets and DrawReplacement.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0001 and ADR-0010 (one `AStealthPawn` class), ADR-0007 (prototype promotion is a rewrite, deletions are an always-ask category)
**ADR Decision Summary**: The prototype `AStealthNpcController` (`MoveToLocation`, `FMath::FRandRange`, `FTimerManager`, `GetRandomReachablePointInRadius` wander) is replaced, not ported; no code moves into `StealthGameplay` until the governing ADR is Accepted (it is), and `BP_CrowdCharacter` is retired in favour of the one `AStealthPawn` class.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: When a production class reaches a path a Blueprint already references, ADR-0007 requires a `[CoreRedirects] +ClassRedirects=(OldName="/Script/StealthGame.X",NewName="/Script/StealthGameplay.Y")` entry in the same change, and affected Blueprints are resaved by explicit path (never mass-save). File deletion is an always-ask category: the implementer asks the user before deleting anything.

**Why this story**: The prototype crowd and the production Director must not both run. This story is the switch-over: it removes the prototype crowd path from the round, retires the Blueprint wrapper, and proves a full round still plays. Without it the epic would ship two crowd systems. Read GDD "Prototype Disposition" in the epic file and ADR-0007 section on promotion.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: population changes only through the Director, `RequestEmergence` and the pawn factory (ADR-0016); one pawn class, one look path (ADR-0001, ADR-0010)
- Forbidden: spawning, hiding or destroying a pawn by any other path; porting prototype code; deleting files without user approval (ADR-0016, ADR-0007)
- Guardrail: the prototype stays compiling until this story lands; no `Content/*.uasset` is created or edited by script (editor-only, by explicit path)

---

## Acceptance Criteria

*From the epic's Prototype Disposition and ADR-0016 Migration Plan, scoped to this story:*

- [ ] GIVEN the Crowd Director owns population, WHEN a round runs and the code is scanned, THEN no round code calls the prototype `SpawnCrowd`, `SpawnCrowdWhenNavReady`, `ScheduleNpcReplacement` or `FindCrowdPoint` (Blueprint call sites included in the scan), and no `AStealthNpcController` is possessed by any pawn.
- [ ] `BP_CrowdCharacter` is retired in favour of the one shared pawn class (ADR-0001, ADR-0010) with a CoreRedirect for any referenced class, and the prototype files are deleted only after the user's approval is recorded in the story.
- [ ] A 2-client PIE round on the graybox with 50 NPCs completes with the same player-visible behaviour classes (walk, pause, ambient exit and entry, strike reaction) observed at least once in 5 min, and the `StealthGame.Crowd.*` suite passes.

---

## Implementation Notes

*Derived from the epic's Prototype Disposition and ADR-0007 section on promotion:*

- Prototype pieces to retire: `Source/StealthGame/Private/StealthNpcController.cpp` and `Public/StealthNpcController.h`; in `Source/StealthGame/Private/StealthGameMode.cpp`: `SpawnCrowd`, `SpawnCrowdWhenNavReady`, `ScheduleNpcReplacement`, `FindCrowdPoint`; the Blueprint wrapper `/Game/StealthGame/Characters/BP_CrowdCharacter`. The prototype `StealthBot` console driver is a test aid and stays.
- The composition root (`AStealthGameMode`, ADR-0007) calls `UCrowdDirector::OnRoundActive` and registers the `ICrowdControl` and `ITakeoverEligibilitySource` faces weakly; Round's spawn list places `NpcCount` NPCs through LSC `RequestStartPlacement` (story 017).
- Order of work: first make the production path run side by side behind a switch, prove the PIE round, then remove the prototype path, then (with approval) delete the files. Never leave a build where neither crowd runs.
- The prototype crowd defaulted to 40 NPCs and the production default is 50 (`NpcCount`); expect behaviour and cost to differ, which is why this follows the spike (story 032).
- Ask the user before deleting files; record the approval and the deleted list in the story.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): This story is the disposition. Replaces `Source/StealthGame/Private/StealthNpcController.cpp`, the crowd spawning in `Source/StealthGame/Private/StealthGameMode.cpp`, and `BP_CrowdCharacter`. Nothing from them is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Stories 004, 012, 026: the Director, trees and schedule being switched to
- The character pool epoch (`production/epics/character-pool-appearance/EPIC.md`): the shared pawn's look and card deck
- Deleting prototype player-side code (HUD, objectives, bushes): other epics own those

---

## Files

- `Source/StealthGame/Private/StealthGameMode.cpp` (remove crowd spawning, call the Director)
- `Source/StealthGame/Private/StealthNpcController.cpp`, `Source/StealthGame/Public/StealthNpcController.h` (delete with approval)
- `Config/DefaultEngine.ini` (CoreRedirects entries, if any)
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdPrototypeRetirementTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.NoPrototypeCrowdPath`
  - Given: the Crowd Director owns population
  - When: a round runs and the code (including Blueprints) is scanned
  - Then: no round code calls `SpawnCrowd`, `SpawnCrowdWhenNavReady`, `ScheduleNpcReplacement` or `FindCrowdPoint`; no `AStealthNpcController` is possessed by any pawn
  - Edge cases: Blueprint call sites included in the scan
- **AC-2**: `StealthGame.Crowd.PrototypeAssetRetirement`
  - Given: `BP_CrowdCharacter` and its referencing assets
  - When: the asset registry and redirect table are inspected
  - Then: `BP_CrowdCharacter` is retired in favour of the one shared pawn class; a CoreRedirect exists for any referenced class; prototype files are deleted only after the user's approval is recorded in the story
  - Edge cases: stale references in maps
- **AC-3**: `StealthGame.Crowd.PIE.PrototypeRetirement`
  - Given: a 2-client PIE round on the graybox with 50 NPCs
  - When: run for 5 min
  - Then: each behaviour class (walk, pause, ambient exit and entry, strike reaction) is observed at least once; the `StealthGame.Crowd.*` suite passes
  - Edge cases: needs a PIE round with 2 clients (deferred to PIE)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdPrototypeRetirementTest.cpp` (automation plus PIE `StealthGame.Crowd.PIE.PrototypeRetirement`) — must exist and pass; PIE round log in `production/qa/evidence/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`, `production/epics/crowd-npc-behaviour/story-026-ambient-schedule-and-emergence.md`
- Unlocks: None
