# Story 005: Service shell and composition-root wiring

> **Epic**: Objectives Chain
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-28

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-1 scope, OC-17 lifecycle, AC-OC-01)
**Requirement**: `TR-oc-002` (bind at `OnLayoutReady`, server-only), `TR-oc-005` (the service is the follow-on owner in the hold-timer slot)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 1 and 3, Key Interfaces, Implementation Guidelines)
**Other ADRs**: ADR-0002: Server resolution queue, server clock and round-end staging (the `TimerFired(StageSignal)` resolver registration and `OnRoundActive` step)
**ADR Decision Summary**: `UObjectivesService` is a server-only `UWorldSubsystem` facade (`NotBlueprintType`) implementing `ICommandPredicate` (Interact), `IInteractTargetOwner` and `IQueueResolver` (`TimerFired(StageSignal)`) over a headless `FObjectivesCore`. It spawns no actor, has no tick function, no `FTimerManager` use and no latent action; it runs only inside queue slots, the `OnLayoutReady` and `OnRoundActive` server delegates and the Roster's rebind handler. The composition root registers it weakly.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0020 overall; ADR-0002 MEDIUM for tick-group and drain behaviour)
**Engine Notes**: No ADR-0020 verification item gates the shell itself. `TWeakInterfacePtr` registration is re-verified in the 5.8.4 source by ADR-0002. A world subsystem is created per world, so the service is new each round (ADR-0020 section 3: "the world, and with it the service, is new each round").

**Control Manifest Rules (this layer)**:
- Required: `FSiteAssignment` is read only through `UStealthLayoutService::GetSiteAssignment_Server()` inside `UObjectivesService`/`FObjectivesCore` — source: ADR-0020
- Forbidden: Never give Objectives a tick function, `FTimerManager` timer or latent action; never spawn an actor for a court, objective or site — source: ADR-0020
- Forbidden: Never include the Objectives `Public/Server/` headers from client modules, Crowd, IIF or Presentation — source: ADR-0020
- Guardrail: Objectives at most 0.05 ms average on the listen host — source: ADR-0020

The service is a thin shell: it owns the core, wires the injected dependencies the core needs (Roster view, Session connection view, Round hooks, pawn control view, queue timers, secret draws) and registers itself where the other systems look for it. All behaviour lives in the core (stories 003, 004, 006-009, 012-015). Because those registrations are weak and consumer-owned, a missed registration is a silent dead feature, so this story makes a dead registration a loud fault.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-01 `PreparingFailed` clause, OC-17) and ADR-0020 sections 1, 3 and Guidelines, scoped to this story. QL-STORY-READY revised: AC4 (the PIE smoke) was added and the include-lint ownership moved to story 022:*

- [x] **AC1** (AC-OC-01 [U] `PreparingFailsOnBadChain`): **GIVEN** an invalid chain asset, or two cooked chains with no selection rule, **WHEN** Round `Preparing` calls `ValidateSelectedChain` **THEN** Round receives `PreparingFailed(ObjectiveChainInvalid)` and no site is bound (stub LSC read count 0); exactly one valid chain succeeds; zero cooked chains also fails.
- [x] **AC2** (ADR-0020 section 1 [U] `ServiceHasNoTickTimerActor`): **GIVEN** `UObjectivesService` driven through Preparing, Active and Ended against a stub world and stub timer manager **THEN** no tick function is registered, the timer manager call count is 0, no latent action exists and the spawn count is 0 (the rival release timer belongs to the Round queue, not the service). The include-lint half of this guideline is owned by `ObjectivesIncludeLint` in story 022.
- [x] **AC3** (ADR-0020 sections 3, 5 [U] `CompositionRootRegistrations`): **GIVEN** a composition root with stub registries and fault sink **WHEN** `HandleLayoutReady(LayoutId, Seed)` then `HandleRoundActive(Epoch, ActivePhaseStart)` run **THEN** the layout is bound, t0 (`ActivePhaseStart`) is stored, both teams' `Prelim1` are `Live`, and the service is registered weakly as `ICommandPredicate(Interact)`, `IInteractTargetOwner` and the `TimerFired(StageSignal)` resolver; destroying the service and then resolving a registration reports exactly once to the fault sink; `HandleRoundActive` before `HandleLayoutReady` reports a fault; a second `HandleRoundActive` for the same epoch is ignored.
- [x] **AC4** (PIE smoke `StealthGame.Objectives.PIE.ServiceBoots`): on a PIE listen server the service exists, the three registrations are live and the fault sink is empty after a round reaches `Active`.

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 1 and 3 and Key Interfaces:*

