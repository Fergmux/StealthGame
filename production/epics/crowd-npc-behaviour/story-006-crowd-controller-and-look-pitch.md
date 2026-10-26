# Story 006: AStealthCrowdController: no path following, Look pitch path and tick order

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-002 = StateTree per NPC (~4 Hz) + follower emitting Move/Look through the IIF; no path-following driving.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0010 (controller class name, hand-over pass, default `UGameplayTasksComponent`), ADR-0003 (pitch write, `RemoteViewPitch16`)
**ADR Decision Summary**: `AStealthCrowdController` is the one `AAIController` subclass for every AI-driven pawn. It has no `PathFollowingComponent`, `bWantsPlayerState` and `bStartAILogicOnPossess` false, overrides `UpdateControlRotation` to write the last `Look` yaw and pitch into the control rotation (no focal point, no `FaceRotation`), and steps its due-time list and follower before `Super::Tick`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `AAIController::Tick` calls `UpdateControlRotation` every frame and the stock implementation zeroes pitch unless the focus actor is a pawn (E1); that is why the override is mandatory. The controller's tick runs before the pawn's CMC because `AController::AddPawnTickDependency` orders them (E2). `AAIController` creates its path-following component as an optional default subobject, so `DoNotCreateDefaultSubobject(TEXT("PathFollowingComponent"))` suppresses it (E3). Verification (2) of story 001 decides whether the fallback is needed.

**Why this story**: Head pitch is a replicated, observable value; if NPC pitch were zeroed or smoothed differently than a human's, it would be an identity tell (GDD CNB-R3, SCL R8.1). Read GDD CNB-R3 and AC-CNB-02, and ADR-0016 section 4 item 4.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: `AStealthCrowdController` does not create a `PathFollowingComponent`; the controller writes `Look` yaw and pitch into the control rotation in its `UpdateControlRotation` override; the controller steps its due-time list and follower before `Super::Tick`; avoidance is expressed as `Move` offsets (`bUseRVOAvoidance` false for every pawn) (ADR-0016)
- Forbidden: `SetFocus`/`SetFocalPoint` or `FaceRotation` in the crowd controller; never call `Possess`/`UnPossess` from Crowd code (ADR-0016)
- Guardrail: controller tick work is in `Crowd.Follower`/`Crowd.Dispatch` scopes; one class for every AI-driven pawn (NPC, participant-bound, `PawnLeavingGame` hold)

---

## Acceptance Criteria

*From ADR-0016 sections 1, 2 and 4.4 and GDD AC-CNB-02 (the 10-minute gate is story 013), scoped to the controller:*

- [x] `AStealthCrowdController` has no `PathFollowingComponent` (or the never-ticked fallback adopted in story 001), `bWantsPlayerState` and `bStartAILogicOnPossess` are false, and it is the one controller class for every AI-driven pawn.
- [x] `UpdateControlRotation` is overridden to `SetControlRotation(LastLook)` with yaw and pitch; no `SetFocus`, `SetFocalPoint` or `FaceRotation` appears in Crowd code; a `Look` pitch of -15 degrees appears in the pawn's `RemoteViewPitch16` through the SCL `HeadLook` filter on an observer client.
- [x] Tick prerequisites are hand-over pass, then Director, then StateTree component, then controller, then the pawn's CMC (asserted at runtime); the follower and due-time list are stepped before `Super::Tick`.
- [x] AI possession adds no component to the pawn (component inventory before and after `OnPossess` is identical); `OnPossess` asks the Director for the pending driver configuration.

---

## Implementation Notes

*Derived from ADR-0016 section 1 and 4 and Implementation Guidelines:*

