# Story 031: Golden seeds and the bump rule

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (ADR-0006 for the golden-vector rule)
**ADR Decision Summary**: Generator goldens are `CityGenGoldenSeedCount` seeds with expected `LayoutHash`, candidate index, outcome and predicate metrics, checked in as a generated `Source/StealthGameplay/Private/Tests/Data/CityGenGoldens.generated.inl` of `constexpr` records (T1 tests do no file I/O; TD ruling 2026-10-24), rewritten only by the rebaseline tool, each storing `GeneratorKitVersion`, compiler and CPU family. A hash change without a `GeneratorKitVersion` bump fails as "unbumped drift".
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0008)
**Engine Notes**: Cross-CPU/cross-compiler stability is claimed only for configurations actually run (MSVC x64 today) [U for others]; a golden run on a different compiler or CPU family than its record warns or skips, never silently passes. Predicate metrics come from LSC Validate on the server navmesh, with a 5% margin so navmesh tiling noise cannot flip a golden.

**Control Manifest Rules (this layer)**:
- Required: Golden vectors, including the Generator goldens, are committed and run on every CI platform; changing a primitive replaces its vectors in the same change — source: ADR-0006
- Forbidden: Never reseed or skip a failing golden to pass a gate; never let golden seeds be live or practice content (they are test data) — source: ADR-0008, GDD R-G11.5
- Guardrail: the golden suite runs in the per-commit gate (about 20 builds of S1-S12 + G-PRE, a few seconds)

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G23, R-G11.2, R-G11.3, R-G11.5), scoped to this story:*

- [ ] **AC1** (AC-G23, `Golden.SeedsReproduce`): **GIVEN** the 20 golden seeds (`CityGenGoldenSeedCount`) **THEN** each reproduces its `LayoutHash`, candidate index and outcome, and a stage change without a `GeneratorKitVersion` bump fails as "unbumped drift" naming the first differing stage.
- [ ] **AC2** (R-G11.2, `Golden.EligibilityAndFormat`): **GIVEN** the golden set **THEN** a seed is eligible only if every ratio predicate passes with >= `CityGenGoldenMarginMinPct` (5%) margin, records live in `CityGenGoldens.generated.inl` as `constexpr` records written only by the rebaseline tool and store `GeneratorKitVersion`, compiler and CPU family.
- [ ] **AC3** (R-G11.5, `Golden.NotContent`): **GIVEN** `IsGoldenSeed(Seed)` **THEN** it is true for exactly the committed seeds so Session's redraw can exclude them from live and practice draws.

---

## Implementation Notes

*Derived from GDD R-G11 and ADR-0008 section 9:*

The rebaseline tool is a commandlet (`StealthTestHarness`) that runs the golden candidates, checks eligibility (every ratio predicate passes with >= 5 points of margin; the margin is measured against the LSC thresholds, not the guarded G-PRE ones) and rewrites `CityGenGoldens.generated.inl`; a T0 check regenerates to a temp location and byte-compares with the checked-in `.inl` ("written only by the rebaseline tool" is read as "the file equals the tool's output", confirm in review). Bump rule: any change to stage code, kit content, slab or stack tables or the collision strategy changes `GeneratorKitVersion` and regenerates the goldens in the same change; a perturbation hook (non-Shipping) changes one stage without a bump and the test must fail with "unbumped drift" naming the first differing stage (stage hashes from story 030); a perturbed run with a bump and regenerated records passes (negative control). `IsGoldenSeed` is a `constexpr`/binary-search query over the committed seeds (seed 0 included in the boundary tests); Session's redraw call site belongs to a Session story (the GDD's Session change request 2 asks Session to redraw a golden `Seed`, as it does for `UsedSeeds`), flagged as a cross-epic follow-up and not implemented here. Golden seeds appear only in scripted tutorials (provisional Onboarding GDD); practice uses fresh seeds.

Prototype replaced: the prototype's "seeds identical on all instances" evidence list (29755, 8924, ...) was a manual check; none of those seeds is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-030-pure-function-determinism-and-stage-hashes.md`: cross-process determinism and the stage-hash log.
- `story-033-cross-client-manifest-parity-under-loss.md`: client-side hashes.
- Session's redraw of golden seeds: a `session-travel` follow-up (not yet a story).
- `story-004-kit-tables-and-generator-kit-version.md`: the table-hash bump check.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Tests/Data/CityGenGoldens.generated.inl`
- `Source/StealthTestHarness/Private/CityGen/CityGenRebaselineCommandlet.cpp`
- `Source/StealthGameplay/Public/Server/Features/CityGenerator/CityGenGoldenSeeds.h` (`IsGoldenSeed`)
- `Source/StealthGameplay/Private/Tests/CityGen/SeedsReproduceTest.cpp` (class `FCityGenGoldenSeedsReproduceTest`, name `StealthGame.CityGen.Golden.SeedsReproduce`)
- `Source/StealthGameplay/Private/Tests/CityGen/GoldenEligibilityAndFormatTest.cpp` (class `FCityGenGoldenEligibilityAndFormatTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/GoldenNotContentTest.cpp` (class `FCityGenGoldenNotContentTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Golden.SeedsReproduce`
  - Given: the 20 committed golden records.
  - When: each seed is run, then once with a test-only perturbation hook in one stage and no version bump.
  - Then: `LayoutHash`, candidate index and outcome match for all 20. The perturbed run fails with "unbumped drift" naming the first differing stage.
  - Edge cases: perturbed plus bumped and regenerated passes (negative control). A compiler or CPU family different from the record warns or skips, never silently passes.
- **AC2** `Golden.EligibilityAndFormat`
  - Given: the golden set and a synthetic margin series.
  - When: eligibility and the records are checked.
  - Then: every golden seed has all ratio-predicate margins >= 5 points (a 4.9 margin is ineligible and 5.0 is eligible). Each `constexpr` record has non-empty `GeneratorKitVersion`, compiler and CPU family.
  - Edge cases: "written only by the rebaseline tool" is read as "the `.inl` is byte-identical to the tool's regeneration" (T0 script, because T1 does no file I/O).
- **AC3** `Golden.NotContent`
  - Given: the 20 golden seeds and 1,000 sampled non-golden seeds, including golden +/- 1.
  - When: `IsGoldenSeed` is evaluated.
  - Then: it is true for exactly the 20 committed seeds and false for every other seed.
  - Edge cases: seed 0.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-030-pure-function-determinism-and-stage-hashes.md`, `story-026-server-kit-spawn-from-manifest.md`, `story-004-kit-tables-and-generator-kit-version.md`; cross-epic `level-socket-contract/story-015-layout-measure-backend-world.md` (Validate metrics)
- Unlocks: `story-032-manifest-fixture-provider.md`, `story-033-cross-client-manifest-parity-under-loss.md`, `story-035-navmesh-frozen-after-hand-off.md`, `story-046-ten-connection-gauntlet-integration-pass.md`
