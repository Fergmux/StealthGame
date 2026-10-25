# Teammate Markers

> **Status**: Reviewed - NEEDS REVISION 2026-10-25 (second-round lean re-review; the 2026-10-24 blockers are resolved, one new formula blocker (F3 camera-plane discontinuity) revised in place; third review pending); reconciled 2026-10-24; ADR-synced 2026-10-24; user decisions applied 2026-10-25 (`RetireAtServerTime` adopted, feed sequence adopted so F5 is active, `bPawnBound` declined; scope: `ShowTeamNames` hold moved to VS (OQ11), "reconnecting" label suffix in MVP (OQ1), effort M (OQ12); Informant Pings reuses the F3 clamp)
> **Author**: design pipeline (autonomous run) + specialist agents
> **Last Updated**: 2026-10-25
> **Last Verified**: 2026-10-24
> **Implements Pillar**: P1 Hide in Plain Sight (enforcement: markers exist only for allies); Fellowship aesthetic (#2). Neutral to P2 (see Player Fantasy)
> **Design Review**: 2026-10-24 full review NEEDS REVISION, blockers revised; 2026-10-25 lean re-review NEEDS REVISION, F3 fix and AC alignment applied (see `design/gdd/reviews/teammate-markers-review-log.md`); re-review pending
> **Creative Director Review (CD-GDD-ALIGN)**: CONCERNS -> REVISED 2026-10-24 (items 1, 4, 5, 6 applied: P2 claim corrected to neutral/by-exclusion, attention playtest criterion AC-TMK-29, release-hold accepted explicitly, OQ1 wording neutral; items 2-3 are scope changes, recorded as pending user decisions OQ11-OQ12)

## Summary

Teammate Markers draws a turquoise pentagon above each of your teammates (filled when active, hatched when hidden in a bush, outline-only when an informant) so that, among fifty identical citizens, you always know where your own people are. It is built on each client only from that player's own team view, which the server sends to teammates alone, so an opponent's client never holds a marker or any team-identifying data. It exists for the Fellowship of a 5v5 team and to make the locked rule "markers exist only on teammates' clients" provable by automated leak tests.

> **Quick reference** — Layer: `Presentation` · Priority: `MVP` · Key deps: `Participant State & Roster, Information Visibility Policy, Concealment (Bushes), Camera System, Tuning Data & Config (soft: Assassination, SCL, Round, Session, Penalty); dependents IIF (IsMarkedAlly), HUD, Informant Pings, Post-Round Reveal, Settings`

## Overview

Teammate Markers is the client-local presentation system that draws one turquoise, downward-pointing pentagon above each of the viewer's teammates, and nothing above anyone else. It is the only person-anchored element in the game before the round is decided (art bible 7.1 rule 1). It shows three states of the marked ally, independent of the viewer: **filled** (Active), **hatched** (concealed in a bush, where the ally sees no body), and **outline-only** (Informant). It is drawn always on top at a constant pixel size and is clamped to the screen edge when the ally is off-screen.

**Data layer.** The system owns no replicated data. Its single input is the viewer's own **C-TeamView** (Roster R6), which the server copies only into each teammate's owner-only PlayerController (Information Visibility Policy R4, R8): per teammate `{PublicHandle, State, PawnRef}`, the Concealment `IsConcealed` flag, and the Policy's quantised fallback position feed when a `PawnRef` is unresolved. An opponent's client never receives a teammate entry for your team, so it never creates a marker, never holds a marker widget and never holds a team-identifying field; the locked rule "markers exist only on teammates' clients; enemies cannot obtain them" is enforced by the absence of data, not by hiding a widget. The system also never iterates or queries the world's pawns: it can only reach a pawn through a `PawnRef` that the server put in this viewer's team view, so it cannot attach a marker to an NPC or an opponent even by a bug.

**Player impact.** Without markers the player cannot find their four allies among fifty identical citizens, cannot split roles around an objective, and cannot avoid striking a teammate; the Fellowship aesthetic (#2) and the first blending test both depend on it. With leaky markers (the prototype computes them from a team field replicated on every pawn) Pillar 1 fails outright, because any client could mark every player. The system exists to give allies perfect knowledge of each other and to give opponents none.

It is deliberately small: no audio, no distance or line-of-sight fade, no enemy marking, no "reconnecting" marker state (a reconnecting ally keeps its filled marker; only its name label gains a "reconnecting" suffix, TM-R11, user decision 2026-10-25), and no settings beyond the hooks that the VS Settings & Accessibility system will expose. Implementation mechanism (UMG overlay versus HUD canvas, C-TeamView struct layout) is ADR material (-> ADR-001 for carriers, -> UI ADR for the overlay).

## Player Fantasy

**"The nod across the plaza."** In a city where every face is the same face, you always know where your people are, and only your people know where you are.

The marker is the one thing in the crowd that does not lie. Everything else on screen is a reading problem: that porter might be a rival, that woman at the well might be a citizen. The pentagon above your ally is certainty, and it is certainty only *you* hold. The enemy standing next to you sees your teammate as one more citizen; you see a small turquoise promise that someone on your side is there.

*Anchor moments* (each mapped to an acceptance criterion):
1. **The watch over an empty bush.** Your ally slips into the leaves. Their body vanishes on your screen at the same instant it vanishes on the enemy's, but their marker stays and turns hatched. You keep watch over a bush that, for everyone else, holds nobody (AC-TMK-10, 11).
2. **The thread through the wall.** You lose sight of your teammate behind a market stall; the pentagon stays exactly where they are, through stone, at the same size as ever. You split up to cover two approaches to the objective without a word (AC-TMK-14, 17).
3. **The informant's outline.** Your teammate is struck down. Their marker leaves as the blow lands, and a little later an outline pentagon appears over a stranger near your spawn: they are back, as a spy in the crowd (AC-TMK-12, 13).

*Candidate framings considered* (specialist panel, creative-director lens): "The thread between us" (the marker as a promise someone has your back) and "The one thing that doesn't lie" (certainty in a crowd of doubt). The chosen framing folds both into the plaza nod because it names the asymmetry the system exists to enforce: perfect knowledge for allies, none for opponents.

*Pillars and aesthetics served:*
- **Fellowship (aesthetic #2):** markers are what make five strangers in a crowd a team (concept MDA table: "teammate-only markers").
- **P1 Hide in Plain Sight:** the fantasy holds only because an opponent never holds a marker, a marker widget or a team field (locked rule).
- **P2 Every Advantage Is a Tell (neutral):** the marker helps you read a non-ally only by exclusion: it removes your allies from the candidates. At 5v5 among about 50 citizens that is negligible, and it is inherent to having teammates (OQ8, AC-TMK-28). Every unmarked figure stays a reading problem.

*The marker must never feel like* a radar blip, a tracker, a highlight or a tool for hunting: it marks friends, never targets, and it never moves, pulses or speaks on its own.

## Detailed Design

*Specialists consulted (one combined panel, budget rule): game-designer, ux-designer, ui-programmer / UE UMG feasibility, art-director, technical-artist, ue-replication / security, systems-designer, qa-lead. The audience and carrier model is the Information Visibility Policy's (IVP R4, R8, R9) and the Roster's (R6); this GDD implements it on the client and does not re-decide it. UE API details marked [verify] were not checked against the installed 5.8.4 source.*

### Core Rules

**TM-R1. One source: the viewer's own C-TeamView.** The marker set on a client is built from exactly two inputs, both carried only in the local player's own PlayerController (owner-only relevancy, Roster R6, IVP R4):
- the **teammate entries** of C-TeamView: `{PublicHandle, State, PawnRef, IsConcealed}` per teammate (Roster R6 lists the first three; the `IsConcealed` entry flag is IVP R3 row `IsConcealed` and Concealment CON-R2; change request CR-1 asks the Roster to list it in R6);
- the Policy's **fallback position feed** (IVP R8, F6): `{PublicHandle, Pos}` for a teammate whose pawn exists on the server but whose `PawnRef` does not resolve on this client. As reconciled in IVP R8 (2026-10-24, CR-2 and CR-11 a-b): `Pos` is the pawn's feet (visual root), and the trigger is evaluated per connection (the pawn exists on the server but is outside that connection's replication scope).

Nothing else is an input: no pawn field, no PlayerState field other than the public name for the label (TM-R11), no GameState field other than the public tuning snapshot, no server query. The system **adds no replicated property, no RPC, no replicated actor, no component on any pawn (including a `UWidgetComponent`), no tag, no material parameter and no stencil or custom-depth value** (IVP R11 allowlist; Character Pool CP-16; IVP L17). Because an opponent's PlayerController is never relevant to you, an opponent's client holds **no** teammate entry and **no** feed entry for your team, so it never creates a marker for your team, and there is nothing to find in its memory or on its wire. This is the locked rule "teammate markers exist only on teammates' clients; enemies must not be able to obtain them", enforced by the absence of data.

**TM-R2. No world iteration; the module can only reach allies.** The marker module never iterates, queries or filters the world's pawns. It reaches a pawn only by resolving a `PawnRef` taken from the local C-TeamView. Banned in the module and enforced by the build-time include and grep check (extends AC-IVP-17 and AC-PSR-42): server query headers (Policy, Roster, Round, Penalty, Objectives, LSC), `TActorIterator`, `GetAllActorsOfClass`, `GetPlayerControllerIterator`, `GameState->PlayerArray` iteration for anything but TM-R11's public name lookup, `GetFirstPlayerController`, pawn `GetPlayerState`/`GetController`/`GetOwner`, `IsPlayerControlled`, `IsBotControlled`, `CanViewerKnow`, and any read of a team, informant or concealment field from a pawn. On the **listen host** the server process holds every PlayerController and every team's C-TeamView; the module reads only the **local** player's controller, exactly as on a remote client (Roster R6 host-presentation rule, Session rule 3). The prototype's `AStealthHUD::DrawTeammateMarkers` (iterates every character, reads a team field replicated on every pawn and derives NPC-ness from the PlayerState pointer) is the pattern this rule forbids; it is evidence of feasibility only and is not ported.

**TM-R3. One marker per teammate entry, at most four.** The marker set is keyed by `PublicHandle`. Validation, applied on every C-TeamView update on the client:
1. An entry whose handle equals the owner's own handle, or whose `PawnRef` resolves to the local player's own pawn, is ignored (no self-marker, art bible 5.2, Character Pool edge cases).
2. Duplicate handles: the first entry in received order is kept.
3. Two entries resolving to the same pawn (a stale ref during a takeover): one marker, the lower `PublicHandle` wins.
4. Capacity is `MarkerCapacityCount = 4` (= hard maximum `TeamSize` 5 minus self; a constant, not a tuning key). Entries beyond it are ignored in ascending `PublicHandle` order.
Every rejection in 1-4 is reported once per update to the client fault log (`LogStealthMarkers`, development builds; an `ensure` in tests) and is never sent to the server. Marker presentation objects are created **lazily**, only when a valid entry first needs drawing, and destroyed when the entry leaves; a client with no teammate entries (1v1 tests, a solo team) never instantiates one.

**TM-R4. Anchor.** For an entry whose `PawnRef` resolves, the anchor is the pawn's **visual root** (the skeletal mesh component's world location, which already includes the simulated proxy's network smoothing, Shared Character & Locomotion) plus `TeammateMarkerAnchorCm` straight up (F2). It is never a head bone: bone evaluation can be skipped on a presentation-hidden or off-screen mesh, which is exactly the concealed-ally case. The anchor is read and projected in the same frame and from the same view that is rendered (F2, Game Feel), so the marker tracks 1:1 with no smoothing and no frame lag. For an entry served by the fallback feed, the anchor is the fed `Pos` (the pawn's feet, the same point as the visual root; adopted in IVP R8 for CR-2) plus the same height. Fed positions are **stepped**: shown where the server last put them, with no interpolation and no extrapolation (an extrapolated position would be a guess drawn as fact). A feed entry whose sequence has not advanced for `TeammateMarkerFeedStaleSec` (client receive time) is hidden until the next update (F5; inactive until the feed carries a sequence, CR-11). A feed entry for a handle whose `PawnRef` currently resolves is ignored: **a resolved `PawnRef` always beats a feed entry**.

**TM-R4a. Reading a null `PawnRef` (no schema change required).** On a UE client a replicated reference to an actor that is not (yet) mapped reads as null, exactly like a reference the server cleared, so the client cannot tell "no pawn" from "pawn not replicated to me" from the `PawnRef` alone. The rule uses the feed as the server's explicit signal:
1. `PawnRef` resolves → `Tracking` (any feed entry for the handle is ignored).
2. `PawnRef` null **and** a feed entry exists for the handle → `Fed`: the server is saying "this teammate has a pawn that is not replicated to you" (IVP R8). Never a retirement.
3. `PawnRef` null **and** no feed entry → a retirement candidate: the TM-R6 hold starts. A transient unmapped reference (the pawn's channel open but its GUID not yet mapped, typically one or two net updates) resolves to the same pawn inside `TeammateMarkerRetireHoldSec` and cancels the hold with no fade (TM-R6 step 4), so it is absorbed invisibly.
**Stated invariant (CR-11 a-b, adopted in IVP R8 at reconciliation 2026-10-24):** the feed entry and the `PawnRef` are fields of the same C-TeamView entry on the same owner-only PlayerController, and the server adds the feed entry in the same replication update in which the pawn leaves that connection's scope while still existing, so a client never sees "null, no feed" for a pawn that still exists for longer than one update. The feed entry is removed only **one feed update after that connection acknowledges the pawn's channel open** (the `PawnRef` resolves only after the open bunch arrives), so the switch from `Fed` back to `Tracking` never passes through "null, no feed" either (ADR-0003 §7, IVP R8; ADR sync 2026-10-24). The feed is sent to the teammate connections only (locked rule). A team-only `bPawnBound` flag in the C-TeamView entry would make this explicit; it was declined for MVP by the user 2026-10-25 (OQ13 closed).

**TM-R5. Fill: three states of the marked ally, independent of the viewer** (art bible 3.4, 7.3 row 1, 7.x resolution). Precedence, highest first:
1. **Outline-only** iff the entry's `State = Informant`.
2. **Two-band stepped hatch** iff the ally is concealed, defined on this client by **one source per entry kind**: for a `Tracking` entry (resolved pawn), `IsAllyBodyHidden(Pawn)` **alone**, Concealment's client-local, fail-closed body-hidden latch (CON-R6b: the same latch that hides the ally's body at the `BushEnter` swap notify and keeps it hidden until the `BushExit` outward leaf crossing; it already folds in `IsConcealed` for a freshly created actor, CON-R6b, CON-R8, IVP R7.3); for a `Fed` entry (no pawn on this client), the C-TeamView entry flag `IsConcealed`. The server-timed `IsConcealed` is never OR-ed into a tracked entry, because it can arrive before the local swap notify or outlive the body's reappearance. Result: for a tracked ally the hatch appears on the **same rendered frame** that the body disappears, and clears on the frame the body reappears at the leaf crossing, because both are driven by one latch.
3. **Filled** otherwise (`State = Active`).
Fill changes are **instant swaps** on the frame of the change, with no cross-fade (a cross-fade would show an intermediate pattern; opacity encodes no state, art bible 3.4). Informants cannot conceal (IIF R7), so outline and hatch never compete.
**`RoundEnded` is keyed on the round phase** (Roster `EndRoundAll` sets every `Active` or `Informant` record to `RoundEnded` when the round ends): while Round's phase is `Ended`, a `RoundEnded` entry keeps the fill it last had this round on this client (filled if none was seen, e.g. after a rejoin during `Ended`), frozen, and the marker stays per TM-R8. A `RoundEnded` entry that this client has seen `Active` or `Informant` this round keeps its last fill, frozen, **also while the phase is not yet `Ended`**: Round writes `EndRoundAll` in the same server step as the `Ended` publication (Round R9 round-end step), but the Roster records and the GameState are different actors and may arrive a net frame apart, so an early `RoundEnded` is no change (cross-review S-B2 2026-10-24). Only an entry never seen `Active` or `Informant` this round (a `Dormant` record that never entered the round) produces no marker outside `Ended`.

**TM-R6. Retirement and the retire hold.** A marker leaves when its entry leaves C-TeamView, or when the entry's `PawnRef` becomes null with **no** feed entry (TM-R4a case 3; the server has no live pawn for that teammate: a kill or reversal retires the binding at the Attack's queue slot, Roster `RetireBinding`; an exile releases the pawn to the crowd in the same Roster call, Penalty PEN-R4.2; disconnect expiry runs `ReleaseToCrowd`). It is **not** retired while a feed entry is present (TM-R4a case 2): that hands over to the fallback feed with no hold and no fade. On retirement:
1. The marker **holds**, frozen in its last fill, at the cached pawn's anchor while that pawn still exists on this client, otherwise at the last anchor position, for `TeammateMarkerRetireHoldSec` (F6). The hold absorbs a transient unmapped reference (TM-R4a) and lets the strike's wind-up play under the marker.
2. **Contact alignment (user decision 2026-10-25, CR-13, OQ3 closed).** The retire update carries the team-only `RetireAtServerTime` (Roster R6: the kill's `ContactServerTime` for a kill or reversal, the slot time for a release). The hold is extended so that the exit fade is half complete at that time: the exit starts at `max(t_r + T_hold, min(t_R − T_fade / 2, t_r + T_Rmax))` (F6; `T_Rmax` is the guard against an absurd value), where `t_R` is the local time at which the client's synchronised server clock, `EstimatedServerNow()` (ADR-0002 §9, the clock Assassination ASN-R21 schedules from), reaches `RetireAtServerTime`. The value is **latched once, at `t_r`**, and only from the entry update in which the `PawnRef` became null (or the entry left); a nonzero value seen in any other update, or one left over from an earlier retirement of the same handle, is ignored, so a stale time can never extend a later hold. A missing or zero value, or a latched value earlier than `t_r + T_hold + T_fade / 2`, gives `t_r + T_hold`. (The 2026-10-24 death-state extension, keyed on `FDeathState` arrival, stays removed: it fired about half the time.)
3. The marker then exits with the `TeammateMarkerFadeSec` fade (art bible 7.6: "disappears on removal only"), and `OnAllyMarkerRetired` fires (TM-R14).
4. If the same handle's `PawnRef` resolves again during the hold to the **same** pawn as the cached one (an entry flicker or a late GUID mapping, TM-R4a), the hold is cancelled and the marker continues on that pawn with no fade. If it resolves to a **different** pawn (an informant takeover landing inside the hold), the held marker completes its exit as normal and a new marker enters on the new pawn with the `TeammateMarkerFadeSec` entrance (TM-R7); it never slides from the old pawn to the new one.
5. **No hold at `Reset` or travel.** When Round's phase becomes `Reset`, or the local client begins a seamless travel, every marker goes to `Absent` at once with no hold and no fade; nothing persists into the next round.
**MVP timing (ally-only, not a Pillar-1 issue):** the Roster transition and the binding retirement are published at the Attack's queue slot, and the visible contact comes `StrikeContactSec` (0.40 s) later (0.75 s for a bush strike). Step 2 keys the exit on `RetireAtServerTime`, so for every strike, open or from a bush and at any `StrikeContactSec`, the exit fade is half complete as the blow lands and the marker is fully gone `T_fade / 2` (0.15 s) after it: the marker **leaves as the blow lands**. At defaults an open kill gives the same 0.25 s start as the plain hold.

**TM-R7. Takeover and appearance swap.** When a teammate's entry receives a new resolved `PawnRef` (informant respawn takeover of an unseen crowd pawn, decided rule C1; a returning informant's adopted pawn; a rejoiner repossessing its held pawn), the marker appears on the new pawn with the `TeammateMarkerFadeSec` entrance, in the fill of the entry's current `State` (outline for an informant). A `PawnRef` that switches directly from one resolved pawn to another (no `none` in between) moves the marker at once, with no hold, no second marker and no fade. The marker is independent of the pawn's `AppearanceId`: a different look, a re-dealt look between rounds or an informant body that differs from the one it died in never changes the marker (Character Pool CP-16: the marker is UI, not appearance).

**TM-R8. View gate.** Markers are drawn only while Camera's client-local `GetViewState()` returns `Live` (CS-R15; Camera owns the definition: the world rendered from a possessed pawn, including the dying-body phase of the viewer's own elimination, CS-R12.1, any exile beat Camera holds on a released body, and the `Ended` phase while it still has a pawn, CS-R12.4). They are not drawn while it returns `Neutral` (black screen while awaiting a pawn, during `Preparing`, during a rejoin, CS-R12.2-4). The marker layer sits below the camera's fade-to-black, which sits below HUD plates, cards and menus (CS-R15, HUD-R4), so markers go out with the black. The gate depends on the view, never on the viewer's role: an informant viewer keeps every ally's marker (IVP R9a), including the hatched marker of a concealed ally. There is no spectator view in MVP (IVP R13), so there is no "follow a teammate" marker case. The marker set itself (and `IsMarkedAlly`, TM-R10) is maintained while the gate is closed; only drawing stops.

**TM-R9. Always on top, constant pixel size, edge-clamped, no line-of-sight logic.** Each marker is a screen-space overlay drawn after post-process and above the 3D scene with no depth test (art bible 3.4, 7.1, 7.3 note (d), 7.8). Its size depends only on the viewport and the user's UI scale (F1), never on distance or field of view. An ally behind a wall keeps a full marker: this is an allies-only position grant accepted by the art bible (7.8 trade-off 4) and costs nothing under Pillar 1 because opponents hold no marker. Markers read no line-of-sight, no eye-origin mask (Camera CS-R5b masks pawn bodies, never UI) and no camera mode: the marker set and positions are identical in first and third person, so third person gains nothing from markers that first person lacks, and a marker never draws, outlines or reveals a body. When the anchor is off-screen or behind the camera, the marker is clamped to the safe rectangle (F3) as the same pentagon with a stepped base: it **does not rotate and is not an arrow** (arrows pointing at a figure are banned, art bible 7.5). Several edge markers on one edge are spread apart along it (F4). On-screen overlapping markers are drawn far-to-near, so the nearest ally is on top.

**TM-R10. `IsMarkedAlly(Pawn)`: the one client-local ally query.** The module exposes `IsMarkedAlly(Pawn) -> bool` on the local client: true iff the pawn is the resolved `PawnRef` of a valid teammate entry (TM-R3) or the cached pawn of a marker in its retire hold (TM-R6). It does not depend on whether the marker is drawn (view gate, off-screen, settings). Consumers: IIF R10 (the honest client never issues `Attack` at a marked ally) and the IIF Strike hint (allies excluded, IIF UI 4; art bible 7.1 item 4). It is never sent to the server and never called by server code; the server's own teammate rule is Assassination ASN-R9.

**TM-R11. Name label.** An opaque Bone-on-Obsidian plate with the ally's display name sits beside a marker (art bible 7.3 row 2), on **one** marker at a time by focus:
- **Focus:** the marker whose anchor projects nearest the screen centre and within `TeammateLabelFocusSize` px of it (ties: nearer ally in world distance, then lower handle).
- **Hold (Vertical Slice, moved by user decision 2026-10-25; OQ11 closed):** a local-only `ShowTeamNames` hold that labels every on-screen and edge marker is not in the MVP; IIF CR-4's action is deferred with it. The MVP has focus labels only.
- **Reconnecting suffix (MVP, user decision 2026-10-25; OQ1 closed):** while the ally's C-TeamView entry flag `bReconnecting` is true (Roster R6, Team audience), its label reads "{Name} · reconnecting". The marker itself does not change (filled, unhatched, no opacity or state change: the pawn is still bound and AI-held during the grace, Round R17), so nothing in the marker's shape encodes connection; the HUD feed carries the event lines (HUD-R10a).
The name is looked up from the public C-Public `DisplayName` by `PublicHandle` (audience All, Roster R6), never through a pawn's PlayerState. A label never responds to any non-ally (none exists to respond to). Streamer mode (VS, art bible 7.7) replaces the name with the alias; it removes no marker by default.

**TM-R12. Local settings hooks (exposed by Settings & Accessibility, VS).** Ally accent colour (art bible 4.7 list), large markers (`k_large` 1.5), Reduce motion (fades 0 ms), high-contrast mode (keylines already 3 px) and UI scale. All are client-local and never replicated (art bible 7.7 rule 3). MVP ships the defaults with no menu. The VS opt-in "hide markers except on hold" stream-sniping option (art bible 7.7) is not in MVP.

**TM-R14. `OnAllyMarkerRetired(PublicHandle)`: the one client-local retire event** (HUD CR-HUD-7). Fired on the local client exactly once per retirement of a teammate entry's binding (TM-R6): at the hold end (`t_end`, F6), when the exit fade starts; also when the marker was not drawn at that moment (view gate closed, `Stale`); and immediately (at `t_r`) if no marker existed for the handle when the retirement was detected. It is not fired at `Reset`, at travel, or when a hold is cancelled (TM-R6 step 4). Consumer: HUD-R10 times the team removal line at the later of the `State` change's arrival and this event. Never replicated, never called by server code.

**TM-R13. No audio, no motion of its own.** Markers emit no sound and have no bob, pulse, scale pop, spin or idle animation (art bible 7.6). The only motions are the entrance and exit fades and the 1:1 anchor tracking. Teammate callouts and removal lines with audio belong to HUD (team feed) and Informant Pings (VS).

### States and Transitions

Per teammate entry, on the viewing client:

| State | Entered when | Drawn as | Exits to |
|---|---|---|---|
| `Absent` | no valid entry for the handle; `Reset`; travel (`InvalidateAllBindings` cleared every `PawnRef`) | nothing | `Entering` when the entry has a resolved `PawnRef` or a fresh feed position |
| `Entering` | from `Absent` (also after a takeover) | current fill, fade in over `TeammateMarkerFadeSec` (0 with Reduce motion) | `Tracking` or `Fed` when the fade completes; `Held` if retired mid-fade (the hold of TM-R6 applies and the entrance finishes during it; no shortcut to `Exiting`) |
| `Tracking` | `PawnRef` resolves | current fill (TM-R5) at the pawn's visual-root anchor, every frame | `Fed` when the ref reads null and a feed entry exists; `Held` when the ref reads null with no feed entry (TM-R4a); `Absent` (no fade) on `Reset` or travel |
| `Fed` | `PawnRef` null, feed entry present and fresh | current fill at the fed position, stepped | `Tracking` when the ref resolves (no fade); `Stale` after `TeammateMarkerFeedStaleSec` without a sequence advance (F5); `Held` when the feed entry is removed while the ref is still null |
| `Stale` | feed silent too long | nothing (the entry and `IsMarkedAlly` are unaffected) | `Fed` on the next feed update; `Tracking` when the ref resolves; `Held` (undrawn, so `OnAllyMarkerRetired` still fires at `t_end`, TM-R14) when the feed entry is removed while the ref is still null, or the entry leaves |
| `Held` | `PawnRef` null with no feed entry (TM-R4a case 3, TM-R6) | frozen last fill at the cached anchor | `Exiting` at the hold end (F6); `Tracking` if the ref resolves to the **same** pawn during the hold (no fade); a **different** pawn resolving creates a new `Entering` marker while this one goes to `Exiting` (TM-R6 step 4) |
| `Exiting` | hold ended (`OnAllyMarkerRetired` fires, TM-R14); entry removed | last fill, fade out over `TeammateMarkerFadeSec` | `Absent` |

Orthogonal to these, the **view gate** (TM-R8) is `Live` or `Neutral`; in `Neutral` nothing is drawn, timers still run, and on returning to `Live` every non-`Absent` marker fades in together. The **fill** (TM-R5) is re-evaluated every frame in `Tracking` and `Fed` and frozen in `Held` and `Exiting`.

### Interactions with Other Systems

| System | In (Markers receive) | Out (Markers provide) | Interface owner |
|---|---|---|---|
| Participant State & Roster (#2) | C-TeamView teammate entries `{PublicHandle, State, PawnRef}` and own handle; C-Public `DisplayName` by handle; `PawnRef` clear at `InvalidateAllBindings` and re-push | — | Roster (carrier), Markers (consumption) |
| Information Visibility Policy (#7) | Team audience rule; `IsConcealed` entry flag (R3); fallback feed `{PublicHandle, Pos}` (R8, F6) | satisfies R8 and R9a; module is in AC-IVP-17's include check | Policy (audience), Markers (presentation) |
| Concealment (Bushes) (#12) | client-local `IsAllyBodyHidden(Pawn)` latch (CON-R6b) for tracked entries; `IsConcealed` via C-TeamView for fed entries | the hatched marker (Concealment UI-3) | Concealment (flag), Markers (glyph) |
| Camera System (#10) | the rendered view transform and world FOV for projection; `GetViewState()` (CS-R15); fade-to-black layer order | — | Camera |
| Shared Character & Locomotion (#9) | the pawn's visual root (smoothed mesh location) | — | SCL |
| Assassination & Defence Reversal (#13) | nothing direct: a kill's retirement arrives through C-TeamView; `StrikeContactSec` and `StrikeNetLeadSec` are the rationale for the hold default (TM-R6) | — | Assassination |
| Input & Interaction Framework (#3) | none in MVP (the `ShowTeamNames` local input, CR-4, is VS) | `IsMarkedAlly(Pawn)` for IIF R10 and the Strike hint | IIF (consumer), Markers (query) |
| NPC-Kill Penalty & Informants (#14) | informant `State`, the exile's release (PEN-R4.2: `PawnRef` null, no death) and the takeover's new `PawnRef` arrive through C-TeamView | — | Penalty (takeover), Roster (carrier) |
| Round & Match Framework (#6) | phase via `FRoundPublicState` (`Ended`, `Reset`) | — | Round |
| Session & Travel (#5) | seamless travel: markers cleared and rebuilt from the re-pushed view | — | Session |
| Tuning Data & Config (#1) | this GDD's `Client` keys (CR-15) through `GetClientTuning()` | — | TDC |
| HUD (#18) | the shared overlay layer L0 (HUD-R4), safe margin `HudSafeMarginPct`, UI scale, draw order | the marker layer as L0 content; `OnAllyMarkerRetired(PublicHandle)` (TM-R14) for HUD-R10 | HUD (layout), Markers (layer content) |
| Settings & Accessibility (#24, VS) | accent colour, `k_large`, Reduce motion, high contrast, streamer alias | — | Settings |
| Informant Pings (#20, MVP since 2026-10-25) | — | the F3 edge-clamp rule (pings clamp off-screen rings to the safe rectangle the same way); rings draw beneath markers in L0 | Pings (use), Markers (rule) |
| Post-Round Reveal (#21, VS) | — | the pentagon glyph (Reveal reuses the silhouette for everyone after `Ended`, fed only by the Reveal's own payload, never by this data path) | VS system |

## Formulas

*systems-designer and technical-artist input via the specialist panel. All pixel values are at the rendered viewport resolution; "at 1080p" means `H_v = 1080`. The fallback feed's bandwidth is the Policy's (IVP F6, at most 104 B/s per client with the per-entry `Seq`) and is not restated here.*

### F1. Marker size

The marker_size formula is defined as:

`W = max(W_min, round(W_0 · u · s_ui · k_large))`, `H = max(H_min, round(H_0 · u · s_ui · k_large))`, with `u = H_v / 1080`; keyline `K = max(3, round(3 · u · s_ui))`; hatch band height `b = max(2, round(2 · W / W_0))`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| W_0, H_0 | int (px) | 28, 32 | constant (art bible 3.4) | Default glyph width and height at 1080p |
| W_min, H_min | int (px) | 24, 28 | constant (art bible 3.4, 7.x C9) | **Absolute** floors, not scaled by `u`: the hatch must stay legible in greyscale at 24 px |
| H_v | int (px) | 720-2160 | runtime | Viewport height |
| u | float | 0.667-2.0 | calculated | Resolution factor |
| s_ui | float | 0.75-1.5 | user setting (HUD UI scale, art bible 7.x C9) | Global UI scale; 1.0 in MVP |
| k_large | float | 1.0 or 1.5 | user setting (`TeammateMarkerLargeMult`, art bible 7.3 row 1) | "Large markers" step; 1.0 in MVP |
| K | int (px) | 3-9 | calculated | Obsidian keyline width (art bible 7.2 item 3: 3 px on the marker) |
| b | int (px) | 2-9 | calculated | Height of each of the two hatch stair bands |
| W, H | int (px) | 24-126, 28-144 | calculated | Drawn glyph size |

**Output Range:** 24 x 28 to 126 x 144 px (4K, UI scale 150 %, large markers). Size never depends on world distance or field of view (art bible 7.1 camera-mode independence). **Examples:** 1080p, defaults: 28 x 32, K 3, b 2. 720p: `round(28 · 0.667) = 19` → floor 24; height 21 → 28. 1440p: 37 x 43, K 4, b 3. 4K: 56 x 64, K 6, b 4. 1080p with `k_large` 1.5: 42 x 48, b 3.

### F2. Anchor and on-screen placement

The marker_anchor formula is defined as:

`A = R + (0, 0, h_a)`; `(p, z_c) = Project(A)`; glyph centre `q = p − (0, g + H/2)` with `g = round(4 · u)`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| R | vector (cm) | world | runtime | Tracked entry: the pawn's visual root (skeletal mesh world location, network-smoothed). Fed entry: the fed `Pos` (feet, CR-2) |
| h_a | float (cm) | 150-260 | data file (`TeammateMarkerAnchorCm`) | Anchor height above the feet; default 195 = stature 175 + headwear cap 7 + 13 clearance (art bible 1.3.2, 1.3.3) |
| Project | function | — | engine (the rendered frame's view) | World to viewport pixels; `z_c` is camera-space depth |
| p | vector (px) | any | calculated | Projected anchor; the glyph's bottom tip sits `g` px above it |
| g | int (px) | 2-8 | calculated | Constant pixel gap so the tip never touches the head at long range |
| q | vector (px) | any | calculated | Glyph centre before clamping (F3) |

**Output Range:** any screen point; F3 decides whether it is drawn there or clamped. **Example:** 1080p, vertical FOV 60°: focal length `f = 540 / tan 30° = 935 px`. The 13 cm clearance above a 182 cm head is `935 · 0.13 / 20 = 6.1 px` at 20 m and 3.0 px at 40 m, so `g` = 4 px keeps the tip off the head at every range; at 3 m the clearance is 41 px and the marker clearly hangs above the head. The prototype mannequins measure 165.6 cm (OI-1 open): the clearance is then 29 cm, which only lifts the marker slightly.

### F3. Edge clamp

The edge_clamp formula is defined as:

`inside = (z_c > 0) ∧ x_min ≤ q_x ≤ x_max ∧ y_min ≤ q_y ≤ y_max`, with `x_min = m·W_v + W/2`, `x_max = (1 − m)·W_v − W/2`, `y_min = m·H_v + H/2`, `y_max = (1 − m)·H_v − H/2`.
If not inside: `d = q − c` when `z_c > 0`; when `z_c ≤ 0` (behind the camera), `d = (D_r, −D_u + σ · max(0, k_b + D_u))` from the unit camera-space direction `D` to `A` (`D_r = D·right`, `D_u = D·up`, `D_f = D·forward ≤ 0`), with the bias ramp `σ = clamp(−D_f / ε_b, 0, 1)`. At `σ = 1` this is `d = (D_r, max(−D_u, k_b))`, whose vertical part is at least `k_b > 0`; at `σ = 0` (the camera plane) it is `(D_r, −D_u)`, which has the direction of the front-side `q − c` as `z_c → 0⁺`, so the marker does not jump when an ally crosses the camera plane (re-review 2026-10-25: the unramped bias moved a level ally about `k_b · h_x` ≈ 212 px at the side-plane crossing). If `|d| < 10⁻⁴` (possible only for `D_r ≈ 0` with `σ < 1`, an anchor nearly straight above or below the camera) use `d = (0, k_b)`. Then `t = min(h_x / |d_x|, h_y / |d_y|)` with `h_x = x_max − c_x`, `h_y = y_max − c_y` and a term dropped when its denominator is 0 (no `min(1, …)` cap: `d` may be a unit vector); clamped centre `q' = c + t · d`, drawn with the stepped-base edge variant. **Hysteresis:** a marker that is clamped returns to on-screen drawing only when `q` lies at least `δ_h = round(4 · u)` px inside the rectangle (and `z_c > 0`), so the stepped-base variant cannot flicker at the boundary.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| m | float | 0.03-0.10 | HUD's `HudSafeMarginPct` / 100 (read, not owned; CR-HUD-7) | Safe margin; default 0.05 (art bible 7.1: 96 x 54 px at 1080p); one quantity with one name for HUD and markers |
| W_v, H_v | int (px) | — | runtime | Viewport size, read every frame (resize, DPI, ultrawide) |
| c | vector (px) | — | calculated | Screen centre `(W_v/2, H_v/2)` |
| d | vector (px, or unitless behind the camera) | — | calculated | Direction from the centre to the marker (taken from the camera-space direction when behind the camera, so the marker never jumps to the mirrored front position) |
| k_b | float | 0.25 | constant | Downward bias behind the camera: a marker fully behind the viewer (`σ = 1`) always sits on the lower part of the side edges or on the bottom edge, never on the top edge, and moves continuously as the viewer turns |
| ε_b | float | 0.5 | constant | Width of the bias ramp in `D_f` below the camera plane (about 30° of turn past the side plane): the bias is 0 at the plane and full from `D_f = −ε_b` |
| D_f | float | −1 to 0 (behind only) | calculated | Forward component of the unit direction to the anchor; `σ = clamp(−D_f / ε_b, 0, 1)` |
| t | float | > 0 | calculated | Scale that brings `c + t·d` onto the safe rectangle |
| δ_h | int (px) | 2-8 | calculated | Re-entry hysteresis band |
| q' | vector (px) | safe rectangle | calculated | Drawn centre of an off-screen marker |

**Output Range:** every drawn glyph lies wholly inside the safe rectangle. **Example:** 1920 x 1080, 28 x 32 glyph: `x ∈ [110, 1810]`, `y ∈ [70, 1010]`, `h_x = 850`, `h_y = 470`. An ally projecting to `q = (2400, 300)`: `d = (1440, −240)`, `t = min(850 / 1440, 470 / 240) = 0.590`, `q' = (1810, 398)`, right edge. An ally behind-left with `D = (D_r, D_u) = (−0.70, −0.10)`: `d = (−0.70, 0.25)`, `t = min(850 / 0.70, 470 / 0.25) = 1,214`, `q' = (960 − 850, 540 + 304) = (110, 844)`: left edge, below centre. An ally exactly behind (`D_r = 0`), whether its anchor is above or below the camera: `d = (0, max(−D_u, 0.25))`, the x term is dropped, `q' = (960, 1010)`: bottom-centre. It is never drawn at the mirrored front position. **Camera-plane crossing** (ally level with the eye, `D_u = 0`, viewer turning): at the side plane `D = (1, 0, D_f = 0)`, `σ = 0`, `d = (1, 0)`, `q' = (1810, 540)`; at `D = (0.968, 0, −0.25)`, `σ = 0.5`, `d = (0.968, 0.125)`, `t = 878`, `q' = (1810, 650)`; at `D = (0.866, 0, −0.5)`, `σ = 1`, `d = (0.866, 0.25)`, `t = 981`, `q' = (1810, 785)`. The marker slides down the right edge by about 245 px over about 30° of turn (about 23 px per frame at 180°/s and 60 fps), never jumping.

### F4. Edge de-overlap

*Cuttable (user decision 2026-10-25, OQ12): if the schedule slips, F4 may be replaced by plain overlap with far-to-near drawing; F3, F5 and F6 stay.*

The edge_spread formula is defined as:

`s_i = Perimeter(q'_i)`; order the markers **cyclically** by `s_i` (ties: lower `PublicHandle`) and open the cycle at the midpoint of the largest gap between neighbours, re-measuring arc length from there, so the first and last markers are the two farthest apart; then relax symmetrically: each group of markers closer than `L` is spread about its own mean, `s'_j = s̄_G + (j − (n_G − 1)/2) · L` for the `n_G` members of group G, merging groups and repeating until no neighbours are closer than `L` (at most `N_e − 1` merges); finally map back to the boundary. `L = W + H + e`: on a straight edge this is more than one glyph length, and across a corner the arc `L` splits into a horizontal part `a` and a vertical part `b` with `a + b = L`, so either `a ≥ W` or `b ≥ H` and the two axis-aligned glyphs cannot overlap.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| Perimeter | function | 0 to P | calculated | Arc-length position on the safe-rectangle boundary, clockwise from its top-left corner; `P = 2 · ((x_max − x_min) + (y_max − y_min))` |
| L | int (px) | 52-286 | calculated | Minimum arc spacing between edge glyph centres (`W + H + e`, so corners are covered) |
| e | int (px) | 0-16 | data file (`TeammateMarkerEdgeGapSize`) | Gap between neighbouring edge glyphs; default 4 |
| N_e | int | 0-4 | runtime | Clamped markers this frame |

**Output Range:** edge markers never overlap, including across a corner and across the cycle's join; with symmetric relaxation each marker moves at most `1.5 · L` = 96 px at 1080p (`L = 28 + 32 + 4 = 64`), and `4 · L` = 256 px is far below `P` = 5,280 px, so the cycle always has room. **Example:** two allies clamp to `(1810, 398)` and `(1810, 410)` (12 px apart on the right edge): they form one group with mean 404 and move to `(1810, 372)` and `(1810, 436)`, 64 px apart. Two allies either side of the top-right corner (one at `(1800, 70)`, one at `(1810, 80)`, arc gap 20 px) are spread to 64 px of arc about the corner: `(1778, 70)` and `(1810, 102)`, so `|dx| = 32 ≥ 28` and the glyphs do not overlap.

### F5. Fallback feed staleness

The feed_stale formula is defined as:

`hidden_i = (t_now − t_seq,i) > T_stale`, where `t_seq,i` is the client receive time of the last update in which entry i's feed sequence advanced; invariant `T_stale ≥ 3 / h_feed` whenever `h_feed ≥ 0.6` Hz; for `0 < h_feed < 0.6` the staleness check is disabled (`3 / h_feed` would exceed the 5 s hard maximum)

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t_now, t_seq,i | float (s) | — | client clock | Client receive time of the last sequence advance of entry i (never server time). Replicated properties are sent only on change, so a stationary ally's unchanged quantised `Pos` produces no update; staleness therefore uses the per-entry sequence `Seq` that the server advances every feed tick (CR-11c; adopted by user decision 2026-10-25, IVP R8, ADR-0003), so **F5 is active** |
| T_stale | float (s) | 1-5 | data file (`TeammateMarkerFeedStaleSec`) | Default 2.0 |
| h_feed | float (Hz) | 0-10 | IVP `TeamPositionFeedHz` (2.0) | The Policy's feed rate |

**Output Range:** boolean. **Example:** at 2 Hz, 2.0 s is four missed sequence advances; `3 / 2 = 1.5 ≤ 2.0` holds. At 0.5 Hz the check is disabled. If the Policy sets `TeamPositionFeedHz = 0` (feed disabled), fed markers never appear, a null `PawnRef` is always treated as TM-R4a case 3, and only tracked markers exist.

### F6. Retire hold end time

The retire_hold formula is defined as:

`t_end = max(t_r + T_hold, min(t_R − T_fade / 2, t_r + T_Rmax))`, with `t_R` omitted (plain `t_r + T_hold`) when `RetireAtServerTime` is 0; the exit fade then runs from `t_end` for `T_fade`, and `OnAllyMarkerRetired` fires at `t_end` (TM-R14). If the same pawn resolves again at any `t < t_end`, the hold is cancelled and nothing fires. (`RetireAtServerTime` adopted by user decision 2026-10-25, TM-R6 step 2; the death-state extension stays removed.)

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t_r | float (s) | — | client clock | Time the entry's `PawnRef` was first read null with no feed entry (TM-R4a case 3), or the entry left |
| T_hold | float (s) | 0-1.0 | data file (`TeammateMarkerRetireHoldSec`) | Default 0.25: absorbs a transient unmapped reference (one or two net updates, 67 ms each at 15 Hz, plus jitter) and lets most of the 0.40 s strike wind-up (`StrikeContactSec`) play under the marker |
| T_fade | float (s) | 0-0.6 | F7 | Exit fade |
| t_R | float (s) | — | client clock mapping of C-TeamView `RetireAtServerTime` (Roster R6) through `EstimatedServerNow()`; latched once at `t_r` from the retiring update only (TM-R6 step 2) | Local time of the blow (kill/reversal) or of the release slot |
| T_Rmax | float (s) | 1.5 | constant (guard) | Upper bound on the contact extension (covers the bush strike's 1.2 s hard-maximum contact, I-ASN-3, plus margin) |

**Output Range:** `t_end − t_r` ∈ [0, 1.5]. **Examples:** release at defaults (exile, `ReleaseToCrowd`; `t_R ≈ t_r`): exit starts 0.25 s after `t_r`, gone at 0.55 s. Open kill: `t_R ≈ t_r + 0.40`, `t_R − 0.15 = t_r + 0.25`, so the exit starts at 0.25 s, half faded as the blow lands, gone at 0.55 s. Bush strike: `t_R ≈ t_r + 0.75`, exit starts at 0.60 s, half faded at contact, gone at 0.90 s. A GUID mapped 0.13 s late: the same pawn resolves at `t_r + 0.13 < t_end`, the hold is cancelled, the marker never moved.

### F7. Fade

The marker_fade formula is defined as:

`α(τ) = S(clamp(τ / T_fade, 0, 1))` for an entrance, `1 − S(…)` for an exit, with `S(x) = 3x² − 2x³`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| τ | float (s) | ≥ 0 | real (undilated, unpaused) time since the transition began | Frame-rate independent (art bible 7.6 rule 5) |
| T_fade | float (s) | 0-0.6 | data file (`TeammateMarkerFadeSec`) | Default 0.30 (art bible 7.6 "Ally marker 300 ms ease"); 0 when Reduce motion is on |
| α | float | 0-1 | calculated | Whole-glyph opacity, a transition only; never a state (art bible 3.4) |

**Output Range:** 0-1. **Example:** 0.15 s into a 0.30 s entrance, `x = 0.5`, `α = 0.5`; at 0.075 s, `α = 0.156`.

### F8. Label focus

The label_focus formula is defined as:

`focus = argmin_i |q_i − c|` over on-screen markers with `|q_i − c| ≤ r_f · u · s_ui`; ties by smaller world distance, then lower `PublicHandle`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| q_i | vector (px) | screen | F2 | Unclamped on-screen glyph centre (edge markers never take focus) |
| r_f | int (px) | 0-200 | data file (`TeammateLabelFocusSize`) | Focus radius at 1080p, default 64; 0 disables focus labels |

**Hysteresis:** the focus moves from the current marker to another only when the other is at least `round(8 · u · s_ui)` px nearer the centre, or the current one leaves the radius; when the focus moves, the old label's exit fade and the new label's entrance overlap, which is the one permitted moment two labels are visible. **Label placement:** the plate sits on the screen-inward side of its marker (right of a marker in the left half, left otherwise) and is clamped inside the safe rectangle. **Output Range:** zero or one focused marker. **Example:** markers at 40 px and 58 px from the centre, `r_f = 64`: the 40 px marker shows its label; the other does not.

**Performance budget (not a formula):** at most 4 markers, ≤ 0.05 ms game thread per frame on the reference PC, zero per-frame heap allocation, at most 2 draw batches; labels are re-laid-out only when the name or scale changes.

## Edge Cases

*systems-designer and ue-replication input via the specialist panel.*

**Audience and leak cases**
- **If a modified opponent client inspects its memory, packets or replicated state for markers:** it finds nothing, because nothing was sent. No teammate entry, feed entry, `IsConcealed` flag or `PawnRef` of the other team is ever in its PlayerController (owner-only), and no pawn carries a team, informant, concealment or marker field (IVP R3, R4, R11). Verified by AC-TMK-01 to 05.
- **If code in the marker module tries to find allies by scanning pawns** (the prototype's `TActorIterator` plus a replicated team field): the build fails on the include and grep check (TM-R2, AC-TMK-06). There is no "check the team on the pawn" fallback, because the pawn carries no team.
- **If the listen host renders markers:** it reads only its local PlayerController's C-TeamView, though its process holds every team's; the fallback feed is never needed on the host because every pawn resolves there. The host's marker set equals a same-team remote client's (AC-TMK-07). The host's memory is the accepted IVP L21 caveat, not a marker leak.
- **If a stream or screenshot shows the viewer's markers:** the watcher learns the streamer's team, which is the viewer's own legitimate knowledge (IVP L23, accepted; art bible 7.7 streamer mask hides names only). No marker setting removes this in MVP.
- **If C-TeamView ever carried an opponent entry (a server bug):** the client would draw it, because the client cannot know the relation; this is why the authoritative guarantee is the server-side copy rule (Roster R6) and the opponent-client tests (AC-TMK-01, 02) run against the real server path, not a mocked view.

**Ally state cases**
- **If the ally is an informant:** outline-only marker over the informant's pawn, for every viewer of the ally's team, active or informant (art bible 7.x Q1 resolved: yes, outline). Opponents see an informant exactly as an NPC (IVP R9b).
- **If the viewer is an informant:** the viewer keeps every ally's marker in every state, including the hatched concealed ally, and gains no marker of anyone else (no x-ray, IVP R9a; Concealment AC-CON-04).
- **If the ally enters a bush:** at the `BushEnter` swap notify the ally's body hides on the viewer's screen and the marker switches to the hatch on the same frame (TM-R5); it never fades or flickers. Opponents' screens show the same body hide and no marker at all.
- **If two allies share a bush:** each sees the other's hatched marker and no body (CON-R10); their markers overlap at almost one anchor and are drawn far-to-near (TM-R9).
- **If the ally exits a bush or emerges with a strike:** the hatch clears on the frame the body reappears at the outward leaf crossing (`BushExitCrossSec`, `BushStrikeEmergeSec`), not when the server un-culls.
- **If the ally is killed while concealed:** the binding is retired and the body stays hidden through its fall (CON-R9); the hatched marker holds `TeammateMarkerRetireHoldSec` at the cached anchor, then exits (TM-R6).
- **If the ally is killed in the open:** the marker holds frozen on the body for `TeammateMarkerRetireHoldSec` and fades out over 0.3 s, so it is half faded as the blow lands and gone about 0.15 s after; it never sits on the corpse through its lie time. A bush-strike kill (longer wind-up) holds longer, keyed on `RetireAtServerTime`, so it too is half faded at contact (TM-R6 step 2, F6).
- **If the ally reverses an attacker or is reversed:** a reversed ally is retired like any victim; a reversing ally keeps its marker (its pawn and state do not change; the counter pose is a world event).
- **If the ally strikes out (exile on the `NpcKillStrikeLimit`-th NPC kill):** Penalty PEN-R4.2 releases the surviving pawn to the crowd in the same Roster call as `Active → Informant`, so the `PawnRef` becomes null with no feed entry: the filled marker holds `TeammateMarkerRetireHoldSec` over a body that is now a walking citizen (allies' screens only, the same accepted case as `ReleaseToCrowd` below), exits, and later reappears outline-only on the adopted pawn (TM-R7). The marker never swaps to outline on the released body.
- **If an informant ally is killed:** identical to any retirement; the informant later reappears outline-only on a newly adopted crowd pawn near the team spawn (decided rule C1).
- **If an informant respawn takes over a crowd pawn the viewer is looking at:** the marker fades in over that figure (allies only). Nothing changes on any opponent's screen (possession is owner-only, IVP R14).
- **If an ally's look differs after a takeover or between rounds:** the marker follows `PawnRef`, never `AppearanceId` (TM-R7).

**Connection, travel and phase cases**
- **If an ally disconnects:** the pawn stays bound and AI-held during grace (Round R17), so the marker stays **filled and unchanged**; C-TeamView carries one Team-audience bit, `bReconnecting` (Roster R6, user decision 2026-10-25), which adds the "· reconnecting" label suffix (TM-R11) and the HUD feed lines (HUD-R10a). There is still no "reconnecting" marker state: shape, fill and opacity never encode connection.
- **If the ally's grace expires (`ReleaseToCrowd`):** the binding is erased in the same server call as the transition, so the entry's `PawnRef` becomes none; the marker holds `TeammateMarkerRetireHoldSec` (0.25 s) and fades out. For that quarter second the frozen marker sits over a pawn that has just become an NPC, on allies' screens only, and `IsMarkedAlly` still returns true for it. **Accepted and harmless** (CD-GDD-ALIGN item 5): it is allies-only (P1 safe), and the only effect is that the honest client cannot strike that fresh NPC for 0.25 s, a strike that would be an NPC-kill penalty anyway. If the ally was concealed, its former allies' clients also lose the actor at the same boundary (IVP edge case), so the hold freezes at the last anchor position. `OnAllyMarkerRetired` fires at the hold end, but HUD-R10 shows no removal line because the `State` did not change to `Informant` for a release on expiry alone (HUD-R10).
- **If the ally rejoins:** a reclaimed Active participant repossesses its held pawn (same `PawnRef`, marker unchanged or re-entering if it had been retired); a reclaimed informant gets an adopted pawn (TM-R7). Nothing is shown to mark the rejoin.
- **If the viewer rejoins:** the viewer is in the neutral view until its own binding (TM-R8); its C-TeamView is re-pushed on `RebindView`, the marker set is rebuilt from it, and every marker fades in together when the view goes live.
- **If seamless travel runs:** `InvalidateAllBindings` clears every `PawnRef` and travel destroys every pawn; all markers go to `Absent` immediately with no hold and no fade (no pawn exists to hold on), and the fallback feed is not sent because no pawn exists (IVP edge case). The set rebuilds from the re-pushed view; markers appear when the viewer's view goes live in the new round.
- **If a `PawnRef` arrives before the pawn actor is mapped (or the pawn arrives first):** a new entry with a null ref and no feed simply has no marker yet (`Absent`, nothing to retire); an existing marker whose ref reads null with no feed starts the hold, and the mapping resolving to the same pawn inside the hold cancels it with no fade (TM-R4a, AC-TMK-30). With a feed entry the marker is `Fed`; when the ref resolves it switches to `Tracking` with no fade and no position jump beyond the feed's grid (feed `Pos` is feet, CR-2).
- **If the round enters `Ended`:** markers stay while the viewer has a live view (allies-only knowledge); the VS Post-Round Reveal later replaces them. `Reset` clears every marker instantly.
- **If the viewer is eliminated or exiled:** markers stay drawn while `GetViewState()` is `Live` (the dying-body phase, CS-R12.1, or the exile beat on the released body) and go out with the fade to black; none are drawn in the neutral view; they return, fading in together, when the viewer's informant pawn goes live. There is no spectating and no follow-a-teammate view in MVP (IVP R13), so no marker case exists for one.

**Data validation cases**
- **If C-TeamView holds more than four teammate entries** (impossible at `TeamSize` ≤ 5): only four markers exist, chosen by ascending handle, and the client logs a fault (TM-R3). A team of five never shows more than four markers.
- **If an entry names the viewer itself, duplicates a handle, or two entries resolve to one pawn:** ignored, first kept, or merged to one marker respectively, with a fault log (TM-R3).
- **If a `PawnRef` resolves to a pawn that is not a teammate's** (a stale ref racing a takeover): the marker follows what the server put in the view, which is the ally's latest pawn; one update later the ref is corrected. The client never second-guesses the view with its own check, because it has no data to check with.
- **If `TeamSize` is 1 (test preset) or a team has one member:** the viewer has no teammate entries and no marker object is ever created.

**View and screen cases**
- **If the ally is behind a wall or masked in third person:** the marker stays, full size, always on top (TM-R9); the body stays hidden by the eye mask exactly as in first person. Markers are identical in 1P and 3P.
- **If the ally is directly behind the viewer:** the marker clamps to the screen edge on the side the ally actually is (F3), never to the mirrored front position; if the ally is exactly behind, bottom-centre.
- **If several allies stand in one direction:** on-screen markers overlap, nearest on top; edge markers are spread along the edge, including around corners (F4). Only one name label shows on focus.
- **If an edge marker falls under a HUD plate** (top-centre objectives, top-right feed, bottom-left stack): the plate draws above the marker layer (HUD-R4), so the marker is covered, never the plate. Whether edge markers should avoid plate rectangles is OQ15 (HUD UX spec).
- **If the ally stands at the screen centre:** the marker is drawn there. The art bible's empty-centre rule applies to persistent HUD elements, and a world-anchored ally marker is not one (art bible 7.1 classes).
- **If the viewport is resized, the DPI changes or the display is ultrawide:** sizes, margins and the safe rectangle are recomputed every frame (F1, F3); the horizontal FOV cap (Camera `FovHMaxDeg`) is the camera's concern.
- **If the HUD is hidden (a debug or photo toggle, development builds only):** the marker layer hides with it; no capture path draws markers outside the normal HUD layer.

## Dependencies

The systems index lists two upstream dependencies (Participant State & Roster, Information Visibility Policy). Writing the rules showed four more real inputs (Concealment for the hatch, Camera for projection and the view gate, Assassination for the hold-default rationale (the run-time death-hold read was removed by the review), TDC for keys); they are listed below; the 2026-10-24 design review added Concealment, Camera and TDC to row 17's Depends On (CR-8).

### Upstream (Teammate Markers depends on)

| System | Interface used | Hard / Soft | Status |
|---|---|---|---|
| Participant State & Roster (#2) | C-TeamView teammate entries `{PublicHandle, State, PawnRef}` and own handle (R6); `PawnRef` cleared at `InvalidateAllBindings`, re-pushed after controller swaps and on `RebindView`; C-Public `DisplayName` by handle; host-presentation rule (AC-PSR-42) | Hard | Reviewed, NEEDS REVISION; reconciled (CR-1, CR-13 applied; CR-12 declined, user decision 2026-10-25) |
| Information Visibility Policy (#7) | Team audience and carrier rule (R4); `IsConcealed` entry flag (R3); fallback position feed and `TeamPositionFeedHz` (R8, F6) as the TM-R4a signal; informant viewers keep markers (R9a); include check (AC-IVP-17); AC-IVP-15 | Hard | Reviewed, NEEDS REVISION; reconciled (CR-2, CR-11 a-c applied in R8; CR-11c by user decision 2026-10-25) |
| Concealment (Bushes) (#12) | client-local `IsAllyBodyHidden(Pawn)` latch (CON-R6b, R8, R16) for tracked entries; `IsConcealed` via C-TeamView for fed entries (CON-R2, UI-3) | Hard (for the hatch state) | Reviewed, NEEDS REVISION; reconciled (CR-6 applied) |
| Camera System (#10) | rendered view transform and world FOV; `GetViewState()` and the fade-to-black layer (CS-R12, CS-R15; CR-3 applied there) | Hard | Reviewed, NEEDS REVISION; reconciled (CR-3 applied) |
| Tuning Data & Config (#1) | `GetClientTuning()`; CR-15 registration of this GDD's `Client` keys; two new invariants (CR-5) | Hard | Reviewed, MAJOR REVISION NEEDED; reconciled (CR-5 applied, I-TM-1) |
| Assassination & Defence Reversal (#13) | none at run time (the death-hold read was removed, TM-R6 step 2); `StrikeContactSec` / `StrikeNetLeadSec` inform the hold default | Soft (rationale only) | Reviewed, NEEDS REVISION; reconciled (CR-9 applied) |
| Shared Character & Locomotion (#9) | the pawn's network-smoothed visual root (mesh location); no pawn field added (SCL R13 allowlist) | Soft | Reviewed, NEEDS REVISION; reconciled (CR-10 applied) |
| Round & Match Framework (#6) | phase (`Ended`, `Reset`) through `FRoundPublicState` | Soft | Reviewed, MAJOR REVISION NEEDED; reconciled |
| Session & Travel (#5) | seamless travel and rejoin timing (markers cleared and rebuilt) | Soft | Reviewed, NEEDS REVISION; reconciled |
| NPC-Kill Penalty & Informants (#14) | informant state, the exile release (PEN-R4.2) and the takeover's new `PawnRef`, all through C-TeamView | Soft | Reviewed, NEEDS REVISION; reconciled (PEN-R4.2 answers OQ10) |

### Downstream (depend on Teammate Markers)

| System | Interface it uses | Hard / Soft | Status |
|---|---|---|---|
| Input & Interaction Framework (#3) | `IsMarkedAlly(Pawn)` (TM-R10) for IIF R10 (no Attack at a marked ally) and the Strike hint's ally exclusion | Hard (for the honest-client rule) | Reviewed, NEEDS REVISION; reconciled (CR-4 `IsMarkedAlly` applied; `ShowTeamNames` pending user) |
| HUD (#18) | the marker layer as L0 (HUD-R4); `OnAllyMarkerRetired` (TM-R14) for the HUD-R10 removal line; HUD owns `HudSafeMarginPct` and UI scale, which Markers read | Soft | Reviewed, NEEDS REVISION; reconciled (CR-HUD-7 applied here; CR-14 applied by HUD) |
| Informant Pings & Team Communication (#20, MVP since 2026-10-25) | the F3 clamp rule for off-screen rings; draw order (rings beneath markers); pings stay places, never markers on people | Soft | Designed (pending independent review) |
| Post-Round Reveal & Results (#21, VS) | the pentagon glyph only (its own data path after `Ended`) | Soft | Not written |
| Settings & Accessibility (#24, VS) | TM-R12 hooks: accent colour, `TeammateMarkerLargeMult`, Reduce motion, high contrast, streamer alias, opt-in hide-markers | Soft | Not written |
| Anti-Cheat & Info-Leak Security (#27, Alpha) | AC-TMK-01 to 06 as audit inputs | Soft | Not written |

### Change requests to other documents (not applied here)

| # | Target | Request |
|---|---|---|
| CR-1 | Participant State & Roster R6, Interactions row 17, Dependencies | List the `IsConcealed` flag in the C-TeamView teammate entry (already required by IVP R3 and Concealment CON-R2); update the Teammate Markers rows from "Not yet written (provisional)" to Designed; OQ8 answered by IVP R8 plus TM-R4 *(Applied by Roster at reconciliation 2026-10-24: R6, OQ8 closed.)* |
| CR-2 | Information Visibility Policy R8, F6, downstream table | Define the fallback feed `Pos` as the pawn's feet (visual root), so the tracked-to-fed handoff does not jump by the capsule half-height; mark Teammate Markers Designed *(Applied by IVP R8 at reconciliation 2026-10-24.)* |
| CR-3 | Camera System Interactions and Downstream | **Applied by Camera** (`GetViewState()`, fade layer above markers and below plates, CS-R15, AC-CS-37). Closed. |
| CR-4 | Input & Interaction Framework | Add a local-only Enhanced Input action `ShowTeamNames` (hold; no RPC; default binding chosen by ux-designer); state that "a pawn bearing its own team's teammate marker" in R10 means `IsMarkedAlly(Pawn)`; list Teammate Markers in Dependencies *(`IsMarkedAlly` and the Dependencies row applied by the IIF at reconciliation 2026-10-24; `ShowTeamNames` PENDING-USER, OQ11.)* |
| CR-5 | Tuning Data & Config invariant list | `TeammateMarkerFeedStaleSec ≥ 3 / TeamPositionFeedHz` when `TeamPositionFeedHz ≥ 0.6`; add Teammate Markers to the soft-consumer list and register its `Client` keys (the `TeammateMarkerDeathHoldCapSec` invariant and key were removed by the 2026-10-24 review; `TeammateMarkerEdgeInsetPct` was replaced by HUD's `HudSafeMarginPct`) *(Applied by TDC at reconciliation 2026-10-24: I-TM-1, soft-consumer row, keys registered as `Client` = All.)* |
| CR-6 | Concealment (Bushes) | **Applied by Concealment** (CON-R6b `IsAllyBodyHidden(Pawn)`, public client read, fail-closed). Closed. |
| CR-7 | Art bible 3.4 / 7.2 | Clarify that the outline-only (informant) marker's 3 px Turquoise stroke carries an Obsidian keyline on **both** sides (a hollow interior over bright ground fails the dual-edge test otherwise); record the anchor as 195 cm above the feet (stature 175 + headwear 7 + 13) |
| CR-8 | Systems index row 17 Depends On | Add Concealment (Bushes), Camera System, Tuning Data & Config (applied to row 17 by the 2026-10-24 review) |
| CR-9 | Assassination & Defence Reversal Dependencies row 17 | Update the Teammate Markers row from "Not written" to Designed; Markers read nothing from Assassination at run time (the `FDeathState` death hold was removed); OQ3 asks whether a team-only `RetireAtServerTime` (the kill's `ContactServerTime`) should ride the retire update *(Applied by Assassination at reconciliation 2026-10-24: Downstream row 17, ASN-R23 ally bullet and AC-ASN-35 state the accepted ally-only early exit.)* |
| CR-10 | Shared Character & Locomotion | List Teammate Markers as a soft consumer of the visual root; restate that no marker data may ever be added to the pawn allowlist *(Applied by SCL at reconciliation 2026-10-24: Downstream row.)* |
| CR-11 | Information Visibility Policy R8, F6 | (a) Define the fallback feed's trigger per connection: the teammate's pawn exists on the server but is not in that connection's replication scope; (b) state the TM-R4a invariant: the feed entry is added in the same C-TeamView update in which the `PawnRef` becomes unresolvable for that connection, and only to teammate connections; (c) add a per-entry feed sequence the server advances every feed tick, so a stationary ally's feed is not mistaken for a stale one (F5) *(a) and (b) applied by IVP R8 at reconciliation 2026-10-24; (c) approved by the user 2026-10-25 and applied in IVP R8 (OQ14 closed).* |
| CR-12 | Participant State & Roster R6 | Optional, robustness only: a team-only `bPawnBound` flag per C-TeamView teammate entry so a client can tell "no pawn" from "pawn not replicated to you" without the feed (TM-R4a). *(DECLINED for MVP by the user 2026-10-25; OQ13 closed.)* |
| CR-13 | Participant State & Roster R6 / Assassination ASN-R21 | Optional: a team-only `RetireAtServerTime` on the retire update (slot time for a release, `ContactServerTime` for a kill) so the marker can leave exactly at the blow. *(Approved by the user 2026-10-25; applied by Roster R6 and here in TM-R6 step 2 / F6; OQ3 closed.)* |
| CR-14 | HUD | CR-HUD-7 applied here: Markers read `HudSafeMarginPct` and provide `OnAllyMarkerRetired(PublicHandle)` (TM-R14); HUD's Dependencies row for Markers can drop "(F6 `t_end`)" in favour of TM-R14 *(Done in HUD: Dependencies row cites TM-R14.)* |
| CR-15 | NPC-Kill Penalty & Informants | None required: PEN-R4.2 (exile releases the pawn) answers this GDD's former OQ10; the Penalty Interactions row is consistent |

**Bidirectionality (re-checked at reconciliation 2026-10-24):** Roster, IVP, Concealment, Camera, Assassination, Penalty, HUD, IIF, TDC and SCL all name Teammate Markers in their Dependencies (CR-1, CR-2, CR-4, CR-5, CR-9, CR-10 applied by their owners).

**Change requests received (reconciliation 2026-10-24)**

| From | Request | Disposition |
|---|---|---|
| IVP (R8 reconciled for our CR-2, CR-11 a-b; its item 7) | Feed `Pos` = feet (visual root); trigger per connection; same-update invariant; consume `PawnRef` plus the R8 feed | APPLIED (TM-R1 feed bullet, TM-R4, TM-R4a invariant now cite IVP R8 as adopted); CR-11c sequence APPLIED 2026-10-25 (user decision, OQ14 closed) |
| Camera (its item 10, our CR-3) | `GetViewState()`, fade layer above markers, below plates | ALREADY DONE (TM-R8, TM-R9) |
| Concealment (its item 14, our CR-6) | Public client read `IsAllyBodyHidden(Pawn)` (CON-R6b) | ALREADY DONE (TM-R5) |
| HUD CR-HUD-7 | Read `HudSafeMarginPct`; `OnAllyMarkerRetired` for the team line | ALREADY DONE (F3, TM-R14) |
| HUD-R10 / HUD-R4 | Marker layer as L0; the ally removal line enters with `OnAllyMarkerRetired` | ALREADY DONE (TM-R14 fires once, also undrawn) |
| Penalty CR-17 / CR-15 | Exile releases the pawn; marker exits with `ReleaseToCrowd` timing, returns outline-only on the adopted pawn | ALREADY DONE (Edge Cases, OQ10 closed) |
| Assassination (ASN-R23, AC-ASN-35 cited a death hold the Markers review removed) | Align ally timing | Resolved in Assassination per the decision-log ruling (2026-10-24T09:15:06Z, CD P2): the accepted ally-only early exit stands; no change here |
| Roster (applied our CR-1, CR-13; CR-12 declined) | `IsConcealed` in the entry; `bPawnBound`; `RetireAtServerTime` | CR-1 ALREADY DONE; CR-13 APPLIED, CR-12 DECLINED (user decision 2026-10-25; OQ3, OQ13 closed) |
| IIF (applied our CR-4 in part) | `IsMarkedAlly` meaning in R10; `ShowTeamNames` | ALREADY DONE (TM-R10); `ShowTeamNames` PENDING-USER (OQ11, already listed) |
| TDC (applied our CR-5; `Client` audience ruling, CR-8) | `Client` = All with a client-only consumer | ALREADY DONE (Tuning Knobs audience column already `Client`; meaning unchanged) |
| SCL (applied our CR-10) | Soft consumer of the visual root; nothing on the pawn allowlist | ALREADY DONE |
| Cross-review 2026-10-24 S-B2 | `RoundEnded` received before `Phase = Ended` must not hide markers | APPLIED (cross-review fixes 2026-10-24): TM-R5 freezes the last fill for an entry seen this round regardless of phase; AC-TMK-10 updated; Round R9 stages `EndRoundAll` with the publication (Round AC 12d) |
| ADR-0003 sync 2026-10-24 | TM-R4a: feed entry kept until one feed update after the channel open is acknowledged | APPLIED (TM-R4a stated invariant) |
| ADR-0008 sync 2026-10-24 | AC preamble test path → `Source/StealthPresentation/Private/Tests/Markers/`; T2/T3 in `StealthTestHarness` | APPLIED (AC preamble) |
| ADR-0021 sync 2026-10-24 | OQ6 answered by §1-§6 (volatile Slate leaf at L0, Camera fade layer above, DPI rule) | APPLIED (OQ6 marked answered) |
| ADR-0021 sync 2026-10-24 | Local `ShowTeamNames` action (TM CR-4) to be added to IIF | RESOLVED by user decision 2026-10-25: moved to VS with the hold (OQ11); not added to IIF in MVP |
| User decision 2026-10-25 (scope; OQ11) | Keep the focus label in MVP, move the `ShowTeamNames` hold and IIF CR-4 to VS | APPLIED (TM-R11, UI-2, AC-TMK-23, `TeammateLabelFocusSize` row) |
| User decision 2026-10-25 (scope; OQ12) | Systems-index effort S → M; F4 edge spreading cuttable | APPLIED (systems-index design-order table; F4 note) |
| User decision 2026-10-25 (scope; OQ1, Session OQ7) | Teammate-only reconnecting indicator in MVP | APPLIED (TM-R11 label suffix from Roster `bReconnecting`; marker unchanged; Edge Cases; AC-TMK-26) |

Counts (updated at the 2026-10-25 re-review): CR-11c and CR-13 applied by user decision, CR-12 (`bPawnBound`) declined by user decision, `ShowTeamNames` moved to VS by user decision, all others applied or already done; none pending.


## Tuning Knobs

Registered under owner `TeammateMarkers` (TDC CR-15 columns, layout as in concealment-bushes.md). Every key is client-local presentation with **one value for every client**: audience `Client` (read through `GetClientTuning()`, no server consumer), latch `Use`, except the user setting marked *. No key has an NPC, player, team or role variant. **CR-15.8 identity-tell review:** no key times NPC replacement, informant respawn, crowd sizing or any crowd-versus-player event; the two hold timers act only on allies' clients and only on allies' markers, so none can be observed by an opponent. Pass.

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / what breaks |
|---|---|---|---|---|---|---|---|---|---|
| `TeammateMarkerAnchorCm` | float | TeammateMarkers | 195 | 150-260 | 185-210 | Client | Use | Cm | F2. Low: the pentagon tip touches or covers a tall head and reads as attached to the wrong figure in a crowd. High: at close range the marker floats far above the ally and drifts toward a neighbour's head. Re-derive when OI-1 (measured stature) closes. |
| `TeammateMarkerEdgeGapSize` | int32 | TeammateMarkers | 4 | 0-16 | 2-8 | Client | Use | px at 1080p | F4. 0: edge markers touch and merge into one shape. High: four edge markers spread far from their true direction. |
| `TeammateMarkerFadeSec` | float | TeammateMarkers | 0.30 | 0-0.6 | 0.25-0.4 | Client | Use | s | F7 (art bible 7.6). 0: markers pop (allowed only via Reduce motion). High: a retired marker lingers and a new informant marker appears late. Never used for fill changes. |
| `TeammateMarkerRetireHoldSec` | float | TeammateMarkers | 0.25 | 0-1.0 | 0.15-0.35 | Client | Use | s | F6, TM-R4a. Low (below two net updates, 0.13 s): a late GUID mapping is no longer absorbed and the marker fades out and back in; the marker leaves well before the blow. High: a released (exiled or abandoned, now NPC) pawn keeps a frozen ally marker visibly long on allies' screens, and `IsMarkedAlly` blocks the honest client's strike on it for that long. |
| `TeammateMarkerFeedStaleSec` | float | TeammateMarkers | 2.0 | 1.0-5.0 | 1.5-3.0 | Client | Use | s | F5 (active: the feed carries a per-entry sequence since the user decision 2026-10-25). Must be ≥ 3 / `TeamPositionFeedHz` when that is ≥ 0.6 Hz (CR-5). Low: fed markers blink under packet loss. High: a fed marker keeps showing a position the ally has long left. |
| `TeammateLabelFocusSize` | int32 | TeammateMarkers | 64 | 0-200 | 48-96 | Client | Use | px at 1080p | F8. 0: no labels in the MVP (the `ShowTeamNames` hold is VS). High: labels pop up constantly while looking around a crowd, competing with the baseline. |
| `TeammateMarkerLargeMult` * | float | TeammateMarkers | 1.0 | 1.0 or 1.5 | 1.0 or 1.5 | Client | Use | Mult | F1. User setting (exposed by Settings & Accessibility, VS; fixed at 1.0 in MVP). Values other than the two steps are rejected (the hatch must land on whole pixels). |

**Constants (not keys; changing one is a GDD and art bible change):** `W_0 x H_0` = 28 x 32 px, absolute floors 24 x 28 px, keyline minimum 3 px, hatch band minimum 2 px, tip gap `round(4·u)` px (art bible 3.4, 7.2, 7.x C9); `MarkerCapacityCount` = 4 (TeamSize hard maximum 5 minus self, TM-R3).

**Interactions between knobs:** `TeammateMarkerRetireHoldSec` plus `TeammateMarkerFadeSec` sets when the marker is gone relative to the blow (`StrikeContactSec`, Assassination): at defaults 0.55 s against contact at 0.40 s. `TeammateMarkerFeedStaleSec` is inert when the Policy's `TeamPositionFeedHz` is below 0.6. `TeammateMarkerEdgeGapSize` matters only when two or more allies are off-screen on one side.

**Referenced, owned elsewhere (never restated or altered here):** `HudSafeMarginPct` (HUD; the F3 margin); `TeamPositionFeedHz`, `TeamPositionFeedGridCm` (IVP); `StrikeContactSec`, `StrikeNetLeadSec` (Assassination; rationale for the hold default); `BushExitCrossSec`, `BushStrikeEmergeSec`, `BushEnterSwapSec` (Concealment; they time the hatch through the body-hidden flag); `NoPawnFadeSec`, `FpFovVDeg`, `TpFovVDeg`, `FovHMaxDeg` (Camera); `TeamSize` (Round, via TDC).

**Deliberately absent:** any distance fade, line-of-sight fade, occlusion state, opacity state, marker on non-allies, enemy-count or reconnecting state, and any shipped distance cap on edge markers (a development-only console variable may cap edge markers by distance for the AC-TMK-29 playtest A/B; it never ships and is not a tuning key). Adding one is a creative-director change (Pillar 1, art bible 3.4 "opacity is never a state encoding").

## Visual/Audio Requirements

*art-director and technical-artist input via the specialist panel; the glyph itself is art bible 3.4 / 7.2 / 7.3 row 1 / 7.6 and is restated here only as the implementation contract.*

**VA-1 Glyph.** Downward-pointing pentagon with a centred two-step notch in the top edge, Turquoise `#0FC2B8` fill with a 3 px Obsidian `#1C1917` keyline (F1 scales it), flat, no gradient, glow, bevel, shadow or bloom. It is the only downward pentagon and the only Turquoise element in the UI (art bible 4.6: turquoise means "a member of my team" and nothing else).

| State | Construction | Must survive |
|---|---|---|
| Filled (Active) | solid Turquoise inside the keyline | greyscale at 24 px against the brightest and darkest ground (dual-edge test, art bible 7.2) |
| Hatched (Concealed) | solid fill with two Obsidian stair bands of height `b` (F1) across the lower half, band ends offset by one 4 px step so they read as a stair (built from the two-step stair, never a free diagonal, art bible 7.5) | separation from both other states in greyscale at 24 px (art bible 7.5 acceptance) |
| Outline-only (Informant) | 3 px Turquoise stroke, hollow, Obsidian keyline outside **and** inside the stroke (CR-7) | contrast of the stroke against a bright hollow interior |
| Edge variant | the same pentagon in the current fill, with a stepped base; no rotation, no arrow | reads as "ally off that way", never as a pointer at a figure |

**VA-2 Rendering (technical-artist constraints; mechanism -> UI ADR).** Screen-space, drawn after post-process (no mood grade, vignette or snap applies, art bible 7.1 draw order), no depth test, constant pixel size, never in world space and never a `UWidgetComponent` or any component on a pawn (TM-R1). Glyph brushes for the three fills are authored at integer sizes or drawn by a UI material with hard step edges, with nearest filtering and no mips, so the stair bands stay crisp at non-integer scales. The layer is volatile (laid out every frame, never inside an invalidation or retainer box), so projection uses the rendered frame's view with zero frame lag [verify]. **Projection contract:** project inside the paint or draw pass (`AHUD` canvas pass or a Slate leaf's paint), never in a UMG property binding or a tick that runs before the camera update; use the view's actual world FOV and transform from the frame being rendered (after the spring arm and camera manager update), not a configured FOV key; apply the resolution and UI scale exactly once (F1's `u · s_ui`), so the engine's DPI scale curve is not multiplied on top [verify the DPI rule against UE 5.8.4]. **Glyph size includes the keyline:** `W x H` is the outer size of the drawn glyph, keyline included; for the outline state (outer keyline, 3 px Turquoise stroke, inner keyline: 9 px per side) the hollow interior at the 24 px floor is only `24 − 2·9 = 6` px at its widest, which AC-TMK-21 checks for greyscale legibility; if it fails, the art-director thins the inner keyline (CR-7) rather than raising the floor. The two candidate mechanisms are one Slate leaf widget in a HUD layer (DPI scaling, explicit z-order, localised labels) or an `AHUD` canvas pass (exact projection, least code, as in the prototype); OQ6.

**VA-3 Motion.** Appear and disappear: 300 ms ease (F7), only on entry, removal and the view gate. Fill changes: instant, on the frame of the change. No bob, pulse, scale pop, spin, overshoot or idle loop, no motion faster than 0.5 Hz, and nothing moves more than 8 px except the 1:1 anchor tracking (art bible 7.6 rules 1-3). Reduce motion sets fades to 0 ms; the retire hold is a timing, not a motion, and stays.

**VA-4 Accessibility.** No information by hue alone: shape plus fill pattern carry every state. The ally accent alternatives (art bible 4.7: warm white, magenta, ultramarine, each to pass protan, deutan and tritan simulation against the world palette before adoption) change only the local fill colour, never shape, keyline or the allies-only rule, and are never replicated (art bible 7.7 rule 3). High-contrast mode changes nothing on the marker (its keylines are already 3 px). Text scale applies to the name label (22 px base, art bible 7.3 row 2), never below its 100 % size.

**VA-5 Name label.** Opaque Obsidian plate with stepped 4 px corners, Bone `#EFE8D6` text at 22 px, beside the marker, never coloured Turquoise (art bible 7.2 item 4). Long names truncate with an ellipsis at the label's width cap; duplicate names already carry Session's sanitised suffix.

**VA-6 Audio.** None. The marker has no appear, disappear, state-change or proximity sound. Art bible 4.7 requires an audio cue for teammate *callouts*, which belong to HUD's team feed and Informant Pings, not to the marker. An ally hears nothing extra when a teammate hides (Concealment VA-6).

**VA-7 What the marker must never do** (art bible 5.2, 7.1, 7.5): appear on, react to or outline any non-ally; change size or opacity with distance or occlusion; use an arrow, eye, crosshair or human silhouette; carry a health, status or role glyph; blink.

> **Asset Spec:** run `/asset-spec system:teammate-markers` after the art bible is approved (three fill brushes, edge variant, label plate).

## Game Feel

The marker should feel like part of the screen, not part of the world: steady, exact and quiet. It never draws the eye unless an ally's state changes.

| Moment | Target | Number |
|---|---|---|
| Tracking a moving ally | Glued to the figure: no trailing, swimming or jitter at any speed, in 1P and 3P | 0 frames of lag (projected from the rendered frame's view); marker-to-anchor error < 1 px in a 60 fps capture (AC-TMK-22) |
| Ally slips into a bush | The body vanishes and the hatch appears as one event | same rendered frame |
| Ally leaves a bush | The hatch clears as the body steps out of the leaves | same frame as the outward leaf crossing |
| Ally is struck down | The marker leaves as the blow lands | exit keyed on `RetireAtServerTime`: half faded at contact, gone 0.15 s after (open kill at defaults: hold 0.25 s then a 0.3 s fade; bush strike: exit starts 0.60 s after the retirement) |
| Informant returns | An outline pentagon fades in over a stranger near the spawn | 0.3 s fade, no sound |
| Turning away from an ally | The marker slides along the screen edge and never flips to the wrong side | F3, continuous direction |
| Looking at an ally | One name label settles beside the most central marker | the HUD plate entrance (400 ms fade, 8 px stepped settle, art bible 7.6); hidden when focus leaves (600 ms fade) |
| Own elimination | Markers stay during the dying-body shot and go out with the black | Camera `NoPawnFadeSec` (0.5 s) |

Feel risks: (1) a frame-late projection makes markers swim during fast camera turns; it is the most visible failure and is guarded by AC-TMK-22. (2) The fallback feed's 2 Hz steps are visible on a fed marker; fed markers are rare (allies are always relevant to allies), so honest steps are preferred over a smoothed guess.

## UI Requirements

| # | Element | Viewer | Rule |
|---|---|---|---|
| UI-1 | Teammate marker (three fills, edge variant) | the marked player's allies (Active and Informant viewers) | Built only from the viewer's own C-TeamView (TM-R1); never created on an opponent's client; always on top, constant pixel size, edge-clamped (TM-R9); no response to any non-ally. |
| UI-2 | Ally name label | allies only | On focus (F8) (the `ShowTeamNames` hold is VS, TM-R11); "{Name} · reconnecting" while `bReconnecting`; name from C-Public by handle; streamer alias in VS. |
| UI-3 | `IsMarkedAlly` exclusion | the owner's Strike hint and Attack issue (IIF) | Not a visible element: it removes the Strike hint from marked allies and stops the honest client's Attack at them (TM-R10). |

Layout: one HUD layer (HUD-R4 L0), below the camera's fade-to-black, HUD plates, menus and cards, above the 3D scene; it shares the HUD's 5 % safe margin and UI scale (art bible 7.1). No marker is shown in the lobby, in the briefing (`Preparing` neutral view) or in any menu. Gamepad and keyboard behave identically: markers are not focusable or clickable.

> **UX Flag — Teammate Markers**: this system has UI requirements. In Pre-Production, run `/ux-design` for `design/ux/hud.md` (the marker layer, label focus and the reconnecting suffix; the `ShowTeamNames` binding is VS) **before** writing epics; stories that touch the marker should cite the UX spec, not this GDD directly.

## Cross-References

| This GDD references | Element | Where used | Status |
|---|---|---|---|
| design/gdd/game-concept.md | Locked rule "teammate markers created only on teammates' clients"; Fellowship aesthetic; anti-pillar "not an identification-tools game"; design risk "markers in 2v2 may reveal enemies by elimination" | Overview, Player Fantasy, TM-R1, OQ8 | Locked |
| design/gdd/participant-state-roster.md | R6 C-TeamView, C-Public, host-presentation rule; R9 `RetireBinding`, `ReleaseToCrowd`, `InvalidateAllBindings`; R11 rejoin; AC-PSR-42; OQ8 | TM-R1-R7, Edge Cases | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/information-visibility-policy.md | R3 rows `IsPlayer`, `IsConcealed`, `IsInformant`; R4 carriers; R8 fallback feed; R9a informant viewers; R10 host; R11 allowlist; R13 no spectator; R14 possession; L17, L23; F6; AC-IVP-15, 17 | TM-R1-R4, TM-R8, Edge Cases, AC | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/concealment-bushes.md | CON-R2 owner and team flag; CON-R6 swap and ally hatch; CON-R8 exit crossing; CON-R9 kill while concealed; CON-R10 co-occupants; CON-R16 presentation-hidden flag; UI-3; AC-CON-01, 03, 04 | TM-R5, TM-R6, Edge Cases | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/camera-system.md | CS-R5b eye mask (bodies only); CS-R12 dying-body phase, neutral view, fade to black; CS-R15 `GetViewState()` and layer order; `NoPawnFadeSec` | TM-R8, TM-R9, Game Feel | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/input-interaction-framework.md | R7 informants cannot hide; R10 no Attack at a marked ally; UI 4 Strike-hint ally exclusion | TM-R5, TM-R10 | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/assassination-defence-reversal.md | ASN-R9 teammates never victims; ASN-R21 death-write timing (why the death hold was removed); `StrikeContactSec`, `StrikeNetLeadSec` | TM-R6, F6 | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/npc-kill-penalty-informants.md | PEN-R4.2 exile releases the pawn; takeover (C1) | TM-R6, TM-R7, Edge Cases | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/hud.md | HUD-R4 layer L0 and plate order; HUD-R10 removal line; `HudSafeMarginPct`; CR-HUD-7 | TM-R8, TM-R14, F3 | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/shared-character-locomotion.md | R13 pawn allowlist (nothing added); network-smoothed visual root | TM-R1, TM-R4 | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/character-pool-appearance.md | CP-16 the marker is UI, not appearance | TM-R7 | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/session-travel.md | rule 3 host parity; rule 24 no connection state to non-owners; OQ7 teammate-reconnecting indicator (decided 2026-10-25: MVP, label suffix here via Roster `bReconnecting`) | Edge Cases, OQ1 | Reviewed, NEEDS REVISION; reconciled |
| design/gdd/round-match-framework.md | R17 bound-AI hold; phases `Preparing`, `Ended`, `Reset` | Edge Cases | Reviewed, MAJOR REVISION NEEDED; reconciled |
| design/gdd/tuning-data-config.md | CR-15 registry columns and key suffixes; CR-11 latch classes | Tuning Knobs | Reviewed, MAJOR REVISION NEEDED; reconciled |
| design/art/art-bible.md | 1.3.2-1.3.3 stature and headwear cap; 3.4 glyph table; 4.6 palette; 4.7 colourblind and accent list; 5.2 viewer-state matrix; 7.1-7.8 HUD rules (7.3 rows 1-2, 7.6 motion, 7.7 leak rules and streamer mask, 7.x C6 and Q1) | VA-1 to VA-7, TM-R5, TM-R9, F1, F2 | Approved direction |
| production/first-playable-report.md; Source/StealthGame/Private/StealthHUD.cpp | Prototype teammate marker (team field replicated on every pawn; NPC test through the PlayerState pointer): evidence of feasibility only, and the leak this GDD forbids | TM-R2 | Evidence (read-only) |

## Acceptance Criteria

*qa-lead input via the specialist panel.* Tags: **type** Logic / Integration / UI / Visual (coding-standards evidence table) and **gate** BLOCKING / ADVISORY; [U] automation test under `Source/StealthPresentation/Private/Tests/Markers/` (headless `-nullrhi` where no rendering is needed; ADR-0007/0008); [PIE-n] tests in `Source/StealthTestHarness/Private/`; [PIE-2] / [PIE-3] multi-client PIE (listen server + clients; [PIE-3] includes one client on the opposing team); [Net] under the IVP `LeakNetProfile` (PktLag 100 ms, 2 % loss); [1P/3P] run once in each camera mode; [Vis] retained screenshots in `production/qa/evidence/` with lead sign-off. Test names are `StealthGame.Markers.*`. The leak tests (AC-TMK-01 to 07) are **ship-gating**: any failure blocks every build that leaves the team, like the IVP CRITICAL rows.

**A. Audience and leak tests (TM-R1, TM-R2; locked rule)**
- **AC-TMK-01** [PIE-3][Net][U] Integration, BLOCKING — `NoOpponentMarkerObjects`: GIVEN a 2v2 match (so the B clients' marker modules are live, not inert) in `Active` with every pawn visible, one A player concealed and one A player an informant, WHEN each B client's marker module and its whole UObject graph are enumerated (marker widget or slot instances, marker set entries, `IsMarkedAlly` results over every pawn the B client holds), THEN each B client's marker set equals exactly `{its own B teammate}`, it holds zero marker objects, set entries or `IsMarkedAlly` hits for any team-A pawn, and each A client's set equals exactly `{its A teammate}`.
- **AC-TMK-02** [PIE-3][Net][U] Integration, BLOCKING — `NoTeamDataOnOpponentClient`: GIVEN the same match, WHEN a reflection dump of the B client's received replicated state is taken (every replicated property of every PlayerController, PlayerState, pawn and component, and the GameState, grouped by class and property name), THEN (a) it holds no C-TeamView entry, `PawnRef`, `IsConcealed` flag or fallback feed entry with a team-A `PublicHandle`, and no PlayerController other than its own; (b) for each pawn property, no property's value set partitions team-A pawns, team-B pawns and NPCs (no value, or value range, occurs only in one group), except the allowlisted per-instance `All` facts (transform, velocity, `AppearanceId`, `FActionState`, `FDeathState`, IVP R3), whose distributions are checked by AC-IVP-01/02 instead.
- **AC-TMK-03** [U] Logic, BLOCKING — `NoMarkerReplicatedFields`: GIVEN the build, WHEN the IVP R11 reflection audit and an RPC enumeration run over the marker module's classes and every class it touches, THEN the module declares no replicated property, no RPC and no replicated actor or component; the accent colour, `TeammateMarkerLargeMult` and `ShowTeamNames` are client-local and appear in no replicated layout.
- **AC-TMK-04** [PIE-3][Net] Integration, BLOCKING — `OpponentPacketAudit`: GIVEN a 60 s capture of every bunch the server sends to a B connection (server-side send hook, decoded by property), run once with a dedicated-style client B and once with the **listen host on team B** and a remote team-A client, WHEN it is searched for any field in any channel that carries a team-A `PublicHandle` outside C-Public `PublicHandle` itself, or that links a team-A pawn's NetGUID to a handle, PlayerState or team (including `APawn::PlayerState`-style references [verify the 5.8.4 default]), THEN there are zero matches (art bible 7.7 test 2, network half).
- **AC-TMK-05** [PIE-3][U] Integration, BLOCKING — `OpponentMemoryAudit`: GIVEN the B client process after 60 s of play, WHEN (a) a UObject-graph and reflection walk enumerates every instance of the C-TeamView entry type, the fallback-feed type and every marker-module class, and (b) in a test build the server plants a 64-bit random canary in a server-only field of each team-A record that the C-TeamView copy path would carry if it leaked, and the B process memory is scanned for those canaries, THEN (a) finds no instance holding team-A data and (b) finds zero canaries, while the same scan of a team-A client finds its teammates' canaries (positive control). A raw scan for 32-bit `PublicHandle` values is not used (chance matches and legitimate C-Public copies make it meaningless).
- **AC-TMK-06** [U] Logic, BLOCKING — `IncludeGrepBan`: GIVEN the marker module's source, WHEN the build-time include check and grep run (extending AC-IVP-17 and AC-PSR-42), THEN no server query header is included and none of `TActorIterator`, `GetAllActorsOfClass`, `GetPlayerControllerIterator`, `GetFirstPlayerController`, pawn `GetPlayerState`/`GetController`/`GetOwner`, `IsPlayerControlled`, `IsBotControlled`, `CanViewerKnow`, or a `PlayerArray` read outside the public-name helper appears.
- **AC-TMK-07** [PIE-2] Integration, BLOCKING — `HostParity`: GIVEN the listen host and one client on the same team, a fixed camera transform, TAA off and **stationary** allies (so network smoothing cannot differ), WHEN each serialises its marker set (handles, fills, screen positions, label state) while looking at the same group, THEN the two are equal within 1 px, and an instrumented host run shows the module read only the local PlayerController's C-TeamView (never another controller's) (extends AC-PSR-42).

**B. Data rules (TM-R3 to TM-R8, TM-R10)**
- **AC-TMK-08** [U] Logic, BLOCKING — `SourceIsTeamViewOnly`: GIVEN a mocked local C-TeamView with three teammate entries and a mocked world containing ten other pawns, WHEN the marker set is built, THEN it has exactly three markers, one per entry, attached to the entries' pawns, and none to any other pawn.
- **AC-TMK-09** [U] Logic, BLOCKING — `ValidationCapacityDedupe`: GIVEN entries that include the own handle, a duplicated handle, two handles resolving to one pawn and six valid entries, WHEN validated, THEN the own handle is ignored, the first duplicate kept, the shared pawn gets one marker (lower handle), at most four markers exist (ascending handle), and one fault is logged per rejection.
- **AC-TMK-10** [U] Logic, BLOCKING — `FillPrecedence`: GIVEN every combination of entry kind ∈ {Tracking, Fed}, `State` ∈ {Active, Informant, RoundEnded}, round phase ∈ {Active, Ended}, `IsConcealed` ∈ {false, true} and `IsAllyBodyHidden` ∈ {false, true}, WHEN the fill is evaluated, THEN Informant → outline; else hatch iff (Tracking: `IsAllyBodyHidden`; Fed: `IsConcealed`), so a tracked entry with `IsConcealed` true and `IsAllyBodyHidden` false is **filled**; else Active → filled; RoundEnded in phase `Ended` → the last fill seen this round (filled if none); RoundEnded outside `Ended` → the last fill seen this round if the entry was seen `Active` or `Informant` (early arrival, Round R9), otherwise no marker; and a fill change is applied on the same frame with no cross-fade.
- **AC-TMK-11** [PIE-2][Net][1P/3P][Vis] Visual, BLOCKING — `HatchOnSwapFrame`: GIVEN an ally walking into a bush in view, WHEN `BushEnter` reaches its swap notify and later `BushExit` reaches its outward crossing, THEN on the viewer's client the hatch appears on the same rendered frame the body hides and clears on the frame the body reappears (frame-stepped capture); an opponent client in the same scene shows no marker at any frame. Evidence `tmk-hatch-{enter,exit}-{1p,3p}-*.png` (Concealment AC-CON-01, IVP AC-IVP-12/15 cross-check).
- **AC-TMK-12** [U] Logic, BLOCKING — `RetireHold`: GIVEN a tracked marker at defaults, WHEN (a) its `PawnRef` reads null with no feed entry and nothing returns, (b) as (a) while the cached pawn receives an `FDeathState`, (c) the ref reads null and a feed entry is present in the same update, (d) the ref reads null with no feed and resolves to the **same** pawn 0.13 s later, (e) it resolves to a **different** pawn 0.10 s later, (f) as (a) with `RetireAtServerTime` = retirement + 0.75 s (bush strike), THEN (a) and (b) the exit fade starts at +0.25 s (±1 frame) and the death state changes nothing, (c) the marker is `Fed` with no hold and no fade, (d) the hold is cancelled, the marker never fades and `OnAllyMarkerRetired` does not fire, (e) the old marker exits at +0.25 s while a new marker fades in on the new pawn, and no marker is ever drawn between the two pawns, (f) the exit fade starts at +0.60 s (±1 frame); AND GIVEN (g) as (a) with `RetireAtServerTime` 0, (h) as (a) with `RetireAtServerTime` = retirement + 3.0 s, (i) as (a) with `RetireAtServerTime` = retirement − 0.5 s, (j) a handle retired with value +0.75 s, re-adopted, and retired again with value 0 and a stale +0.75 s still present in a later non-retiring update, THEN (g) the exit starts at +0.25 s, (h) the exit starts at +1.5 s, because `min(t_R − T_fade / 2, t_r + T_Rmax)` caps the contact term at `t_r + T_Rmax` (F6), (i) it starts at +0.25 s, (j) the second retirement exits at +0.25 s (the stale value is ignored).
- **AC-TMK-13** [PIE-2][Net] Integration, BLOCKING — `InformantReturn`: GIVEN an ally killed in the open, under `LeakNetProfile`, repeated 20 times, WHEN the informant takeover binds a crowd pawn, THEN in every run the viewer's marker is fully gone no later than 0.35 s after the locally rendered contact frame, is still at least partly visible at the contact frame in ≥ 18 of 20 runs (the "leaves as the blow lands" target), never reappears on the corpse, and fades in outline-only on the adopted pawn; an opponent client sees no marker before, during or after.
- **AC-TMK-14** [PIE-2][1P/3P][Vis] Visual, BLOCKING — `AlwaysOnTopConstantSize`: GIVEN an ally at 3 m, 20 m and 60 m, in the open and fully behind a wall, WHEN captured in 1P and 3P (3P with the eye mask hiding the ally's body), THEN the marker is drawn at every distance at the F1 size (±0 px) above the anchor, identical in 1P and 3P, with no opacity change. Evidence `tmk-ontop-*.png`.
- **AC-TMK-15** [PIE-2][Net][U] Integration, BLOCKING — `FeedHandoffAndStale`: GIVEN an ally whose pawn is forced out of the viewer's connection scope while the server feeds positions (default `TeamPositionFeedGridCm` 10), WHEN the ref later resolves, THEN the fed marker sits within one grid cell of the server position (AC-IVP-15), the handoff to tracking moves the anchor by ≤ 1.5 grid cells with no fade (requires CR-2, feet `Pos`), and fed positions are stepped (never interpolated or extrapolated); AND WHEN sequence advances stop (feed `Seq`, user decision 2026-10-25), THEN the marker hides 2.0 s (±1 frame) after the last advance while `IsMarkedAlly` is unaffected, and a stationary fed ally whose sequence keeps advancing never hides.
- **AC-TMK-16a** [PIE-2] Integration, BLOCKING — `ViewGateFollowsCamera`: GIVEN a scripted `GetViewState()` sequence `Neutral` (`Preparing`) → `Live` → `Live` (own dying body) → `Neutral` (awaiting a pawn) → `Live` (informant pawn), WHEN markers are sampled each frame, THEN markers are drawn iff the state is `Live`, go out under the fade to black (cross-checks AC-CS-37), all fade in together on returning to `Live`, and the marker set and `IsMarkedAlly` are unchanged while `Neutral`; an informant viewer shows a concealed ally's hatched marker.
- **AC-TMK-16b** [PIE-2] Integration, BLOCKING — `EndedAndReset`: GIVEN two allies, one `Active` (filled) and one `Informant` (outline), WHEN the round ends (`EndRoundAll` sets both to `RoundEnded`) and later resets, THEN during `Ended` both markers remain with their last fills (filled and outline) while the viewer's view is `Live`, and at `Reset` every marker goes to `Absent` on the same frame with no hold, no fade and no `OnAllyMarkerRetired`.
- **AC-TMK-17** [U] Logic, BLOCKING — `EdgeClamp`: GIVEN 1920 x 1080 and 3440 x 1440 viewports and anchors to the right, above, behind-left, exactly behind (anchor above and below the camera) and in each corner, WHEN F3 runs, THEN every glyph lies wholly inside the 5 % safe rectangle, the right-hand example lands at (1810, 398), the F3 behind-left example lands at (110, 844) on the left edge (never the mirrored front position or the screen centre), an exactly-behind anchor lands at bottom-centre (960, 1010) whether above or below the camera, a viewer turning through 360° moves the edge marker continuously, including as the ally crosses the camera plane (no jump > `L` px between consecutive frames at 60 fps and 180°/s for anchors with `D_u` in [−0.5, 0.25] at the crossing, and ≤ 1.5 · `L` for `D_u` up to +0.5; the F3 crossing example reproduces (1810, 540), (1810, 650), (1810, 785)), an anchor oscillating ±2 px across the boundary does not toggle the edge variant (hysteresis), and the glyph never rotates.
- **AC-TMK-18** [U] Logic, BLOCKING — `EdgeDeOverlap`: GIVEN (a) four allies clamped to one edge within 10 px of each other, (b) two allies either side of each corner, (c) two allies either side of the perimeter origin (top-left corner), WHEN F4 runs, THEN no two glyph rectangles overlap in any case, the F4 examples are reproduced ((1810, 372)/(1810, 436); (1778, 70)/(1810, 102)), the cyclic order follows the true directions with ties broken by handle, and the result is identical on every run.
- **AC-TMK-19** [U] Logic, BLOCKING — `SizeFormula`: GIVEN viewport heights 720, 1080, 1440 and 2160, `s_ui` ∈ {0.75, 1.0, 1.5} and `k_large` ∈ {1.0, 1.5}, WHEN F1 runs, THEN the sizes match the F1 examples (720p 24 x 28, 1080p 28 x 32, 1440p 37 x 43, 4K 56 x 64, 1080p large 42 x 48), no size is below 24 x 28 and the keyline is never below 3 px.
- **AC-TMK-20** [U] Logic, BLOCKING — `AnchorFromVisualRoot`: GIVEN a tracked ally whose body is presentation-hidden (in a bush) and moving under root motion, and a fed ally, WHEN the anchor is computed each frame, THEN it equals the mesh world location (or fed feet position) plus `TeammateMarkerAnchorCm` and keeps updating while the body is hidden (no bone read).
- **AC-TMK-21** [Vis] Visual, BLOCKING — `GlyphStates`: GIVEN captures of the filled, hatched, outline and edge glyphs, and the edge variant in each of the three fills, at 24 px and 28 px over the brightest and darkest worst-case grounds, in colour, greyscale and protan/deutan/tritan simulation, and in each proposed accent colour, WHEN reviewed, THEN a tester names all three states correctly in greyscale every time, the dual-edge test passes for keyline and fill, and the outline state passes with the inner keyline (CR-7). Evidence `tmk-glyph-*.png` with art lead sign-off.
- **AC-TMK-22** [PIE-2][1P/3P][Vis] Visual, BLOCKING — `NoLagTracking`: GIVEN an ally sprinting across the view while the viewer pans at the stick's maximum rate, WHEN a fixed-timestep 60 fps frame sequence is captured with a test-only, unlit reference sphere attached to the ally at the anchor height (rendered by the engine in the same frame, so the harness does not reuse the marker's own projection), THEN the drawn tip is within 1 px of the sphere's rendered top plus the gap on every frame (no one-frame lag, no smoothing). Evidence `tmk-tracking-*.png`.

**C. Labels, consumers and lifecycle**
- **AC-TMK-23** [PIE-2][U] UI, BLOCKING — `LabelFocusAndHold`: GIVEN three on-screen markers at 40, 58 and 120 px from the centre, WHEN no input is held, THEN only the 40 px marker shows its label, and the server receives no RPC or property change from looking around (the `ShowTeamNames` hold part of this test moves to VS with the hold, user decision 2026-10-25). A 40-character and a duplicate name truncate and keep their suffix. Focus hysteresis (F8): with the focus on the 40 px marker, a second marker moving to 36 px (4 px nearer) does not take the label; at 30 px (≥ `round(8 · u · s_ui)` = 8 px nearer at 1080p) it does, with both labels visible only during the overlapping fades; and the focus never moves to an edge marker. Evidence `tmk-label-*.png`.
- **AC-TMK-24** [U] Logic, BLOCKING — `IsMarkedAllyConsistency`: GIVEN tracked, fed, stale, held and released entries and the view gate closed, WHEN `IsMarkedAlly` is queried for each pawn the client holds, THEN it is true for resolved and held ally pawns (drawn or not), false for every other pawn, false after a retarget for the old pawn, and the IIF issues no `Attack` and shows no Strike hint for a true result (IIF R10, UI 4).
- **AC-TMK-25a** [PIE-2] Integration, BLOCKING — `TravelRebuild`: GIVEN a match moving to its next round by seamless travel, WHEN the controller swap and re-push complete, THEN every marker was removed at travel start with no hold, no fade and no `OnAllyMarkerRetired`, the set is rebuilt only from the re-pushed view, no marker attaches to a pawn of the previous round, and markers appear only once `GetViewState()` is `Live`.
- **AC-TMK-25b** [PIE-2] Integration, BLOCKING — `ViewerRejoinRebuild`: GIVEN a viewer who disconnects and rejoins mid-round, WHEN `RebindView` re-pushes its C-TeamView, THEN the marker set equals the server's teammate list, every marker fades in together when the view goes live, and no marker from before the disconnect persists.
- **AC-TMK-26** [PIE-2][Net] Integration, BLOCKING — `DisconnectNoState`: GIVEN an ally who disconnects mid-round, WHEN grace runs and then expires, THEN during grace the ally's marker is unchanged (filled, no "reconnecting" marker state, no flicker) while its focused label reads "{Name} · reconnecting" (and returns to "{Name}" on a reclaim), and at expiry it holds `TeammateMarkerRetireHoldSec` then fades out and never returns over the released pawn.
- **AC-TMK-27** [PIE-2] Integration, ADVISORY — `PerfBudget`: GIVEN four allies on screen with labels and edge markers on the reference PC, WHEN profiled for 60 s, THEN the marker module's game-thread cost is ≤ 0.05 ms per frame (p95), with zero per-frame heap allocations and at most 2 draw batches.
- **AC-TMK-28** [PIE-3] Integration, BLOCKING — `TwoVTwoNoExtraInfo` (concept design risk): GIVEN a 2v2 test match, WHEN the opposing client's screen and state are inspected, THEN it shows and holds nothing about the other team's players beyond what AC-TMK-01/02 allow; the "reveal by elimination" (every unmarked figure is an NPC or an enemy) is recorded as the expected, accepted consequence of having allies, not a defect.
- **AC-TMK-29** [Playtest] Integration, ADVISORY — `MarkerAttentionCheck` (CD-GDD-ALIGN item 4): GIVEN the first blending playtest (≥ 6 matches, observers and post-round interviews), WHEN observers record whether players' gaze and decisions follow the crowd or their allies' markers, and development telemetry logs per player (i) the share of round time an ally marker is within 15 % of screen height of the centre, (ii) the share of time edge markers are shown, and (iii) the median ally pair distance compared with a no-marker baseline session, THEN the playtest report states the share of rounds in which players say they watched markers rather than the crowd, plus (i)-(iii); if markers dominate (> 1 in 3 rounds), if any player describes markers as "radar" or "minimap", or if the median ally pair distance falls by more than 25 % against the baseline (clustering as a P2 tell), OQ5's opt-in "hide markers except on hold" and an edge-marker distance cap (tested with the development-only console variable) go to the creative-director for a decision. Evidence: the playtest report in `production/qa/`.

- **AC-TMK-30** [PIE-2][Net][U] Integration, BLOCKING — `NullRefInterpretation` (TM-R4a): GIVEN a tracked ally, WHEN (a) the test forces the ally pawn's GUID to stay unmapped on the viewer for 0.10 s while its channel is open (no feed), (b) the server takes the pawn out of the viewer's scope (feed entry added), (c) the server clears the binding (no feed), THEN (a) the marker never fades or moves and `OnAllyMarkerRetired` does not fire, (b) the marker goes `Fed` with no hold and no fade in the same update the ref reads null, (c) the marker holds and exits per F6; in (b) a server log asserts the feed entry and the null ref were sent in one update.
- **AC-TMK-31** [U] Logic, BLOCKING — `RetiredEventOnce` (TM-R14): GIVEN retirements (a) with a drawn marker, (b) with the view gate `Neutral`, (c) with no marker existing for the handle, (d) cancelled by the same pawn returning, (e) at `Reset` and at travel, WHEN `OnAllyMarkerRetired` is observed, THEN it fires exactly once at `t_end` (F6: `t_r + T_hold` with no `RetireAtServerTime`, later for a bush strike) in (a) and (b), exactly once at `t_r` in (c), and never in (d) or (e); it is never replicated (AC-TMK-03).
- **AC-TMK-32** [U] Logic, BLOCKING — `DirectPawnSwitch` (TM-R7): GIVEN a tracked marker whose `PawnRef` switches directly from pawn P1 to resolved pawn P2 in one update (no null between), WHEN rendered, THEN on that frame the single marker is at P2's anchor with no hold, no fade, no second marker, and its fill is that of the entry's current `State`; `IsMarkedAlly(P1)` becomes false and `IsMarkedAlly(P2)` true on the same frame.
- **AC-TMK-33** [U] Logic, BLOCKING — `DrawOrderAndFade` (TM-R9, F7): GIVEN three overlapping on-screen markers at 5, 12 and 30 m, WHEN drawn, THEN the 5 m marker is on top and the 30 m at the bottom; AND GIVEN a 0.30 s entrance, THEN α = 0.156 (±0.001) at 0.075 s, 0.5 at 0.15 s and 1.0 at ≥ 0.30 s, measured on undilated real time with global time dilation 0.5 and with the game paused.

Coverage: every core rule TM-R1 to TM-R14 maps to at least one AC (R1: 01-05, 08; R2: 06, 07; R3: 09; R4: 15, 20; R4a: 30; R5: 10, 11, 16b; R6: 12, 13, 26; R7: 13, 32; R8: 16a; R9: 14, 17, 18, 33; R10: 24; R11: 23; R12: 03, 19, 21; R13: 21, 22; R14: 31; tone and attention: 29) and every formula F1 to F8 to one (F1: 19; F2: 20, 22; F3: 17; F4: 18; F5: 15; F6: 12, 31; F7: 33; F8: 23).

## Open Questions

| # | Question | Owner | Resolve by |
|---|---|---|---|
| OQ1 | **CLOSED — decided by the user 2026-10-25:** the teammate-only reconnecting indicator is MVP: the label suffix here (TM-R11) and the HUD feed lines (HUD-R10a), from the Team-audience `bReconnecting` bit (Roster R6). The marker shape stays unchanged. | user / ux-designer | closed 2026-10-25 |
| OQ2 | Is focus alone enough in the first blending test? (The `ShowTeamNames` hold, its binding and IIF CR-4 are VS since the user decision of 2026-10-25.) | ux-designer / IIF author | IIF revision; first graybox playtest |
| OQ3 | **CLOSED — approved by the user 2026-10-25 and applied (TM-R6 step 2, F6, Roster R6).** Original question: add a team-only `RetireAtServerTime` to the retire update (slot time for a release, the kill's `ContactServerTime` for a kill) so the marker can leave exactly at the blow, including for bush strikes. The 2026-10-24 review removed the client-side death-state extension it would replace (it fired about half the time). Recommended: not in MVP; measure in the first networked playtest whether allies notice the marker leaving before a bush strike's contact. | user / creative-director / Roster and Assassination authors | closed 2026-10-25 |
| OQ4 | Informant ally marker for active allies: **resolved** (art bible 7.x Q1): yes, outline-only, the marked player's state. Recorded here so the art bible's open question can be closed. | art-director | closed |
| OQ5 | Stream sniping: the art bible keeps an opt-in "hide markers except on hold" setting for VS if playtests show sniping (IVP L23 accepted). Does the playtest protocol record it? | ux-designer / Settings author | VS Settings GDD |
| OQ6 | **Answered by ADR-0021 §1-§6 (2026-10-24 ADR sync)**: one volatile Slate leaf widget at HUD layer L0 with in-paint projection (§2), under Camera's fade layer (§3), DPI rule §6. Original question: mechanism: one volatile Slate leaf widget in a HUD layer versus an `AHUD` canvas pass (VA-2). Both meet the rules; the choice is about DPI scaling, z-order against HUD plates and the zero-lag guarantee [verify in UE 5.8.4]. | ui-programmer / technical-director | UI ADR before HUD epics |
| OQ7 | Anchor height: 195 cm assumes the art bible's 175 cm working stature (OI-1 open; prototype Meshy citizens measure 165.6 cm). Re-derive `TeammateMarkerAnchorCm` when OI-1 closes. | technical-artist | OI-1 closure |
| OQ8 | Small test configurations (2v2, concept design risk): markers mean every unmarked figure is an NPC or an enemy. Accepted as inherent to having allies (AC-TMK-28); confirm the playtest protocol treats 2v2 reads as non-representative for Pillar-1 evidence. | qa-lead / game-designer | first playtest protocol |
| OQ9 | Overlapping on-screen markers in dense clusters at long range (four allies grouped at 60 m draw nearly on top of each other): is far-to-near drawing enough, or is a small vertical stack needed? A stack would move markers away from their figures (art bible 8 px motion rule). | ux-designer | first graybox playtest |
| OQ10 | Strike-out handling: **resolved** by Penalty PEN-R4.2 (the exiled pawn is released to the crowd in the same Roster call): the marker holds, exits, and returns outline-only on the adopted pawn (Edge Cases). | Penalty author | closed |
| OQ11 | **CLOSED — decided by the user 2026-10-25:** the focus label stays in MVP; the `ShowTeamNames` hold action and IIF CR-4 move to VS (TM-R11). | user / creative-director | closed 2026-10-25 |
| OQ12 | **CLOSED — decided by the user 2026-10-25:** systems-index effort for row 17 is M; F3, F5, F6 stay; F4 (edge spreading) is cuttable to plain overlap if the schedule slips. | user / producer | closed 2026-10-25 |
| OQ13 | **CLOSED — declined for MVP by the user 2026-10-25 (TM-R4a feed inference stands).** Original question: a team-only `bPawnBound` flag per C-TeamView teammate entry, so "no pawn" and "pawn not replicated to you" are explicit instead of inferred from the feed (TM-R4a). Recommended: not required for MVP (allies are never culled for allies, IVP R8); revisit if a cull distance is ever introduced by ADR. | user / Roster author | closed 2026-10-25 |
| OQ14 | **CLOSED — approved by the user 2026-10-25 and applied (IVP R8 feed `Seq`, ADR-0003; F5 active).** Original question: a per-entry feed sequence in the IVP fallback feed so staleness (F5) works for a stationary ally under change-only replication. Recommended: add it with the feed's ADR (one byte per entry). | user / IVP author | closed 2026-10-25 |
| OQ15 | Edge markers under HUD plates: the plate covers the marker (HUD-R4). Should edge clamping avoid the plate rectangles (objectives top-centre, feed top-right, bottom-left stack) instead? Decide in the HUD UX spec. | ux-designer | `/ux-design` for `design/ux/hud.md` |