- Constructor: `AStealthCrowdController(const FObjectInitializer& OI)` with `DoNotCreateDefaultSubobject("PathFollowingComponent")`; `bWantsPlayerState = false`; `bStartAILogicOnPossess = false` (engine defaults, asserted).
- `Tick(DeltaTime)`: step the due-time list and the follower (stories 009, 012), then `Super::Tick`, which calls `UpdateControlRotation` and writes this frame's `Look`.
- `OnPossess`: `Super`, then `Director::AttachDriver(pending config)`; the pending config is registered by `DriveBoundPawn` or `AdoptReleasedPawn` (stories 028, 029). Possession itself is ADR-0010's hand-over pass; Crowd never possesses.
- The pitch path is the only place the crowd writes the control rotation (`SetControlRotation` is otherwise forbidden in Crowd code).
- The component-parity check relies on ADR-0010's default non-replicated `UGameplayTasksComponent` on every `AStealthPawn`; if that default is missing, stop and raise it against ADR-0010 rather than adding a Crowd-only component.
- Tick-order assertion: add prerequisites in `BeginPlay`/`OnPossess`, then check them with a debug-only assertion pass at world start.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces `Source/StealthGame/Private/StealthNpcController.cpp` and `Public/StealthNpcController.h` (prototype `AStealthNpcController`, `MoveToLocation` wandering). Deleted in story 039 with approval; the prototype `bWantsPlayerState = false` line is the one fragment worth keeping.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 009 and 010: the follower and virtual input layer that produce `Look`
- Story 012: the StateTree component and decision grid
- Story 028 and 029: the pending driver configurations consumed in `OnPossess`
- Story 013: the 10-minute instrumented "commands only through the IIF" gate

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/StealthCrowdController.h`
- `Source/StealthGameplay/Features/Crowd/Private/StealthCrowdController.cpp`
- `Source/StealthGame/Private/Composition/StealthCompositionRoot.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdControllerTest.cpp`
- `Source/StealthTestHarness/Private/Tests/Crowd/CrowdPIETests.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.ControllerComponents`
  - Given: the `AStealthCrowdController` CDO and a scene of AI-driven pawns
  - When: components, flags and controller classes are inspected
  - Then: no PathFollowingComponent (or the never-ticked fallback); `bWantsPlayerState` and `bStartAILogicOnPossess` false; all AI-driven pawns share this one class
  - Edge cases: NPC and restricted-driver pawns both checked
- **AC-2**: `StealthGame.Crowd.PIE.LookPitchReachesObserver`
  - Given: LastLook with pitch -15 degrees set on the server, and an observer client
  - When: the controller ticks and the pawn replicates
  - Then: control rotation equals LastLook (yaw and pitch); the observer's `RemoteViewPitch16` decodes to -15 degrees within one quantum after the HeadLook filter; grep finds no SetFocus, SetFocalPoint or FaceRotation
  - Edge cases: filter rate-limit settling time; needs two processes (deferred to PIE)
- **AC-3**: `StealthGame.Crowd.ControllerTickOrder`
  - Given: a pawn, controller, Director, StateTree component and CMC
  - When: tick prerequisites are inspected at BeginPlay and the tick order is logged for one frame
  - Then: order is hand-over pass, Director, StateTree component, controller, pawn CMC; follower and due-time list step before `Super::Tick`
  - Edge cases: a pawn possessed mid-round re-adds its prerequisites
- **AC-4**: `StealthGame.Crowd.PossessAddsNoComponent`
  - Given: a pawn component inventory snapshot
  - When: the crowd controller runs `OnPossess`
  - Then: the inventory is identical before and after; the Director is asked once for the pending driver configuration
  - Edge cases: no pending config (ambient NPC) versus a registered restricted config

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdControllerTest.cpp` (automation plus PIE test `StealthGame.Crowd.PIE.LookPitchReachesObserver`) — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Controller.Shape`, `StealthGame.Crowd.Controller.LookPitch`, `StealthGame.Crowd.Controller.TickOrder`, `StealthGame.Crowd.Controller.ComponentParity`, `StealthGame.Crowd.PIE.LookPitchReachesObserver.CrowdLookPitchReplicates`; 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-001-spike-engine-verifications.md`, `production/epics/crowd-npc-behaviour/story-005-command-sinks-epoch-and-halt-filter.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-007-path-source-and-start-budget.md`, `production/epics/crowd-npc-behaviour/story-009-follower-shaping-pace-and-stops.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`, `production/epics/crowd-npc-behaviour/story-028-bound-pawn-restricted-driver.md`, `production/epics/crowd-npc-behaviour/story-032-spike-60-character-server-cost.md`

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 4/4 passing
**Deviations**: Tick-order prerequisites are wired at PostInitializeComponents as well as BeginPlay (a world without a game mode never dispatches BeginPlay). CreateAiController spawns only AStealthCrowdController (D53).
**Test Evidence**: `StealthGame.Crowd.Controller.Shape`, `StealthGame.Crowd.Controller.LookPitch`, `StealthGame.Crowd.Controller.TickOrder`, `StealthGame.Crowd.Controller.ComponentParity`, `StealthGame.Crowd.PIE.LookPitchReachesObserver.CrowdLookPitchReplicates`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
