# Story 030: Pure-function determinism and per-stage hashes

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-001`, `TR-citygen-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams (ADR-0008 for the multi-process tier)
**ADR Decision Summary**: A public seed rebuilds the identical city on every machine: the Generator is a pure function of (AttemptSeed, GeneratorKitVersion, LayoutParamsHash), uses only integer PCG32 stage streams and canonical iteration, and a per-stage hash of the grid state localises any drift to one stage. Multi-process criteria are never proved in in-process PIE only.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0006; the multi-process runner is ADR-0008, MEDIUM)
**Engine Notes**: "Two fresh processes" runs the Generator core twice from a clean process start (a commandlet or a harness executable launched twice by a T0 script), then once more in a third process with a different player count and join order simulated through the (ignored) service inputs; cross-CPU and cross-compiler stability is claimed only for configurations actually run (MSVC x64 today).

**Control Manifest Rules (this layer)**:
- Required: Golden vectors are committed and run on every CI platform; Generator code runs inside `FPublicOnlyScope` — source: ADR-0006
- Forbidden: Never prove multi-process criteria in in-process PIE only — source: ADR-0008
- Guardrail: the stage-hash log is non-Shipping only and costs nothing in Shipping

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G1, R-G11.1), scoped to this story:*

- [ ] **AC1** (AC-G1, `Determinism.PureFunction`): **GIVEN** fixed (AttemptSeed, GeneratorKitVersion, LayoutParamsHash) **WHEN** the Generator runs in two fresh processes and once more with a different player count and join order **THEN** socket sets and Manifest bytes are identical.
- [ ] **AC2** (R-G11.1, `Determinism.StageHashLocalisation`): **GIVEN** non-Shipping builds **THEN** a per-stage hash of the grid state is logged after each stage, and a deliberately perturbed stage makes the log name that stage as the first differing one (negative control).

---

## Implementation Notes

*Derived from GDD R-G2, R-G11.1 and ADR-0008 section 1:*

AC1 is T3 (`StealthGame.CityGen.MP.PureFunction`): a harness executable or commandlet runs `Build` for a fixed seed set (at least 3 seeds, including one that accepts at candidate > 0) in two fresh processes, writes socket-set and Manifest bytes to files, and a T0 script byte-compares them; launch order is swapped once. The third run feeds a different player count and join order to the surrounding service stub, which the Generator must ignore (R-G2.2). AC2: after each stage the grid hash (story 005 hash) is logged as `LogStealthLayout` lines in non-Shipping builds; perturb stage k (one cell or one RNG draw) for each of S1..S12 with a test seam and assert a diff tool names stage k as the first differing stage for every k; S12 is the last stage, so its perturbation is detected by the Manifest hash. The Shipping-build check (no stage-hash logging) is DEFERRED until a Shipping build exists.

Prototype replaced: the prototype's determinism claim (seeds 29755, 8924, 25482, 17906, 13609 identical on all instances) was checked by eye and a log line; this is the automated replacement.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-031-golden-seeds-and-bump-rule.md`: committed goldens and the bump rule.
- `story-033-cross-client-manifest-parity-under-loss.md`: client-side hash parity.
- `story-003-integer-stage-streams-and-canonical-iteration.md`: the stream-level goldens.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/CityGen/CityGenBuildOnceCommandlet.cpp` (writes socket set and Manifest bytes for a seed)
- `tools/ci/citygen-two-process-compare.ps1` (T0 byte compare)
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageHashLog.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/StageHashLocalisationTest.cpp` (class `FCityGenDeterminismStageHashLocalisationTest`, name `StealthGame.CityGen.Determinism.StageHashLocalisation`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `StealthGame.CityGen.MP.PureFunction`
  - Given: fixed AttemptSeed, kit version and params hash, on at least 3 seeds including one that accepts at candidate > 0.
  - When: two fresh processes run the Generator, then a third with a different player count and join order.
  - Then: socket sets and Manifest bytes are identical (byte compare).
  - Edge cases: swap the process launch order.
- **AC2** `Determinism.StageHashLocalisation`
  - Given: a non-Shipping build logging one grid hash per stage.
  - When: stage k is perturbed (one cell or one RNG draw), for each of S1..S12.
  - Then: the log has one hash after each stage. The diff tool names stage k as the first differing stage for every k.
  - Edge cases: S12 is the last stage. The Shipping build logging nothing is DEFERRED to a Shipping build.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the T3 multi-process compare (script plus commandlet) and the T1 stage-hash test OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-023-candidate-loop-work-unit-cap-and-metrics.md`, `story-024-worker-task-cancellation-and-state-machine.md`; cross-epic `tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: `story-031-golden-seeds-and-bump-rule.md`, `story-033-cross-client-manifest-parity-under-loss.md`
