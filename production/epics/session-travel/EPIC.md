# Epic: Session & Travel

> **Layer**: Foundation
> **GDD**: design/gdd/session-travel.md
> **Architecture Module**: Session & Travel and Seeds & RNG in `StealthFoundation` (connection table, seeds, single round acknowledgement)
> **Status**: Ready
> **Stories**: 93 stories created 2026-10-25 (83 Ready, 10 Blocked); see the Stories table below
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

## Stories

Granularity `fine` (one acceptance criterion per story). Stories 001-004 are the spikes and the harness hook that the Blocked stories wait on. Blocked reasons: 002 (net-emulation spike) gates 068, 069, 077, 078, 080, 085 (079 waits on 078); 003 (spike S3) gates 053; ADR-0001 spike S1 (whole-pawn parity FAIL, `production/qa/spikes/2026-10-25_S1-S2-S14.md`) gates 071 and 072. QL-STORY-READY: 79 ADEQUATE, 14 GAPS accepted as written (1, 2, 6, 7, 26, 31, 32, 78, 80, 85, 86, 88, 91, 93; their QA Test Cases read "not yet defined").

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | PIE harness hook forces client disconnect, rejoin, supersede and host kick | Integration | Ready | ADR-0005 |
| 002 | Net-emulation spike measures RTT, packet rate, burst outages and map load time | Integration | Ready | ADR-0005 |
| 003 | Spike S3: stores and views survive two seamless travels in multi-process standalone | Integration | Ready | ADR-0005 |
| 004 | Spike S4: retained PlayerState adopted on rejoin with no visible change to other clients | Integration | Ready | ADR-0005 |
| 005 | SipHash-2-4 passes the published reference vectors | Logic | Ready | ADR-0006 |
| 006 | PCG32 integer stream reproduces the reference outputs | Logic | Ready | ADR-0006 |
| 007 | UniformBelow, UniformInRange, WeightedPick and Fisher-Yates are unbiased and edge-safe | Logic | Ready | ADR-0006 |
| 008 | FSecretStream counter draws are independent of interleaved draws | Logic | Ready | ADR-0006 |
| 009 | OpenSession draws ServerSecret and MatchNonce from OS entropy and fails closed | Logic | Ready | ADR-0006 |
| 010 | Round seed and sub-seed derivation match the F5 golden vectors | Logic | Ready | ADR-0006 |
| 011 | GetServerSubSeed is unreachable from clients and from generator scope | Logic | Ready | ADR-0006 |
| 012 | CI grep bans engine RNG APIs and the CityGenerator include rule is enforced | Config/Data | Ready | ADR-0006 |
| 013 | ServerSecret and RejoinToken never appear in logs, URLs or replicated properties | Integration | Ready | ADR-0006 |
| 014 | Session lifecycle: OpenSession seeds the host, EndSession clears every store on every exit path | Integration | Ready | ADR-0005 |
| 015 | Illegal ConnectionState transitions assert and leave the state unchanged | Logic | Ready | ADR-0005 |
| 016 | Host record is seeded by OpenSession with the host's IdentityKey and no join handshake | Integration | Ready | ADR-0005 |
| 017 | A host with a client never leaves Connected over 60 s of fake time | Logic | Ready | ADR-0005 |
| 018 | Silence clock fires Soft and Hard inside the F2 bounds and survives host hitches | Logic | Ready | ADR-0005 |
| 019 | A packet or rejoin arriving in the deciding tick wins over the timeout | Logic | Ready | ADR-0005 |
| 020 | A packet from an Unresponsive participant restores Connected and raises OnParticipantResponsive once | Logic | Ready | ADR-0005 |
| 021 | Session validates the live engine timeout configuration at session open | Logic | Ready | ADR-0005 |
| 022 | Shipped config has SessionHeartbeatIntervalSec 0 and no heartbeat RPC on any pawn | Logic | Ready | ADR-0005 |
| 023 | AStealthGameMode derives AGameModeBase with seamless travel and Session hook overrides | Logic | Ready | ADR-0005 |
| 024 | Disconnect and rejoin spawn no pawn and leave the player array unchanged | Integration | Ready | ADR-0005 |
| 025 | No client ever receives an engine entered, left or renamed message | Integration | Ready | ADR-0005 |
| 026 | IdentityKey is per install, overridable and never replicated or logged beyond 4 bytes | Integration | Ready | ADR-0005 |
| 027 | A rejoin with the right key but a wrong RejoinToken is refused and the live connection is untouched | Logic | Ready | ADR-0005 |
| 028 | The first failing admission gate decides the reason code | Logic | Ready | ADR-0005 |
| 029 | Every cell of the admission-mode table yields the specified outcome and reason | Logic | Ready | ADR-0005 |
| 030 | Capacity is 2 x TeamSize and a pending handshake holds its slot until it fails | Logic | Ready | ADR-0005 |
| 031 | Client-supplied Team, SpectatorOnly and Seed options are never read | Integration | Ready | ADR-0005 |
| 032 | Display names are trimmed, stripped, truncated and suffixed; rejoin matches by key only | Logic | Ready | ADR-0005 |
| 033 | A rejoin with a different requested name keeps the original name | Logic | Ready | ADR-0005 |
| 034 | A different ProtocolVersion or TuningSchemaVersion is rejected VersionMismatch with no record change | Integration | Ready | ADR-0005 |
| 035 | A key the host kicked earlier is rejected Refused in every admission mode | Logic | Ready | ADR-0005 |
| 036 | A host kick expires the participant at once with Disconnected(Kicked) then RejoinWindowExpired | Logic | Ready | ADR-0005 |
| 037 | A handshake that never completes is closed and its slot released with no Roster record | Logic | Ready | ADR-0005 |
| 038 | Session forces Closed during travel and honours Round's pushed mode afterwards | Logic | Ready | ADR-0005 |
| 039 | RequestRoundTravel refuses with the exact code for each violated precondition | Logic | Ready | ADR-0005 |
| 040 | Missing or malformed Seed raises OnTravelFailed(BadParams) and never defaults | Logic | Ready | ADR-0005 |
| 041 | FRoundParams reaches every client as one struct and clients never read the travel URL | Integration | Ready | ADR-0005 |
| 042 | Exactly one ServerRoundAck per AckSeq in any of the 24 arrival orders | Logic | Ready | ADR-0005 |
| 043 | A first hash mismatch triggers one resend with AckSeq + 1; a second disconnects with ParamsMismatch | Logic | Ready | ADR-0005 |
| 044 | Stale, duplicate, early and old-connection acknowledgements are dropped and rate-limited | Logic | Ready | ADR-0005 |
| 045 | Acknowledgement state is keyed by ParticipantId, cleared per epoch and never replicated | Logic | Ready | ADR-0005 |
| 046 | Travel deadline is absolute: max(t_travel + 60, t_ready + 15), unmoved by disconnect or rejoin | Logic | Ready | ADR-0005 |
| 047 | A client closing during travel becomes Disconnected(ConnectionClosed) without affecting the others | Integration | Ready | ADR-0005 |
| 048 | A client whose acknowledgement never arrives times out alone at the F7 deadline | Logic | Ready | ADR-0005 |
| 049 | ServerWorldReady is accepted once per epoch and raises OnParticipantWorldReady | Logic | Ready | ADR-0005 |
| 050 | Every client holds the loading screen until the same server frame, a rejoiner until its acknowledgement completes | Integration | Ready | ADR-0005 |
| 051 | RequestFallbackTravel keeps Seed, PresetId and RoundIndex and starts a new epoch and t_travel | Logic | Ready | ADR-0005 |
| 052 | Lobby travel makes silent and disconnected participants Dormant and raises OnLobbyTravelComplete once | Logic | Ready | ADR-0005 |
| 053 | LobbyMap equal to MatchMap completes a seamless same-map travel and admission returns to Open | Integration | Blocked | ADR-0005 |
| 054 | Match-scoped fields survive three rounds unchanged and round-scoped state resets only by explicit call | Integration | Ready | ADR-0005 |
| 055 | PlayerController declares no authoritative Session or Roster state | Logic | Ready | ADR-0005 |
| 056 | Seamless arrival calls RebindView and fresh-login rejoin calls RegisterParticipant(Reclaimed) exactly once | Integration | Ready | ADR-0005 |
| 057 | Round 2 starts without the main menu and every client's acknowledgement matches the server's | Integration | Ready | ADR-0005 |
| 058 | A supersede raises OnParticipantDisconnected(Superseded) at once and OnParticipantRejoined only on acknowledgement | Logic | Ready | ADR-0005 |
| 059 | A supersede moves the participant to Rejoining and must pass the acknowledgement without restarting clocks | Logic | Ready | ADR-0005 |
| 060 | An informant who rejoins inside the window is the same record, still an informant, same team | Integration | Ready | ADR-0005 |
| 061 | A rejoiner who drops mid-rejoin raises no second disconnect and the window keeps its original end | Logic | Ready | ADR-0005 |
| 062 | A lapsed window raises OnRejoinWindowExpired once and later attempts follow ShouldAdmitExpired | Logic | Ready | ADR-0005 |
| 063 | A Lobby disconnect releases the slot at once and the key is readmitted as itself | Logic | Ready | ADR-0005 |
| 064 | Each Rejoining origin ends at exactly its RejoinDeadline with the listed events | Logic | Ready | ADR-0005 |
| 065 | A relaunched client retries the cached address and key until admitted or a non-retryable reason | Integration | Ready | ADR-0005 |
| 066 | Same-tick events arrive in ascending ParticipantId order and Session never spawns or transitions | Logic | Ready | ADR-0005 |
| 067 | Hosts quitting or crashing end the session for every client with no host migration | Integration | Ready | ADR-0005 |
| 068 | Soft timeout config satisfies F3 and a 30-minute soak logs zero false Unresponsive events | Integration | Blocked | ADR-0005 |
| 069 | RejoinWindowSec satisfies F4 against measured load and restart times | Config/Data | Blocked | ADR-0005 |
| 070 | Host and client views come from the same replicated view path | Integration | Ready | ADR-0001 |
| 071 | Per-player connection fields are owner-only or constant and player and NPC pawns export the same references | Logic | Blocked | ADR-0001 |
| 072 | A rejoin leaves the pawn's owner, controller and PlayerState references unchanged and NPC-like | Integration | Blocked | ADR-0001 |
| 073 | A refused in-round connection produces no RPC, replicated change or message on in-round clients | Integration | Ready | ADR-0005 |
| 074 | Spawn order is uncorrelated with player-ness over the 1,000-seed fixture | Logic | Ready | ADR-0006 |
| 075 | No Session player-facing string contains a banned term | Logic | Ready | ADR-0001 |
| 076 | Every field named in rule 25 is listed in the Visibility Policy allowlist with its audience | Logic | Ready | ADR-0001 |
| 077 | A drop and rejoin changes nothing a non-teammate client can observe | Integration | Blocked | ADR-0001 |
| 078 | A pawn whose player went silent 15 s stays within the Crowd baseline | Integration | Blocked | ADR-0001 |
| 079 | Observers pick the connection-event pawn no better than chance over 20 trials | Visual/Feel | Blocked | ADR-0001 |
| 080 | A 2 s loss burst decelerates the pawn to idle with no snap beyond the NPC smoothing threshold | Integration | Blocked | ADR-0001 |
| 081 | Each rejection reason code shows exactly the UI Requirements sentence | Logic | Ready | ADR-0005 |
| 082 | Each Session status overlay and the ZOrder-100 modal have retained screenshots | UI | Ready | ADR-0005 |
| 083 | No widget below ZOrder 100 is hit-testable and Cancel and OK exist only in the Session modal | UI | Ready | ADR-0005 |
| 084 | Lobby clients see 'Name joined' and 'Name left' and team only for their own teammates | Integration | Ready | ADR-0001 |
| 085 | Join appears in the lobby within 3 s on LAN and 5 s at 100 ms, rejections within one RTT plus a frame | Integration | Blocked | ADR-0005 |
| 086 | Pressing Host with a valid name shows Connecting then the Lobby, and Cancel returns unchanged | UI | Ready | ADR-0005 |
| 087 | Typing a host address and pressing Enter shows Connecting and reaches the host's Lobby | UI | Ready | ADR-0005 |
| 088 | First launch disables Join with a reason and an invalid address is reported in words | UI | Ready | ADR-0005 |
| 089 | Each rejection code shows its exact sentence under the address field on the rejected client only | UI | Ready | ADR-0005 |
| 090 | After the host leaves, every client sees 'The session has ended.' and OK returns to the menu with the address kept | UI | Ready | ADR-0005 |
| 091 | After one join attempt, relaunching shows the address with Join enabled and focused | UI | Ready | ADR-0005 |
| 092 | Join during a host travel shows the Busy sentence on the status overlay and retries without input | UI | Ready | ADR-0005 |
| 093 | A display name over 20 characters cannot be entered and the lobby shows it after trimming and suffixing | UI | Ready | ADR-0005 |

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/session-travel.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
