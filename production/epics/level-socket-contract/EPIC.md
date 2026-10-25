# Epic: Level Socket Contract

> **Layer**: Core
> **GDD**: design/gdd/level-socket-contract.md
> **Architecture Module**: Layout Service (LSC) in `StealthGameplay/Core` (`ULayoutService` world subsystem, provider registry, Graybox provider, client Manifest builder)
> **Status**: Ready
> **Stories**: 45 stories (see the Stories table below; 42 Ready, 3 Blocked)
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 8 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

The Level Socket Contract defines what any playable level (the MVP graybox, later the Procedural City Generator) must provide and prove before a round starts. This epic builds the layout service world subsystem with a registered-provider interface (`ILayoutProvider`, implemented by the Graybox provider here and the City Generator later); the socket vocabulary (emergence sockets, start cells and candidate zones, objective courts resolving to `ObjectivePrelimSite(Team, Stage)`/`ObjectiveFinalSite`, `BushSite`, `CrowdRoute`, `TraversalPoint`, `NavArea`); the integer Layout Manifest and `LayoutHash` that clients recompute with no RNG; server navmesh build, the V-* validation predicates and the server-secret Selection (`FSiteAssignment`, `TeamSpawn`) within attempt and total budgets in at most 33 ms slices; reseed and graybox fallback travel; shared emergence sockets and `PickRespawnPawn` for unseen crowd pawns; round-start placement and the spawn gate; the Pillar-1 placement constraints (no area type that distinguishes spawns or objectives); and the server-only query API Round, Objectives and Concealment use.

The service owns zero replicated actors (bush and court components are built locally from the Manifest) and Round talks only to the service, never to a provider. It is deliberately small (effort S): it gives a contract an owner and keeps the Generator off the critical path.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0003: Replication, relevancy and the concealment cull hybrid | Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus a client-local hide, allowlist audit of every replicated property. | HIGH |
| ADR-0014: Layout service, seeded generation and runtime navmesh | Layout-service world subsystem with provider registry, integer Layout Manifest and LayoutHash, server navmesh build and validation, reseed and graybox fallback. | MEDIUM |
| ADR-0019: Informant lifecycle and crowd-pawn takeover | Respawn as a due-time-heap item, takeover of an unseen crowd pawn in one call stack, owner-only tally and T_due on the C-TeamView owner part. | MEDIUM |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

9 of 9 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-lsc-001 | Layout service world subsystem with registered providers; Round talks only to it | ADR-0014 ✅ |
| TR-lsc-002 | Integer Layout Manifest; clients build with no RNG and recompute `LayoutHash` | ADR-0014 ✅ |
| TR-lsc-003 | Zero replicated actors; bush/court components built locally | ADR-0003, ADR-0014 ✅ |
| TR-lsc-004 | Server nav build, Validate and Select within attempt/total budgets; ≤ 33 ms slices | ADR-0014 ✅ |
| TR-lsc-005 | Reseed and graybox fallback travel | ADR-0014 ✅ |
| TR-lsc-006 | Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn | ADR-0014, ADR-0019 ✅ |
| TR-lsc-007 | Round-start placement and spawn gate | ADR-0014 ✅ |
| TR-lsc-008 | Pillar-1 placement constraints (no area type for spawns/objectives) | ADR-0014 ✅ |
| TR-lsc-009 | Server-only query API for site assignment and team spawns | ADR-0014 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** the prototype's implicit level contract: team starts, objectives, bushes, crowd centre and nav bounds are hand-placed in `/Game/StealthGame/Maps/L_Prototype` (a copy of the sample map), `StealthGameMode::FindCrowdPoint`/`SpawnCrowdWhenNavReady` find crowd spawn points and wait for the navmesh, and `StealthCityGenerator` moves objectives and bushes to seeded sites. None of this is promoted; the Graybox provider is built fresh from the Manifest. The prototype map (which still carries the sample's traversal course to the north, known issue 9) is the starting point for the hand-built graybox, and editing it is a Content step outside this epic definition.

## Build Order and Gates

