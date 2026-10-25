# Story 025: Failure modes and teardown

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

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: A failed attempt returns an `ELayoutFailure` through `ILayoutBuildSink::OnAttemptFailed`; the service reseeds (R <= 5) or goes to the graybox fallback. On failure the attempt's `AStealthLayoutGeometry` is destroyed (instances, collision), the navigation build lock is re-armed and the socket set discarded before the next attempt. The Generator never regenerates for a client version mismatch (LSC R4.7).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014)
**Engine Notes**: Teardown of runtime-created ISM components and the nav lock re-arm go through the layout service (story `level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`); this story proves the Generator's side: the right failure id at the right moment and nothing left behind. The timeout cases need an injected clock (`ILayoutClock`, ADR-0014 section 1), never real sleeps.

**Control Manifest Rules (this layer)**:
- Required: Round and every feature reach layout only through `UStealthLayoutService`; the only runtime navmesh build trigger is the service's `RemoveNavigationBuildLock(Custom, Rebuild)` — source: ADR-0014
- Forbidden: Never adapt parameters to the host (J, N and budgets are never changed by a slow machine); never regenerate for a client mismatch; never publish the least-bad candidate — source: ADR-0014, GDD R-G10
- Guardrail: all attempts <= 45 s total, each <= 7 s (`LayoutAttemptTimeoutSec`, LSC); next attempt only if >= `CityGenAttemptStartMinSec` (4 s) remain

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G22, R-G10), scoped to this story:*

- [ ] **AC1** (AC-G22 first clauses, `Failure.GPreExhaustedAndOverflow`): **GIVEN** injected failures **THEN** all candidates failing G-PRE ends `GPreExhausted` with 0 instances spawned, and a candidate over `CityGenManifestEntriesMaxCount` or the instance cap fails `ManifestOverflow` and the next candidate is tried.
- [ ] **AC2** (AC-G22 middle clauses, `Failure.ConfigAndTimeouts`): **GIVEN** a missing kit asset, an absent `GeneratorKitVersion` table or invalid params **THEN** `GeneratorConfigInvalid` is returned at `Build` entry and the service falls back with 0 reseeds; **GIVEN** two consecutive injected timeouts **THEN** `HostTooSlow` and fallback, parameters never adapted, and the next attempt starts only if >= `CityGenAttemptStartMinSec` (4 s) of the 45 s remain.
- [ ] **AC3** (AC-G22 last clause, `Failure.LscRejectTeardown`): **GIVEN** LSC Validate or Select failing after G-PRE passed **THEN** the geometry is torn down with 0 instances and 0 nav tiles before the next attempt, and a client `GeneratorKitVersion` mismatch is resolved by LSC (resend once, then `ParamsMismatch`) with the Generator never regenerating.

---

## Implementation Notes

*Derived from GDD R-G10 and ADR-0014 section 3.4:*

Failure table to implement exactly (GDD R-G10): no candidate passes G-PRE -> `GPreExhausted`, nothing spawned, ~1 s; candidate over entries or instance cap at emission -> `ManifestOverflow`, next candidate; LSC Validate/Select fails -> attempt fails with LSC ids, geometry torn down before the next attempt; attempt over 7 s -> service watchdog cancels, `Timeout`; two consecutive timeouts (`CityGenTimeoutAbortCount` 2) -> `HostTooSlow`; deterministic config error (kit asset missing, `GeneratorKitVersion` table absent, invalid params) -> `GeneratorConfigInvalid` once at `Build` entry, **no reseeds burned**; client kit version differs -> LSC R4.7 resend once then `ParamsMismatch`, the server never regenerates. The config check runs in state `Checking` before any RNG draw (0 draws is part of AC2). Kit asset presence is checked on the game thread by resolving the kit mesh soft references at `BeginAttempt`, before the worker is launched (the worker holds no `UObject`).

When the overflow occurs on the last candidate (j = J-1) the attempt ends `GPreExhausted`... the story's open point: the GDD does not say which code a last-candidate overflow returns; take `GPreExhausted` (nothing was spawned) and record the choice in the dev-story notes. More than 5% of a 50-seed batch's candidates overflowing is a batch-audit failure (story 027), not a runtime action.