Placement: `UObjectivesService` in `StealthGameplay/Public/Server/Features/Objectives/` (a guarded server header) and `FObjectivesCore` in `StealthGameplay/Private/Features/Objectives/`. `ValidateSelectedChain(const FStealthServerTuning&)` runs at `Preparing`; any failure maps to Round's `PreparingFailed(ObjectiveChainInvalid)`. MVP selects the one cooked chain; a second cooked chain without a selection rule is a failure, not a silent pick (ADR-0020 section 2). `HandleLayoutReady(uint64 LayoutId, int32 Seed)` binds (story 003); `HandleRoundActive(FEpochToken Epoch, double ActivePhaseStart)` stores t0, sets both teams' `Prelim1` `Live` (story 004) and pushes the initial views (story 010). The `DebugCompleteStage(ETeam)` entry is story 008.

The injected interfaces the core needs, named in ADR-0020's Key Interfaces: `IObjectivesRosterView`, `ISessionConnectionView`, `IRoundObjectiveHooks {IsActive, DeclareObjectiveWin, AppendEvent}`, `IPawnControlView {GetControlEpoch, IsHandOverPending}`, `IQueueTimers {ScheduleAt, CurrentKey, ServerNow}`, `ISecretDraw {StageSignal}`. Headless tests inject stubs; the composition root injects the real ones. The composition root also binds `URosterStore::OnParticipantViewRebound` (story 012).

Registration is weak (`TWeakInterfacePtr`), per the manifest rule for feature predicates. The fault-sink behaviour (a dead registration reports once) follows the ADR-0007 section 4 rule that every dead registration reports; use `IStealthFaultSink` from the Foundation module (TDC story 008 provides the seam).

The service has no `Tick`, so check that the class does not override `Tick`/`IsTickable` and that it does not call `GetWorldTimerManager()`; the stub world's counters in AC2 prove it. The service must also not include client modules.

Prototype replaced: `AStealthGameMode` wiring of objective actors (`HandleInteractRequest`, `OnObjectiveCompleted`, objective spawning in the generator) is superseded; actual deletion is story 022.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-003-core-chain-validation-and-court-binding.md`: `ValidateChain` and `Bind` logic.
- `story-006-fixture-claims-and-interact-target-owner.md` and `story-007-interact-predicate-and-feint-parity.md`: the bodies behind the registered interfaces.
- `story-012-rejoin-rebind-and-reset.md`: the rebind subscription behaviour.
- `story-022-prototype-retirement-and-static-scan.md`: owns the include lint and the prototype deletion.

---

## Files

*Files changed by /dev-story (2026-10-28; the OC 001-009 batch shares one commit, decision D41). Feature code follows the built features' layout `Source/StealthGameplay/Features/Objectives/{Public/Server,Public/View,Private}` and the tuning rows the TDC convention in StealthFoundation (decisions logged 2026-10-28).*

- `Source/StealthGameplay/Features/Objectives/Public/Server/ObjectivesService.h`, `Private/ObjectivesService.cpp` (`UObjectivesService`: server-only world subsystem, `Bind`/`Unbind`, `ValidateSelectedChain`, `HandleLayoutReady`, `HandleRoundActive`, `HandleRoundEnded`, `HandleRoundReset`, StageSignal resolver registration)
- `Source/StealthGameplay/Features/Objectives/Public/View/ObjectivesChainRegistry.h`, `Private/ObjectivesChainRegistry.cpp` (`UObjectivesChainRegistry`: the cooked chains through the Asset Manager)
- `Source/StealthGameplay/Public/Server/Core/InteractHoldStandIn.h`, `Private/Core/InteractHoldStandIn.cpp` (STAND-IN for the IIF stage-2 Interact hold)
- `Source/StealthGameplay/Public/Server/Core/Round/StealthRoundPreparing.h`, `Private/Core/Round/StealthRoundPreparing.cpp`, `StealthRoundMatchCore.h/.cpp`, `StealthRoundMatchStore.h/.cpp` (abort code `ObjectiveChainInvalid`, `NotifyObjectiveChainInvalid`; the DebugItem command of story 008)
- `Source/StealthGame/Private/Composition/StealthObjectivesWiring.h/.cpp` (the adapters), `StealthCompositionRoot.cpp`, `Public/Server/StealthCompositionRoot.h` (WireObjectives, the registrations, Preparing validation, OnRoundActive, Ended/Reset, drain-end claim check), `StealthCommandWiring.h/.cpp` (Interact commands to the hold stand-in), `Private/StealthGameMode.cpp`, `Public/StealthGameMode.h` (OnLayoutReady hook, the StealthBot `interact`, `interactend`, `debugstage` commands, the prototype HUD mirror)
- Tests: `Source/StealthGameplay/Private/Tests/Objectives/PreparingFailsOnBadChainTest.cpp`, `ServiceHasNoTickTimerActorTest.cpp`, `CompositionRootRegistrationsTest.cpp`, rig `ObjectivesQueueRig.h`; `Source/StealthTestHarness/Private/Tests/PIE/Objectives/ServiceBootsTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS; the PIE smoke and the lint ownership were applied above and the specs cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`PreparingFailsOnBadChain`): invalid selection fails Preparing
  - Given: stub Round queue; case A invalid chain asset; case B two cooked chains and no selection rule
  - When: Round Preparing calls `ValidateSelectedChain`
  - Then: Round receives `PreparingFailed(ObjectiveChainInvalid)`; no site bound (stub LSC read count 0)
  - Edge cases: exactly one valid chain succeeds; zero cooked chains also fails.
