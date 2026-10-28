# Story 011: Team view component and replication

> **Epic**: Objectives Chain
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-28

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-5, OC-12 audience clause, OC-17; AC-OC-03, AC-OC-09 "0 to 2 in one update")
**Requirement**: `TR-oc-003` (`FObjectiveTeamView` rides C-TeamView)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision section 6; Engine Compatibility OBJ-V1, OBJ-V2)
**Other ADRs**: ADR-0001: Crowd identity rules and the C-TeamView carrier (`UTeamViewComponentBase`, owner-only PlayerController); ADR-0003: Replication relevancy and concealment cull (the allowlist, section 8)
**ADR Decision Summary**: `UObjectiveTeamViewComponent : UTeamViewComponentBase` carries one replicated, push-based `FObjectiveTeamView View` (`ReplicatedUsing=OnRep_View`, COND_None because the controller is owner-only) as a default subobject of `AStealthPlayerController`. It is written only by `UObjectivesService`, to each current controller of team T's participants, only if the value differs, and every write marks the property dirty. The controller must set `bReplicateUsingRegisteredSubObjectList = true`; replicated components register after BeginPlay. Presentation never depends on the RepNotify.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0020 MEDIUM where the view rides the C-TeamView mechanism; ADR-0001 owns its HIGH-risk owner-only relevancy and sub-object registration, which its spikes verify)
**Engine Notes**: Facts E1-E4 of ADR-0020: controllers are `bOnlyRelevantToOwner` (`Controller.cpp:67`; `ActorReplication.cpp:388-400`) so a replicated component on the controller reaches only the owning connection; the registered sub-object list is off by default (`net.SubObjects.DefaultUseSubObjectReplicationList` false); push model: `MARK_PROPERTY_DIRTY_FROM_NAME` and `bIsPushBased` exist, `Net.IsPushModelEnabled` defaults **false** (then push-based properties are compared like any other); several changes between two net updates are sent once with the last value. **OBJ-V1** (owner-only reach, including rejoin and seamless travel) and **OBJ-V2** (two writes in one frame arrive as one change) are verified here; they are [U] until this story reports. They are the stories' own verification, not a prerequisite spike.

**Control Manifest Rules (this layer)**:
- Required: Team-audience objective data travels only in `UObjectiveTeamViewComponent::View`; every write marks the push-model property dirty — source: ADR-0020
- Required: The controller sets `bReplicateUsingRegisteredSubObjectList = true` (default false in 5.8.4); the push model is mandatory for this part so the view updates whether `Net.IsPushModelEnabled` is on or off — source: ADR-0020, ADR-0001
- Forbidden: Never put objective data on the GameState, `FRoundPublicState`, a PlayerState, a pawn, a fixture or an RPC; never put it in the Roster's C-TeamView part — source: ADR-0020
- Guardrail: about 16 bytes per view change per team member; nothing to non-team connections — source: ADR-0020

The view is the only objective data any client ever receives. This story is the carrier: the component, its registration on the owner-only controller, its push-model discipline, and the allowlist row the reflection audit requires (ADR-0003 section 8: the audit fails the build when a replicated property has no row). The listen host's own controller holds the same component written by the same code and the host's HUD reads it through its own controller, so host parity is by construction (ADR-0001). Verification items OBJ-V1 and OBJ-V2 are checked here against the real engine, because the whole design assumes the controller carries a team-entitled component to its owner only and coalesces same-frame writes.

---

## Acceptance Criteria

*From ADR-0020 (OBJ-V1, OBJ-V2, section 6) and GDD AC-OC-03/AC-OC-09, scoped to this story:*

- [x] **AC1** (ADR OBJ-V1, [T2 PIE] `StealthGame.Objectives.PIE.TeamViewOwnerOnly`): **GIVEN** `UObjectiveTeamViewComponent` on `AStealthPlayerController` in a multi-client PIE run (2 teams, 1 client each) **THEN** each team's view is present only on its own team's owning connections and the other team's client never receives it, including after a rejoin; the **seamless-travel** case runs in the Session multi-process harness (single-process PIE cannot represent seamless travel, ADR-0005). *(2026-10-28: multi-client and rejoin PASS; the seamless-travel clause NOT RUN, deferred: see Completion Notes.)*
- [x] **AC2** (ADR OBJ-V2, AC-OC-09 clause [T2 PIE] `StealthGame.Objectives.PIE.RivalCountOneUpdate`): **GIVEN** two `RivalPrelimCount` writes in one server frame (0 to 1 to 2) **THEN** the client receives exactly one change with the final value 2; writes in two frames give two changes; the count never decreases.
- [x] **AC3** (ADR section 6 [U] `ObjectivesAllowlistRow` and PIE push-model run): `View` is registered with `bIsPushBased` and every write calls `MARK_PROPERTY_DIRTY_FROM_NAME`; the client view updates with `Net.IsPushModelEnabled` both 1 and 0; the controller uses the registered sub-object list; the ADR-0003 allowlist has exactly one row for `UObjectiveTeamViewComponent::View` (audience Team, carrier C-TeamView, mapped to the IVP R3 rows), and the reflection audit fails when that row is removed (and when a duplicate row is seeded).

