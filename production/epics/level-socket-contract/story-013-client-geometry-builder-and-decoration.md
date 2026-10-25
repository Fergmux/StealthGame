# Story 013: Client geometry builder, inert decoration and zero replicated actors

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-002`, `TR-lsc-003`
*(At write time: TR-lsc-002 = Integer Layout Manifest; clients build with no RNG and recompute `LayoutHash`; TR-lsc-003 = Zero replicated actors; bush/court components built locally)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0003 for zero replicated actors)
**ADR Decision Summary**: One `FLayoutGeometryBuilder` (client-safe) turns Manifest entries into a non-replicated local actor `AStealthLayoutGeometry` (one `UInstancedStaticMeshComponent` per kit mesh x spatial quarter, batch `AddInstances`); decoration is client-built from `FIntStream(PublicSubSeed(Layout, LayoutAttempt), LayoutHash)` with no collision, no nav relevance and bounds top at most 0.5 m. The contract spawns zero replicated actors.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (rendering items inherit HIGH through spike R1, owned by ADR-0022)
**Engine Notes**: `UInstancedStaticMeshComponent::AddInstances(Transforms, bShouldReturnIndices, bWorldSpace, bUpdateNavigation)` is [V]; instances are added in local space on an identity-transform actor with `bUpdateNavigation=false`, before `RegisterComponent()` where the slice plan allows; `AddInstancesById` is Preliminary and not used; HISM is not used (Nanite) [U, spike R1]. The listen host builds the kit once (its server geometry is also its view) and builds only decoration on the client path. The graybox Manifest has 0 entries, so this story is exercised with synthetic Manifest fixtures until the Generator exists.

**Why this story**: The Manifest is useless without the one builder that server and clients share, and Pillar 1 forbids any client-side randomness or replicated layout actor. Read first: R2 items 2-3 and 6, AC-2, AC-3; ADR-0014 sections 3.1-3.4.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `AStealthLayoutGeometry` and every layout-derived component are `bReplicates = false`, created locally; kit instances are added with batch `AddInstances(..., bUpdateNavigation = false)` (ADR-0014)
- Forbidden: Never let clients run the Generator, draw geometry randomness or hold navigation data; never add kit instances with per-instance `AddInstance` loops or one actor per module (ADR-0014)
- Guardrail: Layout (clients) Manifest build in <= 10 ms slices, <= 3 s p95; <= 5,000 instances, about 48 ISM components, <= 20,000 collision shapes (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-2 and AC-3 (Manifest.ZeroReplicatedActors), scoped to this story:*

- [ ] GIVEN a published Manifest, WHEN a client builds its geometry, THEN the RNG call counter on the build path is 0 (`Determinism.ClientBuildNoRng`)
- [ ] GIVEN two clients, THEN cosmetic decoration is byte-identical and every cosmetic component has no collision, no nav modifier and a bounds top at most 50 cm above its floor (`Determinism.CosmeticInert`)
- [ ] GIVEN a published layout, THEN 0 replicated actors are spawned by the contract and `AStealthLayoutGeometry` and every layout-derived component have `bReplicates` false (`Manifest.ZeroReplicatedActors`)

---

## Implementation Notes

*Derived from ADR-0014 section 3:*

- `FLayoutGeometryBuilder` in `StealthGameplay/Public/View/Core/`; `AStealthLayoutGeometry` spawned locally on every machine (`bReplicates=false`, `bNetLoadOnClient=false`) at the identity transform. Client slices are at most 10 ms each, canonical order (component index, then entry order), draining within `ClientLayoutBuildMaxSec` (3 s p95).
- Mobility `Static` once the last instance of the attempt is added (set before registration) [U spike R1]; kit meshes Nanite-enabled with per-module simple collision (<= 4 primitives) on the pawn and `ViewBlock` channels; complex-as-simple forbidden.
- `SetCanEverAffectNavigation(true)` on server components, **false** on client components.
- Decoration set is separate: `FIntStream(PublicSubSeed(Layout, LayoutAttempt), LayoutHash)` with stable-id iteration; non-Shipping builds `ensure` and drop a violating element (bounds top > 50 cm, any collision, any nav relevance). A prop of exactly 50 cm passes, 51 cm fails.
- The RNG call counter hooks the integer stream and also `FMath::Rand` / `FRandomStream` on the build path.
- `DeriveBushSites` and court fixtures/traversal coping built as non-replicated components are covered by neighbouring stories (037) but must use this builder's non-replication rule.
- **Prototype replaced:** `AStealthCityGenerator` (clients run the generator from the public seed; replicated objective/bush relocation) is the model this story supersedes; nothing from it is promoted.

---

## Out of Scope

- Story 008/009: Manifest struct, hash and replication
- Story 024: recompute-and-ack ordering
- Story 037: BushSite derivation and parity
- Kit mesh and render budget work (ADR-0022, rendering epic)

---

## Files

- `Source/StealthGameplay/Public/View/Core/StealthLayoutGeometry.h`
- `Source/StealthGameplay/Private/View/LayoutGeometryBuilder.cpp`
- `Source/StealthGameplay/Private/View/LayoutDecoration.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/ClientBuildTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Determinism.ClientBuildNoRng`
  - Given: a published Manifest fixture.
  - When: a client builds geometry.
  - Then: the RNG call counter on the build path is 0.
  - Edge cases: also hook `FMath::Rand` and `FRandomStream` on the path.
- **AC-2** `Determinism.CosmeticInert`
  - Given: two client builders with the same Manifest.
  - When: cosmetic decoration is generated.
  - Then: byte-identical output; every cosmetic component has no collision, no nav modifier, bounds top <= 50 cm above its floor.
  - Edge cases: a prop of exactly 50 cm passes, 51 cm fails.
- **AC-3** `Manifest.ZeroReplicatedActors`
  - Given: a published layout.
  - When: the contract's spawned actors are counted.
  - Then: 0 replicated actors; `AStealthLayoutGeometry` and every layout-derived component have `bReplicates` false.
  - Edge cases: late-join build.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/ClientBuildTest.cpp` — `StealthGame.Layout.Determinism.ClientBuildNoRng`, `.CosmeticInert`, `StealthGame.Layout.Manifest.ZeroReplicatedActors` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-008-manifest-struct-and-layout-hash.md`
- Cross-epic: `production/epics/tuning-data-config/EPIC.md` (module skeleton)
- Unlocks: `production/epics/level-socket-contract/story-024-preparing-handshake-and-hash-ack.md`, `production/epics/level-socket-contract/story-037-bush-site-parity.md`
