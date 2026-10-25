# Epic: Information Visibility Policy

> **Layer**: Foundation
> **GDD**: design/gdd/information-visibility-policy.md
> **Architecture Module**: Visibility Policy in `StealthFoundation` (net identity boundary: `CanViewerKnow`, carriers, allowlist, cull hook) and owner of the `StealthGame.Visibility.*` leak suite
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories information-visibility-policy`
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 4 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

The Information Visibility Policy is the one server-side rule set that decides who may know what. It exposes `CanViewerKnow(Viewer, Subject, Fact)` (default deny; seven viewer classes against every fact), assigns exactly one replication audience (`ServerOnly`, `Owner`, `Team`, `All`) and one named carrier to every field, lists every Unreal default that would broadcast a player/NPC difference (`APawn::PlayerState`, the player array, `CompressedPing`, `bIsInactive`, `Owner`/`Controller`, relevancy, spectator state) and overrides it, and decides how bush concealment is enforced on the wire (the ADR-0003 hybrid: quantised server cull for non-allies plus a client-local hide).

The epic builds the query and fact table, the single replicated-property allowlist file with its reflection and net-config audits (defaults and live instances), the relevancy/cull hook (`ShouldCullFor(Pawn, Connection)` with `IsNetRelevantFor` override and explicit channel close for the legacy driver), the `IConcealmentCullSource` push API, the teammate position fallback feed, the include boundary that keeps server query headers out of the presentation module, and the leak catalogue with its CRITICAL ship gate and per-commit canaries. Players never see it; they experience the guarantee that nothing on screen, in sound or in the packets says that one is a person unless that person chose to show it. This is Pillar 1's enforcement epic.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0002: Server resolution queue, server clock and round-end staging | One total-order queue keyed (ServerTickIndex, Seq) drained in TG_PostPhysics, due-time heap, typed staged writes, single-assignment latch, client server-time estimator. | MEDIUM |
| ADR-0003: Replication, relevancy and the concealment cull hybrid | Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus a client-local hide, allowlist audit of every replicated property. | HIGH |
| ADR-0007: Code module layout, composition root and include boundaries | Four runtime modules (StealthFoundation, StealthGameplay, StealthPresentation, StealthGame) plus Editor and StealthTestHarness; compile guard, facade split, consumer-owned interfaces. | MEDIUM |
| ADR-0008: Test and automation strategy for leak and parity tests | Headless pure-C++ cores, multi-client and multi-process leak harness, raw-bunch capture, net-emulation profiles, per-commit canaries. | MEDIUM |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

12 of 12 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-ivp-001 | `CanViewerKnow` server query, default deny, seven viewer classes | ADR-0001 ✅ |
| TR-ivp-002 | One carrier kind per audience | ADR-0001 ✅ |
| TR-ivp-003 | UE default overrides of R5 (pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator) | ADR-0001 ✅ |
| TR-ivp-004 | Concealment hybrid: quantised server cull for non-allies + client presentation hide | ADR-0003 ✅ |
| TR-ivp-005 | Relevancy varies by concealment and relation only | ADR-0003 ✅ |
| TR-ivp-006 | Single allowlist file, reflection audit and net-config audit on defaults and live instances | ADR-0003, ADR-0008 ✅ |
| TR-ivp-007 | Leak catalogue with CRITICAL ship gate; per-commit canaries | ADR-0003, ADR-0008 ✅ |
| TR-ivp-008 | Engine spectator state banned; client-local no-pawn view | ADR-0001 ✅ |
| TR-ivp-009 | Teammate fallback position feed on C-TeamView | ADR-0003 ✅ |
| TR-ivp-010 | Iris rerun gate if Iris is enabled | ADR-0003, ADR-0008 ✅ |
| TR-ivp-011 | Presentation modules cannot include server query headers | ADR-0001, ADR-0007 ✅ |
| TR-ivp-012 | Owner-only clock-exchange RPC pair, timestamps only | ADR-0002 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** the prototype's replication of identity: `APawn::PlayerState` and `bConcealed` replicate to all clients, the pawn carries a replicated team field used by `AStealthCharacter::IsTeammateOfLocalViewer()` for markers, and concealment is a client-side visibility toggle (`UpdateConcealmentVisibility`). The first-playable report records this as known issue 2 (a modified client could tell players from NPCs and see hidden players). None of it is promoted; the allowlist, carriers and cull hook are new.
**Reference only:** `Spikes/StealthIdentitySpike.*` (throwaway S1/S2/S14 code, `stealth.Spike.IdentityTrims` console variable).

## Build Order and Gates

- **Depends on (epics)**: participant-state-roster, session-travel; round-match-framework supplies the phase through an injected provider; concealment-bushes pushes cull state through `IConcealmentCullSource`
- **Gates and risks**: Conditional gates: ADR-0001 spikes S1, S2, S14; ADR-0003 spikes S9, S10, S12, S13 (relevancy override and channel close under the legacy NetDriver). Systems-index High-Risk system; the mitigation is ADR-0001 first, one server-side query, and automated tests asserting a non-team client cannot read player-ness. The Iris rerun gate (TR-ivp-010) applies only if Iris is enabled. This is the long pole of the Foundation layer (about 10 d); risk-first Core epics (graybox map, mannequin pool, camera, traversal) do not need it and may start after the bootstrap, TDC, Roster and Session.
- **Estimate**: about 10 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 6.0 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 46 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/information-visibility-policy.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
