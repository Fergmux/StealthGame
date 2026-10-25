# Informant Pings & Team Communication

> **Status**: In Review (design review NEEDS REVISION 2026-10-25, revisions applied; re-review pending)
> **Author**: design pipeline (autonomous, phase 2) on the user's scope decision of 2026-10-25
> **Last Updated**: 2026-10-25
> **Last Verified**: 2026-10-25
> **Implements Pillar**: P1 Hide in Plain Sight (primary: nothing a ping does is visible to a non-ally); P2 Every Advantage Is a Tell (a ping relays only what the sender already saw, paid for with the sender's attention; PNG-R2); P3 One Strike, Lasting Consequence (the eliminated player keeps a job); Fellowship (MDA 2: the informant's value is what they tell the team)
> **Design Review**: NEEDS REVISION 2026-10-25 (lean, one independent sonnet reviewer; first review). Revisions applied in place: own ping-point rule on the `ViewBlock` channel with range measured from the eye pivot on both machines (PNG-R3, F2); no Layout Manifest extent check (LSC dependency removed); `RoundEpoch` dropped from the part (Roster owns it); validation reordered so failures spend no token, bucket tolerance ε and reconnect-keyed bucket (F1); `ClientPingSeq` removed; fixed-size slot array for per-element delta (F4); edge rings without labels; AC-PNG-01/-02/-04/-05/-10/-11/-13 tightened, AC-PNG-14 added. Not applied: the proximity re-mark drop (finding 6; CD-GDD-ALIGN finding 1 ruled manual shadowing intended informant work, OQ7 tracks it). Log: `design/gdd/reviews/informant-pings-team-comms-review-log.md`; re-review pending.
> **Creative Director Review (CD-GDD-ALIGN)**: CONCERNS -> REVISED 2026-10-25 (six text findings applied: I-PNG-1 reworded as a cadence bound with manual shadowing named as intended informant work, `PingBucketPerSec` safe range capped at 0.5, OQ7 chain metric; P2 justification and an informant-only fallback OQ8; "never one body" corrected to "in a dense crowd, a cluster" with sparse areas in OQ5; Player Fantasy reframed toward testing suspects and the race, removal wording and the Active player's use; decision 4's chat half recorded as deferred and a Fellowship playtest metric AC-PNG-13)

## Summary

Informant Pings & Team Communication gives every team one in-game channel: a key that drops a momentary broken ring on the ground where the player is looking, seen only by their own team for eight seconds. Its main user is the informant, the eliminated player who can only walk and watch, whose sightings become something the team can act on (Fellowship, Pillar 3), and it is built so that pinging changes nothing any opponent can see or receive (Pillar 1). There is no in-game text chat or voice in the MVP; external team voice stays a stated playtest requirement.

> **Quick reference** — Layer: `Feature` · Priority: `MVP` (moved from Vertical Slice by user decision 2026-10-25) · Key deps: `Participant State & Roster, Information Visibility Policy, Round & Match Framework, Input & Interaction Framework, Camera System, HUD, NPC-Kill Penalty & Informants, Tuning Data & Config (hard); Teammate Markers, Session & Travel (soft)`

## Overview

Informant Pings & Team Communication is the game's only **in-game** team channel in the MVP. A player presses one key while looking at a place, and a broken Marigold ring appears on the ground there, on the screens of their own team only, for eight seconds, labelled with their name. That is the whole verb: a **momentary, place-based, team-only ping** (game concept, user decision 4: "momentary player-sent pings + team chat; no sticky tracking marker"; art bible 3.4 and 7.x rule 2: "a place, never a person"). The ping half of decision 4 is MVP (user decision 2026-10-25); the **team-chat half is deferred**: in the MVP it is external voice (PNG-R11). It exists so that an eliminated player, now an informant who can only walk and watch, can turn what they see into something the team can act on ("the courtyard by the well, look there"), which is the Fellowship half of the core loop and the reason elimination is a reduced role rather than removal (Pillar 3). Active players may ping too, under the same rule.

The system has a data layer and a presentation layer. **Data:** the sender's PlayerController sends one point to the server (`ServerTeamPing`, a controller-level RPC, never a pawn command); the server checks phase, role, a rate budget and range, then writes one entry into a new team-audience part of C-TeamView (`UTeamPingViewComponent`), which the replication system copies only to the sender's teammates' own controllers (ADR-0001 §5 carrier model). **Presentation:** each teammate's HUD draws the ring in its world-anchored layer (HUD L0 ping slot) and ages it out. Nothing about a ping touches the sender's body, its replicated state, the resolution queue, or anything an opponent's client receives, so a player who pings is indistinguishable from one who does not (Pillar 1). Text team chat is **not** in the MVP: voice outside the game remains a stated playtest requirement for nuance, and the ping is the hook the playtest can now measure in the game itself (PNG-R11).

The GDD stays at the behaviour level; the transport, component and widget mechanisms are recorded as amendments to ADR-0001 (carrier part), ADR-0011 (input action and controller RPC) and ADR-0021 (L0 ping drawing), cited where they apply.

## Player Fantasy

**"Your eyes are still in the city."** A blade found you, or you misread one citizen too many. The card says what you are now, the screen goes dark, and then you are walking again, slow and unseen, in a crowd that does not know you are watching it (and that can still cut you down, if someone reads you). The rival house is still out there, hurrying toward the final court. You see a figure turn its head one beat too late near the fountain. You look at the paving stones beside it and press one key. On four screens across the city, a thin gold ring settles on those stones with your name under it, and your house knows where to look. Nobody else sees anything at all: not the rival, not the crowd, not the figure you were watching.

The player moments to anchor to:
1. **The tip-off.** An informant's ping lands; an ally walks over, reads the cluster inside the ring, and tests the figure the informant was watching. Or the ring lands by a court where someone has been loitering, and the house knows the rival is working a stage there. The informant's reduced role paid off (P3, Fellowship, P4: the race).
2. **The quiet hand.** Pinging costs the sender nothing visible: no gesture, no pose, no sound in the world. A player can mark a place while standing perfectly still in a market (P1).
3. **The ring, not the arrow.** The mark is a place about the size of a small crowd (4 m), never a person, never something that follows anyone. It says "here", and the team still has to read the bodies (P2: the advantage is attention, not identification).

An Active player uses the same key the same way: a quick "over here" to an ally across the square, at the cost of the moment spent looking instead of moving.

