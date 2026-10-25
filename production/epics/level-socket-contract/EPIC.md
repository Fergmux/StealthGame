# Epic: Level Socket Contract

> **Layer**: Core
> **GDD**: design/gdd/level-socket-contract.md
> **Architecture Module**: Layout Service (LSC) in `StealthGameplay/Core` (`ULayoutService` world subsystem, provider registry, Graybox provider, client Manifest builder)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories level-socket-contract`
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
