# Story 027: AppearanceId on the pawn: write once, replicate once, apply once

> **Epic**: Character Pool & Appearance
> **Status**: Blocked — BLOCKED: spike CPA-V1 (story 026) has not reported; ADR-0012 is conditionally accepted and the null-mesh-from-initial-OnRep path must be proven before this story depends on it (fallback: a class-default hidden mesh behind one visibility arbiter, via a superseding ADR).
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-004`, `TR-cpa-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-004 = Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption. TR-cpa-006 = One look path for every viewer and pawn kind; no keyed material parameters.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: `AStealthPawn` declares `UPROPERTY(ReplicatedUsing = OnRep_AppearanceId) uint8 AppearanceId = 255` with `COND_InitialOnly`; only `UStealthPawnFactory::SpawnSharedPawn` writes it, once, between `SpawnActorDeferred` and `FinishSpawning`, through `bool ApplyAppearance(FAppearanceId)`; `ApplyAppearanceVisuals()` is the one idempotent look path (called directly on the authority, from `OnRep_AppearanceId` on clients).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: `COND_InitialOnly` is "only attempt to send on the initial bunch" (E1); `SetSkeletalMeshAsset` (not the deprecated `SetSkeletalMesh`); material is set before the mesh so the one PSO precache request is for the final pair (E11); `RefreshPresentation` never sets mesh or material (E10).

**Why this story**: The property and its single apply function are CPA's one contact with the pawn class, which Shared Character & Locomotion owns (ADR-0010). **Revised per QL-STORY-READY**: the purity claims are split between a behavioural test (AC-3) and lints (AC-4). Read GDD "Replication: what replicates, to whom", CP-10, CP-16, CP-18, AC-CP-10 (second write), AC-CP-15d.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `AppearanceId` is written only by `UStealthPawnFactory` (via `ApplyAppearance(Id)`) before `FinishSpawning`, exactly once, replicated `COND_InitialOnly` with a RepNotify; `ApplyAppearanceVisuals` is the only function that sets a pawn's mesh or material (ADR-0012)
- Forbidden: Never read controller, team, binding, local-ness, `BeginPlay` state or viewer in `ApplyAppearanceVisuals`; never create a `UMaterialInstanceDynamic` or call `LoadSynchronous` there (ADR-0012)
- Guardrail: client `ApplyAppearance` about 2 to 5 ms one-off at round start [U]

---

## Acceptance Criteria

*From GDD AC-CP-10 (second write), AC-CP-15d and ADR-0012 §4; AC-3 split per QL-STORY-READY:*

- [ ] **AC-1** GIVEN `ApplyAppearance(FAppearanceId)` between `SpawnActorDeferred` and `FinishSpawning`, THEN it refuses 255 and an id with no pool row, and a second write returns false, leaves the value unchanged and logs `Error_AppearanceRewrite` (expected-error hook, no crash).
- [ ] **AC-2** GIVEN the pawn class default object (no world), THEN its replicated property list equals the golden file `Source/StealthGameplay/Private/Tests/CharacterPool/Golden/PawnLifetimeProps.txt` with `AppearanceId` `COND_InitialOnly` and default 255, and the `PlayerState` reference is not replicated to non-owners (test `StealthGame.CharacterPool.Net.LifetimePropsAllowlist`, smoke set).
- [ ] **AC-3** GIVEN `ApplyAppearanceVisuals` on an instrumented mesh component, THEN a second call changes nothing, `SetMaterial` is recorded before `SetSkeletalMeshAsset`, and a missing row draws nothing and logs `PoolNotResident` exactly once.
- [ ] **AC-4** GIVEN `LINT-CPA-ApplyVisualsPurity`, THEN `ApplyAppearanceVisuals` is the only function that sets a pawn's mesh or material, it creates no MID, calls no `LoadSynchronous` and reads no controller, team, binding, local-ness or viewer, and it is called explicitly on the authority (no RepNotify there).

---

## Implementation Notes

*Derived from ADR-0012 §4:*

- `OnRep_AppearanceId()` calls `ApplyAppearanceVisuals()`; the apply function reads only `UCharacterPoolSubsystem::GetPool()->FindRow(Id)` (resident) and depends on no `BeginPlay`-initialised state (the initial RepNotify can precede `BeginPlay`).
- `PostNetInit` asserts `AppearanceId != 255` in non-Shipping builds (story 034 tests it).
- `TakeOverCrowdPawn` never touches the property (CP-12b).
- The pawn class and the factory belong to shared-character-locomotion (stories 001, 006); this story adds the property, the two functions and the allowlist golden. Replace the factory stub's appearance call (SCL story 006 "stub them in tests with the same interface") with the real one.
- Forbidden surfaces to keep out: appearance on `PlayerState`/`GameState`/PlayerController; any RPC carrying an appearance; per-connection or per-viewer conditions (CPA "Forbidden surfaces").

**Prototype disposition**: replaces `AStealthCharacter::SetAppearanceIndex`, `ApplyAppearance` and `OnRep_AppearanceIndex` (`Source/StealthGame/`; MID tint and `LoadSynchronous` at first draw). Removal is story 052.

---

## Out of Scope

- Story 026: spike CPA-V1; Story 034: first-replication and no-default-frame tests; Story 036: re-entry and takeover; Story 040: uniform presentation

---

## Files

- `Source/StealthGameplay/Public/Core/SharedPawn/StealthPawn.h` (the property and two functions; the class is SCL's)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnAppearance.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/AppearanceApplyTest.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/Golden/PawnLifetimeProps.txt`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-2 golden file named and AC-3 split into AC-3 and AC-4 with the qa-lead's proposed text; not re-run (unattended; decision logged).*

**Test: apply guard (AC-1)**
- Given: a pawn between `SpawnActorDeferred` and `FinishSpawning`.
- When: `ApplyAppearance` is called with 255, with an id that has no pool row, and twice with a valid id.
- Then: 255 and the missing row are refused; the second write returns false, leaves the value unchanged and logs `Error_AppearanceRewrite`.
- Edge cases: apply after `FinishSpawning`.

**Test: replicated property allowlist (AC-2)** `...Net.LifetimePropsAllowlist`
- Given: the pawn class default object (no world).
- When: `GetLifetimeReplicatedProps` is enumerated.
- Then: the list equals the golden allowlist; `AppearanceId` is `COND_InitialOnly` with default 255; the `PlayerState` reference is not replicated to non-owners.
- Edge cases: a new replicated property fails the test until the golden is updated.

**Test: apply visuals behaviour (AC-3)**
- Given: the instrumented component.
- When: calling twice, and with a row removed.
- Then: second call changes nothing; `SetMaterial` before `SetSkeletalMeshAsset`; missing row draws nothing and logs `PoolNotResident` once.
- Edge cases: the authority path calls it directly.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/CharacterPool/AppearanceApplyTest.cpp` — automation tests `StealthGame.CharacterPool.Net.LifetimePropsAllowlist` (smoke set), `...Net.AppearanceApplyGuard`, `...Net.ApplyVisualsBehaviour` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-026-spike-cpa-v1-null-mesh-initial-onrep.md`, `production/epics/character-pool-appearance/story-025-pool-subsystem-residency.md`, `production/epics/shared-character-locomotion/story-006-single-spawn-site.md`
- Unlocks: `production/epics/character-pool-appearance/story-034-id-before-first-replication-no-default-frame.md`, `production/epics/character-pool-appearance/story-040-uniform-presentation-all-pawn-classes.md`, `production/epics/character-pool-appearance/story-052-retire-prototype-pool-and-dealer.md`