---

## Implementation Notes

*Derived from ADR-0020 Decision section 6 and Key Interfaces:*

`UObjectiveTeamViewComponent : UTeamViewComponentBase` in `StealthGameplay/Public/View/Features/Objectives/`, created as a default subobject of `AStealthPlayerController` (root module); written only by `UObjectivesService`, read by Presentation via `FindComponentByClass` on its own controller. `UPROPERTY(ReplicatedUsing=OnRep_View) FObjectiveTeamView View;` with `GetLifetimeReplicatedProps` registering `FDoRepLifetimeParams{ .bIsPushBased = true }`, and every write using `MARK_PROPERTY_DIRTY_FROM_NAME(UObjectiveTeamViewComponent, View, this)`. `bWithPushModel` defaults to true for Editor targets and false otherwise (`TargetRules.cs:1522-1527`), and `Net.IsPushModelEnabled` defaults to false, so the macros compile to nothing without `WITH_PUSH_MODEL` and the compare path applies; the dual on/off PIE run in AC3 proves the property keeps updating either way. Components replicate through the actor's registered sub-object list after BeginPlay, not `AddReplicatedSubObject`.

`UObjectivesService::PushTeamView(ETeam T)` builds the view (story 010) and writes it to the component of each current controller of team T's participants (Roster `GetTeamMembers(T)`, controller from Session's connection table), only when the value differs; writes happen inside slots or the `OnRoundActive` step, so several writes in one frame reach the client as one change (E4, OBJ-V2: that is how a 0 to 2 rival rise arrives as one update). Informants receive the same view as Active members (Team audience includes the team's informants).

Allowlist row: add one row for `UObjectiveTeamViewComponent::View` to the single checked-in replication allowlist (`Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json`, owned by IVP, rows co-owned by their systems) with audience Team, carrier C-TeamView and the IVP R3 rows (the section 4 field-to-fact table). The reflection audit (IVP story 015) must fail the build without it. The HUD reads the view through its own controller; the HUD epic's UI story consumes it.

Record the OBJ-V1 and OBJ-V2 outcome in the test output and in this story's Test Evidence; if either fails, the fix is a superseding ADR, not an edit to ADR-0020 (conditional acceptance rule).

Prototype replaced: the replicated actor per site (`AStealthObjective`, `bCompleted`, `InteractorCount`) and the stage replicated through `AStealthGameState`; replaced, not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-010-team-view-struct-and-builder.md`: the struct and builder.
- `story-012-rejoin-rebind-and-reset.md`: the rebind-event write and reset.
- `story-019-pie2-team-view-ingress-and-informant-parity.md`: whole-state dump and informant parity.
- IVP epic: the carrier mechanism and its spikes (S1, S2, S14), the reflection audit itself, and the allowlist file.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Features/Objectives/Public/View/ObjectiveTeamViewComponent.h` / `Private/ObjectiveTeamViewComponent.cpp` (push-based `View`, change-only `ApplyView`, test seams for the dirty-mark control)
- `Source/StealthGameplay/Features/Objectives/Public/Server/ObjectivesService.h` / `Private/ObjectivesService.cpp` (the service is the core's view sink: `PushTeamView`, `WriteTeamView`, `BuildTeamView`; `IObjectivesViewTargets` in `ObjectivesSeams.h`)
- `Source/StealthGame/Public/StealthPlayerController.h` / `Private/StealthPlayerController.cpp` (default subobject `ObjectiveTeamView`; `bReplicateUsingRegisteredSubObjectList` was already set; the owner's local "stage banked" line, from the view)
- `Source/StealthGame/Private/Composition/StealthObjectivesWiring.h` / `.cpp`, `StealthCompositionRoot.cpp` (the `IObjectivesViewTargets` adapter; the prototype `TeamStage` mirror removed), `StealthGameMode.h` / `.cpp` (`MirrorProductionObjectiveStage` deleted)
- `Source/StealthGame/Private/StealthHUD.cpp` (objective panel, markers and the automation dump read only the own team's view: card titles from the cooked chain, "Rival steps: N of 2"; the prototype "Enemy progress" line is gone), `StealthObjective.h` / `.cpp` (`GetStandInCourts` / `FindStandInCourt`, the shared stand-in court ids)
- `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` (created, with this row), `Source/StealthFoundation/Public/Server/Testing/StealthReplicationAllowlist.h` / `Private/Testing/StealthReplicationAllowlist.cpp` (parse + per-class audit, stand-in for IVP 013/015)
- `Source/StealthGameplay/StealthGameplay.Build.cs` (`NetCore` private dependency for the push-model mark)
- `Source/StealthGameplay/Private/Tests/Objectives/ObjectivesAllowlistRowTest.cpp`; `Source/StealthTestHarness/Private/Tests/PIE/Objectives/TeamViewOwnerOnlyTest.cpp`, `RivalCountOneUpdateTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; the seamless-travel case must use the Session harness). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`PIE.TeamViewOwnerOnly`): view reaches only own team
  - Given: multi-client PIE (2 teams, 1 client each); `UObjectiveTeamViewComponent` on `AStealthPlayerController`; Session harness for travel
  - When: Active starts; a client rejoins; seamless travel runs (Session harness, not single-process PIE)
  - Then: each team's view is present only on its own team's owning connections; the other team's client never receives it, including after rejoin and travel
  - Edge cases: travel case is excluded from single-process PIE and must run in the Session harness; late joiner before Active.
- **AC-2** (`PIE.RivalCountOneUpdate`): coalesced writes
  - Given: PIE server with controllable frame stepping; client recording property change callbacks
  - When: two `RivalPrelimCount` writes occur in one server frame (0 to 1 to 2)
  - Then: the client receives exactly one change with the final value 2
  - Edge cases: writes across two frames give two changes; count never decreases.
- **AC-3** (`ObjectivesAllowlistRow` + push model): push model and allowlist
  - Given: PIE run with `Net.IsPushModelEnabled` 1 then 0; ADR-0003 allowlist; reflection audit
  - When: view writes occur in each run; the audit executes
  - Then: `View` registered `bIsPushBased` and each write uses `MARK_PROPERTY_DIRTY_FROM_NAME`; client view updates in both CVar states; controller uses the registered sub-object list; allowlist has one row for `UObjectiveTeamViewComponent::View` (audience Team, carrier C-TeamView, IVP R3 rows); removing the row makes the audit fail
  - Edge cases: seeded duplicate row fails; a write without the dirty mark is caught in the push-on run.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two T2 PIE tests under `Source/StealthTestHarness/Private/Tests/PIE/Objectives/` and the T1 audit test under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass; the OBJ-V1/OBJ-V2 outcome is recorded in the run output attached to the story at close

**Status**: [x] Created and passing (2026-10-28): StealthGame.Objectives.ObjectivesAllowlistRow; StealthGame.Objectives.PIE.TeamViewOwnerOnly.EachTeamsViewReachesOnlyItsOwnClients; StealthGame.Objectives.PIE.RivalCountOneUpdate.OneChangePerFrame_PushModelOn / _PushModelOff. OBJ-V1: PASS in single-process PIE (multi-client and rejoin rebind; seamless travel not run). OBJ-V2: PASS (one change for two same-frame writes, two for two frames, push model on and off). Output: production/qa/evidence/objectives-chain-010-012-2026-10-28/objectives-pie-filtered.txt

Run result: OBSERVED - 3-player listen-server PIE on L_Prototype: team 0 banked P1 and P2 through the production chain; team 0's client and the host showed Stage 2/3 then 3/3 "The Ruler's Cacao" with their own markers; team 1's client stayed at Stage 1/3 "Hear the Runner", "Rival steps: 0 of 2", with no enemy stage line (production/qa/evidence/objectives-chain-010-012-2026-10-28/ `oc11-04`..`oc11-08`).

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-010-team-view-struct-and-builder.md`, `production/epics/objectives-chain/story-005-service-shell-and-composition-root.md`; cross-epic: `production/epics/participant-state-roster/story-038-owner-view-feed.md` and `story-041-team-view-repush-on-controller-swap.md` (C-TeamView carrier base and controller swap), `production/epics/information-visibility-policy/story-013-allowlist-content.md` and `story-015-replicated-property-audit.md`, `production/epics/information-visibility-policy/story-021-team-view-never-to-opponents.md`; `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`
- Unlocks: `production/epics/objectives-chain/story-012-rejoin-rebind-and-reset.md`, `production/epics/objectives-chain/story-019-pie2-team-view-ingress-and-informant-parity.md`, `production/epics/objectives-chain/story-021-pie2-end-to-end-cacao-chain.md`

## Completion Notes
**Completed**: 2026-10-28
**Criteria**: AC2 and AC3 passing; AC1 passing for the multi-client and rejoin cases in single-process PIE. AC1's seamless-travel clause was NOT run: the Session multi-process harness (tools/ci/mp and its spike scenarios) has no production-game-mode Objectives scenario yet; deferred to story 019 / the session-travel epic (decision logged). The rejoin case reproduces a new controller on the server (Session's PIE rejoin is not built; the approach of NKP story 017's test).
**Deviations**: the IVP allowlist file, parser and reflection audit did not exist (IVP 013/015 unbuilt): this story created the file at ADR-0008's path with its own row and a per-class audit (exactly one row per replicated property, the condition matches, no stray row) that fails on a removed, duplicated or mis-conditioned row; IVP 013/015 extend it. The push-on "unmarked write is not sent" control is an observation, not an assertion: the engine compares a NetSerialize property after a GC regardless of the mark (RepLayout `IsPropertyDirty`), so the "every write marks dirty" rule is pinned by a source check with a seeded control. The HUD reads only the owner-only view (the prototype's replicated `AStealthGameState::TeamStage` is no longer written on the production path; its deletion is story 022).
**Test Evidence**: production/qa/evidence/objectives-chain-010-012-2026-10-28/
**Reviewed by**: self code review 2026-10-28 (notes in the evidence)
