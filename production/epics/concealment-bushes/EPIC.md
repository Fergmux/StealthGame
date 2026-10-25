# Epic: Concealment (Bushes)

> **Layer**: Feature
> **GDD**: design/gdd/concealment-bushes.md
> **Architecture Module**: Concealment in `StealthGameplay/Features` (per-pawn component, server record, bush hide volumes, client latch; pushes through `IConcealmentCullSource`)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories concealment-bushes`
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 12 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Concealment is the server-authoritative system that lets any pawn, human or NPC, slip into one of the city's 8-14 hideable bushes and vanish from every viewer who is not its ally. It owns the per-pawn component and server record (`Outside`/`Entering`/`Concealed`/`Exiting`/`Emerging`) keyed on the Level Socket Contract's `BushId`, the legality of `Hide(Enter|Exit)` registered as the IIF `CanBegin` predicate (no occupancy check), the timing of enter, exit and strike-from-bush transitions with owner-side warp prediction (S-CON-1), hide clips scheduled from the replicated `StartServerTime`, the commit and release of the concealed state and what breaks it, the spot-attack zone that lets a blind strike reach a concealed occupant (unbiased secret-stream spot pick in the `BushSpotPick` domain), and the server-only `IsConcealedFrom(Viewer, Pawn)`.

It does not decide how concealment is enforced on the wire: that is the Policy's hybrid (quantised server cull for non-allies plus a client-local hide), which this system implements by pushing `SetCullState` through `IConcealmentCullSource` and un-culling in the same server step. It adds no timer of any kind, builds hide volumes locally from the Manifest (they block non-hiding pawns), keeps a fail-closed client latch (`IsAllyBodyHidden`), keeps rustle parity so NPCs use the same path, and writes the concealment slice of the Event Log. For the player it is the one place where a body can leave the crowd on purpose.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0003: Replication, relevancy and the concealment cull hybrid | Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus a client-local hide, allowlist audit of every replicated property. | HIGH |
| ADR-0006: Seeds and deterministic/secret random streams | Public sub-seeds for layout vs SipHash-2-4 keyed server-secret streams; integer PCG32 streams so layout is bit-identical across machines. | LOW |
| ADR-0017: Concealment component and per-viewer presentation | Per-pawn component and server record, bush hide volumes, owner-side warp, fail-closed client latch, server-only IsConcealedFrom. | HIGH |
| ADR-0022: Rendering configuration and performance budgets | Lumen/Nanite/VSM configuration and CPU/GPU frame budgets for the RTX 3060-class 1080p60 target (GPU 14.6 ms). | HIGH |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

11 of 11 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-con-001 | One component per pawn; server-only record and state machine | ADR-0017 ✅ |
| TR-con-002 | Entry/exit legality through IIF predicates; no occupancy check | ADR-0017 ✅ |
| TR-con-003 | Hide volumes block non-hiding pawns | ADR-0017 ✅ |
| TR-con-004 | Owner-side warp for the enter transition (S-CON-1) | ADR-0017 ✅ |
| TR-con-005 | Hide clips scheduled from replicated server start time | ADR-0003, ADR-0017 ✅ |
| TR-con-006 | Fail-closed client latch; hide removes body, attachments, shadow, Lumen and RT | ADR-0017, ADR-0022 ✅ |
| TR-con-007 | `IsConcealedFrom` server-only; blind-strike spot zones | ADR-0017 ✅ |
| TR-con-008 | Server cull per the hybrid; un-cull in the same server step | ADR-0003, ADR-0017 ✅ |
| TR-con-009 | Rustle parity; NPCs through the same path | ADR-0017 ✅ |
| TR-con-010 | Concealment log in the Event Log | ADR-0017 ✅ |
| TR-con-011 | Unbiased secret-stream spot pick (`BushSpotPick` domain) | ADR-0017, ADR-0006 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** `Source/StealthGame/Private/StealthBush.cpp` / `Public/StealthBush.h` (101 + 48 lines: placeholder opaque spheres, known issue 8) and the concealment parts of `StealthCharacter.cpp` (`SetInBush`, `UpdateConcealmentVisibility`, `RevealFromConcealment`, replicated `bConcealed`/`bRevealedUntilExit`), which hide per viewer on the client only. The prototype's per-viewer behaviour (teammate sees the marker, enemy sees an empty bush, blind strike kills the hidden player, striking from a bush reveals until exit) passed networked tests E and F and is the behavioural reference; the replication of `bConcealed` to all clients is the leak the epic closes.

## Build Order and Gates

- **Depends on (epics)**: information-visibility-policy, shared-character-locomotion, input-interaction-framework, level-socket-contract, participant-state-roster, round-match-framework, crowd-npc-behaviour (hard for any external build), objectives-chain (hard peer for P4)
- **Gates and risks**: Conditional gate from ADR-0017: spikes S-CON-1 (owner-side root-motion warp to the anchor), R5, S9/S10 (cull timing). `FActionState.StartServerTime` and `FDeathState.bDiedConcealed` were approved 2026-10-25. Rendering and networking are both HIGH risk: the hide must remove mesh, attachments, shadow, Lumen and ray-tracing presence for non-allies (art bible R5).
- **Estimate**: about 8 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 4.8 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 33 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/concealment-bushes.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
