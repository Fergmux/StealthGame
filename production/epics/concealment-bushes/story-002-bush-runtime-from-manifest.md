# Story 002: Every machine builds the same bush runtime from the Manifest with zero replicated actors, and an empty EntryAxis is rejected

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R1, AC-CON-22, AC-CON-27)
**Requirement**: `TR-con-003` (hide volumes); also `TR-con-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§1 local bush runtime), with ADR-0003 (zero replicated actors) and ADR-0014 (`BushSite` in the Manifest)
**ADR Decision Summary**: `UBushRuntimeSubsystem` builds `FBushRuntime {BushId, Footprint 2.0 x 2.0 m, HideVolume footprint x 2.25 m, EntryAxis set, Anchor}` from the `FLayoutManifest` `BushSite` entries on the server at `OnLayoutReady` and on each client at Manifest build, and spawns one locally owned, non-replicated `ABushVolumeSet` holding one box component per bush. Concealment spawns zero replicated actors.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; this story is local geometry from replicated data, engine fact E5 [V])
**Engine Notes**: A `bReplicates = false` actor spawned independently on server and clients is a valid way to build identical local geometry from replicated data (E5). The `BushVolume` object channel is a collision-profile entry in `Config/DefaultEngine.ini`; this pipeline cannot edit Config, so the implementer adds it.

**Control Manifest Rules (this layer)**:
- Required: Bushes are built locally from the Manifest on every machine — source: ADR-0017
- Forbidden: Never spawn a replicated actor for concealment; never use per-bush replicated actors — source: ADR-0017
- Guardrail: Built once per layout; no per-frame work

The only hideable objects are the Level Socket Contract's `BushSite` entries. A bush whose `EntryAxis` set is empty is a V-BUSH-1 reject and never reaches a round. Until the LSC stories exist the story builds from the graybox Manifest provider (ADR-0014) as its fixture.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-22, AC-CON-27, CON-R1), scoped to this story:*

- [ ] **AC1** (AC-CON-22, [Int][Dep:LSC] `StealthGame.Concealment.BushSiteParity`, LSC AC-52): **GIVEN** a 2-client PIE **WHEN** each machine has built `UBushRuntimeSubsystem` from `FLayoutManifest` **THEN** every bush's `BushId`, `HideVolume` (footprint 2.0 x 2.0 m x 2.25 m), `EntryAxis` set and anchor hash identically on the server and both clients.
- [ ] **AC2** (AC-CON-27, [U] `StealthGame.Concealment.ManifestReject`): **GIVEN** a `BushSite` with an empty `EntryAxis` set **THEN** V-BUSH-1 rejects the layout and no round starts with it.
- [ ] **AC3** (derived, CON-R1, ADR-0017 E5): **GIVEN** the built runtime **THEN** exactly one locally spawned non-replicated `ABushVolumeSet` (`bReplicates = false`) holds one box component per bush on the `BushVolume` object channel, and the world holds zero replicated concealment actors.

---

## Implementation Notes

*Derived from ADR-0017 Decision §1 and Key Interfaces:*

```
struct FBushRuntime { FBushId Id; FBox2D Footprint; FBox HideVolume; uint8 EntryAxisMask; FVector Anchor; };
UBushRuntimeSubsystem : UWorldSubsystem { BuildFromManifest(const FLayoutManifest&); const FBushRuntime* Find(FBushId) const; ... }
```

The anchor is the footprint centre at ground level inside the 0.35 m curb ring; the curb is the planter's height and lies inside the 2.0 x 2.0 m footprint. The bush mesh itself (VA-1) is placed by the Layout Service / Generator from the same Manifest and is not Concealment's. The `BushVolume` object channel and the presentation aim channel are `Config/DefaultEngine.ini` collision-profile entries added by the implementing story (this pipeline cannot edit Config). Parity hashing uses a stable canonical byte form of the four fields; the negative control (a 1 cm anchor perturbation on one client) must change the hash.

**Prototype replaced**: `Source/StealthGame/Private/StealthBush.cpp` and `Public/StealthBush.h` (101 + 48 lines of placeholder opaque spheres, known issue 8) are replaced, not promoted; removal is story 035.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-008-conceal-collision-pure-function.md`: the collision responses of the volumes.
- `story-029-rustle-parity.md`: `PushRustlePulse` and the MPC.
- Level Socket Contract epic: `BushSite` schema and the V-BUSH-1 validator rule itself (this story asserts it rejects).

