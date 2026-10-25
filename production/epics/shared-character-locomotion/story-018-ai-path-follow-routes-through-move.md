# Story 018: AI path following enters only through Move

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R5.7 AI motion enters through Move; AC-SCL-40)
**Requirement**: `TR-scl-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6, "One input path into the CMC"; Alternative 4 rejected)
**ADR Decision Summary**: Human and AI input both reach the CMC as the same input vector and `bWantsRun` bit. `RequestDirectMove` and `RequestPathMove` are overridden to fault and do nothing, so no path-following component, direct velocity request, `SetActorLocation`, launch or teleport may move a visible pawn. Crowd's follower (ADR-0016, input emulation) issues `Move(Direction, Gait)`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `RequestDirectMove`, `RequestPathMove` and `bRequestedMoveUseAcceleration` are CMC members (E8); `bRequestedMoveUseAcceleration` stays true as a backstop. Navigation (`AIModule`, `NavigationSystem`) is allowed in `StealthGameplay` Private only.

**Control Manifest Rules (this layer)**:
- Required: AI movement enters only through `UPawnCommandComponent::Move` (ADR-0010, ADR-0011).
- Forbidden: never move a pawn through `RequestDirectMove`/`RequestPathMove` or AI path following (`bUseAccelerationForPaths`); no `SetActorLocation` or launch on a visible pawn.
- Guardrail: movement shaping under 5 % of the 3.0 ms CMC line (40 server-simulated NPCs plus proxies).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-40** [P] `AiPathFollowRoutesThroughMove`: GIVEN an NPC walking a 60 s navigation path with corners, WHEN calls are traced, THEN 100 % of its accelerating input comes through `Move(Direction, Gait)`, with 0 direct velocity requests, `SetActorLocation` or launch calls on the pawn, |Direction| ∈ {0.8, 0.9, 1.0}, and its release-to-stop is 0.75 s ± 1 frame over 53 ± 3 cm (as AC-SCL-06).

*QA clarification (create-stories 2026-10-25, not a GDD change):* "0 direct velocity requests" is measured as backstop hits on `RequestDirectMove`/`RequestPathMove` counted through the fault sink, plus spies on `SetActorLocation` and `LaunchCharacter`.

---

## Implementation Notes

*Derived from ADR-0010 section 6 and GDD R5.7:*

- Override `RequestDirectMove` and `RequestPathMove` to report to the fault sink and do nothing. Trace in the test with a call-recording shim on those entry points, `SetActorLocation`, `LaunchCharacter` and `AddForce`/velocity setters (a test-only hook, never shipped).
- The pawn side is complete here; the Crowd epic owns the follower that turns path corners into `Move` input. If the follower does not exist yet, drive the NPC with a minimal test follower that only calls `Move`, and flag that the full Crowd follower must re-run this test.
- **Prototype replaced**: `StealthNpcController` (`Source/StealthGame/Private/StealthNpcController.cpp`) drives pawns by path following with `bUseAccelerationForPaths`; ADR-0010 requires it be replaced, not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The Crowd follower and path smoothing (`production/epics/crowd-npc-behaviour/EPIC.md`).
- Story 019: pace-set distribution in crowds.

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMovementComponent.cpp` (`RequestDirectMove`, `RequestPathMove` overrides)
- `Source/StealthGameplay/Private/Tests/Locomotion/AiPathFollowRoutesThroughMoveTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.AiPathFollowRoutesThroughMove`.*

Test: AI path following routes through Move (AC-SCL-40)
  Given: an NPC walking a 60 s navigation path with corners (a test follower that only calls `Move` until Crowd's follower exists)
  When: calls are traced (fault-sink backstop hits on `RequestDirectMove`/`RequestPathMove`, spies on `SetActorLocation` and `LaunchCharacter`)
  Then: 100 % of accelerating input arrives via `Move(Direction, Gait)`; 0 direct velocity requests, `SetActorLocation` or launch calls; |Direction| ∈ {0.8, 0.9, 1.0}; release-to-stop is 0.75 s ± 1 frame over 53 ± 3 cm (as AC-SCL-06)
  Edge cases: the stop figures apply to a 1.0-pace stop only

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/AiPathFollowRoutesThroughMoveTest.cpp`, test name `StealthGame.Locomotion.AiPathFollowRoutesThroughMove` — must exist and pass; retained screenshot of an NPC walking its path in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-011-release-baseline-stop.md`, `story-012-turn-shaping.md`; `production/epics/input-interaction-framework/EPIC.md` (`Move`), `production/epics/crowd-npc-behaviour/EPIC.md` (follower)
- Unlocks: `production/epics/shared-character-locomotion/story-019-npc-pace-distribution.md`
