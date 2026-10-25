# Story 041: Client build time and first-frame completeness

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: spike R1 (ADR-0022, runtime ISM/Lumen/Chaos cost on a 3060-class PC) and spike R4 (client collision-creation time and "first valid trace" after assembly, GDD Open Question 2) have not reported; the 3.0 s p95 and the VSM/Lumen settle thresholds are provisional (dagger) until they do. Owner of the captures: technical-artist plus performance-analyst.
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0022 for the VSM/Lumen settling)
**ADR Decision Summary**: Clients build the kit from the Manifest in <= 10 ms slices in canonical order and drain within `ClientLayoutBuildMaxSec` (3 s p95); geometry, decoration and lighting are complete before the briefing screen lifts (no static lighting; VSM page cache and Lumen surface cache settle behind the briefing, VG-4).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0022; rendering)
**Engine Notes**: Measured on the cooked client on the reference PC (RTX 3060-class, 1080p) over 20 golden seeds. The VSM and Lumen counters are engine stats (name the exact counters used in the dev-story notes: VSM newly-rendered pages per frame and Lumen surface-cache captures per frame); N1 and N2 are set from the R1 capture report, and until then the test records the values and reports NOT ASSESSED for those two checks, never pass.

**Control Manifest Rules (this layer)**:
- Required: Layout (clients) Manifest build in <= 10 ms slices, <= 3 s p95 — source: ADR-0014
- Forbidden: Never prove a performance criterion with figures from a non-reference machine as acceptance evidence (development evidence only) — source: ADR-0022
- Guardrail: no game-thread frame > 33 ms during `Preparing` (LSC R4.5); 0 missing instances in the first `Active` frame

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G16, VG-4), scoped to this story:*

- [ ] **AC1** (AC-G16, `Assembly.ClientBuildNoHitch`, provisional): **GIVEN** 20 golden seeds on the reference PC **THEN** the client build takes <= 3.0 s p95 and no frame exceeds 33 ms.
- [ ] **AC2** (AC-G16 / VG-4, `Assembly.FirstActiveFrame`): **GIVEN** the first `Active` frame **THEN** for the last 30 frames before the briefing lifts VSM newly-rendered pages per frame <= N1 and Lumen surface-cache captures per frame <= N2 (N1, N2 set from the R1 capture report; recorded as measurements until it lands), there are 0 missing kit instances in the first `Active` frame, and every client slice is <= 10 ms on the game thread in canonical order.

---

## Implementation Notes

*Derived from GDD R-G7, R-G9, VG-4 and the QL-STORY-READY review:*

AC1: Gauntlet T3 test (`StealthGame.CityGen.MP.ClientBuildNoHitch`), cooked client, Manifest receipt to build complete, nearest-rank p95 over 20 seeds (the 19th of 20 sorted values; name the percentile method in the test and state cold versus warm shader cache), per-frame game-thread time during the build. AC2: capture frames through the briefing lift (`StealthGame.CityGen.MP.FirstActiveFrame`): the first `Active` frame has client instances equal to the Manifest-derived count (0 missing); every client slice <= 10 ms in canonical order (component index, then entry order); VSM and Lumen settle counters for the last 30 frames before the lift are compared with N1 and N2 once the R1 report sets them. The instance and slice clauses are binding as soon as the test runs; the story cannot close until N1 and N2 are set. Hardware: DEFERRED until a 3060-class reference PC capture exists (an RTX 5090 desktop gives development evidence only, ADR-0022).

Prototype replaced: none (the prototype built the whole city synchronously in `BeginPlay`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- LSC client builder and its slicing: `level-socket-contract/story-013-client-geometry-builder-and-decoration.md`.
- `story-042-attempt-time-budget-and-t-cand-measurement.md`: server attempt time.
- `story-043-kit-gpu-cost-and-server-spawn-nav-time.md`: GPU cost and server spawn + nav.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/CityGen/ClientBuildNoHitchMpTest.cpp` (Gauntlet node)
- `Source/StealthTestHarness/Private/CityGen/FirstActiveFrameMpTest.cpp` (Gauntlet node)
- `production/qa/evidence/citygen/client-build-[date].json` (measurements)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 ADEQUATE; AC2 revised and ADEQUATE; both DEFERRED until the reference PC exists).*

- **AC1** `StealthGame.CityGen.MP.ClientBuildNoHitch`
  - Given: a cooked client on the reference PC, 20 golden seeds, provisional.
  - When: client build time (Manifest receipt to complete) and per-frame game-thread time are measured.
  - Then: p95 <= 3.0 s using nearest-rank (the 19th of 20 sorted values), and no frame > 33 ms during the build.
  - Edge cases: state cold versus warm shader cache. Name the percentile method in the test.
- **AC2** `StealthGame.CityGen.MP.FirstActiveFrame`
  - Given: a cooked client on the reference PC, 20 golden seeds. N1 and N2 are loaded from the R1 report, or null before it lands.
  - When: frames are captured through the briefing lift and the first Active frame, and slices are traced.
  - Then: for the last 30 frames before the lift, VSM newly-rendered pages per frame <= N1 and Lumen surface-cache captures per frame <= N2. The first Active frame has 0 missing kit instances (client instances == Manifest-derived count). Every client slice is <= 10 ms game thread, in canonical order.
  - Edge cases: with N1 or N2 null the test records the values and reports NOT ASSESSED for those two checks, never pass. The story can't close until they are set. The instance and slice clauses are binding now.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two Gauntlet tests and the measurement JSON OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-029-client-decoration.md`, `story-032-manifest-fixture-provider.md`, `story-033-cross-client-manifest-parity-under-loss.md`; spike reports: R1 (ADR-0022), R4 (`level-socket-contract/story-011-spike-r4-graybox-navmesh-timing.md` plus the generator-scale capture in story 043)
- Unlocks: `story-046-ten-connection-gauntlet-integration-pass.md`
