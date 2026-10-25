# Epic: Assassination & Defence Reversal

> **Layer**: Feature
> **GDD**: design/gdd/assassination-defence-reversal.md
> **Architecture Module**: Assassination in `StealthGameplay/Features` (attack resolver, `UEncounterService`, server-only defence component; wire structs in Foundation `Public/View`)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories assassination-defence-reversal`
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 13 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Assassination & Defence Reversal gives the `Prepare` and `Attack` commands their meaning. It owns victim resolution on server capsules (which pawn, if any, an `Attack(TargetPawn | TargetSpot)` reaches, including blind strikes on concealed pawns), the one-strike outcome (kill, reversal or whiff) decided at the Attack's queue slot with live-state re-check, the defence (the protected window of a held `Prepare`, its 60 s cooldown from activation, the counter after a reversal) sampled by key compare, the attacker lockout, the strike, counter and death presentation contract (clip timings, warp limits, body lifetime), the server-only per-pawn defence state, and the owner-only kill confirmation (`FStrikeResult`/`FDefenceResult`, delivered as entries of `ClientCommandReplies`).

Pillar-1 critical rules: the `FActionState` written at the slot is outcome-blind; outcome writes (`DeathWrite`, `CounterPose`) occur at contact time through the pending-write queue and the registered applier; every outcome and victim kind gets a uniform forced net update and one body sequence with possession kept through the fall; nothing outcome-caused is presented before contact; there are no replicated effect actors and no multicast; and clients present contact-timed outcomes from one RTT-compensated estimate of server time with a stated accuracy budget. `OnPawnKilled(Victim, Killer, VictimKind, Cause)` is emitted inside the slot for Penalty and telemetry.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0002: Server resolution queue, server clock and round-end staging | One total-order queue keyed (ServerTickIndex, Seq) drained in TG_PostPhysics, due-time heap, typed staged writes, single-assignment latch, client server-time estimator. | MEDIUM |
| ADR-0003: Replication, relevancy and the concealment cull hybrid | Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus a client-local hide, allowlist audit of every replicated property. | HIGH |
| ADR-0011: Pawn command interface and input pipeline | Seven server-validated entry points with no caller identity, two-stage validation with token buckets, Enhanced Input edges, batched owner replies. | HIGH |
| ADR-0013: Animation: motion matching and significance budget | One AnimBP and PoseSearch set, tiers by screen height only, project-owned external tick-rate control (no URO, budget allocator or SignificanceManager). | HIGH |
| ADR-0018: Encounter data and outcome presentation timing | Outcome-blind FActionState at the queue slot, contact-time pending writes, owner-only results, MotionWarping (Beta in 5.8.4). | HIGH |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

11 of 11 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-asn-001 | Attack resolver re-checks live state at its slot; victim resolution on server capsules | ADR-0002 ✅ |
| TR-asn-002 | Defence sampled at Attack execution by key compare; server-only defence component | ADR-0002 ✅ |
| TR-asn-003 | Outcome-blind `FActionState` at the slot; outcome writes at contact via the pending-write queue | ADR-0003, ADR-0018 ✅ |
| TR-asn-004 | Uniform forced net updates for every outcome and victim kind | ADR-0003 ✅ |
| TR-asn-005 | Owner-only `FStrikeResult`/`FDefenceResult`; idempotent state-derived presentation | ADR-0018 ✅ |
| TR-asn-006 | `OnPawnKilled` emitted inside the slot | ADR-0002 ✅ |
| TR-asn-007 | One body sequence for every victim; possession kept through the fall | ADR-0013, ADR-0018 ✅ |
| TR-asn-008 | Nothing outcome-caused presented before contact (server clock scheduling) | ADR-0018 ✅ |
| TR-asn-009 | No replicated effect actors, no multicast | ADR-0003 ✅ |
| TR-asn-010 | Clients present contact-timed outcomes from one RTT-compensated estimate of server time with a stated accuracy budget (ASN-R21/R23, HUD-R6; added after TD-ARCHITECTURE concern 3) | ADR-0002 ✅ |
| TR-asn-011 | Owner results as entries of the batched `ClientCommandReplies` RPC | ADR-0011, ADR-0018 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Promotes (as reference logic and tests) / replaces:** `Source/StealthGame/Private/StealthRules.cpp` / `Public/StealthRules.h` (`FStealthRules::ResolveAssassination`, `CanStartDefence`, `DefenceCooldownRemaining`, `ShouldEndDefence`, `IsRunningSpeed`; pure engine-independent functions with six passing tests in `StealthRulesTests.cpp`) are the proven behavioural spec for the logic stories and must be re-expressed under the new queue slot, not copied. **Replaces** the strike and defence code in `StealthCharacter.cpp` (`KillCharacter`, `StartDefence`/`StopDefence`, `SetDefenceStance`, `ApplyDeathPresentation`: ragdoll plus a one-second attacker hold, the crouch-based brace, known issues 5 and 6) and `StealthGameMode::HandleAssassinationRequest`/`HandleDefenceRequest`. Networked prototype tests A, C and F (reversal eliminates the attacker, blind strike kills a hidden enemy) are the acceptance reference.

## Build Order and Gates

- **Depends on (epics)**: participant-state-roster, input-interaction-framework, shared-character-locomotion, concealment-bushes, information-visibility-policy, round-match-framework (queue slot, pending writes), tuning-data-config; camera-system (soft)
- **Gates and risks**: ADR-0018 is conditional through its dependencies (spikes S1, S2, S10, S14) and amendment requests to owning ADRs. MotionWarping is Beta in 5.8.4 (the protected `WarpTargets` disable is on the engine-upgrade checklist). Networking and animation are HIGH risk. Simultaneous-event resolution and the defence not becoming a reflex parry are the design risks (systems-index High-Risk table); the mitigation is Round's single resolution queue and prepare as a held state with no attacker warning.
- **Estimate**: about 12 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 7.2 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 49 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/assassination-defence-reversal.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
