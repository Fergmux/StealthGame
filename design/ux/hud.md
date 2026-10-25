# HUD Design

> **Status**: In Design (authored in an unattended run, 2026-10-25; every choice is logged in `production/session-logs/decision-log.md`; not yet reviewed with `/ux-review`)
> **Author**: ux-designer (autonomous run; no user session)
> **Last Updated**: 2026-10-25
> **Platform Targets**: PC (Windows). Keyboard and mouse primary, gamepad partial, touch none (`project.yaml` has no `platform:` block; source is `.claude/docs/technical-preferences.md`, Input & Platform).
> **Accessibility Tier**: Standard with named Comprehensive elevations (`design/accessibility-requirements.md`, Committed 2026-10-25)
> **Governing documents**: `design/gdd/hud.md` (authoritative for behaviour, timing and wording), `design/gdd/teammate-markers.md`, `design/gdd/informant-pings-team-comms.md`, `design/gdd/camera-system.md`, `design/art/art-bible.md` (3.4, 4.6, 4.7, 7.1 to 7.8), `design/ux/interaction-patterns.md`, `docs/architecture/adr-0021-ui-hud-layering-commonui.md`
> **Template**: HUD Design

> **How to read this spec.** The HUD GDD says *what* the HUD shows and *when*. This spec says *where, how big, in what form, and how it is verified*. Where this spec and the GDD disagree, the GDD wins and the disagreement is in Open Questions. Rule ids (HUD-Rnn, TM-Rnn, PNG-Rnn, F-numbers) point to the GDD that owns them; this spec never restates a timing formula, only cites it. Pixel values are at the 1080p reference (`u = 1`, UI scale 1.0, text scale 1.0) unless stated.

---

## HUD Philosophy

**Framing (from the HUD GDD, confirmed at CD-GDD-ALIGN): "Only what is yours to know."** The HUD is the private notebook of an assassin standing in a sunlit square: a few calm marks at the edge of the eye, never a radar. It shows a player their own mistakes, their own readiness, their house's progress and their allies' fate, and nothing else. The city is the picture; the HUD is the margin.

**Classification: minimal but present, with a fixed layout.** Of the four framings the guide offers ("nearly HUD-free", "minimal but present", "information-dense", "adaptive"), this game is **minimal but present** with one deliberate difference from the usual reading: **the HUD never adapts its density to the situation.** There is no combat HUD and exploration HUD, no fade-out after quiet, no reveal on threat. One layout serves every viewer, team, role and state (art bible 7.7 rule 1; HUD-R5), because a HUD whose structure reacted to the world, or differed between players, would be a Pillar 1 tell. The only things that change are *which* of the fixed slots are occupied, and that change is driven by the owner's own state and the owner's own actions, never by who is in front of them.

**Why not the alternatives.**

| Framing | Verdict | Reason |
|---|---|---|
| Nearly HUD-free (Hollow Knight, Firewatch) | Rejected | Private state (wrong-target tally, brace cooldown, stage card) cannot live in the world: anything diegetic is visible to everyone and becomes a tell the player did not choose (art bible 7.1, "diegetic exceptions allowed: none for gameplay state"). |
| Minimal but present (Dark Souls) | **Adopted** | A handful of always-there plates for knowledge the player needs to decide, plus contextual plates that appear only as the result of the owner's own action. |
| Information-dense (Diablo IV, StarCraft II) | Rejected | A dense HUD competes with the crowd the player must read; the game's skill is sustained visual attention on about 60 near-identical figures (accessibility doc, cognitive context). |
| Adaptive (God of War) | Rejected | Adaptive density is a function of the situation; a situation-dependent layout leaks the situation. |

**The five tests every element must pass** (a proposed element that fails any one is cut or redesigned; these are this spec's operational form of the HUD GDD's rules):

1. **Whose is it?** It is about the viewer, the viewer's allies, a place, or a team total. It is never about a person who is not an ally (HUD-R2; art bible 7.1).
2. **Is it identical for every kind of person in front of me?** Until the owner's own strike resolves, nothing may differ between an NPC, an enemy player and an enemy informant (HUD-R3).
3. **Does it arrive no earlier than the owner could have known?** Anything caused by an outcome waits for the owner's own contact frame, fall or removal (HUD-R6, R6a; PAT-FND-5).
4. **Does it have a shape or text backup, and a non-visual twin if it is urgent?** No meaning by hue or opacity alone (art bible 4.7, 7.2).
5. **Does it fit a slot, or displace something?** Every element lives in a named slot of the layout (Layout Zones) and inside the persistent-area budget (Visual Budget). "It is a small icon" is not an analysis.

