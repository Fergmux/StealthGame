# Epic: Session & Travel

> **Layer**: Foundation
> **GDD**: design/gdd/session-travel.md
> **Architecture Module**: Session & Travel and Seeds & RNG in `StealthFoundation` (connection table, seeds, single round acknowledgement)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories session-travel`
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 3 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Session & Travel gets ten people into one listen-server session and keeps them together across a 3-5 round evening. It owns direct join (`ip[:port]`, no matchmaking, no browser, no NAT relay), the ordered admission gates at `PreLogin` (build and tuning compatibility, capacity `2 x TeamSize`, join-in-progress only in `Lobby`, known identities may rejoin in any phase), round travel by one seamless `ServerTravel ?Seed=`, persistence across travel (which participant fields carry over for the match and which reset each round), liveness (per-connection silence timers, rejoin window, supersede, client auto-rejoin), session end when the host leaves (no host migration), and the single round acknowledgement that confirms seed, `LayoutHash`, `TuningHash` and `PoolHash` within the travel deadline.

It decides nothing about gameplay; it reports facts (`OnParticipantDisconnected`, `OnParticipantRejoined`, `OnParticipantTravelComplete`, ...) and Round decides what they mean. This epic also carries the Seeds & RNG service from ADR-0006 (public sub-seed mixer, `ServerSecret`, SipHash-2-4 keyed sub-seeds by `(ESeedDomain, Attempt)`, `FSecretStream`, PCG32 `FIntStream`), the persistent 128-bit `IdentityKey` with launch/PIE override, the default-login-pawn disable, and the PIE disconnect/rejoin/supersede/kick test hook that the reconnect-indicator criteria (AC-HUD-28, AC-PSR-53) need (rebaseline: build the harness hook here and time-box a spike).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0005: Session, seamless travel and match-scoped state stores | Game-instance stores that survive seamless ServerTravel (?Seed=), IdentityKey rejoin, single round acknowledgement, liveness clock, no host migration. | HIGH |
| ADR-0006: Seeds and deterministic/secret random streams | Public sub-seeds for layout vs SipHash-2-4 keyed server-secret streams; integer PCG32 streams so layout is bit-identical across machines. | LOW |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

10 of 10 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-session-001 | Listen-server direct join; ordered admission gates at PreLogin; capacity `2 × TeamSize` | ADR-0005 ✅ |
| TR-session-002 | Persistent 128-bit IdentityKey per install with launch/PIE override | ADR-0005 ✅ |
| TR-session-003 | Round travel by seamless `ServerTravel ?Seed=` with the Roster reset ordering | ADR-0005 ✅ |
| TR-session-004 | Public seed vs server-secret keyed sub-seeds | ADR-0006 ✅ |
| TR-session-005 | Single round acknowledgement (seed, layout hash, tuning hash, pool hash) within the travel deadline | ADR-0005 ✅ |
| TR-session-006 | Liveness silence clock, rejoin window, supersede, client auto-rejoin | ADR-0005 ✅ |
| TR-session-007 | Session never spawns/frees pawns or PlayerStates on connection events; default login pawn disabled | ADR-0005 ✅ |
| TR-session-008 | Host parity: the host's view reads the replicated view path | ADR-0001 ✅ |
| TR-session-009 | No host migration; session-ending notice; client host-loss detection | ADR-0005 ✅ |
| TR-session-010 | Leak-surface checklist (PlayerState fields, ping, join timing) enforced via the allowlist | ADR-0001 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** the prototype's absence of session handling: `StealthGameMode.cpp` restarts the round in place (`StartRound`, `EndRound`, auto-restart timer) with no `ServerTravel`, no grace, no rejoin and no liveness clock; the layout seed is a replicated value on `StealthGameState`. None of this is promoted. The seed replication is superseded by `?Seed=` on travel plus the public sub-seed mixer; the prototype city seeds (29755, 8924, 25482, 17906, 13609, 19972, 13135) are useful only as a smoke reference.
**Reference only:** `Spikes/StealthIdentitySpike.*` for the S1/S2/S14 engine-behaviour evidence the Session ADR cites.

## Build Order and Gates

- **Depends on (epics)**: participant-state-roster (registration API), tuning-data-config (hashes); consumed by round-match-framework and level-socket-contract
- **Gates and risks**: ADR-0005 and ADR-0006 are unconditional. The systems-index risk for this system is seamless-travel persistence (`ServerTravel` recreates PlayerStates each round): classify every Roster field as match-scoped or round-scoped and test with two clients. The producer review flagged the days-per-AC ratio (0.09) as low: re-verify the AC type mix before story breakdown.
- **Estimate**: about 6 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 3.6 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 67 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/session-travel.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
