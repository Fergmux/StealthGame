# Story 029: Client decoration

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0006 for the integer stream)
**ADR Decision Summary**: Decoration (client, and the host as a client) is built after the kit from `FIntStream(PublicSubSeed(Layout, LayoutAttempt), LayoutHash)` by a separate component set with **no collision, no nav relevance** and bounds top <= 0.5 m (LSC R2.3, R-G8); non-Shipping builds `ensure` and drop a violating element.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; rendering items inherit HIGH through R1)
**Engine Notes**: Decoration components are created locally per machine with collision disabled and `CanEverAffectNavigation` false. Macro value variation is in the material (world-aligned, two incommensurate noise scales; no per-instance albedo offset, art 6.7), so decoration does not randomise colour.

**Control Manifest Rules (this layer)**:
- Required: Decoration inputs are `PublicSubSeed(Layout, LayoutAttempt)` and `LayoutHash` only, through `FIntStream`, over cells in canonical order — source: ADR-0014, ADR-0006
- Forbidden: Never give a decoration element collision, a nav effect or a top above 0.5 m (it would become a hidden sight blocker); never draw decoration from a secret stream — source: GDD R-G8, ADR-0006
- Guardrail: kit + decoration instances <= 5,000; decoration <= `CityGenDecorReserveCount` (1,500)

Decoration content: decals, ground-class props (<= 0.5 m) and foliage variant picks. A decoration difference between clients never changes gameplay (no collision, no sight effect), but non-Shipping tests compare a `DecorHash` and fail on drift.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G18, R-G8), scoped to this story:*

- [ ] **AC1** (AC-G18, `Decor.ConstraintsAndParity`): **GIVEN** a 2-client PIE on 5 seeds **THEN** `DecorHash` is equal on both clients and every decoration element has no collision, no nav effect and a top <= 0.5 m above its floor.
- [ ] **AC2** (R-G8.1-2, `Decor.InputsAndDrop`): **GIVEN** decoration **THEN** its only inputs are `PublicSubSeed(Layout, LayoutAttempt)` and `LayoutHash` through the integer stream over cells in canonical order, a violating element is dropped with an `ensure` in non-Shipping builds, and there is no per-instance albedo offset.
- [ ] **AC3** (R-G8.3, `Decor.ReserveCap`): **GIVEN** decoration **THEN** its count is <= `CityGenDecorReserveCount` (1,500) and the sum with kit instances stays within the instance cap.

---

## Implementation Notes

*Derived from GDD R-G8 and ADR-0014 section 3.3:*

The decoration builder lives with the LSC client builder (`level-socket-contract/story-013-client-geometry-builder-and-decoration.md` owns the generic inert-decoration host and its `Determinism.CosmeticInert` test); this story supplies the Generator's rule set (id from `GeneratorKitVersion`): what to place, the families, and the integer draws over canonical cells. `DecorHash` is a non-Shipping FNV-1a hash of the decoration transforms and ids compared between clients in tests. Run the AC1 PIE test with `StealthGame.CityGen.PIE.DecorConstraintsAndParity.HashEqualAndConstrained` (CQTest, `StealthTestHarness`), with a precondition that each seed has > 0 decoration elements. AC2: two runs with identical `PublicSubSeed` and `LayoutHash` but a different server sub-seed, player count, join order and clock must give an identical `DecorHash`; it changes if `LayoutHash` or `PublicSubSeed` changes; shuffling the input cell container order leaves the hash unchanged (canonical iteration); a violating element (collision on, or height 0.6 m) is dropped and `ensure` fires; per-instance albedo-offset custom data count is 0. A 0.50 m element passes and 0.51 m fails. AC3 uses a fixed 50-seed batch (the AC gives no count; assumed): decoration <= 1,500 and decoration + kit <= 5,000.

Prototype replaced: the prototype generated every machine's whole city from the replicated seed (clients ran the generator); only decoration keeps that "build from public inputs" shape here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- LSC generic inert-decoration host: `level-socket-contract/story-013-client-geometry-builder-and-decoration.md`.
- `story-027-instance-entry-and-shape-budget-audit.md`: the 1,000-seed audit of totals.
- `story-044-tier-g1-validation-visual-tests-and-fairness-playtest.md`: how decoration looks.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Features/CityGenerator/CityGenDecorationRules.h` (client-safe)
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenDecorationRules.cpp`
- `Source/StealthTestHarness/Private/CityGen/DecorConstraintsAndParityPieTest.cpp` (CQTest PIE)
- `Source/StealthGameplay/Private/Tests/CityGen/DecorInputsAndDropTest.cpp` (class `FCityGenDecorInputsAndDropTest`, name `StealthGame.CityGen.Decor.InputsAndDrop`)
- `Source/StealthGameplay/Private/Tests/CityGen/DecorReserveCapTest.cpp` (class `FCityGenDecorReserveCapTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `StealthGame.CityGen.PIE.DecorConstraintsAndParity.HashEqualAndConstrained`
  - Given: 2 clients on 5 seeds.
  - When: decoration builds.
  - Then: `DecorHash` is equal on both clients. Every element has no collision, `CanEverAffectNavigation` false, and top - floor <= 0.5 m.
  - Edge cases: 0.50 m passes and 0.51 m fails. A seed with 0 decoration elements fails the precondition.
- **AC2** `Decor.InputsAndDrop`
  - Given: two runs with identical `PublicSubSeed` and `LayoutHash` but different server sub-seed, player count, join order and clock.
  - When: decoration runs, then again with a violating element injected (collision on, or height 0.6 m).
  - Then: `DecorHash` is identical across the two runs. It changes if `LayoutHash` or `PublicSubSeed` changes. The violating element is dropped and an `ensure` fires in a non-Shipping build. Per-instance albedo-offset custom data count is 0.
  - Edge cases: shuffling the input cell container order leaves the hash unchanged, which proves canonical iteration.
- **AC3** `Decor.ReserveCap`
  - Given: a fixed 50-seed batch (the AC gives no seed count, so the batch is assumed).
  - When: instances are counted.
  - Then: decoration <= 1,500 and decoration + kit <= 5,000.
  - Edge cases: exactly 1,500 passes.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the T2 PIE test plus the two logic tests above OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-004-kit-tables-and-generator-kit-version.md`, `story-003-integer-stage-streams-and-canonical-iteration.md`; cross-epic `level-socket-contract/story-013-client-geometry-builder-and-decoration.md`
- Unlocks: `story-041-client-build-time-and-first-frame-completeness.md`, `story-044-tier-g1-validation-visual-tests-and-fairness-playtest.md`
