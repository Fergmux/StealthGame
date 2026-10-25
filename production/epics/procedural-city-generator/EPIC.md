# Epic: Procedural City Generator

> **Layer**: Feature
> **GDD**: design/gdd/procedural-city-generator.md
> **Architecture Module**: City Generator in `StealthGameplay/Features` (12-stage pipeline, integer grid pre-validator, `ILayoutProvider` of kind `Generated`, client decoration, golden seeds)
> **Status**: Ready
> **Stories**: 47 stories (see the Stories table below)
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

## Stories

Granularity `balanced` (2-4 acceptance criteria per story; GDD criteria with several independent cases are split into sub-criteria; criteria derived from an ADR or the epic where the GDD has none are labelled "derived" in the story). Manifest Version embedded: 2026-10-25f (the control-manifest header; the epic header says 25e, the 25f changes touch no Feature-layer City Generator rule). In dependency order: 001-004 foundations (keys, provider shell, streams, kit tables), 005-019 the pipeline stages S1-S12 and emission, 020-023 G-PRE and the candidate loop, 024-035 runtime (worker, failures, spawn, navigation, decoration, determinism, goldens, fixture, parity, fallback, freeze), 036-043 tools, audits and measurements, 044-045 tier G1 and prototype retirement, 046-047 the 10-player integration pass this epic owns.

**Gates.** Story 008 is the PCG OQ9 / cross-review C-W24 grid-only final-court enumeration script (200 skeletons); its script and report are committed (verdict CONCERNS, 2026-10-25; rule MOVING, jitter 0, radius 16 m) and it released 010, 011, 022 and 038. 027 is BLOCKED on spike R1 (ADR-0022) and the Manifest wire-size spike (LSC story 010); 039 on spike R4 (LSC story 011); 041, 042, 043 on R1 and/or R4 and reference hardware (043 repeats them at generator scale); 044 on the mass-production gate (R1-R3); 046 and 047 (the 10-player pass: a ten-connection Gauntlet run and a real-network 5v5 session) on R1, R4 and the net-emulation spike (SCL story 045). Spike R1 has no story in any epic yet (performance-analyst owns the 3060-class capture); R4's graybox half is LSC story 011.