Prototype replaced: `StealthCityGenerator` logged a reachability check but had no failure ladder; none is carried over.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `level-socket-contract/story-003-attempt-loop-timeout-budget-reseed.md` and `story-005-fallback-travel-and-host-loss.md`: the service-side reseed counter, watchdog and fallback travel.
- `story-024-worker-task-cancellation-and-state-machine.md`: cancellation mechanics and the state table.
- `story-034-fallback-short-circuit.md`: the multi-process fallback proof.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenFailure.h`
- `Source/StealthGameplay/Private/Features/CityGenerator/StealthCityGeneratorProvider.cpp` (config check, failure mapping)
- `Source/StealthGameplay/Private/Tests/CityGen/GPreExhaustedAndOverflowTest.cpp` (class `FCityGenFailureGPreExhaustedAndOverflowTest`, name `StealthGame.CityGen.Failure.GPreExhaustedAndOverflow`)
- `Source/StealthGameplay/Private/Tests/CityGen/ConfigAndTimeoutsTest.cpp` (class `FCityGenFailureConfigAndTimeoutsTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/LscRejectTeardownTest.cpp` (class `FCityGenFailureLscRejectTeardownTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). AC-G22 is Logic/BLOCKING in the GDD and can run as a T1 plain-core test with a fake LSC service and an injectable clock; the world-facing teardown count (0 instances, 0 nav tiles) is asserted in a world fixture.*

- **AC1** `Failure.GPreExhaustedAndOverflow`
  - Given: a fake service and a fixed seed. Case A forces every candidate to fail G-PRE. Case B gives candidate 0 entries = `CityGenManifestEntriesMaxCount` + 1 (repeat with instances = cap + 1) and candidate 1 valid.
  - When: `Build` runs to completion.
  - Then: A ends `GPreExhausted` with 0 instances spawned. B reports candidate 0 `ManifestOverflow` and candidate 1 is tried and accepted.
  - Edge cases: entries == 3,500 is accepted. Overflow on the last candidate (j = J-1) ends Exhausted (see Implementation Notes for the code choice).
- **AC2** `Failure.ConfigAndTimeouts`
  - Given: three configs (missing kit asset, absent `GeneratorKitVersion` row, invalid params). Also a fake clock with a 45 s budget and two consecutive injected worker timeouts.
  - When: `Build` is called for each case.
  - Then: each config returns `GeneratorConfigInvalid` at Checking, with 0 RNG draws and 0 reseeds, and the service falls back. The timeout case returns `HostTooSlow` plus fallback, and the params hash is unchanged. A next attempt starts at 4.0 s remaining and does not start at 3.9 s.
  - Edge cases: timeout, success, timeout is not `HostTooSlow`.
- **AC3** `Failure.LscRejectTeardown`
  - Given: a G-PRE-passing candidate taken to HandedOff, with an LSC stub failing Validate (case A) or Select (case B). A second stub reports a client kit-version mismatch.
  - When: LSC rejects.
  - Then: instances, collision shapes and nav tiles are all 0 before the next `BeginAttempt`. The mismatch gets one resend, then `ParamsMismatch`, with `Build` call count staying 1.
  - Edge cases: a watchdog cancel during NavBuilding also tears down to 0.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` (T1 plain-core with the fake service, plus the world fixture for the teardown count) OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-023-candidate-loop-work-unit-cap-and-metrics.md`, `story-024-worker-task-cancellation-and-state-machine.md`, `story-026-server-kit-spawn-from-manifest.md`; cross-epic `level-socket-contract/story-003-attempt-loop-timeout-budget-reseed.md`, `story-005-fallback-travel-and-host-loss.md`, `story-012-navmesh-lifecycle-and-freeze.md`
- Unlocks: `story-034-fallback-short-circuit.md`, `story-039-g-pre-false-pass-rate-and-guard-band-sign.md`
