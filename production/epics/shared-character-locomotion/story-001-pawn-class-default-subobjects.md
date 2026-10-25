# Story 001: One Shared Pawn class with an identical default subobject set

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R1 One class; R3 Possession parity)
**Requirement**: `TR-scl-001`, `TR-scl-013`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (also ADR-0001, ADR-0007 for placement)
**ADR Decision Summary**: Every body in the city is one `AStealthPawn : ACharacter` on one CMC subclass (`UStealthMovementComponent`), with the same default subobjects on every instance, created only by the factory, and an engine `Restart()` path that cannot zero velocity or reset the movement mode.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Mover/ChaosMover/NetworkPrediction are Experimental or removed in 5.8: no code, Blueprint or asset may name one (the sample-enabled plugins are disabled by a `.uproject` edit that is a pending user item, not part of this story). `UMotionWarpingComponent` is Beta in 5.8.4 (`GetOwnerAdapter()` replaces the removed `GetCharacterOwner`; `WarpTargets` is a protected member whose replication is disabled with `DISABLE_REPLICATED_PROPERTY_FAST`). `Possess`/`UnPossess` are `virtual final`: override `OnPossess`/`OnUnPossess` only.

**Control Manifest Rules (this layer)**:
- Required: the Shared Pawn uses `UStealthMovementComponent` (a CMC subclass); every `AStealthPawn` carries the same default subobjects, including a non-replicated `UGameplayTasksComponent` and `UStealthMotionWarpingComponent` with `WarpTargets` replication disabled (ADR-0010, ADR-0016, ADR-0018).
- Forbidden: never reference a Mover, ChaosMover or NetworkPrediction type; never add a component on possession; never call `StopMovementImmediately`, `SetDefaultMovementMode` or `Super::Restart` from `AStealthPawn::Restart`; never zero velocity in `PawnClientRestart`.
- Guardrail: movement shaping stays well under 5 % of the 3.0 ms CMC game-thread line (a later story measures it; this story adds no per-tick work).

---

## Acceptance Criteria

*Derived from ADR-0010 section 2 and Implementation Guidelines (no GDD acceptance criterion owns the class scaffold; AC-SCL-43 and AC-SCL-44 check it from the outside in stories 032 and 041):*

- [ ] GIVEN the compiled `StealthGameplay` module, WHEN the `AStealthPawn` class default object and a spawned instance are reflected, THEN `AStealthPawn` derives from `ACharacter` with `UStealthMovementComponent` as its movement component, carries a non-replicated `UGameplayTasksComponent` and a `UStealthMotionWarpingComponent` (with `WarpTargets` replication disabled) as default subobjects, has `bUseControllerRotationYaw = false`, `bOrientRotationToMovement = true` and `bUseRVOAvoidance = false`, `AutoPossessAI = Disabled`, `AutoPossessPlayer = Disabled`, `SpawnDefaultController` overridden to do nothing, and its `Restart()` override neither calls `Super` nor changes velocity or movement mode.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the `Restart()` override is asserted through its observable consequence (velocity, movement mode and pending input vector unchanged after the call); "identical default subobjects" is a fixture list of (class, name, replicates) compared across two instances; a source scan for the `Super` call is secondary.

---

## Implementation Notes

*Derived from ADR-0010 Decision sections 2 and 4.2 and its Implementation Guidelines:*

- Create `AStealthPawn` in `StealthGameplay/Core/SharedPawn/` (ADR-0007: `Core/` must never include `Features/`; public headers go in `Public/View/` or `Public/Server/`). `UStealthMovementComponent` is set with `ObjectInitializer.SetDefaultSubobjectClass`. This story creates the class and subobject set only; shaping rules, factory, look path and animation arrive in later stories, so every later story builds on the same default subobject list and none may add a component at runtime.
- `AStealthPawn::Restart()` calls `RecalculateBaseEyeHeight()` only. This is safe because `APawn::Restart` does nothing else (E2) and crouch and jump do not exist in the MVP. `PawnClientRestart()` calls `ResetPredictionData_Client()` then `APawn::PawnClientRestart()` directly, skipping `ACharacter`'s `StopMovementImmediately`.
- `AStealthPawn::AIControllerClass` is set to `AStealthCrowdController` (declared by the Crowd epic; use a soft class reference until it exists). `AStealthPawn` disables `WarpTargets` replication in its warping subclass (`COND_Never` audit row) identically for every pawn.
- The sample's `CBP_SandboxCharacter` is reparented onto this class later (a Content step with the never-resave hazards in `production/never-resave-assets.md`; add the `[CoreRedirects]` entry in the same change when the class moves module, per ADR-0007). Do not touch any `.uasset` here.
- **Prototype replaced**: this class replaces `AStealthCharacter` (`Source/StealthGame/Public/StealthCharacter.h`, `Private/StealthCharacter.cpp`, 675 lines: movement, appearance, death presentation and per-feature RPCs in one class) and prototype `StealthMovementComponent.*`. Promotion is a rewrite, not a file move; the prototype classes keep compiling in `StealthGame` until each is replaced and are deleted only with user approval.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002: capsule size, `MaxStepHeight` and body constants.
- Story 006: `UStealthPawnFactory::SpawnSharedPawn` and the dead default-pawn paths.
- Story 009 onward: movement shaping inside `UStealthMovementComponent`.
- Story 032: the replicated-property allowlist.

---

## Files

*The code and test files this story creates or changes. `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/StealthPawn.h` (planned)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp` (planned)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMovementComponent.cpp` (skeleton only, planned)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMotionWarpingComponent.cpp` (planned)
- `Source/StealthGameplay/Private/Tests/Locomotion/PawnClassDefaultsTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.PawnClassDefaults`.*

Test: class and default-subobject parity (ADR-0010 section 2)
  Given: the `AStealthPawn` CDO and two spawned instances, one possessed by a PlayerController stand-in and one by `AStealthCrowdController`
  When: the component lists are enumerated, an AI possession runs, and `Restart()` is called on a pawn with Velocity (140,0,0) in `MOVE_Walking`
  Then: the class derives `ACharacter` with `UStealthMovementComponent`; both instances have the same ordered list of (component class, name, replicates); a `UGameplayTasksComponent` exists on both with replication off and AI possession adds no component; the motion-warping component is `UStealthMotionWarpingComponent` with `WarpTargets` absent from the replicated set (COND_Never); after `Restart()` Velocity and movement mode are unchanged and the pending input vector is not consumed
  Edge cases: the `ReceiveRestarted` event still fires (`DispatchRestart`); the reparented `CBP_SandboxCharacter` is checked in a separate Content step with no resave; a source scan for a `Super::Restart` call is secondary

---

## Test Evidence

*Governed by `qa.level` (full for this project): Logic stories need an automated test that exists and passes.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/PawnClassDefaultsTest.cpp`, test name `StealthGame.Locomotion.PawnClassDefaults` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: the module skeleton and `-nullrhi` test runner from `production/epics/tuning-data-config/EPIC.md` (Exit A, bootstrap; stories not yet written, path TBD)
- Unlocks: `production/epics/shared-character-locomotion/story-002-body-constants-published.md`, `story-004-class-parity-scan.md`, `story-006-single-spawn-site.md`, `story-009-stop-floor.md`