---

## Files

- `Source/StealthGameplay/Features/Concealment/Public/Server/BushRuntimeSubsystem.h`
- `Source/StealthGameplay/Features/Concealment/Private/BushRuntimeSubsystem.cpp`
- `Source/StealthGameplay/Features/Concealment/Private/BushVolumeSet.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/ManifestRejectTest.cpp` (class `FConcealmentManifestRejectTest`, name `StealthGame.Concealment.ManifestReject`)
- `Source/StealthTestHarness/Private/Tests/PIE/BushSiteParityTest.cpp` (`NETWORK_TEST_CLASS(BushSiteParity, "StealthGame.Concealment.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: BushSiteParity
  - Given: a 2-client PIE on the graybox Manifest fixture (ADR-0014).
  - When: each machine has run `BuildFromManifest`.
  - Then: per bush, the hash of {BushId, HideVolume 2.0 x 2.0 x 2.25 m, EntryAxisMask, Anchor} is identical on the server and both clients.
  - Edge cases: at least one bush; negative control: a 1 cm anchor perturbation on one client must give a different hash. The real LSC Manifest check is deferred until LSC stories exist.
- **AC2**: ManifestReject
  - Given: a BushSite with an empty EntryAxis.
  - When: V-BUSH-1 runs.
  - Then: the layout is rejected and round start is refused.
  - Edge cases: one empty bush among valid ones rejects the whole layout; a single axis is accepted.
- **AC3**: SingleVolumeSet
  - Given: the built runtime on all machines.
  - When: world actors are enumerated.
  - Then: exactly one `ABushVolumeSet` with `bReplicates = false`, one box per bush on the `BushVolume` channel, zero replicated concealment actors.
  - Edge cases: a zero-bush layout behaviour is unspecified; the implementer confirms with the LSC owner.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `ManifestRejectTest.cpp` and the PIE `BushSiteParityTest.cpp` — must exist and pass, OR a documented playtest

**Status**: [x] Created and passing (2026-10-29): StealthGame.Concealment.ManifestReject, StealthGame.Concealment.SingleVolumeSet, StealthGame.Concealment.BushRuntimeHash, StealthGame.Concealment.PIE.BushSiteParity.EveryMachineBuildsTheSameBushes; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: OBSERVED - 3-player listen-server PIE on L_Prototype (production/qa/evidence/concealment-001-026-2026-10-29/, con-*.png per viewer)

---

## Dependencies

- Depends on: level-socket-contract epic (`BushSite` / `FLayoutManifest`; stories not yet written), ADR-0014 graybox provider (Core Layout Service)
- Unlocks: `story-004-hide-enter-execution-and-commit.md`, `story-007-hide-legality-predicate.md`, `story-008-conceal-collision-pure-function.md`, `story-015-spot-zone-and-occupants.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1-AC3 passing. Every machine builds UBushRuntimeSubsystem from the stand-in Manifest's BushSite records and each bush's {BushId, HideVolume 2.0 x 2.0 x 2.25 m, EntryAxis, Anchor} hashes identically on the server and both clients (1 cm perturbation control); an empty EntryAxis rejects the layout (V-BUSH-1 -> NotifyLayoutAborted); one local non-replicated ABushVolumeSet per machine.
**Deviations**: D49 STAND-IN: the graybox Manifest's BushSite records are gathered by the composition root (StealthConcealmentWiring::GatherBushSites) from L_Prototype's ten AStealthBush actors, sorted by name, ground-traced, all four sides open; the actors keep only their placement (NoCollision, no gameplay). Built from AStealthGameState::BuildCityForSeed on every machine.
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