- **AC-2** (`ServiceHasNoTickTimerActor`): no tick, timer, latent action, spawn
  - Given: `UObjectivesService`, stub world and stub timer manager
  - When: the service is constructed and driven through Preparing, Active, Ended
  - Then: no tick function registered; timer manager call count 0; no latent action; spawn count 0
  - Edge cases: a seeded violation (a tick override) fails the check; the rival release timer is owned by the Round queue, not the service.
- **AC-3** (`CompositionRootRegistrations`): wiring and weak registration
  - Given: composition root with stub registries and fault sink
  - When: `HandleLayoutReady(LayoutId, Seed)`, then `HandleRoundActive(Epoch, ActivePhaseStart)`
  - Then: layout bound; t0 stored; both teams' Prelim1 Live; service registered weakly as `ICommandPredicate(Interact)`, `IInteractTargetOwner` and `TimerFired(StageSignal)` resolver; destroying the service then resolving a registration reports once to the fault sink
  - Edge cases: `HandleRoundActive` before `HandleLayoutReady` reports a fault; a second `HandleRoundActive` for the same epoch is ignored.
- **AC-4** (`PIE.ServiceBoots`, specified from the qa-lead's rewrite text for the added smoke): service boots in PIE
  - Given: PIE listen server with the production composition root
  - When: the round reaches Active
  - Then: the service exists on the server only, the three registrations are live and the fault sink is empty
  - Edge cases: the service is absent on clients; a client world creating one is a failure.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three T1 tests under `Source/StealthGameplay/Private/Tests/Objectives/` and the T2 smoke under `Source/StealthTestHarness/Private/Tests/PIE/Objectives/` — must exist and pass

**Status**: [x] Created and passing (2026-10-28): StealthGame.Objectives.PreparingFailsOnBadChain, ServiceHasNoTickTimerActor, CompositionRootRegistrations, StealthGame.Objectives.PIE.ServiceBoots.ServerOnlyServiceRegisteredWithNoFaultAtActive; evidence production/qa/evidence/objectives-chain-001-009-2026-10-28/

Run result: OBSERVED - 3-player listen-server PIE on L_Prototype: the service bound, both teams Stage1 at Active (`oc-01`, `oc-02`, the `[Bot] Objectives: bound=1` log line).

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-003-core-chain-validation-and-court-binding.md`, `production/epics/objectives-chain/story-004-core-site-state-and-progress.md`; cross-epic: round-match-framework (`PreparingFailed`, `OnRoundActive`, queue and `RegisterTimerResolver`; stories being written in parallel); `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md` (`RegisterInteractTargetOwner`, predicate registry); `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md` and `story-008-fault-sink-and-test-seams-define.md`
- Unlocks: `production/epics/objectives-chain/story-006-fixture-claims-and-interact-target-owner.md`, `production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md`, `production/epics/objectives-chain/story-012-rejoin-rebind-and-reset.md`

## Completion Notes
**Completed**: 2026-10-28
**Criteria**: AC1-AC4 passing. AC2's "stub timer manager call count 0" is proved structurally: the service is neither an FTickableGameObject nor a UTickableWorldSubsystem and the Objectives sources name no Tick / IsTickable / timer manager / SetTimer / latent action / SpawnActor (comment- and literal-blind scan with a seeded control), and a full Preparing -> Active (a tend) -> Ended drive spawns nothing.
**Deviations**: Preparing's validation runs in the composition root's tuning-ready step (an invalid chain calls `NotifyObjectiveChainInvalid` instead of `NotifyTuningReady`); Round's abort set gained `ObjectiveChainInvalid` (14 codes; an ADR-0002 8.6 amendment, decision logged). The injected seams follow ADR-0020's Key Interfaces with the queue reads folded into `IRoundObjectiveHooks` (CurrentKey, ServerNow). STAND-INs (composition root): the IIF Interact hold (`FInteractHoldStandIn`: predicate at Begin, the court from the command or the nearest in reach, D read once, HoldTimer(Complete) on Round's queue; no release debounce, reply batch or FHoldStatus), the LSC assignment and court geometry (L_Prototype's objective actors), the team view (the prototype HUD mirror) and the rival release (none scheduled). Tuning: the schema defaults until the TDC store feeds features the frozen set.
**Test Evidence**: production/qa/evidence/objectives-chain-001-009-2026-10-28/
**Reviewed by**: self code review 2026-10-28 (APPROVED; notes in the evidence)
