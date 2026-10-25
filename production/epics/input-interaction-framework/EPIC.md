# Epic: Input & Interaction Framework

> **Layer**: Core
> **GDD**: design/gdd/input-interaction-framework.md
> **Architecture Module**: Pawn Command Interface (IIF) in `StealthGameplay/Core` (`UPawnCommandComponent`, action hub, Enhanced Input contexts)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories input-interaction-framework`
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 5 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

The Input & Interaction Framework is the pawn command interface: the single set of server-validated entry points (`Move`, `Look`, `Traverse`, `Interact(Begin|End)`, `Prepare(Begin|End)`, `Attack(TargetSpot|TargetPawn)`, `Hide(Enter|Exit)`) through which anything drives a Shared Pawn. A human's keyboard, mouse or gamepad reaches them through Enhanced Input on the `APlayerController`; an NPC's AI controller calls the very same entry points, so below the boundary no system can tell who issued a command and no `IsAI` parameter exists anywhere (the ADR-0001 parity boundary).

The epic delivers `UPawnCommandComponent` (non-replicated, no caller identity), two-stage server validation (ingress with per-pawn token buckets for AI and humans alike, then the command check at the queue slot against Roster legality class, locomotion state, the pawn's single action slot and the owning feature's `CanBegin` predicate), hold timers as queue items from server time with the 0.10 s release debounce, the single action slot priority (Attack > Prepare > Interact > Hide), the server-only `UActionHub` (`OnActionStarted/Cancelled/Completed`), exactly one owner reply per resolution-voided command through the batched `ClientCommandReplies` RPC, Active/Informant input contexts and the `IMC_Common` mappings (including `IA_Ping` and the MVP walk-slower `IA_WalkSlower` Axis1D: Ctrl 0.8, Alt 0.9, both 0.8, keyboard only), the `PossessionEpoch` token, the fixed-size replicated `FActionState` with one RepNotify, and the input latency budget (F5). This epic is the lead epic for MVP item 11 (keyboard walk-slower; SCL and Crowd carry their parts).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0002: Server resolution queue, server clock and round-end staging | One total-order queue keyed (ServerTickIndex, Seq) drained in TG_PostPhysics, due-time heap, typed staged writes, single-assignment latch, client server-time estimator. | MEDIUM |
| ADR-0003: Replication, relevancy and the concealment cull hybrid | Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus a client-local hide, allowlist audit of every replicated property. | HIGH |
| ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity | One AStealthPawn class on CMC, factory-only spawn, disconnect never destroys a pawn, stop-and-blend hand-over, possession-invariant net config. | HIGH |
| ADR-0011: Pawn command interface and input pipeline | Seven server-validated entry points with no caller identity, two-stage validation with token buckets, Enhanced Input edges, batched owner replies. | HIGH |
| ADR-0021: UI/HUD layering, CommonUI and Enhanced Input | UMG layer/ZOrder stack drawn after post-process, Common UI for menus only, client-local settings hooks, ping rings in HUD L0. | HIGH |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

13 of 13 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-iif-001 | One non-replicated command component per pawn with the seven entry points; no caller identity anywhere | ADR-0011 ✅ |
| TR-iif-002 | Human Move/Look through CMC saved moves; AI through the same entry points; one control rotation, body-yaw rule and pitch path | ADR-0010, ADR-0011 ✅ |
| TR-iif-003 | Discrete commands as Enhanced Input edges; the Hold trigger never carries server meaning | ADR-0011, ADR-0021 ✅ |
| TR-iif-004 | Two-stage server validation (ingress with token buckets; command check at the queue slot) | ADR-0011 ✅ |
| TR-iif-005 | Hold timers as queue items from server time; 0.10 s release debounce for every pawn | ADR-0002 ✅ |
| TR-iif-006 | One action slot per pawn, priority Attack > Prepare > Interact > Hide | ADR-0002 ✅ |
| TR-iif-007 | Exactly one owner reply per resolution-voided command; ingress rejections silent | ADR-0002, ADR-0011 ✅ |
| TR-iif-008 | Server-only action hub with `OnActionStarted/Cancelled/Completed` in one world-level hub | ADR-0011 ✅ |
| TR-iif-009 | Replicated fixed-size `FActionState` with one RepNotify; nothing else per action replicated except owner data | ADR-0003 ✅ |
| TR-iif-010 | Attack payload from the camera aim ray; no lock-on, no target-dependent presentation | ADR-0011 ✅ |
| TR-iif-011 | Input contexts swap on owner state (UX only); AI subject to the same token buckets | ADR-0011 ✅ |
| TR-iif-012 | Input latency budget (F5) met for human commands | ADR-0011 ✅ |
| TR-iif-013 | `PossessionEpoch` owner-only via `ClientSetPossessionEpoch`; stale epochs rejected at ingress | ADR-0011 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** the per-feature input and RPC code in `StealthCharacter.cpp`: `SetupPlayerInputComponent`, `OnAssassinatePressed`, `OnDefencePressed/Released`, `OnInteractPressed/Released`, and the Server RPCs `ServerAssassinate(Target, StrikePoint)`, `ServerSetDefence`, `ServerSetInteracting` (the prototype names a victim object and lets the client choose the hold state, with no token buckets, no queue slot, no debounce). `StealthPlayerController.*` (input mode, controller RPCs) and `StealthNpcController.*` (AI movement not going through the player path) are also superseded. Prototype key bindings (WASD, Shift run, Ctrl jog toggle, LMB/F attack, hold Q, hold E, T ping, V camera, C crouch) are the starting defaults to re-map under ADR-0011.

## Build Order and Gates

- **Depends on (epics)**: tuning-data-config, participant-state-roster, information-visibility-policy (`CanViewerKnow` for `PawnRef`), round-match-framework (queue and resolver registration), camera-system (`GetAimRay`), shared-character-locomotion (published state)
- **Gates and risks**: Conditional gate from ADR-0011: Engine Compatibility verification items V1-V6 and an `input-interaction-framework.md` re-review. Inherits the ADR-0010 spikes through the pawn. The walk-slower modifier collides with the sample's Ctrl jog toggle (first-playable controls) and Alt has OS and PIE side effects (rebaseline Risk Factors): remap the jog toggle at implementation. The producer review flagged the days-per-AC ratio (0.10) as low: re-verify the AC type mix before story breakdown.
- **Estimate**: about 8 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 4.8 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 80 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/input-interaction-framework.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
