# Story 034: Fallback short-circuit

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0008 for the multi-process tier)
**ADR Decision Summary**: `LayoutProviderMode` `GeneratedWithFallback` makes the graybox the permanent fallback map: after attempts are exhausted (LSC R8.3), or immediately on a deterministic configuration error, the service goes to fallback travel and Round re-enters `Preparing` on the graybox. Round and the Generator never include each other's headers.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; seamless-travel behaviour under PIE is [U], ADR-0005/0008)
**Engine Notes**: The fallback is a travel to the graybox map. PIE hard-travels unless `net.AllowPIESeamlessTravel=1`, so this proof is **T3** (Gauntlet multi-process, a cooked Development build, `StealthGame.CityGen.MP.FallbackShortCircuit`), not a single-process PIE run (the GDD's "2-client PIE" wording is superseded by ADR-0005/0008: flagged for the GDD owner).

**Control Manifest Rules (this layer)**:
- Required: Seamless-travel evidence comes only from T3 Gauntlet multi-process runs of a cooked Development build — source: ADR-0008 (Technical Setup 2026-10-25b)
- Forbidden: Never use a single-process T2 PIE run as seamless-travel evidence; never let Round and the Generator include each other's headers — source: ADR-0008, ADR-0014
- Guardrail: a config error burns 0 reseeds; the whole fallback decision is immediate

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G35), scoped to this story:*

- [ ] **AC1** (AC-G35, `Cross.FallbackShortCircuit`): **GIVEN** `GeneratedWithFallback` with a missing kit asset injected **THEN** the service reaches fallback travel with 0 reseeds and Round re-enters `Preparing` on the graybox (proved by a T3 multi-process run, not PIE).
- [ ] **AC2** (AC-G35 include graph, `Cross.NoGeneratorSymbolInRound`): **GIVEN** the include graph **THEN** no Generator symbol is referenced by Round.

---

## Implementation Notes

*Derived from GDD R-G10, R-G1 and ADR-0008:*

Inject a missing kit asset on the host (a test seam that makes the kit mesh soft reference resolve to nothing, compiled out of Shipping). `Build` returns `GeneratorConfigInvalid` at `Checking` (story 025), the service skips reseeds, goes to `FallbackTravel` (LSC story 005), and Round re-enters `Preparing` on the graybox map; assert reseed count 0, `Build` call count 1, travel to the graybox, and the state sequence from the service's attempt records. The include-graph check (`StealthGame.CityGen.Build.NoGeneratorSymbolInRound`, T0 lint) fails on any CityGen header included, directly or transitively through a shared header, and any CityGen symbol referenced by Round sources. A PIE run is not accepted as evidence for AC1 (record in the dev-story notes).

Prototype replaced: the prototype had no fallback (a failed reachability check was only logged).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-025-failure-modes-and-teardown.md`: the `GeneratorConfigInvalid` semantics and the T1 failure ladder.
- LSC fallback travel and host loss: `level-socket-contract/story-005-fallback-travel-and-host-loss.md`.
- Round's re-entry into `Preparing`: `round-match-framework` (round-state stories).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/CityGen/FallbackShortCircuitMpTest.cpp` (Gauntlet node `StealthGame.CityGen.MP.FallbackShortCircuit`)
- `Source/StealthGameplay/Private/Tests/CityGen/NoGeneratorSymbolInRoundTest.cpp` (class `FCityGenBuildNoGeneratorSymbolInRoundTest`, name `StealthGame.CityGen.Build.NoGeneratorSymbolInRound`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE; the GDD's "2-client PIE" departs from ADR-0005/0008 and is replaced by T3).*

- **AC1** `StealthGame.CityGen.MP.FallbackShortCircuit`
  - Given: a Gauntlet host plus 1 client with a kit asset missing on the host.
  - When: `Build` runs and the service falls back.
  - Then: `GeneratorConfigInvalid` is returned at Build entry. The reseed count is 0 and `Build` call count is 1. Fallback travel occurs and Round re-enters `Preparing` on the graybox map.
  - Edge cases: a PIE run is not accepted as evidence.
- **AC2** `Build.NoGeneratorSymbolInRound`
  - Given: the Round sources and the include graph.
  - When: the T0 include-graph lint runs.
  - Then: 0 CityGen headers are included, directly or transitively, and 0 CityGen symbols are referenced.
  - Edge cases: transitive include through a shared header.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the T3 Gauntlet test and the T0 lint test OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-025-failure-modes-and-teardown.md`, `story-002-provider-shell-registration-and-secret-input-isolation.md`; cross-epic `level-socket-contract/story-005-fallback-travel-and-host-loss.md`, `story-026-graybox-provider-markers-and-bake.md`, `tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: `story-046-ten-connection-gauntlet-integration-pass.md`