*Tone.* Level and civic, like the HUD ("only what is yours to know"): the ping is a note passed to friends, not a targeting laser. No "spotted", "detected", "tracked", "enemy" or "target" vocabulary anywhere (Roster and IIF language rules). Aesthetics served: **Fellowship** (primary), Challenge (the team still has to read the cluster), Fantasy (the informant as the house's eyes).

*Framing: direct (the player operates it), category Gameplay; recommended option taken in autonomous mode. `creative-director` was not consulted at section level (the orchestrator brief limits this run to one creative-director call, the CD-GDD-ALIGN gate in Phase 5).*

## Detailed Design

### Core Rules

*Specialists: `game-designer`, `systems-designer`, `ue-replication-specialist` were not spawned for this section (the orchestrator brief for this phase-2 run allows one creative-director gate call and one design-review call; MVP scope, lean). The rules reuse contracts that already passed review in the dependency GDDs and ADRs; each rule cites them. Review manually before production.*

**A. What a ping is**

**PNG-R1. A ping is a momentary mark of a place, for the sender's team only.** It is a ground ring (art bible 3.4 "sighting ping": open ring with a 60° gap, world radius ≥ 4 m, no centre mark) centred on one world point the sender chose, shown on the screens of every member of the sender's team, the sender included, for `PingLifetimeSec` (8 s), and then gone. It never attaches to, follows, sizes itself by, or is computed from any pawn; the server never receives a pawn identity with it. There is one ping kind in the MVP, and it means "look here": what to look for is the team's own convention (voice, habit). The ring's 4 m radius is deliberately cluster scale: a ping marks a place, and in a dense crowd a cluster of figures (art bible 7.x rule 2). In a sparse street the ring may hold a single figure; that is no more than the sender saying "the one by the well" by voice, but it edges toward the anti-pillar "not an identification-tools game", so it is a playtest check (OQ5). This is how "watch this person" is expressed without the system ever naming an identity: the sender marks the place, and the team reads the people.

**PNG-R2. Who may ping.** A participant may ping when all of these hold: Round phase `Active` and Round's latch not yet set (Round R9); own Roster `State` is `Active` or `Informant`; the participant has a bound pawn (an informant awaiting a takeover, on the neutral black view, cannot ping: there is no view to aim); the participant is connected. Active players and informants use the same key, the same rate budget and the same ring: **no rule distinguishes them** (decision: informant use is the core purpose, but a separate rule for Active players would add an exception for no Pillar-1 benefit, and an Active player's ping is Fellowship too; logged). The ring does not encode the sender's role; allies already know every teammate's role from C-TeamView. **Pillar 2 justification:** a ping is a team advantage that shows no readable deviation to opponents, which P2 normally forbids; it is accepted because a ping conveys only what the sender already saw (exactly what external voice conveys, in a coarser form), its price is the sender's attention (looking at a place instead of moving, reading or tending), and an opponent loses nothing it could have read. If playtests show Active players' pings replacing crowd-reading rather than relaying it, pings are restricted to informants (OQ8; the MVP keeps both).

**B. Sending**