**Philosophy conflict check (guide, Section B).** The Must Show list is 7 items (brace indicator, tally, stage card, own steps, rival steps, informant brackets while an informant, ally markers). That is within what the philosophy tolerates, because (a) each is small, (b) the brace indicator and tally are hidden or frozen when they cannot be used, and (c) the cap of 8 % of the screen for persistent plates is enforced by a build-time lint (Visual Budget). No conflict to surface.

**Player journey.** `design/player-journey.md` does not exist. This spec therefore assumes the phases the GDDs imply: Lobby, Briefing, Active (reading the crowd, tending, striking, bracing), the removal moment, the informant phase, and Ended. See Open Question HUD-UX-14.

**Aesthetics served** (concept MDA): Challenge (feedback clarity of the read), Fellowship (team steps, ally lines, pings), Fantasy (civic, level language keeps the player in the assassin's role), and Sensation by restraint (the city stays the picture).

**Tone and language** (art bible 7.6 "plaster setting, not a game show"). Short declarative words; level and civic; never "dead", "lives", "revive", "spectator", "ghost", "detected", "spotted", "tracked", "registered", "draw", "tie", "timer", "time left" or a stealth-meter vocabulary (Roster, IIF and Round language rules; enforced by a lint over `ST_Hud`, AC-HUD-12).

---

## Information Architecture

### Full Information Inventory

Every item the game systems say they need to communicate on screen, aggregated from the UI Requirements section of every system GDD (denominator: **19 system GDDs** in `design/gdd/`, excluding `game-concept.md`, `systems-index.md` and `gdd-cross-review-2026-10-24.md`, which are not systems; all 19 have a UI Requirements section, matched 19 of 19). Element ids HUD-E01 to HUD-E22 are the HUD GDD's; "(17)" is the Teammate Markers layer.

Audiences: **Self** = the viewing player only; **Team** = the viewer's team including its informants; **Public** = every viewer, but only at the phase named. Carriers are the three the HUD may read (HUD-R1): owner data, C-TeamView, `FRoundPublicState`, plus purely local facts.

| # | Information | Source requirement(s) | Audience | Carrier | Category |
|---|---|---|---|---|---|
| E01 | Brace readiness (Ready, Pending, Protected, End, Cooling) with "Brace" label, bound glyph and cooldown numeral | UI-ASN-3, IIF UI-2, HUD-R12 | Self | owner cooldown end, `FHoldStatus` | Must Show (Active) |
| E02 | Wrong-target tally: line, `L` squares, threshold triangle; frozen as a record when an informant | UI-NKP-1, UI-ASN-2, HUD-R7 | Self | `FStrikeResult.TallyAfter`, owner re-push, latched `L` | Must Show |
| E03 | Stage card: title, one line, clue "A court [ClueTag phrase]" | Objectives UI 1, OC-15 | Team | `FObjectiveTeamView` | Must Show |
| E04 | Own steps "Your house", exactly 3 | Objectives UI 2 | Team | `StageIndex` | Must Show |
| E05 | Rival steps "Rival house", exactly 2, never a final step, never a time | Objectives UI 3, OC-12 | Team | `RivalPrelimCount` | Must Show |
| E06 | Team feed: ally removal, ally reconnecting, ally back (max 3 lines) | UI-NKP-5, HUD-R10, R10a | Team | teammate `State`, `bReconnecting`, `DisplayName` | Contextual |
| E07 | Result plate for the owner's own strike: Target exiled, Wrong target, No contact, Reversed | UI-ASN-1, UI-NKP-2, CON UI-5, HUD-R6 | Self | `FStrikeResult` | Contextual |
| E08 | Defence plate "Strike reversed" | UI-ASN-4 | Self (the reversing defender) | `FDefenceResult` | Contextual |
| E09 | Informant brackets (four frame corners) | HUD-R15, art bible 7.3 row 10 | Self (informant) | own `State` | Must Show (informant) |
| E10 | Removal card: "Informant" plus cause subtitle plus "You watch for your team now." | UI-ASN-5, UI-NKP-3, HUD-R9, OQ5 (D11) | Self (the removed player) | own `State`, `bExiled`, `FDeathState` | Contextual (once) |
| E11 | Returning countdown "Returning in {s}" / "Returning" | UI-NKP-4, Camera neutral view | Self (informant awaiting a body) | `T_due` | Contextual |
| E12 | Interact ring (progress; Marigold with four ticks at an own live site after `TendCueDelaySec`) | IIF UI 1, Objectives UI 4 | Self | `FHoldStatus`, team live-site ids | Contextual |
| E13 | Context prompt: glyph plus verb (Tend, Slip in, Slip out) | IIF UI 3, CON UI-1, SCL UI | Self (Active) | public fixture geometry, `PresentedOwnState`, own `IsConcealed` | Contextual |
| E14 | Strike hint "[glyph] Strike" | IIF UI 4, UI-ASN-7, CON UI-4 | Self (Active) | local aim ray, rendering, reach | Contextual |
| E15 | Rejection plate: Not now, Busy, Too far | IIF UI 5, UI-ASN-6 | Self | reply code | Contextual |
| E16 | Informant plate "Informants can only observe" | IIF UI 6 | Self (informant) | reply `NotNow` | Contextual |
| E17 | Third-person hint (once per profile) | Camera UI Requirements | Self | local camera mode | Contextual |
| E18 | End banner (Victory or Defeat, reason, score), called-off line, lobby line | Round UI 2, Objectives UI 6, HUD-R14 | Public (at `Ended`, `Reset`, `Lobby`) | `FRoundPublicState` | Contextual |
| E19 | Latency icon | IIF UI 7 | Self | own RTT | Contextual |
| E20 | Status overlays (connecting, loading, connection lost, session ended) and rejection sentences | Session UI Requirements | Self | own connection status | Contextual |
| E21 | Round summary (two text lines, own house first) | Round R9 (a), HUD-R14 | Public (at `Ended`) | `FRoundResult.TeamSummary` | Contextual |
| E22 | Ping ring, edge ring, sender label | PNG-UI-1 to PNG-UI-3 | Team | `FTeamPingView` | Contextual |
| (17) | Teammate marker (filled, hatched, outline, edge variant) | TM UI-1, CON UI-3 | Team | C-TeamView, `IsAllyBodyHidden` | Must Show (Live view) |
| (17b) | Ally name label (focus; "· reconnecting" suffix) | TM UI-2, TM-R11 | Team | `DisplayName`, `bReconnecting` | Contextual |
| (n/a) | `IsMarkedAlly` exclusion of the strike hint | TM UI-3 | Self | local marker model | Not an element (a rule on E14) |
| (n/a) | Concealed view grade and leaf-edge vignette | CON UI-2 | Self | post-process (Camera) | Not a HUD element (PAT-FBK-2) |

**Not a HUD surface** (named so a reviewer does not read the omission as a gap): Host/Join menu, lobby panel and rejection messages (`design/ux/main-menu.md`, Session); the in-session menu (`design/ux/pause-menu.md`); Camera's options panel and every rebinding or accessibility control (Settings & Accessibility, VS); host controls (Round, console); the in-round player list (Roster OQ11, not MVP); the post-round reveal (Post-Round Reveal, VS); the briefing screen (Round, not yet specced).

### GDD Requirement Coverage

| GDD | UI Requirements items | Covered by (this spec) | Status |
|---|---|---|---|
| assassination-defence-reversal | UI-ASN-1 to UI-ASN-7 | E07, E08, E01, E02, E10, E15, E14 | All covered |
| npc-kill-penalty-informants | UI-NKP-1 to UI-NKP-5 | E02, E07, E10, E11, E06 | All covered (wording per D11) |
| objectives-chain | UI 1 to 6 | E03, E04, E05, E12, E13, E18 | All covered |
| concealment-bushes | UI-1 to UI-5 | E13 (verb), marker hatch, E14 (absent for concealed), E07; UI-2 is post-process | All covered; UI-2 out of HUD by design |
| input-interaction-framework | UI 1 to 8 | E12, E01, E13, E14, E15, E16, E19; UI 8 (rebinding) is Settings | Covered; UI 8 not a HUD surface |
| teammate-markers | UI-1 to UI-3 | (17), (17b), rule on E14 | All covered |
| informant-pings-team-comms | PNG-UI-1 to PNG-UI-4 | E22; PNG-UI-4 (glyph) is a controls reference with no on-screen prompt in MVP | Covered; PNG-UI-4 deliberately not drawn |
| camera-system | one-time hint, neutral-view plates, options panel | E17, E11, PAT-FBK-1; options panel is Settings | Covered; options panel not a HUD surface |
| round-match-framework | host controls, end banner, lobby line | E18, E21; host controls not a HUD surface | Covered |
| session-travel | main menu, lobby panel, status overlays, rejection messages, rule 24 | E20 (display part on L4), rule 24 as Never list | Covered; menus in `main-menu.md` |
| participant-state-roster | data supplier; language rule; frozen name list | E09, E10, E06 wording; no player list in MVP | Covered |
| information-visibility-policy | binding rules, no screen | Philosophy tests 1 to 3; AC-HUD-11 | Covered (rules, not elements) |
| shared-character-locomotion | owner-predicted `IsRunning` / `IsInTraversal` greys prompts | E13 legality (prompt hidden while running or traversing) | Covered |
| hud | E01 to E22 | all sections | All covered |
| crowd-npc-behaviour, character-pool-appearance, level-socket-contract, procedural-city-generator, tuning-data-config | each states "no player-facing UI" | none (correctly absent) | Confirmed headless by reading (logged) |

**Count: 14 of 14 GDDs that carry HUD-relevant requirements are covered; 5 of 19 are confirmed headless.** A GDD that predates the section is not assumed headless: all five headless GDDs were read and each states, in its own UI Requirements section, that no player-facing UI exists (and that a UI element about crowd, look, layout or tuning would breach Pillar 1).

### Categorization

The HUD GDD's inventory is already categorized by its owners and passed two reviews; this spec adopts it unchanged (decision logged) and adds the contribution of the UX view: what is **never** on demand.

| Category | Items | Rule |
|---|---|---|
| **Must Show** (visible whenever the viewer state allows; the player needs it for core decisions) | E01 brace indicator (Active), E02 tally (Active; frozen record as an informant), E03 stage card, E04 own steps, E05 rival steps, E09 informant brackets (informant), ally markers (17) while the view is Live | 7 items. Fixed slots, never reflowed, never reused by another element (HUD-R5). |
| **Contextual** (visible only as the result of the owner's own state or action, or a team event) | E06 feed line, E07 result plate, E08 defence plate, E10 removal card, E11 countdown, E12 ring, E13 prompt, E14 strike hint, E15 rejection plate, E16 informant plate, E17 third-person hint, E18 banner and lobby and called-off lines, E19 latency icon, E20 status overlays, E21 round summary, E22 ping rings, (17b) name label | Transient slots (Result, Notice) hold one message and drop, never queue (PAT-FND-4). |
| **On Demand** (the player must request it) | **None in MVP, by design.** The only request path is the menu key, which opens the pause menu overlay (not HUD). A hold-to-show of ally names (`ShowTeamNames`) is deferred to the Vertical Slice (TM OQ11, user decision 2026-10-25); an on-demand notice log is not committed (accessibility LIM-C2). | Adding an on-demand HUD reveal is a Settings & Accessibility decision and must not add facts (IVP R15). |
| **Hidden** (carried by the world or by audio, never on-screen text) | Opponents' positions, counts, kinds and states; elapsed Active time or any round clock; the strike limit as a number (squares encode it); other players' tallies, cooldowns and braces; that a strike is aimed at you; who struck whom; connection state of anyone but a reconnecting ally; the owner's concealed state (grade only); crowd tells (posture, pace, tends) | The world carries every tell (Sections 5 and 6 of the art bible); the HUD never restates or amplifies them (art bible 7.1). |

**Conflict check against the philosophy.** Must Show is 7 items and Contextual is 17; only the 7 are persistent. No item is categorized as growing the persistent set. If a later system proposes a persistent element, it must name the budget line it affects and what it displaces (Visual Budget, displacement rule).

---

## Layout Zones

The anchors are a data contract fixed by the HUD GDD (HUD-R5) and art bible 7.1; this section fixes the **pixel geometry inside them**. All coordinates are 1080p reference pixels, origin top-left; multiply by `u = s_ui · H / 1080` (F1; ADR-0021 §6) for any other viewport.

### Reference frame

| Quantity | Value at 1920 x 1080 | Rule |
|---|---|---|
| Safe rectangle | `[96, 1824] x [54, 1026]` (5 % margin, `HudSafeMarginPct`) | Nothing in L1 to L3 and no edge-clamped L0 glyph leaves it; the L4 display plate and the fade layer are exempt (full-screen by nature). |
| Cluster inset | 12 px inside the safe rectangle on both axes | Keeps the four informant brackets (which hug the safe-rectangle corners) from touching the clusters. Applies in every state, so nothing moves when brackets appear. |
| Centre | Box `[768, 1152] x [432, 648]` (20 % x 20 %) | **No persistent element at any text scale.** Cards and the banner are transient and may use the central band (art bible 7.3 row 11). |
| Result slot | centre `(960, 756)` = 70 % of height | HUD-R5, F1. |
| Notice slot | top `y = 756 + 32 + 12 = 800` | F1 (`y_result + h/2 + g`, one-line Result plate 64 high). |

### Layers (HUD-R4, ADR-0021 §1)

| Layer | ZOrder | Widget | Holds |
|---|---|---|---|
| L0 world-anchored | 10 | `UStealthWorldAnchorLayer` (one volatile Slate leaf) | ping rings and labels (beneath), teammate markers and focus labels, interact ring, context prompt |
| Fade | 20 | `UStealthScreenFadeLayer` | Camera's published black; covers L0, not L1 and above |
| L1 persistent plates | 30 | `UStealthHudPersistentLayer` | brace cluster, tally, rejection plate, objectives cluster, team feed, informant brackets, latency icon |
| L2 lower-centre | 40 | `UStealthHudTransientLayer` | Result slot, Notice slot |
| L3 cards and banners | 50 | `UStealthHudCardLayer` | removal card, end banner, round summary, called-off line, lobby line |
| L4 status overlays | 60 | `UStealthStatusOverlayLayer` | Session's display-only connection and loading text |
| Menus | 100 | `UStealthMenuRoot` (Common UI) | pause menu, lobby, Session status action modal |

Every layer below 100 is `HitTestInvisible`, takes no input and is never a Common UI activatable. The HUD therefore has **no focus order and no pointer behaviour at all**; it cannot be tabbed into or clicked, on either input method.

### Wireframe (schematic, not to scale; dimensions are in the zone table)

```
 safe rectangle (96,54)-(1824,1026), state: SelfActive (informant brackets absent)
 +--------------------------------------------------------------------------------------------+
 |                         [Your house ]                 [Rival house]                 [lat]  |
 |                         [ #  #  .   ]                 [ #  .     ]            +-----------+|
 |                         +------------------------------------------+            |{Ally} --  ||
 |                         | STAGE CARD   title / one line / clue     |            |eliminated ||
 |                         +------------------------------------------+            |{Ally} back||
 |                                                                                  +-----------+|
 |                                                                                              |
 |                          . . . centre 20 % x 20 %: always empty . . .                        |
 |                                                                                              |
 |                                    +----------------------+                                  |
 |                                    |  RESULT SLOT (70 %)  |                                  |
 |                                    +----------------------+                                  |
 |                                     [glyph] Strike   (NOTICE SLOT)                           |
 |  +-------------------+                                                                       |
 |  | [# # .] [^]       |   tally                                                               |
 |  | Wrong targets: 2  |                                                                       |
 |  +-------------------+                                                                       |
 |  (O)  [Q] Brace in 0:43   [Not now]   brace row; rejection plate beside it (transient)       |
 +--------------------------------------------------------------------------------------------+
 informant state: brace row collapsed (tally stays, frozen), four corner brackets L-- --7 / L-- --J added
 L0 (not drawn above): ally markers, ping rings, interact ring and prompt float inside the safe rectangle
```

### Zone table

| Zone | Layer | Rect at 1080p (x, y, w x h) | Contents | Growth rule (text scale or longer strings) |
|---|---|---|---|---|
| **Z1 Bottom-left cluster** | L1 | `(108, 870)` to `(408, 1014)`, 300 x 144 | tally plate `(108, 870)` 244 x 80; brace row `y 958-1014`: ring `(108, 958)` 56 x 56, label plate `(174, 966)` 232 x 40 | anchored bottom-left; grows **up and right** |
| **Z2 Top-centre cluster** | L1 | `(730, 66)` to `(1190, 220)`, 460 wide | steps row `y 66-122`: own plate `(730, 66)` 161 x 56, rival plate `(1082, 66)` 108 x 56 (right-aligned to the card); stage card `(730, 128)` 460 x 92, centred on `x = 960` | anchored top-centre; grows **down and outward**; card capped at 40 % of safe width from text scale 150 % |
| **Z3 Top-right cluster** | L1 | latency plate `(1772, 66)` 40 x 40; feed `(1392, 114)` to `(1812, 214)`, 420 x 100 | feed lines 30 high, 5 gap, right-aligned; latency icon above the first line | anchored top-right; grows **down and left**; feed width limited by Z2's right edge |
| **Z4 Result slot** | L2 | plate 300 x 64 centred `(960, 756)`: `(810, 724)` | result plates, defence plate, informant plate | grows with text; width capped at 60 % of safe width; wraps to 2 lines |
| **Z5 Notice slot** | L2 | top `y = 800`, centred on `x = 960`, one plate at a time, up to 560 x 44 | countdown, third-person hint, strike hint | grows downward; never overlaps Z4 (top follows Z4's actual height, F1) |
| **Z6 Card band** | L3 | removal card 640 x 160 centred `(960, 464)` (`y 384-544`); banner stack 720 x 238 with top `y = 340` (`y 340-578`); lobby and called-off line plate 560 x 44 centred `(960, 540)` | removal card (4 s), end banner plus summary | each at most 30 % of the screen; central band is allowed for transient cards |
| **Z7 Frame** | L1 | four brackets 32 x 32, one at each safe-rectangle corner | informant brackets | fixed |
| **Z8 World layer** | L0 | the safe rectangle (edge clamp) | markers, labels, rings, prompt | constant pixel size |
| **Z9 Status** | L4 / fade | status plate top `y = 236`, centred, up to 96 high; fade is full-screen | Session text; Camera black | text may wrap to 2 lines |

**Slot rules.**

- **One layout, no reflow.** An element that does not apply is *collapsed*, and its slot stays empty; a persistent element's slot is never reused by a different element (HUD-R5). The two shared transient slots (Z4, Z5) are the only exceptions, ordered by the priority lists in Feedback & Notification Systems.
- **Bottom-left stack (resolves interaction-patterns P5).** Top to bottom: tally plate, then the brace row. The **rejection plate sits beside the brace row**, to its right, `10 px` beyond the cluster edge and on the label plate's baseline (`x = 418`, `y = 966`), so it never covers the tally. It is transient and outside the persistent footprint. HUD-R5's wording "stacked upward" for the rejection plate disagrees with HUD-R13, HUD-E15 and IIF UI 5 ("beside"); this spec follows the majority and records the conflict (Open Question HUD-UX-1).
- **Top-right.** The latency icon plate sits on the safe-rectangle edge **above** the feed's first line and never displaces a line (GDD edge case); an informant's top-right bracket has its arms along the safe-rectangle top and right edges (`x 1792-1824`, `y 54-86`) and clears the icon plate by 10 px on both axes.
- **Top-centre.** The own bar is drawn at 1.25 x on Obsidian Raised; the rival bar is plain Obsidian. The empty gap between the two plates is deliberate: the rival bar is never adjacent to the own bar, so the two reads never merge.
- **Self-owned feedback never sits on or around the viewer's own body**, in either camera mode (art bible 7.1): no ring, glow or plate is anchored to the owner's pawn. The interact ring and the prompt anchor to the **object**.
- **Camera-mode independence.** One layout serves first and third person; no element changes position or size with the camera mode.

### Aspect ratios and ultrawide (answers HUD OQ9)

| Aspect | Support | Layout rule |
|---|---|---|
| 16:9 | Reference | as above |
| 16:10 | Supported; **the binding case for the area budget** (narrowest supported) | as above; area at 7.73 % |
| 21:9, 32:9 | Supported | The **corner clusters (Z1, Z2, Z3) are clamped to a centred 16:9 region of the safe rectangle** (width = safe height x 16/9) so the eyes never travel to the far edges. The frame brackets (Z7) and the L0 edge clamp use the **full** safe rectangle. Z4 and Z5 stay centred. |
| 4:3, 5:4 | **Not supported** | Every rule holds except the area cap (9.27 % at 4:3 for the Self-Active state, 8.01 % for an informant, 9.89 % at 5:4); no resolution is locked. |

The safe margin stays a tuning key (`HudSafeMarginPct`) in MVP and is **not** a player setting; if Settings & Accessibility later exposes it for a TV or console target, it moves out of the key table to a client-local setting (TDC CR-14) and F1 must be re-run (Open Question HUD-UX-9).

### Text-scale reflow (paper layout; the layout resolver and AC-HUD-15 are authoritative)

Text scale grows text, not geometry (rings, squares and the marker keep their 100 % size, art bible 7.4). Plates grow with their text and never truncate; names ellipsise at the plate cap. Derived with an assumed average glyph advance of 0.5 em (Regular) and 0.56 em (Semibold, tabular); **the display face is not chosen yet, so every width below is NOT MEASURED** (Open Question HUD-UX-8).

| Element | 100 % | 125 % | 150 % | 200 % |
|---|---|---|---|---|
| Z1 cluster (w x h) | 300 x 144 | 336 x 150 | 378 x 158 | 460 x 182 |
| Z1 top y (pause-menu clearance, PM-13) | 870 (80.6 %) | 864 | 856 | 832 (77.0 %) |
| Z2 own / rival plate width | 161 / 108 | 161 / 131 | 161 / 154 | 196 / 200 |
| Z2 stage card (w x h, strings within budget) | 460 x 92 | 575 x 111 | 690 x 131 (capped at 40 % = 691) | 691 x 267 (title 1 line, line and clue 2 lines each) |
| Z2 bottom y | 220 | 244 | 270 | 418 (clears the centre box at 432 by 14 px) |
| Z3 feed (w x h; 3 entries) | 420 x 100 | 549 x 121 (1 line each) | 491 x 256 (up to 2 lines each) | 490 x 334 (2 lines each; x 1322-1812) |
| Z3 feed bottom y (top is fixed at 114) | 214 | 235 | 370 | 448 (x >= 1321, outside the centre box) |
| Z4 result plate (w x h) | 300 x 64 | 311 x 64 | 357 x 64 | 450 x 73 |
| Overlap check | none | none | Z3 left edge 1321 > Z2 right edge 1305 | Z1 right edge 568 < Z4 left edge 735; Z3 left edge 1322 > Z2 right edge 1306 |

**String budgets (binding; they keep the stage card on its reserved footprint).** At 100 % English: stage-card title at most 27 characters (Semibold 22 px), card line at most 48 characters (18 px), clue string at most 43 characters including "A court " (20 px). A localisation that wraps one of these at 100 % **breaches F1** (one extra card line adds 0.64 percentage points at 16:10); it must be shortened, not wrapped (Open Question HUD-UX-15). From text scale 125 % wrapping is expected and accepted.

### Clearance with the pause menu (answers pause-menu PM-13)

The pause menu docks left and assumes "panel bottom at most 70 % of height" (y = 756). The tallest bottom-left cluster top is y = 832 (200 % text), so the panel clears the cluster by at least 76 px at every text scale and does not cover the brace or tally. The HUD keeps rendering and updating beneath the menu (the world is not paused, `design/ux/pause-menu.md` PM-R4).

---

## HUD Element Specifications

[To be designed]

---

## HUD States by Gameplay Context

[To be designed]

---

## Information Hierarchy

[To be designed]

---

## Visual Budget

[To be designed]

---

## Feedback & Notification Systems

[To be designed]

---

## Platform Adaptation

[To be designed]

---

## Accessibility

[To be designed]

---

## Tuning Knobs

[To be designed]

---

## Acceptance Criteria

[To be designed]

---

## Open Questions

[To be designed]
