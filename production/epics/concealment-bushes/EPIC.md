# Epic: Concealment (Bushes)

> **Layer**: Feature
> **GDD**: design/gdd/concealment-bushes.md
> **Architecture Module**: Concealment in `StealthGameplay/Features` (per-pawn component, server record, bush hide volumes, client latch; pushes through `IConcealmentCullSource`)
> **Status**: Ready
> **Stories**: 35 stories (see Stories table below)
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

## Stories

Stories embed Control Manifest version 2026-10-25f (the manifest header moved from 2026-10-25e after the epic was written). Three spike gates condition the Blocked stories: S-CON-1 (story 010), R5 (story 021) and S9/S10 (Information Visibility Policy stories 004 and 005, S12 story 006).

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Concealment tuning keys are registered and the F3 timing chain is validated at load | Logic | Complete | ADR-0017 |
| 002 | Every machine builds the same bush runtime from the Manifest with zero replicated actors | Integration | Complete | ADR-0017 |
| 003 | The per-pawn Concealment state machine and occupancy record exist as headless core | Logic | Complete | ADR-0017 |
| 004 | Hide(Enter) executes in its slot, commits at the swap and a failed guard exits ordinarily | Logic | Complete | ADR-0017 |
| 005 | Hide(Exit) picks an open, unblocked side, is never refused and un-culls in the same step | Logic | Complete | ADR-0017 |
| 006 | Ended and Reset leave concealment consistent and no dwell timer exists | Logic | Complete | ADR-0017 |
| 007 | CanBegin for HideEnter and HideExit gives the same reply for every pawn kind and occupancy | Logic | Complete | ADR-0017 |
| 008 | Collision is a pure function of state and elapsed time, re-derived after correction replay | Logic | Complete | ADR-0017 |
| 009 | Concealed and transitioning pawns are invisible to NPC perception and absent from steering | Integration | Complete | ADR-0017 |
| 010 | Spike S-CON-1: choose the owner-side warp mechanism that meets AC-CON-31 | Integration | Complete | ADR-0017 |
| 011 | Every pawn is warped to the anchor on enter and root-moved out on exit by the same server path | Integration | Complete | ADR-0017 |
| 012 | The owner's hide and exit show no snap and start moving within 50 ms | Integration | Complete | ADR-0017 |
| 013 | Inside a bush the body is still, the view is clamped and Prepare and Interact are refused | Visual/Feel | Complete | ADR-0017 |
| 014 | An Attack from inside breaks concealment, un-culls in the same step and ends outside | Logic | Complete | ADR-0017 |
| 015 | A strike point targets a bush within 75 cm and GetSpotOccupants returns the non-ally record | Logic | Complete | ADR-0017 |
| 016 | The victim among reachable bush occupants is an unbiased draw from the BushSpotPick stream | Logic | Complete | ADR-0017, ADR-0006 |
| 017 | Killing a concealed pawn leaves the record at the slot and re-opens only at the death write | Logic | Complete | ADR-0017 |
| 018 | A pawn struck while Entering dies as a visible pawn before the commit | Logic | Complete | ADR-0017 |
| 019 | EvaluateBodyHidden is a pure, fail-closed, state-derived function | Logic | Complete | ADR-0017 |
| 020 | A fresh or late-arriving pawn actor starts hidden and exposes IsAllyBodyHidden | Integration | Complete | ADR-0017, ADR-0003 |
| 021 | Spike R5: a hidden pawn leaves no shadow, VSM, Lumen, reflection or ray-traced trace | Visual/Feel | Complete | ADR-0017, ADR-0022 |
| 022 | A latched pawn drops every render proxy, the aim channel response and audio on clients and host | Integration | Complete | ADR-0017, ADR-0022 |
| 023 | Commit and crossing set and clear the owner and ally concealed flags through their only writers | Integration | Complete | ADR-0017 |
| 024 | IsConcealedFrom matches a checked-in truth table and is server-only | Logic | Complete | ADR-0017 |
| 025 | Commit, exit, break and kill drive the IVP cull through IConcealmentCullSource | Integration | Complete | ADR-0017, ADR-0003 |
| 026 | Opponents get no trace of a concealed pawn; co-occupant rivals blind; informants no x-ray | Integration | Complete | ADR-0017, ADR-0003 |
| 027 | On exit and bush-emerge the body is never drawn before the leaf crossing | Integration | Ready | ADR-0017, ADR-0003 |
| 028 | At the swap server time the body vanishes on the same frame for opponent and ally clients | Visual/Feel | Ready | ADR-0017 |
| 029 | Every enter, exit and emerge plays one identical rustle; an occupied bush plays nothing | Visual/Feel | Ready | ADR-0017 |
| 030 | The concealment log goes to the Event Log only and feeds the F1, F2 and metric calculators | Logic | Ready | ADR-0017 |
| 031 | Logged internal matches satisfy the parity, hiding-is-a-pause and ambush metrics | Integration | Blocked (needs >= 10 logged matches) | ADR-0017 |
| 032 | A disconnect, grace expiry or rejoin never cancels or exposes a concealed pawn instantly | Integration | Ready | ADR-0017, ADR-0003 |
| 033 | Blind strikes look identical to the attacker; killed-concealed body never drawn on non-owner | Integration | Ready | ADR-0017, ADR-0018 |
| 034 | The R5 leak matrix passes on the production hide for every LOD and lighting cell | Visual/Feel | Ready | ADR-0017, ADR-0022 |
| 035 | The prototype concealment code is retired and the proven networked behaviours still hold | Integration | Blocked (stories 028, 033, 034) | ADR-0017, ADR-0003 |

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/concealment-bushes.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