**PNG-R3. Input.** One Boolean input action `IA_Ping` (press; default trigger only; keyboard **G**, gamepad **D-pad Up**; rebindable under ADR-0011 §9) in `IMC_Common`, so it exists in both the Active and the Informant contexts (IIF R13). On the press the client computes the **ping point** with its own rule (revised at design review 2026-10-25; it does not reuse CS-R8's `WorldPoint`, which clamps to strike range and snaps to a `HideVolume` entry): (1) trace Camera's aim ray (`GetAimRay()`, CS-R8) on the `ViewBlock` channel, which ignores every pawn profile (ADR-0015), so the ray passes through people and stops at world geometry; (2) from that hit, trace straight down at most 300 cm on the same channel to the first walkable surface (a bush's leaves are not walkable, so a hit in foliage lands on the ground under it); (3) the result `P` is the ping point. **Range origin:** the client and the server both measure range from the sender pawn's eye pivot `E` (CS-R4, capsule-derived, identical in first and third person), never from the camera: the ping is valid on the client only if `|P − E| ≤ PingRangeCm` (F2). If step 1 or 2 finds nothing, or `P` is out of range, or the client's local mirror of the rate budget (F1) is empty, or PNG-R2 fails on owner-known data (own `State`, phase, bound pawn), **nothing is sent and nothing is shown** (no plate, no sound: a refused ping has no feedback in the MVP, OQ3). There is no hold, chord, auto-repeat, reticle, snap or magnetism. The press is never routed through Common UI (ADR-0011 §9 rule for gameplay input). Because the action changes no pawn state, it works mid-move, mid-hold and mid-brace, and it never sends `End` for or interrupts any outstanding pawn command.

**PNG-R4. Transport.** The client sends `ServerTeamPing(FNetTeamPing { Point })` (one 10 cm quantised world point, `FVector_NetQuantize10`; nothing else, in particular no pawn id and no client sequence: the RPC is reliable and ordered, so the server needs no de-duplication), a reliable Server RPC on its own `AStealthPlayerController`. It is **not a pawn command**: it never enters `ServerPawnCommand`, the IIF action slot, Round's resolution queue or `FActionState`, and it names no pawn (ADR-0011 amendment, user decision 2026-10-25). The listen host's own ping calls the same server function through the same path as a remote client's (host parity, Session rule 3). Reliable delivery is bounded by the rate budget, so the reliable buffer cannot grow (F1).

**PNG-R5. Server validation.** In this order; the first failure drops the ping silently (no reply, no client-visible log line; a server stat counts drops by reason for telemetry, ADR-0023):
1. Phase is `Active` and Round's latch is not set (a ping arriving after the latch is dropped, like any late input).
2. The sender's participant `State` is `Active` or `Informant` and its pawn is bound.
3. The point is finite and within `PingRangeCm + PingRangeSlackCm` of the sender pawn's eye pivot `E` at validation (F2). The range alone bounds the point to a 61.5 m sphere around a pawn that stands in the playable city, so no separate world-extent check is needed (revised at design review: the Layout Manifest publishes no extent).
4. The sender's token bucket holds at least `1 − ε` tokens (F1, `ε` = 0.05 token, a constant that absorbs client/server clock skew at the refill boundary); one token is spent. A ping that fails steps 1-3 never spends a token.
There is **no line-of-sight check**: the point is the sender's own claim, and a modified client that pings through walls only tells its own team where to look, which voice could do anyway. On acceptance the server stamps `ServerTime = ServerNow()`, increments the sender's `PingSeq` (uint8, wrapping, per participant, reset at `OnRosterReset`) and writes the entry (PNG-R6).

**C. Delivery and display**

**PNG-R6. Carrier: one team-ping part of C-TeamView.** A new C-TeamView part, `UTeamPingViewComponent` (a `UTeamViewComponentBase` subclass, ADR-0001 §5 "feature team sub-structs ride C-TeamView"), sits on every `AStealthPlayerController`. The server writes the same view into the controller of every member of the sender's team (the sender's own included, informants included): `FTeamPingView { Entries[TeamSize] }`, a **fixed-size** array with **one slot per team member** (so only the changed slot's changed fields replicate; a dynamic array would need a fast-array serializer to get the same per-element delta, F4), each `FTeamPingEntry { SenderHandle (PublicHandle), PingSeq, Point, ServerTime }`. The part carries no `RoundEpoch` of its own: the Roster owns `RoundEpoch` (Roster R6 owner part), and the client reads it there. A sender has at most one live ping: a new accepted ping overwrites that sender's slot, so at most `TeamSize` rings exist per team at once. The part is pushed on change only, coalesced once per server frame, like every C-TeamView part. An opponent's controller is never relevant to this team's clients, so opponents hold no entry, no slot and no widget for it: the team-only rule is enforced by the absence of data (ADR-0001 §5), and the Information Visibility Policy carries the fact row (`TeamPing`, Team audience; IVP R3).

**PNG-R7. Display on teammates' screens.** A client shows a ping when its own controller's `UTeamPingViewComponent` holds an entry whose `PingSeq` differs from the last `PingSeq` it showed (or marked shown) for that `SenderHandle`, and whose age `a = EstimatedServerNow() − ServerTime` is below `PingLifetimeSec` (F3). It shows the ring from that moment until `ServerTime + PingLifetimeSec`, with the age shown by stepped stroke weight at thirds of its life (art bible 3.4), never by opacity, and the sender's name under the ring (C-Public `DisplayName` by `SenderHandle`; "Ally" if unresolved; the own name for one's own ping). A new entry from the same sender replaces the previous ring at once (no cross-fade). On arrival the client plays one quiet 2D UI tick (UI bus, never positional, never in the world). The ring is drawn in the HUD's L0 world-anchored layer, beneath the teammate markers (HUD-R4, HUD-R11 ping slot), is collapsed with L0 while the view is `Neutral` (Camera CS-R15), and when its centre is off-screen or behind the camera it is clamped to the HUD safe rectangle as a 32 px broken ring: static, unrotated, never an arrow, **without a name label** (art bible 7.5; the same clamp rule as Teammate Markers F3). Overlapping edge rings are not spread (no F4 de-overlap): they draw in arrival order, newest on top. The sender sees its own ping only from the server's echo; there is no local prediction, so a dropped ping never flashes on the sender's screen (Game Feel).

**PNG-R8. No feed line in the MVP.** A ping creates no team-feed line, no plate, no card and no counter. The HUD's reserved sighting feed-line type stays reserved (VS, Event Feedback & Cues). Rationale: the ring carries the name; a feed line per ping would compete with the removal and reconnect lines under the 3-line cap (HUD F8).

**D. Pillar 1 guarantees**

**PNG-R9. Pinging is invisible to everyone outside the sender's team.** Hard requirements, each tested (AC-PNG-01, -02):
- The sender's pawn plays no gesture, pose, sound or animation for a ping, and no replicated pawn field, `ActionPose`, net-update timing (no `ForceNetUpdate`) or relevancy changes because of one.
- The ping writes nothing to `FRoundPublicState`, the C-Public PlayerState fields or any `All`-audience carrier, and no world-space audio or effect is spawned.
- The server handler runs at the RPC's arrival, reads no pawn other than the sender's own (for the range check), and changes no pawn, crowd or queue state.
- The only body correlate of pinging is that the sender looked at the place first: an ordinary, chosen gaze (Crowd Tell Matrix T13), identical to looking without pinging.

**PNG-R10. Lifecycle.** The server clears every `UTeamPingViewComponent` (all slots empty) at `OnRosterReset`, at travel (`InvalidateAllBindings`) and in Round's round-end step (R9 (a)-(h): pings end with the round); clients also hide every ring at once when they enter the HUD `Ended` state, without waiting for the cleared part. A sender who disconnects leaves its live ping to expire normally. On a HUD rebuild (reconnect, travel, `RoundEpoch` change; HUD-R17) the client marks every entry present at the rebuild as already shown, so no ping is replayed. A sender's slot survives its own state change (an `Active` sender who is eliminated keeps its live ring until expiry). The token bucket is keyed by `ParticipantId`, so it survives a reconnect (a rejoin never refills it); it resets only at `OnRosterReset`.

**E. Team communication beyond the ping**

**PNG-R11. No in-game text chat or voice in the MVP.** The team-chat half of concept decision 4 is **deferred**, not dropped. Decision (user "accept all recommendations", 2026-10-25; option recommended because the engine and session provide no cheap path): a minimal team text chat would carry free text on the wire (ADR-0021 forbids text on the wire; every player-facing string comes from string tables), would need a text-entry input mode in the middle of a round (a menu-mode switch that ends every outstanding gameplay `Begin`, ADR-0011 §9) and moderation, and the Visibility Policy keeps in-engine voice and chat disabled (IVP L11). Therefore: **external team voice remains a stated MVP playtest requirement** for nuance, and the playtest protocol now records the in-game informant hook (the ping) as **tested** (it was "untested until VS" before this decision). Team text chat and in-engine voice are Vertical Slice candidates (OQ2).

**PNG-R12. Language.** Every ping string (the name label, and any future tooltip or binding label "Ping") avoids "spotted", "detected", "tracked", "enemy", "target", "dead", "ghost" and "spectator" (Roster and IIF language rules). The input is labelled **"Ping"** in bindings and glyph prompts.

### States and Transitions

**Per sender slot (server, in `FTeamPingView`):**

| State | Entered when | Leaves when |
|---|---|---|
| `Empty` | round start (`OnRosterReset`), travel, round-end step | an accepted ping from this sender → `Live` |
| `Live` | an accepted ping (PNG-R5) writes `{SenderHandle, PingSeq, Point, ServerTime}` | another accepted ping from the same sender → `Live` (overwritten); clear → `Empty`. Expiry is not a server transition: the slot keeps the stale entry, and clients age it out (F3) |

**Per ring (client presentation):**

| State | Entered when | Shows | Leaves when |
|---|---|---|---|
| `Hidden` | no unseen live entry | nothing | new `(SenderHandle, PingSeq)` with age < `PingLifetimeSec` → `Showing` |
| `Showing` (third 1, 2, 3) | arrival (UI tick plays once) | ring at the point with the name label, or clamped at the edge without a label; stroke 6 / 4 / 3 px by age thirds | age ≥ `PingLifetimeSec` → `Hidden`; same sender's new entry → `Showing` (new ring, old one removed at once); HUD enters `Ended`, rebuild or `RoundEpoch` change → `Hidden` |

### Interactions with Other Systems

| System | Flows into Pings | Flows out of Pings | Owner of the interface |
|---|---|---|---|
| Participant State & Roster (2) | sender `State`, team membership, `PublicHandle`; C-Public `DisplayName`; `OnRosterReset`; `RoundEpoch` (client rebuild detection, from the Roster owner part) | none | Roster |
| Round & Match Framework (6) | phase, latch, round-end step (clear) | none | Round |
| Information Visibility Policy (7) | the `TeamPing` fact row (Team audience) and the allowlist row for the new part | none | IVP |
| Input & Interaction Framework (3) | `IMC_Common` context, binding table (R15), release-safety rules (a ping press is not an outstanding `Begin`) | `IA_Ping` binding row | IIF (input), Pings (meaning) |
| Camera System (10) | aim ray `GetAimRay()` (CS-R8; the ping point rule itself is PNG-R3's), eye pivot for the range check (CS-R4), `GetViewState()` | none | Camera |
| HUD (18) | L0 layer, safe rectangle, UI scale, rebuild rule (HUD-R17) | ring and label view-model in the L0 ping slot | HUD (layout), Pings (content) |
| Teammate Markers (17) | the edge-clamp rule (F3) reused for off-screen rings; draw order (rings beneath markers) | none | Markers (rule), Pings (use) |
| NPC-Kill Penalty & Informants (14) | the informant contract PEN-R12 (what an informant may do) | the PEN-R12 row "talks to its team" now names the in-game ping | Penalty |
| Session & Travel (5) | host parity, travel, reconnect | none | Session |
| Tuning Data & Config (1) | the Pings keys (Tuning Knobs) | key registration | TDC |
| Telemetry (ADR-0023) | none | server stats: pings accepted and dropped by reason, per role (never per person in any client-visible form) | Pings |

## Formulas

*`systems-designer` not spawned (orchestrator limit for this run; see Core Rules). The four formulas are small and reuse the IIF token-bucket shape (IIF F3) and the art bible's ping parameters. Review manually before production.*

### F1. Ping rate budget (token bucket, per participant)

The ping_bucket formula is defined as:

`tokens(t) = min(PingBucketSize, tokens(t_last) + PingBucketPerSec · (t − t_last))`; on the server a ping is accepted only if `tokens(t) ≥ 1 − ε` (ε = 0.05 token, constant), and acceptance spends one token (the bucket may go to −0.05 at most, never lower). The bucket starts full at `Active` entry, is keyed by `ParticipantId` (it survives reconnects) and is reset at `OnRosterReset`. The client keeps a mirror of its own bucket from the same public keys, advanced on its own `EstimatedServerNow()` from its own send times, and sends only while its mirror is ≥ 1 (strict). Because the client is strict and the server tolerates ε (100 ms of refill at defaults), a press the client allows is not dropped by refill-boundary clock skew up to 100 ms.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| PingBucketSize | int | 1-5 | data file (2) | Burst size (tokens) |
| PingBucketPerSec | float | 0.1-2.0 | data file (0.5) | Refill rate (tokens per second) |
| t, t_last | double (s) | — | calculated | Server time now and at the last bucket update |

**Output Range:** burst of `PingBucketSize` pings, then one per `1 / PingBucketPerSec` s (2 s at defaults). Team ceiling (5 senders) at steady state: `5 × PingBucketPerSec` = 2.5 pings/s; worst burst `5 × PingBucketSize` = 10 accepted pings, but at most 5 rings shown (one slot per sender, PNG-R6): a sender's second burst ping replaces its first, so the burst exists to let a sender correct a misplaced ring at once, not to place two.
**Example:** an informant presses Ping 4 times within 1 s from a full bucket at defaults: pings 1 and 2 are accepted (tokens 2 → 1 → 0), presses 3 and 4 find the mirror below 1 and nothing is sent; the bucket reaches 1 token again 2 s after the second ping.

### F2. Range check (client at the press; server validation step 3)

The ping_range formula is defined as:

`accept_range = |P − E| ≤ PingRangeCm + PingRangeSlackCm`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| P | vector (cm) | world | RPC (10 cm quantised) | The ping point |
| E | vector (cm) | world | calculated (client at the press, server at validation) | Sender pawn's eye pivot (capsule-derived, Camera CS-R4; the same point in first and third person, never the camera) |
| PingRangeCm | float | 2000-15000 | data file (6000) | Client aim range for a ping |
| PingRangeSlackCm | float | 50-400 | data file (150) | Server tolerance for movement and quantisation between the press and validation |

**Output Range:** accepted within 61.5 m at defaults. A client point from a press made while walking at 140 cm/s with 150 ms RTT has moved at most 21 cm, far inside the slack; a runner (450 cm/s) moves at most 68 cm.
**Example:** sender eye at (0, 0, 165), point at (5,900, 1,000, 0): distance 5,986 cm ≤ 6,150 → accepted. Point at (6,200, 0, 0): 6,202 cm > 6,150 → dropped.

### F3. Ring age and stroke (client presentation)

The ping_age formula is defined as:

`a = max(0, EstimatedServerNow() − ServerTime)`; visible iff `a < PingLifetimeSec`; `stroke = 6 px if a < L/3, 4 px if a < 2L/3, else 3 px` (art bible 3.4 proposed weights; × UI scale), with `L = PingLifetimeSec`. Negative ages from clock skew count as 0.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| ServerTime | double (s) | — | `FTeamPingEntry` | Server time the ping was accepted |
| EstimatedServerNow() | double (s) | — | ADR-0002 §9 | Client's slewed server-clock estimate |
| PingLifetimeSec | float | 4-12 | data file (8; art bible "start 8 s") | Ring lifetime |

**Output Range:** each ring is visible at most `PingLifetimeSec`; on a receiver with one-way delay `d` it is visible `PingLifetimeSec − d` (≈ 7.95 s at 50 ms). An entry that arrives older than `PingLifetimeSec` (a long stall) is never shown.
**Example:** `PingLifetimeSec` 8; a ring accepted at server 120.00 and received at 120.06 is drawn at 6 px until 122.67, 4 px until 125.33, 3 px until 128.00, then hidden.

### F4. Bandwidth bound (per receiving connection)

The ping_bandwidth formula is defined as:

`B_conn = r_team · S_entry`, with `r_team ≤ TeamSize × PingBucketPerSec` at steady state.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| r_team | float (pings/s) | 0-10 | calculated (F1) | Accepted pings per second across the team |
| S_entry | int (B) | ≈ 20 | constant (estimate) | One changed slot of the fixed-size array (per-element delta, PNG-R6): handle (1-2 B), seq (1 B), quantised point (≤ 9 B), server time (8 B), plus property header. A dynamic array without a fast-array serializer would resend all slots (≈ 100 B) and is not allowed |
| TeamSize | int | 1-5 | data file (5, Round) | Members per team |

**Output Range:** ≤ 2.5 × 20 = 50 B/s per teammate connection at defaults in steady state; a one-time full burst adds at most 10 × 20 = 200 B, so a 60 s window averages ≤ 54 B/s; the AC-PNG-11 limit is 80 B/s to leave headroom for header estimates. Negligible against the 50 KB/s per-client budget (ADR-0003). Opponent connections: 0 B (never relevant).
**Example:** a team where two informants each ping every 2 s: r = 1.0 ping/s → about 20 B/s to each of the five teammates.

## Edge Cases

*`systems-designer` not spawned (see Core Rules).*

- **If the sender is eliminated (strike, reversal or exile) while its ping is live:** the ring stays until it expires (PNG-R10); after the sender's own removal presentation it may ping again as an informant once it has a body (PNG-R2). The ring itself never changes on the sender's elimination, so it cannot announce the elimination earlier than the HUD's removal line (HUD-R10).
- **If a ping is pressed in the window between a strike's slot and its contact on the sender (the sender is already eliminated on the server but does not know):** server step 2 still sees `Active` until the `Informant` transition is written; if the state is already `Informant`, an informant may ping, so the ping is accepted either way and is identical. No reply exists, so nothing tells the victim early (ASN-R6).
- **If an informant is awaiting a takeover (no pawn, neutral view):** the client sends nothing (no aim); a forged RPC fails step 2 (no bound pawn) and is dropped silently.
- **If the sender's pawn is concealed in a bush:** an Active player may ping from inside a bush. Nothing changes on its pawn or on opponents' clients (PNG-R9), so pinging cannot break concealment and is not an `OnConcealmentBroken` trigger (Concealment listens only to the Attack action event).
- **If the aim ray points into a bush, at a pawn or at the sky:** PNG-R3's rule applies: the `ViewBlock` trace passes through pawns (no pawn is ever hit, so none can be named), a hit in foliage drops to the walkable ground below within 300 cm, and with no hit or a point beyond `PingRangeCm` from the eye pivot (sky, a far roof edge) nothing is sent.
- **If the camera is in third person:** the aim ray starts at the boom (about 185 cm behind the eye), but range is always measured from the eye pivot on both machines (PNG-R3, F2), so a point the client accepts is at most `PingRangeCm` from `E` and passes the server check with the full `PingRangeSlackCm` left for movement.
- **If the sender is an informant who is killed while its ring is live:** the ring stays to expiry (PNG-R10); the informant cannot ping again until it has a body (PNG-R2). Nothing about the ring changes at the kill, so it announces nothing.
- **If a sender reconnects:** its token bucket is unchanged (keyed by `ParticipantId`, F1), so disconnecting never refills pings.
- **If the ping lands on a bush where an enemy is concealed:** the sender could not see the concealed enemy (it is culled for the sender, IVP R7), so the ping carries no knowledge of it. Allies' rings over a bush reveal nothing the sender did not already know.
- **If an Active player pings repeatedly near itself:** allies learn where a teammate is, which they already know from its marker. Opponents learn nothing (no carrier). No restriction.
- **If a player spams pings to annoy its own team:** the bucket caps it at one per 2 s after a burst of 2, and one slot per sender keeps at most one of its rings on screen. Griefing one's own team is otherwise out of scope for the MVP (concept Design Risks; OQ1).
- **If two teammates ping within the same server frame:** both slots change and replicate in one coalesced update; both rings appear on the same frame on each client.
- **If a ping's server time is ahead of the client's estimate (clock slew):** `a` is clamped to 0 (F3), so the ring shows for at most `PingLifetimeSec` and never more.
- **If the receiving client stalls longer than `PingLifetimeSec`:** entries that are already expired on arrival are marked seen and never shown (F3).
- **If the round ends (latch) while a ping RPC is in flight:** step 1 drops it; the round-end step clears every slot, and clients hide every ring on entering `Ended` (PNG-R10), so no ring is ever visible over the held pawns at `Ended` (Round R9 (f): the city holds; nothing points at anyone).
- **If a teammate disconnects or reconnects:** the disconnected player's own live ping expires normally; a reconnecting client rebuilds with every present entry marked shown (no replay, HUD-R17); entries accepted after the rebuild show normally.
- **If the sender's name is unresolved on a receiver (late C-Public):** the label reads "Ally"; it updates if the name arrives while the ring is showing.
- **If `TeamSize` is 1 (test preset):** the sender's own controller still receives its echo; the ping shows only on its own screen.
- **If the listen host pings:** the host's local controller calls the same server function, passes the same validation and bucket, and receives its own echo through the same C-TeamView part path as a remote client (host parity, AC-PNG-09).
- **If a modified client forges points (outside range, NaN):** server step 3 drops them without spending a token; a ping through walls within range is accepted (no LOS check, PNG-R5) and only affects its own team.

## Dependencies

### Upstream (Pings depend on)

| System | What Pings use | Hard / Soft | Status of that GDD | Bidirectional? |
|---|---|---|---|---|
| Participant State & Roster (2) | sender `State`, team, bound pawn, `PublicHandle`; C-Public `DisplayName`; `OnRosterReset`; C-TeamView as the carrier family (R6, feature parts) | Hard | Reviewed; reconciled | Yes: Roster R6 lists the ping part among feature parts (user decision 2026-10-25 row added there) |
| Information Visibility Policy (7) | Team audience; new fact row `TeamPing`; allowlist row for `UTeamPingViewComponent` | Hard | Reviewed; reconciled | Yes: IVP R3 row and Downstream row updated (user decision 2026-10-25) |
| Round & Match Framework (6) | phase `Active`, latch, round-end step (clear) | Hard | Reviewed; reconciled | Yes: Round R9 (a) clears the ping parts in the round-end step and its change table records it (user decision 2026-10-25) |
| Input & Interaction Framework (3) | `IMC_Common`, binding table R15 (new `IA_Ping` row), release-safety rules | Hard | Reviewed; reconciled | Yes: IIF R13/R15 add the ping row (user decision 2026-10-25) |
| Camera System (10) | `GetAimRay()` (CS-R8; the ping-point rule is PNG-R3's own), eye pivot (CS-R4), `GetViewState()` (CS-R15) | Hard | Reviewed; reconciled | Camera's consumers of `GetAimRay()` are IIF and the strike hint; Pings are a third consumer of the same function (no Camera change) |
| HUD (18) | L0 ping slot, safe rectangle, UI scale, rebuild rule HUD-R17 | Hard | Reviewed; reconciled | Yes: HUD-R11 ping slot now built (user decision 2026-10-25) |
| Teammate Markers (17) | edge-clamp rule (F3) and draw order | Soft | Reviewed; reconciled | Yes (Markers Downstream row) |
| NPC-Kill Penalty & Informants (14) | the informant contract PEN-R12 | Hard | Reviewed; reconciled | Yes: PEN-R12 "talks to its team" row updated (user decision 2026-10-25) |
| Session & Travel (5) | host parity, travel and reconnect events | Soft | Reviewed; reconciled | One-directional |
| Tuning Data & Config (1) | registration of the five Pings keys | Hard | Reviewed; reconciled | Yes: TDC registered-owner row added (user decision 2026-10-25) |

### Downstream (these depend on Pings)

| System | What it needs | Hard / Soft | Status |
|---|---|---|---|
| Event Feedback & Cues (23, VS) | the arrival tick as a cue anchor; the reserved sighting feed-line type | Soft | Not written |
| Settings & Accessibility (24, VS) | rebinding of `IA_Ping`; an optional ping-sound volume | Soft | Not written |
| Telemetry & Analytics (26, Alpha) | the accepted and dropped ping stats (Fellowship and informant-agency metrics) | Soft | Not written |

## Tuning Knobs

Registered under owner `InformantPings` (TDC CR-15 columns). **CR-15.8 identity-tell review:** no key varies by role, team, kind or outcome; the rate keys are one budget for every sender (Active and Informant alike); no key times anything opponents can observe. Pass.

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / what breaks |
|---|---|---|---|---|---|---|---|---|---|
| `PingLifetimeSec` | float | InformantPings | 8.0 | 4.0-12.0 | 6.0-10.0 | Client | Use | s | F3; art bible 3.4 "start 8 s". Low: a ring vanishes before an ally can walk there and look. High: stale rings point at places where the cluster has dispersed, and rings start to read as a sticky mark (concept decision 4). |
| `PingBucketSize` | int32 | InformantPings | 2 | 1-5 | 2-3 | All | Round | Count | F1 burst. The burst only serves correction (one slot per sender, so a burst never shows two rings). Low (1): correcting a misplaced ring costs a 2 s wait. High: a sender can re-place its ring several times in quick succession, which reads more like a moving mark. |
| `PingBucketPerSec` | float | InformantPings | 0.5 | 0.1-2.0 | 0.33-0.5 (while `PingLifetimeSec` ≥ 6, I-PNG-1) | All | Round | PerSec | F1 refill. Low: informants feel muted (Fellowship). High: spam, and re-marks frequent enough to read as a stepped track behind a walker (I-PNG-1 warns above 0.5). |
| `PingRangeCm` | float | InformantPings | 6000 | 2000-15000 | 4000-8000 | All | Round | Cm | F2 and the client's aim range. Low: an informant must stand close to the cluster it marks (its own exposure grows). High: pings across the whole district from one rooftop-less street, which the 4 m ring then makes imprecise. |
| `PingRangeSlackCm` | float | InformantPings | 150 | 50-400 | 100-200 | ServerOnly | Round | Cm | F2 server tolerance. Low: honest pings at max range dropped for movement skew. High: forged far pings accepted. |

**Cross-field invariant** (registered in TDC): **I-PNG-1** (warning) `PingBucketPerSec ≤ 0.5` whenever `PingLifetimeSec ≥ 6`. It **bounds the cadence** of one sender's rings (sustained one per 2 s after a burst of `PingBucketSize`); it does **not** prevent manual shadowing. A walker covers about 2.8 m in 2 s, inside the 4 m ring, so an informant who keeps re-marking a walking figure can keep it covered. That is intended informant work (concept Key Dynamics: "shadow suspects and ping sightings"): it costs the sender continuous attention and its own exposure as a figure that follows another, and it never produces a sticky marker (each ring is a static place that expires; the system never follows anyone). Whether chains of re-marks become a de facto tracker is OQ7.

**Constants (not keys; a change is a GDD and art bible change):** ring world radius ≥ 4 m, screen minimum 32 px, 60° gap, stroke 6 / 4 / 3 px by thirds, keyline 3 px (4 px beyond 20 m) (art bible 3.4, 7.3 row 9); one slot per sender; one ping kind.

**Referenced, owned elsewhere:** `TeamSize` (Round), `HudSafeMarginPct` and UI scale (HUD), the Teammate Markers edge clamp (TM F3), `EstimatedServerNow()` (ADR-0002 §9).

## Visual/Audio Requirements

*`art-director` not spawned (orchestrator limit). The art bible already specifies the sighting ping (3.4 shape table, 4.6 Marigold, 7.3 row 9, 7.5 no arrows, 7.x rule 2), so this section binds it rather than designing it. Review manually before production.*

- **VA-PNG-1 Ring.** Open ring with a 60° gap, placed flat on the ground at the ping point; world radius ≥ 4 m (cluster scale), screen minimum 32 px; Marigold with a mandatory Obsidian keyline (3 px, 4 px beyond 20 m); **no centre mark**; static after placement (no pulse, bob, spin or scale pop, art bible 7.6); age by stepped stroke weight only (F3), never by opacity (art bible 3.4: "opacity is never a state encoding"). The gap's orientation is fixed in world space (north) so it never "points" at anything.
- **VA-PNG-2 Edge ring.** Off-screen or behind the camera: the same broken ring at 32 px, clamped to the HUD safe rectangle, static, unrotated, never an arrow (art bible 7.5), using the Teammate Markers F3 clamp. Edge rings draw beneath edge markers.
- **VA-PNG-3 Name label.** The sender's display name in Bone text (18 px floor, HUD-R16) on a plate with the ally chrome (Turquoise keyline; art bible 4.6 permits Turquoise for "ally-only chat/ping chrome"), placed just below the ring's screen-space bottom; ellipsised at the plate width; never larger than the ring.
- **VA-PNG-4 Draw order.** In HUD L0: rings and their labels beneath teammate markers; above the 3D scene with no depth test (like markers, so a ring behind a wall is still readable to the team; the ring is allies-only, so this costs nothing under P1).
- **VA-PNG-5 Audio.** One quiet 2D UI tick on the UI bus when a new ring appears on a receiving client (the sender included, on the echo). Never positional, never routed into the world mix, never audible to anyone outside the team (no world sound exists to hear). No sound on expiry. The audio-director supplies the asset with the HUD cue list (HUD OQ11).
- **VA-PNG-6 Nothing on the body.** No pose, gesture, hand raise, head nod, VFX or sound on the sender's pawn (PNG-R9). NPCs never "ping", so any body-side ping tell would be a player-only behaviour (Crowd R4): none exists.
- **Accessibility.** The ring is distinguishable from the teammate marker by shape (ring vs pentagon) and from the interact ring by size and placement (ground vs object), not by colour alone; the tick has the ring as its visual twin and the ring has the tick as its audio twin.

> 📌 **Asset Spec** — Visual/Audio requirements are defined. After the art bible is approved, run `/asset-spec system:informant-pings-team-comms` to produce per-asset visual descriptions, dimensions, and generation prompts from this section.

## Game Feel

- **Responsiveness.** Press → own ring visible after one server round trip (no local prediction, PNG-R7): target p95 ≤ own RTT + 50 ms (≈ 110 ms at 60 ms RTT). Teammates see it after their own one-way delay plus at most one server frame. A ping is a note, not an action: it needs to be prompt, not instant.
- **Weightlessness.** Pinging never slows, stops, turns or interrupts the body (it is not a command, PNG-R4), so it can be done mid-stride, mid-tend or mid-brace with no feel change. There is no aim assist, snapping or magnetism; the ring lands where the camera looks, on the ground.
- **Refusal feel.** A refused press (no ground in range, empty budget, no body) does nothing at all in the MVP. This is the deliberate low-noise choice; OQ3 asks whether a soft local "not ready" tick is needed after the first playtest.
- **Not a reflex tool.** The 2 s refill (F1) and the 4 m cluster ring keep pings in the register of "look over here", not live targeting. The ring never follows anyone.

## UI Requirements

| # | Element | Shown to | Data (carrier) | Anchor / layer | Rule |
|---|---|---|---|---|---|
| PNG-UI-1 | Ground ring | the sender's team (Active and Informant viewers, the sender included) | `FTeamPingEntry` (C-TeamView ping part) | world point on the ground, HUD L0 ping slot | PNG-R7, F3, VA-PNG-1 |
| PNG-UI-2 | Edge ring | same | same | HUD safe rectangle edge, L0 | PNG-R7, VA-PNG-2 |
| PNG-UI-3 | Sender name label | same | C-Public `DisplayName` by `SenderHandle` | below the ring, L0 | PNG-R7, VA-PNG-3 |
| PNG-UI-4 | "Ping" binding glyph | own player | binding (client-local) | controls reference only (no prompt on screen in the MVP) | PNG-R3, PNG-R12 |

No element exists for opponents; no element is created, sized, timed or positioned by a non-ally pawn (HUD-R2). All widgets are created at HUD init and toggled (ADR-0021).

> 📌 **UX Flag — Informant Pings & Team Communication**: This system has UI requirements. In Phase 4 (Pre-Production), run `/ux-design` for `design/ux/hud.md` (the L0 ping slot, edge rings and labels) **before** writing epics. Stories that reference the ping UI should cite `design/ux/hud.md`, not this GDD directly.

## Cross-References

| This GDD references | Document | Section | Relationship | Status |
|---|---|---|---|---|
| User decision 4 (momentary pings + team chat, no sticky tracking marker); MVP Definition (informant ping in MVP, user decision 2026-10-25); Fellowship aesthetic; Design Risks (ping spam) | design/gdd/game-concept.md | Pending User Decisions, MVP Definition, MDA, Risks | Implements | Approved concept |
| Row 20 (now MVP, user decision 2026-10-25) | design/gdd/systems-index.md | Systems Enumeration | Owns row 20 | — |
| C-TeamView carrier family, feature parts, `PublicHandle`, `DisplayName`, `OnRosterReset` | design/gdd/participant-state-roster.md | R6 | Rides the carrier | Reviewed; reconciled |
| R3 matrix (new `TeamPing` row), R4 carriers, L11 voice/chat | design/gdd/information-visibility-policy.md | Core Rules | Obeys; adds a fact row | Reviewed; reconciled |
| R9 latch and round-end step | design/gdd/round-match-framework.md | Core Rules | Reads; clears at the round-end step | Reviewed; reconciled |
| R4 edges, R13 contexts, R15 bindings, language rule | design/gdd/input-interaction-framework.md | Core Rules | Adds `IA_Ping` | Reviewed; reconciled |
| CS-R4 eye pivot, CS-R8 aim ray and ground point, CS-R15 view state | design/gdd/camera-system.md | Core Rules | Consumes | Reviewed; reconciled |
| HUD-R2, R4 L0, R11 ping slot, R16 text floors, R17 rebuild | design/gdd/hud.md | Core Rules | Draws in the HUD layer | Reviewed; reconciled |
| TM F3 edge clamp, TM-R9 draw rules | design/gdd/teammate-markers.md | Formulas, Core Rules | Reuses the clamp | Reviewed; reconciled |
| PEN-R12 informant contract, OQ-NKP-7 | design/gdd/npc-kill-penalty-informants.md | Core Rules, Open Questions | Fulfils the informant's team channel | Reviewed; reconciled |
| Tell Matrix T13 (gaze), R4 (no NPC-only or player-only behaviour) | design/gdd/crowd-npc-behaviour.md | Core Rules | Obeys | Reviewed; reconciled |
| Rule 3 host parity, rule 24 | design/gdd/session-travel.md | Core Rules | Obeys | Reviewed; reconciled |
| CR-14, CR-15 | design/gdd/tuning-data-config.md | Detailed Design | Registers keys | Reviewed; reconciled |
| 3.4 sighting ping, 4.6 Marigold/Turquoise, 7.3 row 9, 7.5, 7.6, 7.x rule 2 | design/art/art-bible.md | Shape language, palette, UI | Obeys | Approved |
| ADR-0001 §5 (carrier part), ADR-0011 §9 (input, controller RPC), ADR-0021 (L0 drawing), ADR-0002 §9 (clock), ADR-0003 (bandwidth), ADR-0023 (stats) | docs/architecture/ | Decision | Implementation homes (amended 2026-10-25) | Accepted |

## Acceptance Criteria

*`qa-lead` not spawned (orchestrator limit); the criteria follow the HUD/IVP determinism rule (fixed 60 Hz step, identical seeds, dumps by tick index). Tags: `[Logic]` automated unit test, `[Integration]` automated multi-client PIE test, `[UI]` retained screenshot in `production/qa/evidence/`; gate `BLOCKING` unless stated. Tests use the `StealthGame.Pings.*` prefix (core in `StealthGameplay`, presenter in `StealthPresentation`, multi-client in `StealthTestHarness`; ADR-0007/0008).*

**Pillar 1 (leak and parity)**
- **AC-PNG-01** [Integration/BLOCKING] `StealthGame.Pings.PIE.OpponentSeesNothing` (PNG-R6, R9): GIVEN a 2v2 PIE with a team-0 informant and a team-0 Active player each pinging 3 times, run once with a team-0 player as the listen host and once with a **team-1 player as the listen host**, WHEN team-1 clients' replicated-property logs, received RPC lists, HUD view-model dumps and audio-event logs (and, in the second run, the team-1 host's own HUD view-model and audio dumps) are compared with a control run with no pings, THEN they are identical on every tick (no ping part, entry, widget, sound or RPC on a team-1 machine's presentation), and both team-0 allies' dumps show each ring.
- **AC-PNG-02** [Integration/BLOCKING] `StealthGame.Pings.PIE.PawnParity` (PNG-R4, R9): GIVEN a sender standing still and, in a second run, walking a fixed path, WHEN it pings 5 times, THEN the sender pawn's replicated state as received by an opponent client (`FActionState`, movement, head-look, net-update times, relevancy) is byte-identical to the control run without pings, no `ForceNetUpdate` occurs on it, and no resolution-queue item or IIF command is created; the captured `ServerTeamPing` payload is exactly one quantised `Point` (no pawn id, no other field).
- **AC-PNG-03** [Logic/BLOCKING] `StealthGame.Pings.AuditCarrier` (PNG-R6): GIVEN the build, WHEN the IVP R11 replicated-property audit runs, THEN `UTeamPingViewComponent` is registered only on `AStealthPlayerController`, its fields match the IVP allowlist row (Team audience), and no ping field exists on `AStealthPawn`, `AStealthPlayerState` or the GameState.

**Sending and validation**
- **AC-PNG-04** [Logic/BLOCKING] `StealthGame.Pings.Validation` (PNG-R2, R5, F2): GIVEN the server handler with an injected Roster, Round and clock, and **a full bucket at the start of each case** (each case is a separate fixture), WHEN a ping arrives with, in turn, phase `Preparing`, phase `Active` after the latch, sender `RoundEnded`, an informant with no bound pawn, a point 6,151 cm from the eye pivot, a point 6,149 cm from the eye pivot, and a NaN point, THEN only the 6,149 cm ping is accepted, every other one is dropped with its own reason counted (phase, latch, state, pawn, range, range, non-finite) and no reply sent, and the bucket of every dropped case still holds 2 tokens (failures before step 4 spend nothing); an accepted ping from an `Active` sender and one from an `Informant` sender produce entries of identical shape.
- **AC-PNG-05** [Logic/BLOCKING] `StealthGame.Pings.Bucket` (F1): GIVEN defaults (size 2, 0.5/s, ε 0.05) and a full bucket at t = 0, WHEN pings arrive at t = 0.0, 0.1, 0.2, 2.1, 2.2 and 6.0, THEN the pings at 0.0, 0.1, 2.1 and 6.0 are accepted and 0.2 and 2.2 are dropped; the client mirror suppresses sending at 0.2 and 2.2; AND GIVEN an empty bucket (0.0) at server time 10.0, a ping arriving at server time 11.90 (tokens 0.95) is accepted and one at 11.89 (0.945) is dropped, while a client mirror running 0.1 s behind allows its first press only at its 12.0, which the server accepts; the bucket is unchanged across a disconnect and rejoin and full again after `OnRosterReset`.
- **AC-PNG-06** [Integration/BLOCKING] `StealthGame.Pings.PIE.InputContexts` (PNG-R3): GIVEN an Active player and an informant with a body, WHEN each presses `IA_Ping` aimed at the ground 20 m away, THEN each sends exactly one `ServerTeamPing` and nothing through `ServerPawnCommand`; WHEN the informant presses it while awaiting a takeover (neutral view), or either aims at the sky, THEN nothing is sent and nothing is shown; a press during a held Interact or Brace does not end or alter that hold.

**Display**
- **AC-PNG-14** [Integration/BLOCKING] `StealthGame.Pings.PIE.EchoLatency` (Game Feel): GIVEN a 2-client PIE with 60 ms emulated RTT on the sender and 1 % loss off, WHEN the sender pings 20 times at 2.5 s intervals, THEN the p95 time from the `IA_Ping` press to the first presenter frame showing the own ring is ≤ 110 ms, and a teammate on 60 ms RTT shows each ring ≤ 80 ms after the server's acceptance (one-way delay plus one server frame).
- **AC-PNG-07** [Logic/BLOCKING] `StealthGame.Pings.RingAge` (F3, PNG-R7): GIVEN `PingLifetimeSec` 8 and an entry with `ServerTime` 120.00, WHEN the presenter samples at estimated server times 120.06, 122.66, 122.68, 125.34, 127.99 and 128.00, THEN it shows stroke 6, 6, 4, 3, 3 px and then hidden; an entry received at age 8.2 s is never shown; a second entry from the same sender replaces the first ring on the frame it arrives; a clock estimate 0.3 s behind `ServerTime` gives age 0.
- **AC-PNG-08** [Integration/BLOCKING] `StealthGame.Pings.PIE.LifecycleNoReplay` (PNG-R10): GIVEN live rings on a client, WHEN (a) the round ends by an objective win, (b) the client disconnects and rejoins inside the grace, (c) a new round starts, THEN in (a) every ring is hidden on the client's `Ended` entry tick and the server part is empty after the round-end step; in (b) and (c) no ring present before the rebuild reappears, and a ping accepted after the rebuild shows normally.
- **AC-PNG-09** [Integration/BLOCKING] `StealthGame.Pings.PIE.HostParity`: GIVEN the listen host and a remote client on the same team, WHEN the host pings and then the client pings, THEN both pings pass the same validation and bucket, and both machines' ordered ring event lists (timestamps excluded) are identical.
- **AC-PNG-10** [UI/BLOCKING] Run-and-observe: a 2v2 PIE round in which an informant pings a market cluster produces retained screenshots on an ally's screen of the on-screen ring with the name label at each of the three stroke weights, an edge ring for a ping behind the camera (no label), and the same moment on an opponent's screen showing nothing (coding standards: "a parse check is not a run"); AND greyscale versions of captures showing a ring, a teammate marker and an interact ring are sorted correctly by 3 of 3 reviewers who did not author the UI (a blind sort, recorded in the evidence note).

**Bandwidth and language**
- **AC-PNG-11** [Integration/ADVISORY] `StealthGame.Pings.Bandwidth` (F4): GIVEN a 5v5 test round where all ten players ping at the maximum rate for 60 s, WHEN per-connection outgoing bytes are profiled, THEN the ping part adds ≤ 80 B/s per receiving connection averaged over the 60 s and 0 B to every opponent connection, and a single changed slot costs ≤ 32 B on the wire (per-element delta, PNG-R6).
- **AC-PNG-12** [Logic/BLOCKING] `StealthGame.Pings.Strings` (PNG-R12): GIVEN the string table entries used by pings and the binding label, WHEN scanned, THEN none contains spotted, detected, tracked, enemy, target, dead, ghost or spectator, and the binding label reads "Ping".

**Playtest metric (Fellowship, advisory)**
- **AC-PNG-13** [Playtest/ADVISORY] `FellowshipPingUse` (concept Review Follow-up 5; OQ6): GIVEN the first MVP playtest sessions with informants, WHEN server ping stats (ADR-0023) and positions are analysed, THEN the report gives pings per informant-minute and the share of informant pings followed within 10 s by an ally's pawn entering 8 m of the ring centre; pass when that share is ≥ 40 % (placeholder target, confirmed in the playtest plan before the session); a share below 20 % opens OQ1 and OQ8 (pings are not acted on, and Fellowship #2 is under-delivered).

*Coverage (revised at design review 2026-10-25):* Game Feel latency (AC-PNG-14); PNG-R1 (AC-PNG-07, -10), R2 (-04, -06), R3 (-06), R4 (-02, -06), R5 (-04, -05), R6 (-01, -03), R7 (-07, -10), R8 (-01: no feed line in the dumps), R9 (-01, -02), R10 (-08), R11 (documented decision; no test), R12 (-12); F1 (-05), F2 (-04), F3 (-07), F4 (-11).

## Open Questions

| # | Question | Owner | Resolve by |
|---|---|---|---|
| OQ1 | Ping spam and griefing of one's own team (concept Design Risks: "informants spamming pings"): are the bucket (one per 2 s) and one slot per sender enough? First-playtest metric: pings per informant-minute and the share of rounds with a team-reported ping complaint. | game-designer | first playtest |
| OQ2 | Team text chat and in-engine voice: Vertical Slice candidates (PNG-R11). If added, text must come from string tables (canned callouts) or the "no text on the wire" rule (ADR-0021) needs a superseding decision with moderation. | user / creative-director | VS planning |
| OQ3 | Should a refused press give a soft local "not ready" tick (owner-only, 2D)? MVP gives no feedback. Decide after the first playtest. Related: the Ping key has no on-screen prompt in MVP, so discoverability relies on the controls reference and playtest briefing (Onboarding, Alpha). | ux-designer | first playtest |
| OQ4 | A second ping kind (for example "danger here" vs "look here")? Out of MVP scope; a second glyph must stay a place and pass the art bible's shape language. | creative-director | VS |
| OQ5 | Ring scale at both extremes: is 4 m too coarse in a dense market (the ring may cover 8-10 figures), and is it effectively an identification in sparse streets (one figure inside)? Playtest both; the radius is an art bible constant, not a key. | game-designer + art-director | first playtest |
| OQ6 | Telemetry for AC-PNG-13: log ping placements server-side (`NeverReleased`, ADR-0023 probe) to measure ally follow-up within 10 s / 8 m and how often a ping precedes a correct strike within 30 s near the ring. | analytics (ADR-0023 owner) | before the first MVP playtest |
| OQ7 | Quasi-tracking: share of ping chains (same sender, each ring within 4 m of its previous ring, ≤ 3 s apart) in playtests. If chains are common and read as a tracker, lengthen the refill or cap chains. | game-designer + creative-director | first playtest |
| OQ8 | If playtests show Active players' pings replacing crowd-reading (Active pings far outnumber informant pings and precede most strikes), restrict pings to informants (PNG-R2 P2 justification). | creative-director | first playtest |
