# Story 006: Every Shared Pawn comes from one spawn site

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R2 One spawn factory; R3; AC-SCL-02)
**Requirement**: `TR-scl-001` (one Shared Pawn class; one spawn factory; takeover function)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 3)
**ADR Decision Summary**: `UStealthPawnFactory` (server-only world subsystem) is the only code that spawns a Shared Pawn or calls `Possess`/`UnPossess` on one. One call stack: deferred spawn, `ApplyAppearance`, `BindPawn` (if a participant), `FinishSpawning`, possess. The engine's default pawn paths are dead.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `UWorld::SpawnActorDeferred` plus `AActor::FinishSpawning` and `APawn::SpawnDefaultController` (virtual) verified in 5.8.4 (E5). `AController::OnPossess` and `APlayerController::OnPossess` reset velocity-adjacent state through `Restart` (E1/E2): the possession wrapper is story 027/026's concern, but this story must call possess through `FScopedPossessionParity`. Spawning a CMC pawn inside the `TG_PostPhysics` drain ticks it the same frame (ADR-0002 E4 spike; if it fails, factory spawns move to ADR-0002's post-drain pass with this sequence unchanged).

**Control Manifest Rules (this layer)**:
- Required: every pawn is created by `UStealthPawnFactory::SpawnSharedPawn`: deferred spawn, `ApplyAppearance`, `BindPawn` (if a participant), `FinishSpawning`, possess, in one call stack; every spawn sets `Instigator == Self` and spawns no replicated child actor; `Possess`/`UnPossess` are called only by the factory and `AStealthPlayerController::PawnLeavingGame`, always through `FScopedPossessionParity`.
- Forbidden: never create or possess a pawn with `RestartPlayer`, `SpawnDefaultPawnFor`, `SpawnDefaultPawnAtTransform`, `SpawnDefaultController` or a `SpawnActor` of a pawn class outside the factory; never add a component on possession.
- Guardrail: client `ApplyAppearance` about 2-5 ms one-off at round start (ADR-0012).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-02** [P] `SingleSpawnSite`: GIVEN a round start, an NPC kill with its replacement and an informant kill with its respawn, WHEN pawn creation is traced, THEN every created Shared Pawn came from `SpawnSharedPawn` (round start: all `2·TeamSize + NpcCount`; each replacement: exactly 1), the informant respawn created 0 pawns (it is a takeover, AC-SCL-38), `RestartPlayer`/`SpawnDefaultPawnFor` created none, and every spawned pawn has `Instigator == Self`.

*QA clarification (create-stories 2026-10-25, not a GDD change):* "Self" in `Instigator == Self` is the pawn itself; `TeamSize` comes from the tuning default; the informant-respawn clause is asserted here with a stubbed takeover (0 pawns created) and with the real path in story 028.

---

## Implementation Notes

*Derived from ADR-0010 section 3 and Implementation Guidelines:*

- Implement `UStealthPawnFactory` in `StealthGameplay/Core` (`Public/Server/` face, ADR-0007) as a `UWorldSubsystem`, authority-only; assert authority and game thread on every call and report misuse to the fault sink. `SpawnSharedPawn(Transform, AppearanceId, ControlKind {Ai, Player}, Participant?, ExpectedEpoch)` returns `FSpawnResult {Pawn, Bind}`. Callers: Round (round-start mixed spawn) and Crowd (NPC replacement, including the `PawnRecovery` own-card respawn). The sequence is 3.1 steps 1-5 of ADR-0010.
- A factory-stamped, server-only creation token marks every legitimately created pawn so the test (and a debug guard) can detect any pawn that came from elsewhere. `AStealthGameMode` never calls `RestartPlayer`, `SpawnDefaultPawnFor` or `SpawnDefaultPawnAtTransform` (ADR-0005 makes `HandleStartingNewPlayer` a no-op); `DefaultPawnClass` is null; `AStealthPawn::SpawnDefaultController` is a no-op.
- Appearance (`ApplyAppearance(AppearanceId)`) and `BindPawn` are other epics' functions (Character Pool & Appearance; Participant State & Roster): call them through their facades; stub them in tests with the same interface.
- Replication begins on the next net tick, so a client's first bunch already carries `AppearanceId` and no binding.
- **Prototype replaced**: `AStealthGameMode`'s `RestartPlayer` round start and post-possession appearance draw, and the spawn paths in `StealthNpcController`, are replaced (ADR-0010 Implementation Guidelines last bullet: replaced, not ported). Do not delete the prototype files without user approval.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 007: bind rejection destroys the pawn.
- Story 008: spawn-bunch parity on the wire.
- Story 028: `TakeOverCrowdPawn` and its eligibility predicate.
- Stories 026 and 027: the hand-over pass and disconnect hook.

---

## Files


- Source/StealthGameplay/Public/Server/Core/StealthPawnFactory.h, Private/Core/PawnFactory/StealthPawnFactory.cpp (new: UStealthPawnFactory, IStealthPawnFactoryHooks, creation token, untokened guard)
- Source/StealthGame/Private/StealthGameMode.cpp (RestartPlayer / default pawn paths removed; players, informant respawns and NPCs through the factory; HandleStartingNewPlayer no-op)
- Source/StealthGame/Private/Composition/StealthCompositionRoot.cpp (factory hooks: appearance, BindPawn, prototype NPC controller)
- Source/StealthGameplay/Private/Tests/Locomotion/SingleSpawnSiteTest.cpp (new)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.SingleSpawnSite` [P].*

Test: single spawn site (AC-SCL-02)
  Given: a round start, an NPC kill with its replacement and an informant kill with its respawn, with pawn creation traced
  When: the three events run
  Then: every created pawn came from `SpawnSharedPawn` (round start: all `2·TeamSize + NpcCount`; each replacement exactly 1); the informant respawn creates 0 pawns (stubbed takeover here, real path in story 028); `RestartPlayer` and `SpawnDefaultPawnFor` create none; every spawned pawn has `Instigator == Self` (the pawn itself)
  Edge cases: `TeamSize` is the tuning default; a pawn without the factory creation token fails the test

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/SingleSpawnSiteTest.cpp`, test name `StealthGame.Locomotion.SingleSpawnSite` (PIE, trace of pawn creation) — must exist and pass; plus a retained screenshot of a started round with every pawn present in `production/qa/evidence/` per `.claude/docs/run-and-observe.md` (player-observable change)

**Status**: [x] Created and passing (`StealthGame.Locomotion.SingleSpawnSite`, 2026-10-26); retained screenshot `production/qa/evidence/integration-spine-2026-10-26/01-host-active.png`

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md`; `production/epics/participant-state-roster/EPIC.md` (`BindPawn`), `production/epics/character-pool-appearance/EPIC.md` (`ApplyAppearance`), `production/epics/round-match-framework/EPIC.md` (round-start caller), `production/epics/crowd-npc-behaviour/EPIC.md` (`AStealthCrowdController`, replacement caller); stories not yet written, paths TBD
- Unlocks: `production/epics/shared-character-locomotion/story-007-factory-bind-fail-destroys.md`, `story-008-spawn-bunch-parity.md`, `story-026-disconnect-keeps-pawn-under-crowd-ai.md`, `story-027-handover-blend.md`, `story-028-informant-respawn-takes-over-unseen-crowd-pawn.md`

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 1/1 passing (round start 2 x TeamSize + NpcCount, replacement 1, stubbed takeover 0, tokens, Instigator == self, static scan of the composition root)
**Deviations**: Factory lives in Private/Core/PawnFactory (the class-parity scan forbids controller/Roster symbols in Core/SharedPawn). FScopedPossessionParity does not exist yet (SCL-026/027): Possess is called directly. The trace test runs in a T1 test world, not PIE; the live round start is observed in PIE (43 factory pawns, 0 untokened). The prototype informant respawn still creates a new pawn through the factory (the takeover is SCL-028).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Locomotion/SingleSpawnSiteTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: OBSERVED: integration-spine-2026-10-26/01-host-active.png (dev line: factory pawns 43, untokened 0)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
