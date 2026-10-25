# Epic: Informant Pings & Team Communication

> **Layer**: Feature
> **GDD**: design/gdd/informant-pings-team-comms.md
> **Architecture Module**: Informant Pings in `StealthGameplay/Features/Pings` (`UTeamPingService` over headless `FTeamPingCore`, `UTeamPingViewComponent`; controller RPC `ServerTeamPing` declared on `AStealthPlayerController` in the composition root)
> **Status**: Ready
> **Stories**: 27 stories (see Stories below)
> **Engine Risk**: HIGH (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 17 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Informant Pings & Team Communication is the game's only in-game team channel in the MVP: a momentary, place-based, team-only ping. A player presses one key while looking at a place, and a broken Marigold ring appears on the ground there on the screens of their own team only for `PingLifetimeSec`, labelled with their name; it never attaches to, follows or is computed from a pawn, and there is one ping kind. It exists so an eliminated player (an informant who can only walk and watch) can turn what they see into something the team can act on. Team text chat is not in the MVP; external voice remains a stated playtest requirement.

The data layer is a reliable controller-level Server RPC (`ServerTeamPing(FNetTeamPing {FVector_NetQuantize10 Point})`, never a pawn command, never the action slot, the queue or `FActionState`) validated in order (phase and latch; Roster state, bound pawn and concealment through `IsInBushOrTransition`; range from the sender's eye pivot with a server slack; the per-`ParticipantId` token bucket that survives reconnect and resets only at `OnRosterReset`), writing one slot into the team-audience `UTeamPingViewComponent` (`FTeamPingView`, a fixed-size array, slot i = Roster team slot i, one changed slot at most 32 B, at most 80 B/s per receiving teammate, 0 B to opponents). The presentation layer is the HUD L0 ring presenter (stepped stroke weight at thirds, never opacity; edge rings using the Markers F3 clamp; label dropped at `OnAllyMarkerRetired`; no replay on rebuild; hidden on `Ended`). The five `InformantPings` tuning keys and the I-PNG-1 warning register with TDC; server-only accepted/dropped statistics feed ADR-0023 telemetry.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0001: Crowd identity rules | Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. | HIGH |
| ADR-0002: Server resolution queue, server clock and round-end staging | One total-order queue keyed (ServerTickIndex, Seq) drained in TG_PostPhysics, due-time heap, typed staged writes, single-assignment latch, client server-time estimator. | MEDIUM |
| ADR-0003: Replication, relevancy and the concealment cull hybrid | Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus a client-local hide, allowlist audit of every replicated property. | HIGH |
| ADR-0004: Tuning data pipeline and validator | One cooked data-asset class of per-owner sub-structs, C++ schema rows, server-only resolve/validate/freeze with TuningHash and ClientHash, no config/CVar back doors. | LOW |
| ADR-0005: Session, seamless travel and match-scoped state stores | Game-instance stores that survive seamless ServerTravel (?Seed=), IdentityKey rejoin, single round acknowledgement, liveness clock, no host migration. | HIGH |
| ADR-0007: Code module layout, composition root and include boundaries | Four runtime modules (StealthFoundation, StealthGameplay, StealthPresentation, StealthGame) plus Editor and StealthTestHarness; compile guard, facade split, consumer-owned interfaces. | MEDIUM |
| ADR-0008: Test and automation strategy for leak and parity tests | Headless pure-C++ cores, multi-client and multi-process leak harness, raw-bunch capture, net-emulation profiles, per-commit canaries. | MEDIUM |
| ADR-0011: Pawn command interface and input pipeline | Seven server-validated entry points with no caller identity, two-stage validation with token buckets, Enhanced Input edges, batched owner replies. | HIGH |
| ADR-0015: Camera system and LOS envelope | Client-local 1P/3P camera, -60/+40 view envelope, eye-origin visibility mask via a custom-primitive-data dither, one shared GetAimRay. | HIGH |
| ADR-0017: Concealment component and per-viewer presentation | Per-pawn component and server record, bush hide volumes, owner-side warp, fail-closed client latch, server-only IsConcealedFrom. | HIGH |
| ADR-0018: Encounter data and outcome presentation timing | Outcome-blind FActionState at the queue slot, contact-time pending writes, owner-only results, MotionWarping (Beta in 5.8.4). | HIGH |
| ADR-0021: UI/HUD layering, CommonUI and Enhanced Input | UMG layer/ZOrder stack drawn after post-process, Common UI for menus only, client-local settings hooks, ping rings in HUD L0. | HIGH |
| ADR-0023: Playtest telemetry hooks | Telemetry section of the Event Log store, probes and detectors at Feature call sites, server-only statistics. | LOW |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

24 of 24 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-png-001 | PNG-R1: a ping is a momentary place-only ground mark shown to the sender's team only for `PingLifetimeSec`; it never attaches to, follows or is computed from a pawn, the server never receives a pawn identity with it, and there is one ping kind | ADR-0001, ADR-0011, ADR-0021 ✅ |
| TR-png-002 | PNG-R2, PNG-R5 steps 1-2: a sender may ping only in phase `Active` before Round's latch, with Roster `State` `Active` or `Informant`, a bound pawn and a live connection; one rule for Active players and informants (no role branch, identical entry shape) | ADR-0011, ADR-0002 ✅ |
| TR-png-003 | PNG-R2, PNG-R3, PNG-R5 step 2 (rulings D26, D33): a concealed sender cannot ping; the server uses Concealment's `IsInBushOrTransition` (true for every state but `Outside`, so `Emerging` counts) and the owning client the same predicate, fail-closed; only this refusal gives owner-only feedback (refusal tone + 'Not now' plate), invisible to everyone else | ADR-0017, ADR-0011, ADR-0021 ✅ |
| TR-png-004 | PNG-R3: `IA_Ping` is one Boolean press action (default trigger) in `IMC_Common` for both Active and Informant contexts, rebindable, never routed through Common UI, no hold/chord/repeat, and it never ends or interrupts an outstanding pawn command | ADR-0011, ADR-0021 ✅ |
| TR-png-005 | PNG-R3: the ping point is the world query of Camera `GetAimRay()` on `ViewBlock` (pawns and bush foliage ignored; third person re-traced from the eye pivot), dropped at most 300 cm to the first walkable surface; with no hit or no walkable ground nothing is sent | ADR-0015, ADR-0011 ✅ |
| TR-png-006 | F2: range is measured from the sender pawn's eye pivot on both machines (client `\|P - E\| <= PingRangeCm`; server `<= PingRangeCm + PingRangeSlackCm`), never from the camera; non-finite points are dropped | ADR-0011, ADR-0015 ✅ |
| TR-png-007 | PNG-R3, edge case C-W32: the client gate reads `PresentedOwnState` and the bound-pawn state, never the raw own `State`; from the own fall start until a body is bound nothing is sent; presses in the strike window are dropped server-side with no token, reply, echo, tone or plate | ADR-0011, ADR-0018, ADR-0021 ✅ |
| TR-png-008 | PNG-R4: transport is `ServerTeamPing(FNetTeamPing {FVector_NetQuantize10 Point})`, a reliable Server RPC on the sender's own `AStealthPlayerController`; it is not a pawn command (never `ServerPawnCommand`, the action slot, the resolution queue or `FActionState`), names no pawn, and the listen host uses the same path | ADR-0011, ADR-0002, ADR-0005 ✅ |
| TR-png-009 | PNG-R5: server validation in order phase/latch, state/pawn/concealment, range, bucket; the first failure drops silently with the drop reason counted; failures before the bucket step spend no token; no line-of-sight check; acceptance stamps `ServerTime = ServerNow()` and advances the sender's wrapping `PingSeq` | ADR-0011, ADR-0023 ✅ |
| TR-png-010 | F1: per-participant token bucket (`PingBucketSize`, `PingBucketPerSec`, server tolerance `epsilon = PingBucketPerSec * PingSkewTolSec` with constant 0.1 s), full at `Active` entry, keyed by `ParticipantId` so it survives reconnect, reset only at `OnRosterReset`; the client keeps a strict mirror | ADR-0011, ADR-0004, ADR-0007 ✅ |
| TR-png-011 | PNG-R6: the carrier is the C-TeamView part `UTeamPingViewComponent` with `FTeamPingView`, a fixed-size array with one slot per team member (slot i = Roster team slot i, never compacted or reordered), each `{SenderHandle, PingSeq, Point, ServerTime}`; no stage, court, site, clue or `RoundEpoch` data; pushed change-only, coalesced per server frame | ADR-0001, ADR-0003 ✅ |
| TR-png-012 | PNG-R6, PNG-R9, IVP `TeamPing` row: team-only delivery by relevancy (owner-only PlayerController); opponent machines hold no ping part, entry, RPC, widget or sound, the host included when it is an opponent; the part is on the replicated-property allowlist with Team audience only on `AStealthPlayerController` | ADR-0001, ADR-0003, ADR-0008 ✅ |
| TR-png-013 | PNG-R6 (ruling D25): one active ring per sender; a new accepted ping overwrites the sender's slot and replaces its ring at once on every client; the replacing ping still spends a token and nothing is refunded | ADR-0001, ADR-0021 ✅ |
| TR-png-014 | PNG-R7, F3: a teammate shows a ring for each new `(SenderHandle, PingSeq)` whose age `EstimatedServerNow() - ServerTime` is below `PingLifetimeSec`; age is shown by stepped stroke weight at thirds (never opacity), negative ages clamp to 0, stale arrivals are never shown; the sender sees its ring only from the server echo; one 2D UI tick on arrival | ADR-0021, ADR-0002 ✅ |
| TR-png-015 | PNG-R7, VA-PNG-2: rings draw in HUD L0 beneath teammate markers with no depth test, collapse with L0 in the `Neutral` view, and clamp off-screen or behind-camera rings to the safe rectangle as unlabelled edge rings using the Teammate Markers F3 clamp, bias ramp and hysteresis | ADR-0021, ADR-0015 ✅ |
| TR-png-016 | PNG-R7: the ring label is the sender's C-Public `DisplayName` by `SenderHandle` ('Ally' if unresolved); it is dropped at that ally's `OnAllyMarkerRetired` while the ring stays to expiry; the sender's own screen keeps its own-name label | ADR-0021 ✅ |
| TR-png-017 | PNG-R8: a ping creates no team-feed line, plate, card or counter in the MVP | ADR-0021 ✅ |
| TR-png-018 | PNG-R9: pinging is invisible outside the team: no pawn gesture, pose, sound, `ActionPose`, replicated pawn field, `ForceNetUpdate` or relevancy change, nothing on `FRoundPublicState`, C-Public or any All-audience carrier, no world audio/VFX; the handler reads only the sender's own pawn (Crowd Tell Matrix Ping row: no tell) | ADR-0001, ADR-0003, ADR-0008 ✅ |
| TR-png-019 | PNG-R10: every ping part is cleared at `OnRosterReset`, at travel (`InvalidateAllBindings`) and in Round's round-end step; clients hide every ring on entering `Ended`; a HUD rebuild marks present entries shown (no replay); a disconnected sender's ring expires normally | ADR-0002, ADR-0001, ADR-0005, ADR-0007, ADR-0021 ✅ |
| TR-png-020 | PNG-R11, PNG-R12: no in-game text chat or in-engine voice in the MVP (no free text on the wire); every ping string avoids the banned vocabulary and the binding label reads 'Ping' | ADR-0021, ADR-0003 ✅ |
| TR-png-021 | F4: ping bandwidth stays within 80 B/s per receiving teammate connection (about 50 B/s steady at defaults) and 0 B to opponent connections; one changed slot costs at most 32 B (per-element delta; a dynamic array without a fast-array serializer is not allowed) | ADR-0003, ADR-0001 ✅ |
| TR-png-022 | Tuning Knobs: the five keys of owner `InformantPings` (`PingLifetimeSec`, `PingBucketSize`, `PingBucketPerSec`, `PingRangeCm`, `PingRangeSlackCm`) with their audiences and latches, and the cross-field warning I-PNG-1, are registered in the tuning schema | ADR-0004 ✅ |
| TR-png-023 | PNG-R5, AC-PNG-13: server-only statistics of accepted pings and drops by reason, split by role class, feed playtest telemetry; never per person in any client-visible form | ADR-0023 ✅ |
| TR-png-024 | Game Feel, AC-PNG-14: own ring visible p95 <= own RTT + 50 ms after the press; a teammate shows it within its one-way delay plus one server frame of acceptance | ADR-0001, ADR-0002, ADR-0008 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** the prototype ping (about 60 lines): `AStealthCharacter::OnPingPressed` (T key) and `ServerPing(FVector_NetQuantize)`, `StealthGameMode::HandlePing`, `StealthPlayerController::ClientReceivePing` and the ring drawing in `StealthHUD.cpp` (a client RPC to team controllers, a 4 s ring and a 1.5 s cooldown, pinging the **sender's own position**). The approved design replaces nearly all of it: the ping point is the camera aim-ray world query (never a pawn), the carrier is C-TeamView, validation is the ordered five-step check with the token bucket, and the ring is drawn in HUD L0. The prototype `StealthBot ... ping` command and its networked test are a useful smoke reference only.

## Build Order and Gates

- **Depends on (epics)**: participant-state-roster (C-TeamView carrier), information-visibility-policy, round-match-framework, input-interaction-framework (`IA_Ping`), camera-system (`GetAimRay`), hud (L0 layer; an upward Feature -> Presentation scheduling dependency, recorded here and not a code dependency: the HUD reads the part through `View/` headers), npc-kill-penalty-informants, concealment-bushes (`IsInBushOrTransition`), tuning-data-config; teammate-markers (soft). Schedule after HUD and Teammate Markers (rebaseline: about 4 extra working days if built before the shared carriers).
- **Gates and risks**: Added to the MVP by user decision 2026-10-25 (item 8). The behaviour is covered by Technical-Setup amendments to ADR-0001, 0002, 0007, 0011, 0017, 0021 and 0023 (check they are applied before promotion from the prototype; rebaseline Dependencies). ADR-0021 UI-V1..UI-V4 gate the HUD L0 ring presenter. Open scope risk: AC-PNG-10 needs three reviewers who did not author the UI to blind-sort greyscale captures, which a solo developer cannot supply unaided (rebaseline Risk Factors); either fresh-context agents count as reviewers or the criterion moves to ADVISORY, and that is a decision to make before stories are written. Pillar-1 leak risk is Low (no pawn, queue or relevancy change); AC-PNG-01, -02 and -03 are BLOCKING and use the opponent-as-host run. Days-per-AC (0.63) is UI-evidence-heavy; confirm at story breakdown.
- **Estimate**: about 12.5 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 7.5 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 20 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Stories

Granularity `balanced` (2-4 acceptance criteria per story; GDD criteria with several independent cases are split into sub-criteria; criteria derived from an ADR or the epic where the GDD has none are labelled "derived"). Stories embed Control Manifest 2026-10-25f (the current manifest; the header above still reads 2026-10-25e). Rulings applied: D25 (one ring per sender), D26/D33 (no ping while concealed, `Emerging` included), D34 (OQ7 threshold, story 025), D36 (AC-PNG-10 blind sort ADVISORY with a three-agent pre-screen, story 021; the retained screenshots stay required). Stories 019, 020 and 021 are Blocked on ADR-0021 UI-V1..UI-V4 (the gates named in Build Order and Gates). QL-STORY-READY: 15 ADEQUATE (001, 002, 003, 004, 007, 008, 010, 011, 013, 014, 015, 016, 017, 021, 027) and 12 GAPS (005, 006, 009, 012, 018, 019, 020, 022, 023, 024, 025, 026) whose flagged criteria were revised on the qa-lead's own proposed rewrites (each story's QA Test Cases says what changed); 0 INADEQUATE. Open items for the GDD owner: AC-PNG-04's reason list has a second `Range` (the accepted ping is not a drop); AC-PNG-10's gate level in the GDD text (BLOCKING) differs from D36 (ADVISORY until human reviewers run it at the first playtest). Story sizes are per-story session sizes and sum to more than the epic's 12.5-day bottom-up figure: 8 S, 16 M, 3 L; re-baseline against velocity before sprint planning (the epic's own note flags days-per-AC for confirmation at story breakdown).

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Ping tuning keys and invariant I-PNG-1 | Logic | Ready | ADR-0004 |
| 002 | Team ping wire types and fixed-slot view component | Integration | Ready | ADR-0001 |
| 003 | Ping carrier registration and team-only audience audit | Logic | Ready | ADR-0003 |
| 004 | Ping token bucket core | Logic | Ready | ADR-0011 |
| 005 | Server validation core: order, drop reasons and range | Logic | Ready | ADR-0011 |
| 006 | ServerTeamPing handler, service and host parity | Integration | Ready | ADR-0011 |
| 007 | Ping lifecycle: clears, reset and travel | Integration | Ready | ADR-0002 |
| 008 | Client ping-point rule | Logic | Ready | ADR-0015 |
| 009 | Client gate and bucket mirror | Logic | Ready | ADR-0011 |
| 010 | IA_Ping action, binding and send path | Integration | Ready | ADR-0011 |
| 011 | Concealed-press refusal end to end | Integration | Ready | ADR-0017 |
| 012 | Strike-window drop | Integration | Ready | ADR-0018 |
| 013 | Opponent sees nothing | Integration | Ready | ADR-0001 |
| 014 | Sender pawn parity | Integration | Ready | ADR-0008 |
| 015 | One ring per sender | Integration | Ready | ADR-0001 |
| 016 | Ring model and age | Logic | Ready | ADR-0021 |
| 017 | Ring labels and the label drop | Logic | Ready | ADR-0021 |
| 018 | Edge ring metrics and clamp | Logic | Ready | ADR-0021 |
| 019 | Ring painter in the L0 leaf | UI | Blocked (ADR-0021 UI-V1, UI-V3) | ADR-0021 |
| 020 | Edge ring painter and UI scale | UI | Blocked (ADR-0021 UI-V1, UI-V4) | ADR-0021 |
| 021 | Ring UI evidence and blind-sort pre-screen | UI | Blocked (ADR-0021 UI-V1..UI-V4; stories 019, 020) | ADR-0021 |
| 022 | Echo latency and no local prediction | Integration | Ready | ADR-0008 |
| 023 | Bandwidth bound | Integration | Ready | ADR-0003 |
| 024 | Ping telemetry statistics | Integration | Ready | ADR-0023 |
| 025 | Playtest metrics: Fellowship follow-up and walker coverage | Integration | Ready | ADR-0023 |
| 026 | Ping language, scope and no-feed lints | Logic | Ready | ADR-0021 |
| 027 | Retire the prototype ping | Integration | Ready | ADR-0011 |

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/informant-pings-team-comms.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
