# Epic: Information Visibility Policy

> **Layer**: Foundation
> **GDD**: design/gdd/information-visibility-policy.md
> **Architecture Module**: Visibility Policy in `StealthFoundation` (net identity boundary: `CanViewerKnow`, carriers, allowlist, cull hook) and owner of the `StealthGame.Visibility.*` leak suite
> **Status**: Ready
> **Stories**: 55 stories (24 Ready, 31 Blocked); see the Stories table below
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

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Spike S1, multi-process raw-bunch capture shows no identity pointers to non-owners | Integration | Ready | ADR-0001 |
| 002 | Spike S2, PlayerState trims hold across seamless travel and rejoin | Integration | Blocked | ADR-0001 |
| 003 | Spike S14 and S1, rejoin and PawnRecovery possession stay invisible to non-owners | Integration | Blocked | ADR-0001 |
| 004 | Spike S9, the explicit channel close lands at the deadline and the channel stays closed | Integration | Ready | ADR-0003 |
| 005 | Spike S10, a re-opened pawn is a new actor that presents correctly from replicated state | Integration | Ready | ADR-0003 |
| 006 | Spike S12, the cull delay and un-cull lead are recomputed from measured L99 | Integration | Ready | ADR-0003 |
| 007 | Spike S13, CMC cadence and corrections match for PC- and AI-possessed pawns | Integration | Ready | ADR-0003 |
| 008 | Spike, fixed-width NetSerialize structs are sent with identical bit sizes on the legacy driver | Integration | Ready | ADR-0003 |
| 009 | Viewer classes follow the R2 order and class invariants | Logic | Ready | ADR-0001 |
| 010 | CanViewerKnow denies by default and reports every fault once | Logic | Ready | ADR-0001 |
| 011 | Every fact answers exactly as the R3 matrix says for every viewer class | Logic | Ready | ADR-0001 |
| 012 | Answers about an unbound subject never depend on who asks | Logic | Ready | ADR-0001 |
| 013 | The replicated-property allowlist file holds every required row and nothing forbidden | Logic | Ready | ADR-0003 |
| 014 | Every audience row sits on its one carrier kind | Logic | Ready | ADR-0001 |
| 015 | The replicated-property audit fails the build on any property missing from the allowlist or with a different condition | Logic | Ready | ADR-0003 |
| 016 | The engine spectator state is banned: the grep finds no banned symbol and every banned call is a faulting no-op | Integration | Ready | ADR-0001 |
| 017 | Every pawn spawn path sets Instigator to Self and no replicated spawned actor carries a replicated Owner or Instigator | Integration | Blocked | ADR-0001 |
| 018 | Effective replication settings override the engine defaults for player and NPC pawns alike (provisional on S1, S13, S14) | Integration | Blocked | ADR-0001 |
| 019 | The per-commit possession canary shows no PlayerState, Controller or Owner in any non-owner changelist | Integration | Ready | ADR-0001 |
| 020 | The in-round player array stays frozen through disconnect, rejoin, elimination and informant conversion | Integration | Blocked | ADR-0001 |
| 021 | No opponent connection ever receives a C-TeamView entry, PawnRef, IsConcealed flag or fallback position of the other team | Integration | Blocked | ADR-0001 |
| 022 | Public structs are global only: no field is keyed by team or participant and none varies with opponent counts or objective progress | Logic | Blocked | ADR-0001 |
| 023 | The cull predicate equals owner OR ally OR NOT concealed across all 72 combinations (provisional on S9) | Logic | Blocked | ADR-0003 |
| 024 | A culled concealed player pawn has no actor on any non-ally client and stays relevant to its owner and allies (provisional on S9) | Integration | Blocked | ADR-0003 |
| 025 | Each cull lands at the first global grid point at or after t0 + 250 ms, with withheld acks shifting nothing and early exit cancelling it (provisional on S9, S12) | Integration | Blocked | ADR-0003 |
| 026 | The cull delay knob is computed by F3 and validated to 100-1000 ms, a multiple of 50, and at least L99 + 1/f_net | Logic | Ready | ADR-0003 |
| 027 | On a BushEnter swap the body and its shadow, reflection, Lumen and decals hide on the same rendered frame on ally and opponent clients | Visual/Feel | Blocked | ADR-0003 |
| 028 | An attack on a culled pawn resolves against capsules identically by spot or id, and the victim re-opens only at the death write | Integration | Blocked | ADR-0003 |
| 029 | The un-culled pawn exists on the non-ally client before the exit-transition's outward leaf-plane instant | Integration | Blocked | ADR-0003 |
| 030 | A concealed player who disconnects stays culled and becomes culled for former allies at one boundary | Integration | Blocked | ADR-0003 |
| 031 | A player-only cull never ships: bConcealServerCull true requires NPC bush use and a passing AC-IVP-25 run | Integration | Blocked | ADR-0003 |
| 032 | The teammate fallback position feed costs 13 B per entry and at most 104 B/s per client | Logic | Ready | ADR-0003 |
| 033 | The teammate marker tracks a concealed or unresolved ally and opponents create no marker | Integration | Blocked | ADR-0003 |
| 034 | Presentation modules include no server query headers and call no identity-deriving APIs | Logic | Ready | ADR-0007 |
| 035 | A Shipping listen-server build exposes no server data through commands or overlays and writes the Event Log only at Ended or Reset | Integration | Blocked | ADR-0007 |
| 036 | Every viewer becomes PostRound in the round-end step before any Ended data is sent | Integration | Blocked | ADR-0002 |
| 037 | Informant takeover never selects a concealed or transitioning pawn | Logic | Blocked | ADR-0001 |
| 038 | Kill confirmation reaches only the attacker and the fall, removal and replacement sequence is identical for NPC, Active and informant victims | Integration | Blocked | ADR-0001 |
| 039 | HidePosteriorFactor outside 1.0-1.5 fails the harness before AC-IVP-25 runs | Logic | Ready | ADR-0008 |
| 040 | NPCs enter bushes at least 0.633 as often as players, through the same Concealment call site | Integration | Blocked | ADR-0008 |
| 041 | Crowd's Tell Matrix carries the bush enter/leave red-herring row tied to F5 at k = 1.5 | Config/Data | Ready | ADR-0008 |
| 042 | Rejection reply, ActionPose and kill-confirmation delays are statistically identical for player and NPC targets | Integration | Blocked | ADR-0008 |
| 043 | Transport classifier AUC stays at chance for the player and AI paths | Integration | Blocked | ADR-0008 |
| 044 | Raw-bunch schema parity holds, structural leak count S = 0 | Integration | Blocked | ADR-0008 |
| 045 | Human strip test hits stay below h_fail | Visual/Feel | Blocked | ADR-0008 |
| 046 | The Iris rerun gate blocks an Iris build that lacks recorded Iris passes | Logic | Ready | ADR-0008 |
| 047 | Every leak catalogue row L1-L34 has an owner, a status and the right citation | Config/Data | Ready | ADR-0008 |
| 048 | A playtest session record must name a non-competing host or exclude the host team's rows | Logic | Ready | ADR-0008 |
| 049 | No external build ships while a CRITICAL leak row is Spk or Req | Logic | Ready | ADR-0008 |
| 050 | The rival progress signal has the R3 shape and a shared release grid | Logic | Blocked | ADR-0001 |
| 051 | Interact ingress is identical at live sites, non-site courts, rival sites and decoys | Integration | Blocked | ADR-0001 |
| 052 | A strike's target id gives no oracle, every id class resolves like a spot attack | Integration | Blocked | ADR-0003 |
| 053 | Pings, reconnect state and the round summary reach only their allowed audience | Integration | Blocked | ADR-0001 |
| 054 | Pawn net config is equal across NPC, remote, host and just-taken-over pawns | Integration | Blocked | ADR-0003 |
| 055 | The clock exchange RPCs are owner-only, timestamps-only and under 50 B/s | Integration | Blocked | ADR-0002 |

Blocked stories name their blocker in the story header: an unreported spike (S1 multi-process, S2 travel and rejoin, S14 rejoin and PawnRecovery, S9, S10, S12, S13) or an epic that must land first. Spike stories 001-008 are the gates; stories 002 and 003 wait for the session-travel, participant-state-roster and shared-character-locomotion epics. Granularity is `fine`: one acceptance criterion per story (46 GDD criteria, one derived criterion for TR-ivp-012, eight spike criteria).