- **Depends on (epics)**: tuning-data-config, session-travel (seed), participant-state-roster (soft); `ITakeoverEligibilitySource` implemented later by crowd-npc-behaviour; procedural-city-generator registers as a second provider
- **Gates and risks**: ADR-0014 is unconditional. Navigation is MEDIUM risk (Recast runtime build, `FindPathSync`, `ProjectPointToNavigation`); the navmesh-build and Manifest-wire-size spikes (art bible R4, LSC Open Question 2) matter mainly to the generator epic. Build order is risk-first: the hand-built graybox map and its provider come first and remain the permanent fallback. The producer review flagged the days-per-AC ratio (0.09) as low: re-verify the AC type mix before story breakdown.
- **Estimate**: about 4 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 2.4 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 44 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/level-socket-contract.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done

## Stories

Created by `/create-stories` on 2026-10-25 at `story_granularity: balanced` (2-4 acceptance criteria per story). Build order follows ADR-0014's Migration Plan (service core, Manifest, navmesh, predicates and Selection, graybox, runtime services) with the two conditioning spikes first. QL-STORY-READY ran (qa-lead, two passes): 44 of 45 stories ADEQUATE after criteria revisions, story 037 GAPS (accepted, blocked).

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Layout service skeleton, provider registry and Round isolation | Logic | Ready | ADR-0014 |
| 002 | Layout service state machine and time-sliced work driver | Logic | Ready | ADR-0014 |
| 003 | Attempt loop: timeout, total budget and reseed | Logic | Ready | ADR-0014 |
| 004 | Abort paths and Event Log header | Logic | Ready | ADR-0014 |
| 005 | Fallback travel to the graybox and host-loss abort | Integration | Ready | ADR-0014 |
| 006 | Formulas D1, D2, D3 and D4 pass their test vectors | Logic | Ready | ADR-0014 |
| 007 | Formulas D5, D6, D6b and D9 pass their test vectors | Logic | Ready | ADR-0014 |
| 008 | Layout Manifest struct and LayoutHash (D7) | Logic | Ready | ADR-0014 |
| 009 | Chunked Manifest replication, idempotent handler and late join | Integration | Ready | ADR-0014, ADR-0003 |
| 010 | Spike: Manifest wire size and behaviour under loss | Integration | Ready | ADR-0014 |
| 011 | Spike R4: graybox runtime navmesh timing and freeze counters | Integration | Ready | ADR-0014 |
| 012 | Navmesh lock lifecycle and freeze | Integration | Blocked (spike R4, story 011) | ADR-0014 |
| 013 | Client geometry builder, inert decoration and zero replicated actors | Logic | Ready | ADR-0014 |
| 014 | Socket records, determinism and required data | Logic | Ready | ADR-0014 |
| 015 | World measure backend: navmesh paths, nav-grid bound and ViewBlock traces | Integration | Blocked (spike R4, story 011) | ADR-0014 |
| 016 | Predicates V-EMERGE and alley shape (witness sets) | Logic | Ready | ADR-0014 |
| 017 | Predicates V-NAV, V-AREA, V-SIGHT, V-ROUTE and V-TRAV | Logic | Ready | ADR-0014 |
| 018 | Predicates V-SEP, V-REACH, V-RESPAWN, V-ZONE and V-ZONEFILL | Logic | Ready | ADR-0014 |
| 019 | Predicates V-COURT, V-POOL, V-BUSH and V-CLUE with the court ClueTag | Logic | Ready | ADR-0014 |
| 020 | CrowdRoute graph with pre-baked path polylines | Logic | Ready | ADR-0014 |
| 021 | Selection: combos, V-FAIR and the single final (with D8) | Logic | Ready | ADR-0014 |
| 022 | Selection draw: secret streams and the two-stage uniform draw | Logic | Ready | ADR-0014 |
| 023 | Server-only query API and leak blindness | Integration | Ready | ADR-0014, ADR-0003 |
| 024 | Preparing handshake and hash acknowledgement | Integration | Ready | ADR-0014 |
| 025 | Spawn gate and no world view before Active | Integration | Ready | ADR-0014 |
| 026 | Graybox provider, editor-only markers and the opaque bake | Logic | Ready | ADR-0014 |
| 027 | Graybox Validate button, commandlet parity and CI gate | Logic | Ready | ADR-0014 |
| 028 | Graybox map authoring and Graybox.Validates | Config/Data | Ready | ADR-0014 |
| 029 | Round-start placement: one function, zone fill and density cap | Logic | Ready | ADR-0014 |
| 030 | Emergence: kind-blind request, clear check and uniform pick | Logic | Ready | ADR-0014 |
| 031 | Emergence: kill decoupling and exit-cancel rate bound | Integration | Ready | ADR-0014 |
| 032 | Emergence presentation: no visible pop | Visual/Feel | Ready | ADR-0014 |
| 033 | PickRespawnPawn: eligibility, unseen test and uniform draw | Logic | Ready | ADR-0019, ADR-0014 |
| 034 | PickRespawnPawn: starvation widening and region membership | Logic | Ready | ADR-0019, ADR-0014 |
| 035 | Informant respawn takeover: integration | Integration | Ready | ADR-0019, ADR-0014 |
| 036 | Crowd and Objectives contract tests | Integration | Ready | ADR-0014 |
| 037 | BushSite derivation and server/client parity | Integration | Blocked (design gap: graybox BushSite source undefined) | ADR-0014 |
| 038 | Batch audits: bush and emergence uncorrelated with roles | Logic | Ready | ADR-0014 |
| 039 | Packaged-client leak scan: markers and role strings | Integration | Ready | ADR-0014 |
| 040 | Graybox sun azimuth lock and cornice shade bands (OI-4) | Visual/Feel | Ready | ADR-0014 (V-POOL) |
| 041 | Graybox per-area shade ratios by grey card (OI-6) | Visual/Feel | Ready | N/A (art bible 4.5) |
| 042 | Bush and planting value band: leg-and-forearm read test (OI-2) | Visual/Feel | Ready | N/A (art bible 3.6, 4.1) |
| 043 | Bush greyscale and colour-swatch identification tests (VA-4) | Visual/Feel | Ready | N/A (human test) |
| 044 | Static-figure, sun-pool beacon and paired-capture tests | Visual/Feel | Ready | N/A (human test) |
| 045 | Graybox fairness playtest (per-zone win interval) | Integration | Ready | ADR-0014 |

