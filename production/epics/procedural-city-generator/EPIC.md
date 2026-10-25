# Epic: Procedural City Generator

> **Layer**: Feature
> **GDD**: design/gdd/procedural-city-generator.md
> **Architecture Module**: City Generator in `StealthGameplay/Features` (12-stage pipeline, integer grid pre-validator, `ILayoutProvider` of kind `Generated`, client decoration, golden seeds)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories procedural-city-generator`
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 16 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

The Procedural City Generator is the second implementation of the Level Socket Contract (not Unreal's PCG framework, which it does not use). It registers with the layout service as the provider of kind `Generated` and, from `PublicSubSeed(Layout, LayoutAttempt)`, assembles a compact city about 124-132 m square (default 128 m) from the 2.0 m modular kit (art bible 6.2, M01-M12), handing the service the socket set and the integer Layout Manifest. The contract owns what a valid city is; this epic owns how one is made: the macro frame and street/canal/causeway/plaza/court skeleton on the integer grid, module selection, rotation and anti-repetition (art bible 6.7), socket placement built to pass the V-* predicates, a grid pre-validator that discards most bad cities before geometry exists, and the build split (server builds and validates, clients instantiate from the Manifest, decoration is client-side and deterministic without collision).

Determinism is the hard requirement: the generator is a pure function of attempt seed, kit version and params; it uses only integer streams (PCG32; `FRandomStream` is not used for layout), asserts on any call to server sub-seeds, iterates canonically, and has committed golden vectors and golden seeds that must be stable across CPUs and compilers. Budgets: server spawn plus nav at most 3 s, client build at most 3 s p95, Manifest at most 3,500 entries, at most 5,000 instances (ISM per module type with Nanite, batch `AddInstances`, static mobility) and 20,000 shapes. Failure modes route to reseed or the graybox fallback.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0006: Seeds and deterministic/secret random streams | Public sub-seeds for layout vs SipHash-2-4 keyed server-secret streams; integer PCG32 streams so layout is bit-identical across machines. | LOW |
| ADR-0008: Test and automation strategy for leak and parity tests | Headless pure-C++ cores, multi-client and multi-process leak harness, raw-bunch capture, net-emulation profiles, per-commit canaries. | MEDIUM |
| ADR-0014: Layout service, seeded generation and runtime navmesh | Layout-service world subsystem with provider registry, integer Layout Manifest and LayoutHash, server navmesh build and validation, reseed and graybox fallback. | MEDIUM |
| ADR-0022: Rendering configuration and performance budgets | Lumen/Nanite/VSM configuration and CPU/GPU frame budgets for the RTX 3060-class 1080p60 target (GPU 14.6 ms). | HIGH |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

8 of 8 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-citygen-001 | Pure function of attempt seed, kit version and params; asserts on server sub-seeds | ADR-0014 ✅ |
| TR-citygen-002 | Integer-only RNG and canonical iteration; golden seeds stable across CPUs/compilers | ADR-0006, ADR-0008 ✅ |
| TR-citygen-003 | Twelve-stage pipeline with integer pre-validator; time-sliced | ADR-0014 ✅ |
| TR-citygen-004 | ISM per module (Nanite), batch add, static mobility; ≤ 5,000 instances, ≤ 20,000 shapes | ADR-0014, ADR-0022 ✅ |
| TR-citygen-005 | Deterministic client decoration without collision | ADR-0014 ✅ |
| TR-citygen-006 | Server spawn + nav ≤ 3 s; client build ≤ 3 s p95 | ADR-0014 ✅ |
| TR-citygen-007 | Manifest ≤ 3,500 entries | ADR-0014 ✅ |
| TR-citygen-008 | Failure modes → reseed or graybox fallback | ADR-0014 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** `Source/StealthGame/Private/StealthCityGenerator.cpp` / `Public/StealthCityGenerator.h` (428 + 133 lines: a grid city of houses, halls, temple, stalls, gardens and low walls built identically on every machine from a replicated server seed, objectives and bushes moved to seeded sites, dynamic nav rebuild, a reachability check logged). The prototype proved end-to-end seeded generation (seeds 29755, 8924, 25482, 17906, 13609 identical on all instances; 19972 on 6; 13135 on 10) and is the origin of the kit and the validation idea, but the production pipeline is a rewrite onto integer-grid PCG32 and the Manifest. **Kept (assets):** the Meshy kit pieces (`SM_Kit_House`, `SM_Kit_Temple`, `SM_Kit_Stall` under `/Game/StealthGame/Environment/Kit/`, sources in `assets/environment/kit/`) and the Blender kit export procedure are inputs to the production kit. No `.uasset` is modified by this epic definition.

## Build Order and Gates

- **Depends on (epics)**: level-socket-contract (the contract and service it plugs into), session-travel (`PublicSubSeed(Layout, Attempt)` only), tuning-data-config (`CityGen*` keys); built last in the MVP order with the graybox map as the permanent fallback
- **Gates and risks**: ADR-0014 is unconditional, but the GDD is provisional on spikes R1, R4 (runtime ISM/Lumen/Chaos cost, navmesh build time) and LSC Open Question 2 (Manifest wire size); ADR-0022 R1-R3 apply to the 5,000-instance Nanite budget. Systems-index High-Risk system (identical layout on all clients, runtime navmesh, validation, objective reachability). Decided 2026-10-25: the Generator GDD is not held for spikes but stays provisional on their results. This epic owns the cross-cutting **10-player integration pass** (about 6 d: 5v5 over a real network, not in-process PIE) as an exit criterion, because it is the last gameplay epic in build order.
- **Estimate**: about 14 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 8.4 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 36 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/procedural-city-generator.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
