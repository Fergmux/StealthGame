# Story 001: One Shared Pawn class with an identical default subobject set

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-25

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

- [x] GIVEN the compiled `StealthGameplay` module, WHEN the `AStealthPawn` class default object and a spawned instance are reflected, THEN `AStealthPawn` derives from `ACharacter` with `UStealthMovementComponent` as its movement component, carries a non-replicated `UGameplayTasksComponent` and a `UStealthMotionWarpingComponent` (with `WarpTargets` replication disabled) as default subobjects, has `bUseControllerRotationYaw = false`, `bOrientRotationToMovement = true` and `bUseRVOAvoidance = false`, `AutoPossessAI = Disabled`, `AutoPossessPlayer = Disabled`, `SpawnDefaultController` overridden to do nothing, and its `Restart()` override neither calls `Super` nor changes velocity or movement mode.

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

- `Source/StealthGameplay/Public/View/Core/StealthPawn.h` (header in `View/Core/` per the ADR-0010 Key Interfaces amendment, architecture review 2026-10-25 Conflict 4; was planned under `Server/Core/`)
- `Source/StealthGameplay/Public/View/Core/StealthPawnMovementComponent.h` (declares `UStealthMovementComponent`; file name avoids the prototype's `StealthMovementComponent.h`, ADR-0007 rule (f))
- `Source/StealthGameplay/Public/View/Core/StealthMotionWarpingComponent.h`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnMovementComponent.cpp` (skeleton: kind-blind defaults only)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMotionWarpingComponent.cpp`
- `Source/StealthGameplay/StealthGameplay.Build.cs` (MotionWarping public; GameplayTasks, AIModule private)
- `Source/StealthGameplay/Private/Tests/Locomotion/PawnClassDefaultsTest.cpp` (test `StealthGame.Locomotion.PawnClassDefaults`)
- `Source/StealthGame/Public/StealthMovementComponent.h`, `Private/StealthMovementComponent.cpp`, `Private/StealthCharacter.cpp` (prototype class renamed `UStealthPrototypeMovementComponent`; UHT forbids two `UStealthMovementComponent` classes)
- `Config/DefaultEngine.ini` (`[CoreRedirects]` for the renamed prototype class)

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

**Status**: [x] Created and passing (`StealthGame.Locomotion.PawnClassDefaults`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md` (module skeleton) and `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md` (`-nullrhi` test runner), both Complete (readiness fix 2026-10-25: the TBD path is now known)
- Unlocks: `production/epics/shared-character-locomotion/story-002-body-constants-published.md`, `story-004-class-parity-scan.md`, `story-006-single-spawn-site.md`, `story-009-stop-floor.md`

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (`StealthGame.Locomotion.PawnClassDefaults`: CDO derives `ACharacter` with `UStealthMovementComponent`; `bUseControllerRotationYaw` false, `bOrientRotationToMovement` true, `bUseRVOAvoidance` false, `AutoPossessAI`/`AutoPossessPlayer` Disabled; `SpawnDefaultController` spawns nothing even with an AI controller class set; two spawned instances (one possessed by a `APlayerController` stand-in, one by `AAIController`) have the same (class, name, replicates) list and AI possession adds no component; exactly one non-replicated `UGameplayTasksComponent` (the default subobject); `UStealthMotionWarpingComponent.WarpTargets` lifetime row is `COND_Never` (control: the engine parent's row is replicated); `Restart()` keeps Velocity (140,0,0), `MOVE_Walking` and the pending input vector; `DispatchRestart(true)` (the `PawnClientRestart` path) keeps velocity and mode (control: `StopMovementImmediately` zeroes it); AI possession keeps velocity and mode; secondary source scan finds no `Super::Restart`/`ACharacter::Restart`/`SetDefaultMovementMode` call and no `NotifyRestarted`/`DispatchRestart` override, so `ReceiveRestarted` still fires)
**Decisions** (logged): pawn and movement headers in `Public/View/Core/` (ADR-0010 amendment beats the story's planned `Server/Core/` path; a `Server/` UCLASS would also have to be `NotBlueprintable`, which the reparented `CBP_SandboxCharacter` cannot be); prototype `UStealthMovementComponent` renamed to `UStealthPrototypeMovementComponent` with a `[CoreRedirects]` entry (two UCLASSes of one name cannot coexist; prototype files keep their names so `prototype-exempt.txt` does not grow); production header named `StealthPawnMovementComponent.h` (the generated headers of two `StealthMovementComponent.h` files would collide on the composition root's include path); `AIControllerClass = nullptr` (no `AStealthCrowdController` yet, and `SpawnDefaultController` is a no-op, so no soft reference is needed); `AAIController` stands in for `AStealthCrowdController` in the test.
**Deviations**: header paths and the prototype rename above (files outside the planned list: prototype movement component files, `StealthCharacter.cpp`, `DefaultEngine.ini`, `StealthGameplay.Build.cs`).
**Test Evidence**: `production/qa/evidence/story-001-pawn-class-default-subobjects/test-run.txt` (build `Result: Succeeded`; 26/26 `StealthGame.` tests Success; watched to fail first) and `stamp.txt`
**Run result**: N/A — class scaffold with no pawn spawned in any map yet (the factory is SCL-006); the renamed prototype class was checked live in the editor (both sample character CDOs resolve `StealthPrototypeMovementComponent` through the redirect)
**Reviewed by**: focused code review (self-applied, unattended run): no defects found
**Readiness fixes**: Depends-on TBD replaced by the Complete TDC story paths; Manifest Version 2026-10-25e -> 2026-10-25f after reviewing "Changes in 2026-10-25f" (Core additions are the HeadLook filter, factory `ControlKind`, pings, appearance and camera rules: none applies to this scaffold); lint `ac_below_min` (1 < 3) accepted: the single criterion is compound and the QA clarification splits it into five checked facts.