### Notes from story creation (for the GDD and epic owners)

- **TR-lsc-010 is missing from this epic's requirements table** (the registry has 10 entries; the table lists 9). Story 019 covers it (court `ClueTag`, V-CLUE).
- **Art-bible items assigned here:** OI-4 (story 040), OI-6 (story 041) and OI-2 (story 042) are art-bible "graybox" checks and the graybox map is delivered by this epic (story 028); GDD AC-45..49 are the VA-4/VA-3/VA-1/VA-5 human tests (stories 043, 044). Stories 040-044 have no registry TR (`TR-lsc-???`).
- **Spikes first:** stories 010 (Manifest wire size) and 011 (navmesh R4) condition stories 012 and 015 (011) and the Generator epic (010). Story 012 and 015 stay Blocked until story 011 reports.
- **Design gap blocking story 037:** GDD R2.7 gives the graybox an empty Manifest, but R2.6 and ADR-0014 derive `BushSite` records from the Manifest; where graybox bush sites reach clients is undefined.
- **GDD defects found by the QA review** (to fix in the GDD): AC-30 D4 row uses T = 12 (Formulas section says T = 9, S = 0.225); AC-5 ack tuple and `OnLayoutReady` order disagree with R4.4/R4.5; AC-13 asks for at least 2,000 cancellations, unreachable at the 25% cap (story 031 uses at least 300); AC-55's sample size gives a fair zone only about 66% chance to pass (story 045 uses a two-tier rule); AC-47's "fails at 3 or more" wording; R8.5 vs AC-44 sub-seed list.
- **Cross-epic prerequisite:** the ADR-0007 module skeleton (`production/epics/tuning-data-config/EPIC.md`); until it lands, code and tests go under `Source/StealthGame/`.