QL-STORY-READY: first review 20 ADEQUATE (002, 003, 005, 007, 011, 015, 018, 019, 022, 024, 025, 028, 029, 030, 031, 032, 033, 034, 035, 038) and 27 GAPS (001, 004, 006, 008, 009, 010, 012, 013, 014, 016, 017, 020, 021, 023, 026, 027, 036, 037, 039, 040, 041, 042, 043, 044, 045, 046, 047, each with at least one flagged criterion), 0 INADEQUATE; the flagged criteria were revised (qa-lead rewrites with open points resolved from the GDD or logged as proposals) and re-reviewed once: all became ADEQUATE except 008 AC3 (verdict rule), 012 AC3 (storey counting), 014 AC1 (M10 height), 040 AC1 and 044 AC3, whose qa-lead rewrites were adopted without a third round (decisions logged; the GDD owners should ratify 012, 014 and the proposed constants flagged in the stories).

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | CityGen tuning keys, invariants I-PCG-1..7 and params struct | Logic | Ready | ADR-0014 |
| 002 | Provider shell, registration and secret-input isolation | Logic | Ready | ADR-0014 |
| 003 | Integer stage streams and canonical iteration | Logic | Ready | ADR-0006 |
| 004 | Kit tables and `GeneratorKitVersion` | Config/Data | Ready | ADR-0014 |
| 005 | Grid model, S1 Frame and F1 walkable target | Logic | Ready | ADR-0014 |
| 006 | S2 Skeleton: lattice, widths, jogs, transpose and jitter | Logic | Ready | ADR-0014 |
| 007 | F7 skeleton fairness pre-check | Logic | Ready | ADR-0014 |
| 008 | OQ9 / C-W24 grid-only final-court enumeration script | Logic | In Progress (script + report done; C++ wrappers DEFERRED) | ADR-0014 |
| 009 | S3 Area types | Logic | Ready | ADR-0014 |
| 010 | S4 Courts | Logic | Ready (008 reported CONCERNS) | ADR-0014 |
| 011 | S5 Zones, anchors and start cells | Logic | Ready (008 reported CONCERNS) | ADR-0014 |
| 012 | S6 Facade bay sequencer and F3 periodicity | Logic | Ready | ADR-0014 |
| 013 | S7 Emergence alleys and ExitPoints | Logic | Ready | ADR-0014 |
| 014 | S8 Traversal points | Logic | Ready | ADR-0014 |
| 015 | S9 Bushes and S10 props, role-blind | Logic | Ready | ADR-0014 |
| 016 | S11 Repairs and the occurrence rule | Logic | Ready | ADR-0014 |
| 017 | S12 Sockets, SocketIds, slab cover and skeleton transform | Logic | Ready | ADR-0014 |
| 018 | S12 CrowdRoute | Logic | Ready | ADR-0014 |
| 019 | Placement, bay stacks and Manifest emission | Logic | Ready | ADR-0014 |
| 020 | G-PRE grid measure backend and guard bands | Logic | Ready | ADR-0014 |
| 021 | G-PRE predicates A: navigation, reach, sight, emergence, clue, counts | Logic | Ready | ADR-0014 |
| 022 | G-PRE predicates B: court, pool, bush, route, zones, fairness and role counts | Logic | Ready (008 reported CONCERNS) | ADR-0014 |
| 023 | Candidate loop, work-unit cap and attempt metrics | Logic | Ready | ADR-0014 |
| 024 | Worker task, cancellation and the generator state machine | Integration | Ready | ADR-0014 |
| 025 | Failure modes and teardown | Integration | Ready | ADR-0014 |
| 026 | Server kit spawn from the Manifest | Integration | Ready | ADR-0014 |
| 027 | Instance, entry and shape budget audit | Logic | Blocked (spike R1, LSC OQ2) | ADR-0014 |
| 028 | Navigation integration: water, stairs, client-none | Integration | Ready | ADR-0014 |
| 029 | Client decoration | Integration | Ready | ADR-0014 |
| 030 | Pure-function determinism and per-stage hashes | Integration | Ready | ADR-0006 |
| 031 | Golden seeds and the bump rule | Logic | Ready | ADR-0008 |
| 032 | Manifest fixture provider | Integration | Ready | ADR-0014 |
| 033 | Cross-client Manifest parity under loss | Integration | Ready | ADR-0014 |
| 034 | Fallback short-circuit | Integration | Ready | ADR-0014 |
| 035 | Navmesh frozen after hand-off | Integration | Ready | ADR-0014 |
| 036 | Developer commands and debug overlay | UI | Ready | ADR-0014 |
| 037 | Batch harness and the F4/F6 audits | Logic | Ready | ADR-0014 |
| 038 | Role-prior and LSC batch audits | Logic | Ready (008 reported CONCERNS) | ADR-0014 |
| 039 | G-PRE false-pass rate and guard-band sign | Integration | Blocked (spike R4) | ADR-0014 |
| 040 | Stack and slab visual equivalence | Visual/Feel | Ready | ADR-0022 |
| 041 | Client build time and first-frame completeness | Integration | Blocked (spikes R1, R4) | ADR-0014 |
| 042 | Attempt time budget and t_cand measurement | Integration | Blocked (spike R4) | ADR-0014 |
| 043 | Kit GPU cost and server spawn plus nav time | Integration | Blocked (spikes R1, R4) | ADR-0022 |
| 044 | Tier G1 validation: visual tests and fairness playtest | Visual/Feel | Blocked (mass-production gate) | ADR-0014 |
| 045 | Retire the prototype generator | Integration | Ready | ADR-0014 |
| 046 | Ten-connection Gauntlet integration pass | Integration | Blocked (spikes R1, R4, net-emulation) | ADR-0008 |
| 047 | Ten-player real-network session and sign-off | Integration | Blocked (spikes R1, R4, net-emulation) | ADR-0008 |

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/procedural-city-generator.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
