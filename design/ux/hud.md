# HUD Design

> **Status**: In Design (authored in an unattended run, 2026-10-25; every choice is logged in `production/session-logs/decision-log.md`; reviewed with `/ux-review` 2026-10-25: NEEDS REVISION, revisions applied in place, report in `production/design-closeout/ux-review-hud.md`)
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
- **Bottom-left stack (resolves interaction-patterns P5).** Top to bottom: tally plate, then the brace row. The **rejection plate sits beside the brace row**, to its right, `10 px` beyond the cluster edge and on the label plate's baseline (`x = 418`, `y = 966`), so it never covers the tally. At text scale above 100 % the cluster's right edge moves, so the plate's `x` is always **the cluster's right edge + 10 px** (454, 496 and 578 at 125, 150 and 200 %; `y` stays on the label plate's baseline). It is transient and outside the persistent footprint. HUD-R5's wording "stacked upward" for the rejection plate disagrees with HUD-R13, HUD-E15 and IIF UI 5 ("beside"); this spec follows the majority and records the conflict (Open Question HUD-UX-1).
- **Top-right.** The latency icon plate sits on the safe-rectangle edge **above** the feed's first line and never displaces a line (GDD edge case); an informant's top-right bracket has its arms along the safe-rectangle top and right edges (`x 1792-1824`, `y 54-86`) and clears the icon plate by 10 px on both axes.
- **Top-centre.** The own bar is drawn at 1.25 x on Obsidian Raised; the rival bar is plain Obsidian. The empty gap between the two plates is deliberate: the rival bar is never adjacent to the own bar, so the two reads never merge.
- **Self-owned feedback never sits on or around the viewer's own body**, in either camera mode (art bible 7.1): no ring, glow or plate is anchored to the owner's pawn. The interact ring and the prompt anchor to the **object**.
- **Camera-mode independence.** One layout serves first and third person; no element changes position or size with the camera mode.

### Aspect ratios and ultrawide (answers HUD OQ9)

| Aspect | Support | Layout rule |
|---|---|---|
| 16:9 | Reference | as above |
| 16:10 | Supported; **the binding case for the area budget** (narrowest supported) | as above; Self-Active area 7.12 % (Visual Budget) |
| 21:9, 32:9 | Supported | The **corner clusters (Z1, Z2, Z3) are clamped to a centred 16:9 region of the safe rectangle** (width = safe height x 16/9) so the eyes never travel to the far edges. The frame brackets (Z7) and the L0 edge clamp use the **full** safe rectangle. Z4 and Z5 stay centred. |
| 4:3, 5:4 | **Not supported** | Every rule holds except the area cap (Self-Active 8.55 % at 4:3 and 9.12 % at 5:4; informant 8.01 % and 8.55 %); no resolution is locked. |

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

**String budgets (binding; they keep the stage card on its reserved footprint).** At 100 % English: stage-card title at most 27 characters (Semibold 22 px), card line at most 48 characters (18 px), clue string at most 43 characters including "A court " (20 px). A localisation that wraps one of these at 100 % spends 0.64 of the 0.88 percentage points of margin at 16:10 (one extra card line: 7.12 % to 7.76 %); **two wrapped lines breach F1** (8.40 %). Localisation must therefore shorten, not wrap (Open Question HUD-UX-15). From text scale 125 % wrapping is expected and accepted.

### Clearance with the pause menu (answers pause-menu PM-13)

The pause menu docks left and assumes "panel bottom at most 70 % of height" (y = 756). The tallest bottom-left cluster top is y = 832 (200 % text), so the panel clears the cluster by at least 76 px at every text scale and does not cover the brace or tally. The HUD keeps rendering and updating beneath the menu (the world is not paused, `design/ux/pause-menu.md` PM-R4).

---

## HUD Element Specifications

Every element below is a **dumb widget** applying a view-model value (ADR-0021 §4): no property bindings, no animation assets, no logic in the widget Blueprint. Timelines are sampled by the presenter from `now - t0` (HUD F3). Every element has a pattern reference in `design/ux/interaction-patterns.md` (PAT-*); this spec adds geometry, text and states and cites the GDD rule that owns each timing. "Reduce motion" behaviour is stated once in Accessibility and referenced here only where an element has a special case.

### Shared visual system

| Item | Value | Source |
|---|---|---|
| Plate | Obsidian Ink `#1C1917`, 100 % opaque, stepped 4 px corners (two steps), 2 px stroke; no shadow, gradient or glow | PAT-FND-1, art bible 3.4, 7.2 |
| Palette | Bone `#EFE8D6`, Bone Dim `#B9B09C` (18 px labels only), Marigold `#E3A72F` (progress, intel), Cochineal Bright `#E04A62` on dark (glyphs only; text never below 28 px), Cochineal Deep `#B01E3C` (bars; Bone text on it), Obsidian Raised `#2B2622` (own objective bar). **Turquoise appears only on ally chrome** (marker, ping label stroke; Open Question HUD-UX-3) | art bible 4.6 |
| Keyline | 2 px Obsidian outside every coloured glyph; 3 px on the marker and the ping (ping 4 px beyond 20 m) | art bible 3.4, 7.2 item 3 |
| Type | **Display** 48 px (banner header) and **Heading** 28 px (card title): display accent, functional Semibold fallback; **Primary** 22 px Semibold tabular (result text, stage title, tally line, countdown, numerals, marker label); **Body** 20 px Regular (prompts, feed, rejection, strike hint, clue, brace label, hints); **Minimum** 18 px Regular, Bone Dim allowed (step labels, card line, summary lines, ping label). Tracking +2 %, line height 1.3. No Light or Thin. Text on an opaque plate only | art bible 7.4, HUD-R16, VA-HUD-2 |
| Floors | any text 18 px; text the player acts on 20 px; changing numerals 22 px; Cochineal Bright text 28 px (at 1080p, scaled by `H/1080`, never lowered by UI scale) | HUD-R16, accessibility V-03 |
| Contrast | text below 28 px at least 4.5:1 on its plate; larger text and graphics at least 3:1; Bone about 14:1, Bone Dim about 8:1, Marigold about 8:1, Cochineal Bright about 4.4:1 (graphic use only), Bone on Cochineal Deep about 5.5:1 (to be re-measured on the real plate, art bible OI-3) | art bible 4.6, HUD-R16 |
| Glyph set | ally = down pentagon (Turquoise); danger = up triangle with stepped base (Cochineal); tally = stepped square; stamp = stepped square with one horizontal bar (Bone); duration or place = circle; progress = stepped bar; informant = corner brackets. No arrow, eye, crosshair, skull, heart, footprint or human silhouette | PAT-FND-3, art bible 7.5 |
| Glyph plate | stepped square 32 x 32, Bone outline, holds the **currently bound** key or button; an unbound action shows "Unbound" in Bone Dim; never a hard-coded key name | PAT-FND-2 |
| Motion | stepped urgent entrance 250 ms (k = 4); non-urgent plates 400 ms ease-out with 8 px settle in whole 4 px steps, 600 ms out; step fill 500 ms; sweeps and rings are the only linear motion; nothing pulses faster than 0.5 Hz; no scale pop, shake, bounce or overshoot | art bible 7.6, HUD F3 |
| Rendering | flat, resolution-scaled, opaque materials, nearest filtering, no mips; no bloom or emissive; drawn after post-process; all widgets created once and collapsed when unused | VA-HUD-5, ADR-0021 §1 |

### String table `ST_Hud` (all player-facing text; none travels on the wire)

Dynamic values are formatted locally. Wording is the HUD GDD's, including the user-ratified removal wording of 2026-10-25 (D11): title **"Informant"**, subtitles **"Too many wrong targets"** and **"Eliminated for the round"**, plate **"Target exiled"**.

| Key | Text | Used by |
|---|---|---|
| `Hud.Brace.Label` | Brace | E01 |
| `Hud.Brace.Cooling` | Brace in {m}:{ss} | E01 (F5) |
| `Hud.Glyph.Unbound` | Unbound | every glyph plate |
| `Hud.Tally.Line` | Wrong targets: {n} | E02 |
| `Hud.Step.Own` / `Hud.Step.Rival` | Your house / Rival house | E04 / E05 |
| `Hud.Prompt.Tend` / `.SlipIn` / `.SlipOut` | Tend / Slip in / Slip out | E13 |
| `Hud.Hint.Strike` | Strike | E14 |
| `Hud.Reject.NotNow` / `.Busy` / `.OutOfReach` | Not now / Busy / Too far | E15 |
| `Hud.Informant.Plate` | Informants can only observe | E16 |
| `Hud.Hint.ThirdPerson` | Third person shows only what your character can see. | E17 |
| `Hud.Result.Exiled` / `.WrongTarget` / `.NoContact` / `.Reversed` | Target exiled / Wrong target / No contact / Reversed | E07 |
| `Hud.Result.Defence` | Strike reversed | E08 |
| `Hud.Card.Title` | Informant | E10 |
| `Hud.Card.Sub.Exile` / `.Sub.Removed` | Too many wrong targets / Eliminated for the round | E10 |
| `Hud.Card.Line2` | You watch for your team now. | E10 |
| `Hud.Feed.Removed` | {Ally} — eliminated for the round | E06 |
| `Hud.Feed.Reconnecting` / `.Back` | {Ally} — reconnecting / {Ally} — back | E06 |
| `Hud.Ally.Unresolved` | Ally | E06, E22 label, (17b) |
| `Hud.Marker.Reconnecting` | {Name} · reconnecting | (17b) |
| `Hud.Countdown` / `Hud.Countdown.Zero` | Returning in {s} / Returning | E11 |
| `Hud.Banner.Victory` / `.Defeat` | Victory / Defeat | E18 |
| `Hud.Banner.Reason.Objective` / `.Elimination` | completed the final objective / every rival eliminated for the round | E18 |
| `Hud.Banner.Score` | Your house {a} · Rival house {b} | E18 |
| `Hud.Summary.Own` / `.Rival` | Your house — steps {s} of 3 · eliminated {e} · wrong targets {w} / Rival house — steps {s} of 3 · eliminated {e} · wrong targets {w} | E21 |
| `Hud.CalledOff` | The round was called off | E18 |
| `Hud.Lobby.Wait` | The host will begin the next round | E18 |

A lint over this table enforces the language rule (banned words, AC-HUD-12). No string names the strike limit as a number, a clock, a count of opponents, a killer, a kind or a team.

### E01 Brace indicator

*Must Show (Active) · Z1 · L1 · PAT-PLT-1 · HUD-R12, F5*

- **Form.** A 56 x 56 opaque plate containing a circular **hollow track** (outer diameter 44 px, band 4 px: two concentric 2 px Bone outlines) and, to its right, a label plate 232 x 40 holding the glyph plate and the text: **"[glyph] Brace"**, and while Cooling **"[glyph] Brace in 0:43"** (Primary 22 px Semibold tabular numerals, `Hud.Brace.Cooling`). The label "Brace" is persistent in every state (ruling D3); it may later be hidden by Onboarding. Bone and Bone Dim only, **no signal hue**.
- **States** (all identical in structure for every owner; no cue of any kind when a strike is aimed at the viewer, ASN-R6):

| State | Track | Inner square | Label text | Timing |
|---|---|---|---|---|
| Ready | empty hollow track | none | Brace | none |
| Pending | track outline thickens one step (2 to 3 px); nothing fills | none | Brace | on the press, within one frame; no protection shown |
| Protected | band fills in 4 stepped segments, Bone | stepped 16 x 16 square appears at once (Bone) | Brace | only on server confirmation (`FHoldStatus`); one segment per `DefenceDurationSec`/4; never shrinks, narrows, closes or flashes; no sweep drawn |
| End | band empties, square removed with a 250 ms stepped exit | removed | Brace | presented when `ServerNowEst` reaches `FHoldStatus.ServerEndTime` (on a reversal the contact time), never on receipt; identical for release, cap, cancel and reversal |
| Cooling | band fills **linearly clockwise from 12 o'clock** in Bone Dim (the only linear motion) | none | Brace in m:ss | from `LastPrepareActivation`; numeral changes only when `ceil(r)` changes; holds "0:01" until the estimated server end plus half own RTT, then Ready (D5) |

- **Hidden** when `PresentedOwnState = Informant` (collapsed, slot stays empty). No ready-again cue is visible or audible to anyone else; the owner's own ready-again is a soft 2D UI-bus tone.
- **Reduce motion:** entrance and exit instant; the sweep stays (real-time progress) and advances in 1 s steps (F5).
- **Hold-to-toggle** changes nothing here: the indicator is identical either way (art bible 7.x question 2).

### E02 Wrong-target tally

*Must Show (Active; frozen record as an informant) · Z1 · L1 · PAT-PLT-2 · HUD-R7, F6*

- **Form.** One plate 244 x 80, fixed width regardless of `L` (so nothing reflows between presets). Row 1 (24 px): `L` stepped squares 24 x 24, gap 6, left-aligned (`L` from 2 to 5, default 3), then the static **upward danger triangle** 24 x 24 at the row's right end (reserved even when absent). Row 2: **"Wrong targets: {n}"**, Primary 22 px Semibold tabular. The limit is never written as a number.
- **Squares.** Empty = 2 px Bone outline; filled = Cochineal Bright fill inside the same Bone outline; `min(n, L)` filled.
- **Triangle.** Cochineal Bright, 2 px Obsidian keyline, stepped base; visible from `n >= L - 1` while `PresentedOwnState = Active`, kept for `n > L`; static, never pulses. Removed when the owner becomes an informant (D4).
- **Update.** Changes only at the show time of the plate that carries the increment (HUD-R7): one square fills in a **300 ms stepped fill** (four 75 ms steps), the numeral changes in **one step** (no roll-up), one tally tick, and a threshold tick once `n >= L - 1`. A held re-push is never shown early and never lowers the value within a round.
- **As an informant.** Squares and numeral frozen at their last shown value as a record, no triangle, no animation.
- **Reduce motion:** the fill is instant.

### E03 Stage card

*Must Show · Z2 · L1 · PAT-PLT-3 · HUD-R11, F9*

- **Form.** One plate 460 x 92 under the steps row. Three left-aligned lines: **title** (Primary 22 px Semibold), **one line** (Minimum 18 px Bone Dim), **clue** "A court [ClueTag phrase]" (Body 20 px Bone). Strings obey the budgets in Layout Zones (title at most 27, line at most 48, clue at most 43 characters at 100 %).
- **Update.** Turns over on a counted completion in **500 ms** (`HudObjectiveStepSec`): the old text fades out over the first half and the new text in over the second, the plate itself unchanged. Content arrives only at `Active` entry; the cluster is empty during `Preparing`.
- **Never** a waypoint, compass, arrow, distance read-out, map or world plate (OC-15).

### E04 Own steps and E05 Rival steps

*Must Show · Z2 · L1 · PAT-PLT-3 · HUD-R11, F9*

| Property | E04 Own steps | E05 Rival steps |
|---|---|---|
| Label | "Your house" (18 px) | "Rival house" (18 px) |
| Steps | exactly 3 (Stage 1, Stage 2, Final) | exactly 2 |
| Step size | 45 x 18 (1.25 x of 36 x 14), gap 5 | 36 x 14, gap 4 |
| Plate | 161 x 56 on Obsidian Raised | 108 x 56 on Obsidian |
| Fill | Marigold, 2 px Obsidian keyline; empty steps 2 px Bone Dim outline | same |
| Data | `StageIndex` mapped 0, 1, 2; the Final fills **only in `Ended` by the win** | `rival_shown = clamp(max(RivalPrelimCount, prev), 0, 2)`, monotonic within a `RoundEpoch` |
| Animation | a step fills in **500 ms** ease-out | a rise of 2 fills both steps in **one** 500 ms fill with **one** tick; never a final step, a sub-stage fill or a time |

On an own objective win the Final step fills over `HudObjectiveStepSec` from the F10-gated `Ended` arrival; the banner and the cluster fade wait for the fill (ruling D2). No "n/5" label and no Turquoise bar, ever.

### E06 Team feed

*Contextual · Z3 · L1 · PAT-PLT-4 · HUD-R10, R10a, F8*

- **Form.** Up to `HudFeedLineCount` (3) line plates, 30 px high, 5 px gap, right-aligned in the 420 px footprint, newest at the bottom, oldest evicted only after `HudFeedMinDwellSec`. Body 20 px Bone. A line is `[up triangle 16 px, Cochineal Bright] text` for a removal and text only for reconnecting and back lines (the triangle is finality; a reconnect is not).
- **Line types** (all identical for every cause; no time, ping or reason): `{Ally} — eliminated for the round`, `{Ally} — reconnecting`, `{Ally} — back`. Names are ellipsised at the remaining plate width; unresolved reads "Ally".
- **Timing.** Removal line at the later of the state's arrival and `OnAllyMarkerRetired` (so it enters with the marker's exit fade); entrance non-urgent (400 ms ease-out, 8 px settle), exit 600 ms; life `HudFeedLineLifeSec` (6.0 s); pending queue of at most 4. Marker retirement alone never creates a line; a `RoundEnded` state never creates one; rebuilds replay none.
- **Never** shown on the disconnected player's own screen, on any opponent's screen, or for a join or leave (Session rule 24). The reserved sighting line type stays unused (PNG-R8).

### E07 Result plate and E08 Defence plate

*Contextual · Z4 Result slot · L2 · PAT-TRN-1 · HUD-R6, F2, F3*

- **Form.** One plate 300 x 64 (width computed from the **widest result string of the active locale**, never from the shown string, so size is outcome-independent, HUD-R3). Left: a 40 x 40 stamp or glyph slot (always reserved); right: the text, Primary 22 px Semibold Bone, centred in the remaining width.

| Outcome | Text | Glyph slot |
|---|---|---|
| `RivalEliminated` | Target exiled | Bone stepped-square stamp |
| `WrongTarget` | Wrong target | Cochineal Bright upward triangle (byte-identical for an NPC and an informant victim) |
| `NoContact` | No contact | empty (a miss is not finality; no Cochineal) |
| `Reversed` (attacker) | Reversed | Cochineal Bright upward triangle; the removal card follows and preempts |
| Defence (defender) | Strike reversed | Bone stepped-square stamp; never names the attacker or its team |

- **Timeline** (one for every outcome): `Pending` until the F2 show time (the owner's own contact frame, never earlier) then **250 ms stepped in** (four 62.5 ms steps), **3.0 s hold** (x `HudReadHoldMult`), **600 ms out** (fade, no movement); total 3.85 s. Preempted by the removal card over `HudPreemptExitSec` (250 ms stepped). A late result is shown on processing and never dropped for lateness in live play.
- **Parity.** Same size, slot, entrance, hold and exit for all five; only text and glyph differ, and only after the show time. An informant never receives a result plate.
- **Cues.** One owner-only 2D result cue per plate with a distinct id per outcome, fired at the show time (VA-HUD-6, CR-HUD-15); the tally tick fires with the plate.

### E09 Informant brackets

*Must Show (informant) · Z7 · L1 · PAT-PLT-5 · HUD-R15*

- **Form.** Four stepped corner brackets, one per safe-rectangle corner, arm length 32 px, Bone 2 px stroke with a 2 px Obsidian keyline on both sides (so the frame reads over any ground), no fill, no label.
- **Timing.** Presented at `PresentedOwnState = Informant` (the removal-card entrance time, F4): 400 ms ease-out fade in, static afterwards. On a rebuild or a rejoin after the grace they are present at once, with no animation. They fade with the other L1 clusters at the end banner (600 ms).
- **Rules.** Never visible to any other viewer. The informant grade is Camera's post-process and is not part of this element; the brackets carry the state when Reduce screen effects scales the grade down.

### E10 Removal card

*Contextual (once per removal) · Z6 · L3 · PAT-CRD-1 · HUD-R9, F4*

- **Form.** One card 640 x 160 centred at `(960, 464)`, at most 30 % of the screen (about 4.8 %), never full-bleed. A **title bar** 640 x 56 in opaque Cochineal Deep holding an upward stepped triangle (40 px, Bone) and **"Informant"** (Heading 28 px, Bone); beneath it an Obsidian body plate 640 x 104 with the **cause subtitle** (Primary 22 px Semibold Bone) and **"You watch for your team now."** (Body 20 px Bone). No striped band (the pattern limit of 5 repeats is never approached).
- **Text** (D11, identical shape for every cause): subtitle **"Too many wrong targets"** for the strike-limit exile, **"Eliminated for the round"** for an assassination or a reversal. The card size is computed from the widest subtitle so it is cause-independent.
- **Timeline.** Enters at F4's card time (exile: plate show time + `ExileBeatSec` 0.8 s; assassinated or reversed: own fall start + `HudRemovalCardDelaySec` 0.8 s): the Result-slot plate leaves over 250 ms stepped, the card **enters over 600 ms** (eased fade), **holds 4.0 s** (x `HudReadHoldMult`), then **collapses over 800 ms** (alpha 1 to 0, ease-in-out; the frame brackets are already present, see Open Question HUD-UX-16). Fires the client-local `OnRemovalCardEntered` once (Camera's exile `ViewRelease`, the removal cue).
- **Never** a card for an `InformantKilled` (no card, no exile cue), after a rejoin past the grace, or when `Ended` arrives before the card has entered; a showing card exits over `HudPreemptExitSec` as the banner enters. Shown once, never replayed. No field names the killer, its kind or its team; no kill cam.
- **Photosensitivity.** The grade, plate, preempt exit, card and camera fade are one budgeted sequence (VA-HUD-4, AC-HUD-18). After 0.25 s exactly one text block is on screen.

### E11 Returning countdown

*Contextual · Z5 Notice slot · L2 · PAT-TRN-5 · UI-NKP-4, F7*

- **Form.** One plate (Primary 22 px Semibold tabular, Bone on Obsidian) centred in the Notice slot: **"Returning in {s}"** in whole-second steps, then the static **"Returning"** at `s <= 0` while no pawn is bound. No animation, count-up, "searching" wording, pulse or colour change near zero.
- **Visibility.** Only in `InformantAwaiting`, on the neutral view, after the removal card has collapsed (or, for a killed informant, after Camera's fade completes). Highest priority in the Notice slot, where nothing else can use it in that state.
- **Data.** `s = ceil(T_due - ServerNowEst)`; live from the owner part, correct after a reclaim or rejoin. Never any other countdown (no round clock exists).

### E12 Interact ring

*Contextual · Z8 world layer · L0 · PAT-WLD-4 · HUD-R13, IIF UI 1 and F4*

- **Form.** A 48 px circle centred on the **target object's anchor** (a court fixture or a bush; never a person, never the screen centre, never the owner's own body). Track: 2 px Bone Dim; progress: 4 px stroke filling **linearly clockwise** from 12 o'clock; 2 px Obsidian keyline both sides.
- **Colour state.** Bone for the first `TendCueDelaySec` (1.0 s) after Begin at **every** court. Only after that delay, at an **own live site**, the stroke turns **Marigold and gains four short outward ticks (8 px) at the cardinal points**; every other court keeps the plain Bone ring. The ticks are the non-colour backup; the two states must differ by at least 3:1 luminance contrast (art-director to confirm, CR-HUD-8 item 11; Open Question HUD-UX-12).
- **Prediction.** Starts at the local press, re-based on `FHoldStatus`, clamps at 0.98, never shows completion before the server's `OnActionCompleted`; empties **in one step** on any cancel, rejection or release (a cancel caused by the owner's own removal is held to `OnRoleChangePresented`, then hidden).
- **Edge.** Clamped to the safe rectangle at constant size when its target leaves the view.
- **Reduce motion:** the sweep stays (real-time progress); a stepped "no sweep" option is a post-VS candidate (accessibility F12).

### E13 Context prompt

*Contextual · Z8 · L0 · PAT-WLD-3 · HUD-R13, IIF UI 3, CON UI-1*

- **Form.** A plate 32 high holding the glyph plate and the verb (Body 20 px): **"Tend"**, **"Slip in"**, **"Slip out"**, anchored 8 px above the object's anchor, constant pixel size. A hint, never a gate: the command is valid from the moment the object is in reach, whether or not the prompt has finished appearing.
- **Rules.** **Identical at every court fixture in reach** (own, rival, locked or the hidden final), because it is computed from public fixture geometry and `PresentedOwnState` only, never from team live-site data (OC-6). The verb "Slip in" or "Slip out" follows the owner's own `IsConcealed`. Hidden while the owner is running or traversing, never shown to an informant, hidden at once at the owner's own fall start (ruling D1).
- **While a hold is active** the prompt collapses and the ring (E12) takes its place; it returns when the hold ends. *(UX decision of this spec, not in the GDD: two overlays at one anchor would crowd a 48 px target; it depends only on the owner's own action and is leak-neutral. Logged.)*
- **Off-screen:** hidden (a prompt is for an object in reach and view; only the ring edge-clamps).
- **Entrance:** 400 ms ease-out with 8 px stepped settle; 600 ms out.

### E14 Strike hint

*Contextual · Z5 Notice slot · L2 · PAT-TRN-3 · HUD-R8, IIF UI 4*

- **Form.** A plate 40 high, centred in the Notice slot: **"[glyph] Strike"** (Body 20 px, Bone). **No reticle, outline, highlight, lock-on, colour or sound differs by target class**; the screen centre stays empty.
- **Predicate** (level-triggered, re-evaluated every frame): the camera aim ray hits a **rendered** non-ally pawn in reach, and the owner's `PresentedOwnState` is `Active`. It reads no team, controller, kind, concealment or `PlayerState` field. Hidden while the owner's action slot is `Striking` (1.75 s lockout), while any card is up, from the owner's own fall start, and for a spot, a bush, a marked ally and any concealed pawn (no actor exists).
- **Priority.** Lowest in the Notice slot; it shows whenever the predicate holds and the slot is free, including the frame a higher-priority notice leaves.
- **Fallback** if playtests show aiming fails: a static 4 px Bone stepped square at the centre that never changes state (art bible 7.1 amendment; IIF OQ9). Not built in MVP. Its visible share of Active time is a playtest metric (HUD OQ12; Open Question HUD-UX-12b).

### E15 Rejection plate and E16 Informant plate

*Contextual · E15 beside the brace row, E16 Z4 Result slot · L1 / L2 · PAT-TRN-2 · HUD-R13, IIF UI 5 and 6*

| Plate | Text | Position | In / hold / out | Rate limit | Notes |
|---|---|---|---|---|---|
| E15 | Not now, Busy, Too far | `(418, 966)` at 100 % (Z1 right edge + 10 px at every scale), beside the brace row, height 40, fit-to-text (Body 20 px) | 400 ms / 1.5 s / 600 ms (eased) | once per `RejectPlateCooldownSec` per code | a `Cooldown` rejection shows **no** plate (the Cooling numeral says it); the ring empties on a rejection; replies to a pawn struck in the same drain are presented no earlier than its contact time |
| E16 | Informants can only observe | Result slot | 400 ms / 2 s / 600 ms | once per `InformantPlateCooldownSec` | caught locally; a press before the removal card has collapsed shows nothing (dropped, not deferred) |

A quiet, level, owner-only 2D tone accompanies E15 (IIF UI 5); E16 has no sound (Open Question HUD-UX-6).

### E17 Third-person hint

*Contextual (once per profile) · Z5 · L2 · PAT-TRN-4 · Camera UI Requirements*

One plate (Body 20 px) **"Third person shows only what your character can see."**, 4 s hold, on the first **live** third-person frame (never during the briefing, never triggered by masking). The once-per-profile flag is consumed only when the hint is actually shown; if the Notice slot is busy it is retried on the next third-person entry. The line also lives in the controls help and a loading tip (accessibility C-03).

### E18 End banner, called-off line and lobby line

*Contextual · Z6 · L3 · PAT-CRD-2 · HUD-R14, F10*

- **Banner stack** (720 wide, top `y = 340`): header bar 720 x 80 with **"Victory"** (Bone bar, Obsidian text) or **"Defeat"** (Cochineal Deep bar, Bone text), Display 48 px; reason plate 720 x 40 with exactly one approved phrase (Body 20 px); score plate 720 x 36 **"Your house {a} · Rival house {b}"** (Body 20 px, a team total, art bible 7.1); then E21.
- **Reason phrases** (Round's approved set, verbatim): "completed the final objective" or "every rival eliminated for the round" (also for a forfeit, which is never named). The word on the bar, not its colour, carries the outcome. Capitalisation and the defeated viewer's reading of the reason are open (Open Question HUD-UX-5).
- **Show time** per F10 (never before the owner's own plate or fall; at most `HudBannerMaxDelaySec` of extras). On entry the persistent L1 clusters fade over 600 ms, and every ping ring hides. No duration, no per-participant list, no opponent colours.
- **Called off** and **lobby line:** one plate 560 x 44 centred at `(960, 540)`: "The round was called off" (clears every transient element at once), "The host will begin the next round" (non-host, in `Lobby`; no "waiting for players" text and no count beyond the lobby's `n / capacity`).
- **Not built here:** the post-round reveal (Vertical Slice); the banner yields to it at hand-off.

### E19 Latency icon

*Contextual · Z3 · L1 · PAT-PLT-6 · HUD-R13, IIF UI 7*

- **Form (closes interaction-patterns P10).** A **three-step stair glyph** (ascending stepped bars, the art-bible stair profile) in Bone on a 40 x 40 Obsidian plate at `(1772, 66)`, **top step drawn hollow** (outline only) to mean "weak". Shape, not colour, carries the meaning; no number, ping value or text is shown (nothing about the connection of anyone else, Session rule 24).
- **Visibility.** Owner-only; shown when own RTT (smoothed lag of the owner's net connection) exceeds `HudLatencyWarnRttSec` (0.20 s); hidden only below 0.8 x that value and after at least 2 s shown; never on the listen-server host.
- **Motion.** Non-urgent entrance 400 ms, exit 600 ms; static while shown.

### E20 Status overlays

*Contextual · Z9 · L4 (display only) · PAT-CRD-3 · Session UI Requirements, ADR-0021 L4 input rule*

- **Form.** One opaque plate (Primary 22 px, Bone on Obsidian), top `y = 236`, centred, up to 96 high, one message at a time, text from the string table: "Connecting...", "Loading the city...", "Connection lost. Reconnecting...", "The session has ended."; the rejection sentences per reason code are shown by the menu flow (`main-menu.md`), not here. A spinner, if used, has a static equivalent under Reduce motion.
- **Input.** **Display only.** The Cancel and OK buttons are a Session-owned modal pushed onto the menu root at ZOrder 100 (ADR-0021 L4 split, resolves P7); this layer never takes input.
- **Orthogonal** to every viewer state; sits over any of them. Messages persist until resolved (no timed auto-dismiss of actionable text).

### E21 Round summary

*Contextual · Z6, under the banner · L3 · extends PAT-CRD-2 · HUD-R14, HUD-E21*

One plate 720 x 64 directly under the score plate, entering with the banner at the F10 show time: two Minimum 18 px Bone lines, **own house first**: "Your house — steps {s} of 3 · eliminated {e} · wrong targets {w}" and "Rival house — steps {s} of 3 · eliminated {e} · wrong targets {w}". Plain text, no icons, no colour coding of the numbers, no duration, no per-participant list. Numbers are integers from `FRoundResult.TeamSummary` and the HUD never derives them locally. Stays until the HUD leaves `Ended`; never shown in `CalledOff`.

### E22 Team ping rings

*Contextual · Z8 world layer, beneath the markers · L0 · extends PAT-WLD-5 · PNG-R7, F3, VA-PNG-1 to 5*

- **Ground ring.** Open ring with a 60 degree gap on the ground at the ping point, world radius at least 4 m, **screen minimum 32 px**, Marigold, mandatory Obsidian keyline (3 px, 4 px beyond 20 m), **no centre mark**, static after placement (no pulse, bob, spin or scale pop). The gap is fixed in world space (north) so it never points at anything. Age is shown by **stepped stroke weight** 6, 4 and 3 px at thirds of `PingLifetimeSec` (8 s), x UI scale, **never by opacity**.
- **Edge ring.** When the centre is off-screen or behind the camera: the same broken ring at 32 px clamped to the safe rectangle (the Teammate Markers F3 clamp), static, unrotated, never an arrow, **no label**, drawn beneath edge markers and not de-overlapped (arrival order, newest on top).
- **Sender label.** The sender's display name below the ring: Minimum 18 px Bone on an Obsidian plate with a 2 px Turquoise stroke (ally chrome, art bible 4.6, follows the local ally accent; conflict with VA-HUD-1 in Open Question HUD-UX-3); ellipsised at the plate width; never larger than the ring; own ping shows the own name; unresolved reads "Ally".
- **Timing.** Appears 300 ms stepped when the owner-side echo arrives (no local prediction); a new ping from the same sender replaces its ring at once (no cross-fade); exit 600 ms ease (stepped under Reduce screen effects). One quiet 2D UI tick on arrival, on the UI bus. Hidden with L0 while the view is `Neutral`, hidden at `Ended`, never replayed after a rebuild.
- **Not drawn:** a "Ping" key prompt (no on-screen prompt in MVP; discoverability is the controls reference, PNG OQ3), a feed line per ping (PNG-R8), a refusal cue (a refused press does nothing).

### (17) Teammate marker and (17b) ally name label

*Must Show while the view is `Live` · Z8 · L0 · PAT-WLD-1 and PAT-WLD-2 · TM-R1 to R14, F1 to F8*

- **Glyph** (Teammate Markers owns content; this spec owns the layer): downward pentagon with a centred two-step top notch, 28 x 32 px default (floors 24 x 28, absolute), Turquoise `#0FC2B8` (or the local accent) with a 3 px Obsidian keyline, constant pixel size, always on top with no depth test, no distance, occlusion or opacity encoding.
- **Fill = the marked player's state, independent of the viewer:** solid = Active; two-band stepped hatch across the lower half = concealed; 3 px outline-only with keyline inside and outside = informant. Fill changes are **instant swaps** on the frame of the change.
- **Edge variant.** Off-screen or behind the camera: the same pentagon with a stepped base, clamped to the safe rectangle, **not rotated and not an arrow**, spread along the edge (F4; cuttable per TM OQ12).
- **Appear and disappear** 300 ms ease on entry, removal and the view gate only; a retiring marker holds, then exits so that it is half faded as the blow lands (TM-R6). No sound, bob, pulse or idle motion.
- **Label (17b).** An Obsidian plate, Primary 22 px Semibold Bone, on **one** marker at a time by focus (nearest the centre within `TeammateLabelFocusSize` 64 px), on the screen-inward side of the marker, clamped inside the safe rectangle; "{Name} · reconnecting" while the ally's `bReconnecting` is true (the marker itself never changes). Entrance as a non-urgent plate (400 ms, 8 px settle), exit 600 ms. The `ShowTeamNames` hold is Vertical Slice, not built.
- **Views and states.** Drawn only while `GetViewState() = Live`, so an informant viewer keeps every ally's marker (including a hatched one), the dying-body phase keeps them, and the neutral view hides them; they go out with Camera's black (the fade layer is above L0). Markers are never focusable or clickable.

**Component inventory (for the UMG implementation).** One widget per layer (table above); inside L1: `W_BraceCluster`, `W_TallyPlate`, `W_ObjectivesCluster` (own bar, rival bar, stage card), `W_TeamFeed` (3 pre-created line plates), `W_InformantFrame`, `W_LatencyIcon`; inside L2: `W_ResultSlot`, `W_NoticeSlot`; inside L3: `W_RemovalCard`, `W_EndBanner` (with `W_RoundSummary`), `W_CenterLine`; inside L4: `W_StatusPlate`; inside L0: one Slate leaf with `FWorldAnchorViewModel`. **No per-figure widget class exists** other than the leaf's marker brush (AC-HUD-11; art bible 7.7 test 3).

---

## HUD States by Gameplay Context

The guide asks for exploration, combat, dialogue or cutscene, and paused. This game has no health, no combat loop, no dialogue and no cutscene, and **the world never pauses**, so the four contexts map as follows. The HUD GDD's viewer-state table (`Lobby`, `Briefing`, `ActiveAlive`, `Eliminating`, `InformantAwaiting`, `InformantBodied`, `Ended`, `CalledOff`) is the authoritative state machine; this section adds what each state looks like.

| Guide context | This game | HUD states |
|---|---|---|
| Exploration | Reading the crowd, walking, tending an objective, hiding in a bush: the baseline of an Active player | `ActiveAlive` (owner sub-states below) |
| Combat | The encounter: a strike, a brace, a reversal, a removal. There is no "combat HUD": the encounter adds only transient plates in the two slots and the owner's own sub-state changes, **never a density change** | `ActiveAlive` sub-states `Striking`, `Bracing`; `Eliminating` |
| Dialogue or cutscene | None exists (no authored plot, no spoken dialogue). The nearest analogues are the moments the player has no pawn or no control: briefing, the neutral view while awaiting a body, the takeover fade-in | `Briefing`, `InformantAwaiting` |
| Paused | **There is no pause.** The in-session menu is an overlay on a live world (`pause-menu.md`); the HUD keeps updating beneath it. A Session status overlay can sit over any state | overlay "Menu open"; overlay "Status" |

### Element visibility by viewer state

`●` shown whenever the state applies; `○` shown only when its own trigger holds (Contextual); `–` collapsed (its slot stays empty); `f` frozen record; `x` fades out over 600 ms when the banner enters.

| Element | Lobby | Briefing | ActiveAlive | Eliminating | InformantAwaiting | InformantBodied | Ended | CalledOff |
|---|---|---|---|---|---|---|---|---|
| E01 Brace indicator | – | – | ● | ● until the card, then – | – | – | – | – |
| E02 Tally | – | – | ● | ● | ● f | ● f | x | – |
| E03 to E05 Objectives cluster | – | – | ● | ● | ● | ● | ● then x | – |
| E06 Team feed | – | – | ○ | ○ | ○ | ○ | x | – |
| E07 Result plate | – | – | ○ | ○ | – | – | ○ (own pending plate) | – |
| E08 Defence plate | – | – | ○ | – | – | – | ○ (own pending) | – |
| E09 Informant brackets | – | – | – | appear at the card | ● | ● | x | – |
| E10 Removal card | – | – | – | ○ once | – | – | – | – |
| E11 Returning countdown | – | – | – | – | ○ | – | – | – |
| E12 Interact ring | – | – | ○ | ○ until the fall start | – | – | – | – |
| E13 Context prompt | – | – | ○ | ○ until the fall start | – | – | – | – |
| E14 Strike hint | – | – | ○ | ○ until the fall start | – | – | – | – |
| E15 Rejection plate | – | – | ○ | ○ | – | – | – | – |
| E16 Informant plate | – | – | – | – | ○ | ○ | – | – |
| E17 Third-person hint | – | – | ○ | – | – | ○ | – | – |
| E18 Banner, lobby line, called-off line | lobby line (non-host) | – | – | – | – | – | banner | called-off line |
| E19 Latency icon | – | – | ○ | ○ | ○ | ○ | – | – |
| E20 Status overlay | ○ | ○ | ○ | ○ | ○ | ○ | ○ | ○ |
| E21 Round summary | – | – | – | – | – | – | ● | – |
| E22 Ping rings | – | – | ○ | ○ | – (L0 collapsed) | ○ | hidden on entry | – |
| (17) Markers and labels | – | – | ● | ● | – (Neutral) | ● | ● while the view is Live | – |

`Briefing` shows nothing from the HUD: the briefing and loading screen belong to Round and Session. In `Lobby` the HUD adds only the lobby line for a non-host; the host panel is Round's.

### Owner sub-states inside ActiveAlive (what changes, and what must not)

| Sub-state | Trigger | What changes on the HUD | What must not change |
|---|---|---|---|
| Idle, reading | Normal play | Nothing | — |
| In reach of an object | Court fixture or bush in reach, not running or traversing | E13 prompt appears (identical at every fixture) | Nothing else; no team-dependent difference |
| Tending | Interact held | E12 ring fills from the press (predicted); E13 collapses; from `TendCueDelaySec` the ring turns Marigold with ticks only at an own live site | The prompt never reveals a live site |
| In a bush | Own `IsConcealed` | E13 verb reads "Slip out" | No HUD icon, meter or "hidden" text (the grade is Camera's) |
| Aiming at a non-ally | Aim ray hits a rendered non-ally pawn in reach | E14 appears, identically for an NPC, enemy or informant | Nothing differs by kind; never for a concealed pawn (no actor) |
| Striking | Own Attack accepted | E14 hides for the 1.75 s lockout; at the owner's contact frame E07 enters and the tally steps (if wrong) | Nothing before the contact frame; the brace sweep and any ring keep running |
| Bracing | Prepare pressed | E01 Pending, then Protected on server confirmation, then End and Cooling | No change when a strike is aimed at the owner |
| Being struck | An enemy strike on the owner | **Nothing** before the owner's contact frame (cooldown sweep and any ring keep running); at the fall start the hint, prompt and ring hide, the card follows 0.8 s later; a braced owner sees E08 and the brace End at contact | No warning, sound or layout change (ASN-R6, AC-HUD-05) |
| First third-person frame | First live 3P use per profile | E17 in the Notice slot for 4 s | Never triggered by masking |
| Slow connection | Own RTT above `HudLatencyWarnRttSec` | E19 appears (hysteresis) | Nothing about any other player |

### State and variant table (guide form)

| State / Variant | Trigger | What changes |
|---|---|---|
| Default | Active, tally 0, brace Ready | `L` outline squares, "Wrong targets: 0", Ready ring, objectives cluster populated, feed empty |
| Low-resource analogue: threshold | `n >= L - 1` | Static upward triangle added to the tally, one threshold tick; nothing pulses, recolours or escalates beyond it |
| Cooling | After a brace ends | Numeral and linear sweep; Ready after the numeral holds at "0:01" |
| Empty / no-data | Preparing, or a rival count of 0 | Objectives cluster empty (not shown) in `Preparing`; rival bar shows 0 steps; feed collapsed; E22 none |
| 1v0 development round | Objectives forces the rival signal off | Rival steps stay 0 throughout; nothing else changes |
| Reconnect or rebuild | `RoundEpoch` change, travel, rejoin inside the grace | Every transient element discarded; persistent elements rebuilt from the carriers with **no animation and no replayed cue** (HUD-R17); no card, no feed line, no ping replay |
| Rejoin after the grace | Returning as an informant | `InformantAwaiting` or `InformantBodied` with brackets present at once and **no removal card** (HUD OQ6) |
| Listen-server host | Host plays | Same view-model as a client (host parity); the latency icon never shows |
| Round ends while the owner's own removal is pending | `Phase = Ended` before the card | No card; banner at the own fall start (F10) |

### Transition choreography

| Transition | Sequence (all timings from the GDD) | What slams in, fades or steps |
|---|---|---|
| Round start (`Active`) | Neutral briefing gives way to the world; Camera fades in | Persistent clusters are present from the first `Active` frame (the new `RoundEpoch` takes the rebuild path, HUD-R17: no animation, no cue); team views populate at `Active` entry (OC-17); no "round start" banner |
| Own strike resolves | Contact frame, then the plate | Plate: four-step urgent entrance; one square fills over 300 ms if wrong |
| Own removal | Fall start; hint, prompt and ring hide at once; +0.8 s card; world black from `DeathFallSec`; card holds 4.0 s | Result-slot plate steps out over 250 ms as the card enters (600 ms); brace hides and brackets appear at the card entrance; card fades out over 800 ms |
| Informant takeover | Camera cuts to the new body with the informant grade already on, then fades in over 0.5 s | Countdown plate leaves with the cut; markers return with the Live view; brackets unchanged |
| Round end | Own final fill if the owner's team won; own plate readable at least 1.0 s; banner | L1 clusters fade over 600 ms as the banner enters; rings hide; markers freeze in their last fill and remain while the view is Live |
| Menu opens | Menu key | Menu overlay at ZOrder 100; HUD unchanged beneath (no scrim, no dim) |

---

## Information Hierarchy

Priority tiers govern which element survives when two compete for a slot, when text scale or a localisation pushes a state over the Visual Budget, or when a story proposes an addition. **MUST KEEP** is never dropped or delayed beyond its GDD rule; **SHOULD KEEP** is dropped only after every CAN HIDE item; **CAN HIDE** goes first; **ALWAYS HIDE** is never shown in the stated condition. "What carries it if hidden" names the persistent or world carrier that makes dropping safe (the argument LIM-C2 depends on).

| Element | Tier | Reason | What carries the information if it is hidden |
|---|---|---|---|
| E02 Wrong-target tally | MUST KEEP | The only record of the owner's accumulated risk; the owner cannot reconstruct it | Nothing replaces it; the threshold tick (audio) twins the triangle |
| E07 / E08 Result and defence plates | MUST KEEP | The only confirmation of the verdict of the owner's own strike or brace (Pillar 3 payoff) | The tally (wrong target), the objectives and feed (indirect); the result cue (audio) |
| E10 Removal card | MUST KEEP | Tells the owner what they are now (the role, not a punishment) | Informant brackets, frozen tally, countdown |
| E18 Banner (and lobby, called-off lines) | MUST KEEP | The round's outcome; a shared moment | The summary E21, the match score line |
| E03 Stage card, E04 Own steps | MUST KEEP | The race objective and its wayfinding clue (no waypoint exists) | Nothing replaces the clue |
| E01 Brace indicator | MUST KEEP while Active | The defence's truthful state; without it the owner would press into a cooldown | The rejection reply `Cooldown` is shown only through this numeral |
| (17) Teammate markers | MUST KEEP while the view is Live | The only team-identification tool (the UI hero shape); never reduced by scale settings | Nothing; the world carries no hue-coded team information |
| E20 Status overlay | MUST KEEP | The only explanation of a connection problem | Nothing |
| E11 Returning countdown | MUST KEEP while awaiting a body | The informant's only information on the neutral view | Nothing (the black view shows only HUD plates) |
| E05 Rival steps | SHOULD KEEP | Race pressure without a reveal (Pillar 4) | The stage card and own steps carry the owner's own race |
| E06 Team feed | SHOULD KEEP | Plain ally facts (removal, reconnecting) | The ally's marker (leaves, reads outline) and the world |
| E09 Informant brackets | SHOULD KEEP | Shape twin of the informant state when Reduce screen effects softens the grade | The informant grade, frozen tally, card, countdown |
| E12 Interact ring | SHOULD KEEP | Progress of a 3 s hold, predicted | The world animation of the tend (visible to others by design) |
| E13 Context prompt | SHOULD KEEP | Affordance; a hint, never a gate | The command is valid in reach whether or not the prompt shows |
| E14 Strike hint | SHOULD KEEP | The one attack affordance; no reticle exists | The press still works; Help text; the fallback centre square |
| E15 Rejection plate | SHOULD KEEP | Why a press did nothing | A repeated press re-triggers it |
| E22 Ping rings and labels | SHOULD KEEP | Team place-mark channel; the label names the sender | External voice; the ring's own tick |
| E21 Round summary | CAN HIDE | Secondary to the banner; text only | The banner and score line |
| E16 Informant plate | CAN HIDE | Explains a refused press for an informant | Brackets and the Help text |
| E17 Third-person hint | CAN HIDE | One-time teaching line | The controls help and loading tip (C-03) |
| E19 Latency icon | CAN HIDE | Owner diagnostic; its loss costs no decision | None needed |
| (17b) Ally name label | CAN HIDE | Names are on demand by focus | The marker; external voice |
| Any element anchored to, sized by, coloured by or timed by a non-ally person | ALWAYS HIDE | Pillar 1; none exists | — |
| Elapsed Active time, any round clock, opponent counts, remaining-rival icons, the strike limit as a number | ALWAYS HIDE | Round R20, Roster R6, UI-NKP-1 | — |
| Result plates, brace indicator, prompts, ring and strike hint for an informant | ALWAYS HIDE | An informant has no strike, no brace and no interaction | Brackets, frozen tally |
| A stale transient (a plate after a card has taken the slot; a hint during `Striking`) | ALWAYS HIDE | Stale information misleads | The persistent record |

**The drop order for an over-budget state** (only reachable at text scale 125 % and above or with a long localisation, never at 100 % English) follows the table upward from the bottom: CAN HIDE first, then SHOULD KEEP, never MUST KEEP; the centre-empty rule and the safe rectangle are never relaxed.

---

## Visual Budget

### Budget lines

| Line | Limit | Designed value | Source |
|---|---|---|---|
| Persistent plate area, 100 % scales, 16:10 (narrowest supported aspect) | at most **8 %** of the screen (`HudPersistentAreaLimitPct`) | **7.12 %** Self-Active (worst state); 6.68 % informant | F1, art bible 7.1 |
| Centre box (20 % x 20 %) | no persistent element at any text scale | none | HUD-R5 |
| Simultaneous HUD groups | 7: bottom-left, top-centre, top-right, Result slot, Notice slot, rejection plate, world layer | 7 | accessibility C-07 |
| Plate-class elements visible at once in L1 and L2 (worst state, Self-Active with every transient) | 12: E01, E02, E03, E04, E05, E19, three feed lines, Result, Notice, rejection plate | 12 | this spec |
| World-layer glyphs at once | 16: four markers, one marker label, five ping rings with up to five labels, one interact ring or prompt | at most 16 | TM-R3, PNG-R6 |
| Text blocks on screen after a card entrance | exactly 1 in the central region after 0.25 s; at most Result + Notice in the lower column | per HUD-R9, HUD-R13 | HUD GDD |
| Transient overlay share | each at most 30 % of the screen, never full-bleed | removal card 4.9 %, banner stack 8.3 % | art bible 7.3 row 11 |
| Signal hues | three, each with one job; Turquoise only on ally chrome | 3 | art bible 4.6 |
| Frame cost | HUD at most 0.3 ms game thread and 0.2 ms render thread; with markers at most 0.5 ms; UI GPU at most 0.5 ms at 1080p, on the reference GPU | ADVISORY until profiled | AC-HUD-22, ADR-0021 |

### F1 re-run: footprints and worst state

**Convention.** A footprint is the **reserved rectangle of each plate or plate-less glyph** drawn in the state, at its reserved size; gaps between plates are not counted, and the stacked feed is counted as its 3-line box (420 x 100, gaps inside it included). This reads art bible 7.1 literally ("total persistent corner-**plate** area") and does not charge the budget for pixels the city is drawn through (for example the empty middle of the steps row). The strict alternative (one box per cluster) is recorded below as a sensitivity only; the plate convention is ratified (D16, HUD-UX-7 resolved). World-anchored L0 glyphs (markers, rings, the interact ring and prompt) and transient plates are outside F1 by definition.

| Footprint | w x h | Area (ref px²) | Self-Active | Informant (Awaiting or Bodied) |
|---|---|---|---|---|
| E02 tally plate | 244 x 80 | 19,520 | yes | yes (frozen) |
| E01 brace ring plate | 56 x 56 | 3,136 | yes | no (collapsed) |
| E01 brace label plate | 232 x 40 | 9,280 | yes | no (collapsed) |
| E04 own steps plate | 161 x 56 | 9,016 | yes | yes |
| E05 rival steps plate | 108 x 56 | 6,048 | yes | yes |
| E03 stage card | 460 x 92 | 42,320 | yes | yes |
| E06 feed (3 lines, reserved box) | 420 x 100 | 42,000 | yes | yes |
| E19 latency icon plate | 40 x 40 | 1,600 | yes (when shown) | yes (when shown) |
| E09 informant brackets, 4 x 32 x 32 | 4 x (32 x 32) | 4,096 | no | yes |
| **Total** | | | **132,920** | **124,600** |

Informant state is smaller because the brace row is collapsed and its slot is **not** reused (no reflow); the brackets and the latency icon are the footprints the HUD re-review asked for. The `Eliminating` state equals Self-Active (nothing is frozen until the card). `Ended` is smaller (clusters fade as the banner, a transient, enters).

**A_persist = Σ(w·h)·u² / (W·H)**, checked at 100 % UI and text scale:

| Viewport | Self-Active | Informant | Note |
|---|---|---|---|
| 1920 x 1200 (16:10) and 1280 x 800 | **7.12 %** | 6.68 % | the binding case; limit 8 % passes with **0.88 points (16,379 ref px²) of margin** |
| 1920 x 1080, 1280 x 720, 2560 x 1440, 3840 x 2160 (16:9) | 6.41 % | 6.01 % | scale-invariant |
| 3440 x 1440 (21:9), corner clusters clamped to the centred 16:9 region | 6.41 % within the region (4.77 % of the full screen) | 6.01 % (4.47 %) | clamp rule in Layout Zones |
| 5120 x 1440 (32:9) | 3.21 % of the full screen | 3.00 % | |
| 1440 x 1080 (4:3), 1350 x 1080 (5:4) | 8.55 %, 9.12 % | 8.01 %, 8.55 % | unsupported aspects; cap not enforced |

**Lint.** `HudPersistentAreaLimitPct >= A_persist(16:10, worst state)`: **8 >= 7.12, passes.** The HUD GDD's "7.6 %" for the reference layout is replaced by this value when the lint's data is regenerated from the footprint table above (`A_persist` must be computed by the layout resolver from the real widgets, AC-HUD-15, not from this table once the face is chosen).

**Sensitivity (what would break it).**

| Case | Self-Active at 16:10 | Verdict |
|---|---|---|
| One stage-card line wraps (+460 x 26) | 7.76 % | inside |
| Two lines wrap | 8.40 % | **breach**: localisation must shorten strings, not wrap them (string budgets, Layout Zones) |
| Four markers added to the sum, default size (4 x 28 x 32) | 7.31 % | inside |
| Four markers at the 150 % large setting (4 x 42 x 48) | 7.55 % | inside |
| Strict convention: one box per cluster (BL 300 x 144, TC 460 x 154, TR 420 x 148) | 9.44 % | **fails**: the plate convention was ratified by D16 (HUD-UX-7 resolved); this reading is rejected |
| Text scale 125 % or more | may exceed | accepted by art bible 7.4; the centre-empty rule and the safe rectangle still hold |

### Displacement rule (how the budget is defended)

Any proposed persistent element or footprint increase must state, in its story: (1) which line of this table it affects, (2) the new total at 16:10, and (3) what existing element it displaces or shrinks to stay at or below 8 %. The remaining room is 16,379 reference px²; it is not a reserve for features, and the creative-director owns any raise of `HudPersistentAreaLimitPct` (hard range 7 to 12, safe range 8 to 9). Items that would have to be dropped, in order, are the CAN HIDE and then SHOULD KEEP items of the Information Hierarchy; MUST KEEP items are never dropped.

---

## Feedback & Notification Systems

### Notification contract

**There are no toasts.** Every notification is bound to a named slot and has a trigger system, a position, a hold, a priority and a queue rule. Holds are multiplied by `HudReadHoldMult` (1.0 in MVP). "Dropped" means discarded, never delayed; "held" means presentation withheld until the owner's own contact frame, fall or removal (PAT-FND-5), after which it is shown, never discarded for lateness in live play.

| Notification | Trigger system | Position | In / hold / out | Max at once | Priority | Queue behaviour | Dismissible |
|---|---|---|---|---|---|---|---|
| Result plate (E07), defence plate (E08) | Assassination, Penalty | Result slot (Z4) | 250 ms stepped / 3.0 s / 600 ms | 1 | Result slot 2 | **Held** until the owner's contact frame (F2); never dropped for lateness; a next plate (at least 1.75 s later) preempts over 250 ms; duplicates and older sequences dropped | No |
| Removal card (E10) | Penalty, Assassination | Card band (Z6) | 600 ms / 4.0 s / 800 ms | 1 (once per removal) | Result slot 1 (preempts) | **Held** to the F4 card time; never queued; never replayed; suppressed for `InformantKilled` and when `Ended` arrives first | No |
| Informant plate (E16) | IIF | Result slot | 400 ms / 2.0 s / 600 ms | 1 | Result slot 3 | **Dropped** when the slot is occupied or the card has not collapsed; at most once per `InformantPlateCooldownSec` | No |
| Returning countdown (E11) | Penalty | Notice slot (Z5) | none (steps) / while awaiting / cut | 1 | Notice slot 1 | Cannot compete: only in `InformantAwaiting` | No |
| Third-person hint (E17) | Camera | Notice slot | 400 ms / 4.0 s / 600 ms | 1 | Notice slot 2 | **Dropped** if the slot is busy; the once-per-profile flag is consumed only when shown, so it retries next third-person entry | No |
| Strike hint (E14) | IIF | Notice slot | 400 ms / while the predicate holds / 600 ms | 1 | Notice slot 3 | **Level-triggered**: shown whenever the predicate holds and the slot is free, including the frame a higher notice leaves | No |
| Rejection plate (E15) | IIF | beside the brace row | 400 ms / 1.5 s / 600 ms | 1 | n/a (own position) | At most once per `RejectPlateCooldownSec` per code; a different code arriving while one shows **replaces** it with a stepped swap (UX decision of this spec); no plate for `Cooldown` | No |
| Team feed line (E06) | Roster, Teammate Markers | Top-right (Z3) | 400 ms / `HudFeedLineLifeSec` 6.0 s / 600 ms | 3 | n/a | **Queued** (the only queue): at most 4 pending; the oldest line is evicted only after `HudFeedMinDwellSec` 2.0 s; a retire alone creates no line | No |
| Stage-card turn-over, own step fill, rival step fill | Objectives | Z2 in place | 500 ms | 1 each | n/a | A rival rise of 2 in one update merges into **one** fill and one tick | No |
| Tally square fill | Penalty | Z1 in place | 300 ms stepped | 1 | n/a | Occurs with its plate, never before | No |
| Ping ring (E22) | Informant Pings | L0 ground and edge | 300 ms stepped / 8 s / 600 ms | 1 per sender, at most 5 | n/a | A new ping from the same sender **replaces** its ring at once; no queue; none replayed after a rebuild | No (expires) |
| End banner and summary (E18, E21) | Round | Banner stack (Z6) | per F10 | 1 | above everything in L3 | **Held** to the F10 show time (never before the owner's own plate or fall; extras capped at 1.6 s) | No |
| Called-off line, lobby line | Round | Z6 centre line | 400 ms / until the phase leaves / 600 ms | 1 | n/a | The called-off line clears every transient element at once | No |
| Latency icon (E19) | IIF (own RTT) | Z3 | 400 ms / while above threshold / 600 ms | 1 | n/a | Hysteresis: hidden only below 0.8 x threshold and after 2 s shown | No |
| Status overlay (E20) | Session | Status plate (Z9) | none / until resolved / none | 1 | above L3 | The latest state replaces the previous; actionable text never auto-dismisses | Via the menu-root modal |
| Marker enter and exit (17) | Teammate Markers | L0 | 300 ms ease | 4 | n/a | Retire hold, then exit as the blow lands (TM-R6) | No |

### Slot priority lists (restated for the implementer)

- **Result slot:** removal card (its own band, preempts) > result plate or defence plate > informant plate.
- **Notice slot:** returning countdown > third-person hint > strike hint.

### Queue rules

1. **Slots drop, they do not queue** (PAT-FND-4): a queued plate would arrive late and could correlate with a different encounter, and a backlog of stale text is worse than a missed repeat. Dropping is safe only because every dropped message is repeated by a persistent element (tally, steps, brackets, the cooldown numeral).
2. **The single exception is the feed's pending queue** (at most 4, the largest burst a five-player team can produce).
3. **Nothing is combat-aware.** The HUD does not hold, defer or batch notifications by "combat"; there is no combat state. The only deferral is the outcome-gating of PAT-FND-5, which is a leak rule, not a comfort rule.
4. **Nothing merges across time** except a rival rise of two steps in one update. Results are at least 1.75 s apart by the strike lockout.
5. **Rebuilds clear everything.** On a `RoundEpoch` change, travel or reconnect every pending or showing transient is discarded and nothing is replayed (HUD-R17); a result whose show time passed more than `HudReplayWindowSec` before the rebuild is not shown.
6. **No critical class exists.** The only element that bypasses slots is the Session status overlay, because it is about the owner's own connection.

### Cue events (owner-only or team-only, 2D on the UI bus, never spatialised, never on a pawn)

The HUD owns **when** a cue event fires; the audio-director owns the sound (mood: dry, short, then silence; no buzzer, fanfare or sting). Every cue has a text twin, so a player can mute the UI bus without losing information (accessibility A-04, C-05).

| Cue event | Fired at | Audience | Text twin |
|---|---|---|---|
| Result cue, one id per outcome (Target exiled, Wrong target, No contact, Reversed, Strike reversed) | the plate's show time (after contact, so legal); identical for `WrongTarget` whatever the victim kind | Self | the plate |
| Tally tick; threshold tick when `n >= L - 1` | with the plate; chosen by `n` alone | Self | squares, numeral, triangle |
| Removal cue | with the card entrance (the `OnRemovalCardEntered` event), never with the plate | Self | the card |
| Stage tone | an own counted completion | Team | stage card turn-over and own step fill |
| Rival tick | a rival-steps rise, once per update | Team | rival step fill |
| Brace protected and protection ended | on the state change as presented | Self | indicator state |
| Brace ready-again (soft) | when the numeral resolves to Ready | Self only; unobservable to anyone else | Ready ring |
| Rejection tone (quiet, level) | with E15 (IIF UI 5; conflict with HUD VA-HUD-6 in HUD-UX-6) | Self | the plate |
| Ping tick | when a ring appears (the sender hears it on the echo) | Team | the ring |
| **Being targeted** | **never** | n/a | none by design (ASN-R6) |

Gamepad haptic twins (accessibility A-07) are Vertical Slice, optional, and outcome-uniform: never before the owner's contact frame.

---

## Platform Adaptation

The HUD ships for **PC (Windows)**; keyboard and mouse is primary, gamepad partial, touch none. One layout serves every supported display; no element moves, shrinks to a different design or is dropped on any supported configuration.

| Platform or configuration | Safe margin | Resolution and aspect range | Input | HUD-specific notes |
|---|---|---|---|---|
| **PC, 1920 x 1080 reference** | 5 % (96 x 54) | 1280 x 720 minimum to 3840 x 2160 shipping range (F1 hard range to 7680 x 4320); 16:10 to 32:9 | mouse and keyboard; gamepad optional | All geometry in this spec. Persistent area 6.41 % at 16:9. |
| **PC, 16:10** (1920 x 1200, 1280 x 800) | 5 % | as above | as above | **Binding case for F1** (7.12 %). 1280 x 800 is also the handheld-PC resolution: see Open Question HUD-UX-10. |
| **PC, ultrawide** (3440 x 1440, 5120 x 1440) | 5 % | 21:9 and 32:9 | as above | Corner clusters clamped to a centred 16:9 region; brackets and L0 edge clamp use the full safe rectangle; Result and Notice slots stay centred. FOV is capped by Camera (108 degrees horizontal), not by the HUD. |
| **PC, 4:3 and 5:4** | 5 % | not supported | as above | Works; the area cap is not enforced (8.55 % and 9.12 %); no resolution is locked. |
| **Windowed and resizable** | recomputed per frame | any | as above | Layout, `u` and the L0 clamp re-evaluate from the viewport each frame; a resize or a UI-scale change relayouts **without replaying any animation or cue**. |
| **Gamepad** (partial) | 5 % | as above | pad; the HUD takes no input | Glyphs swap to the pad set on the first pad input (M-02). No on-screen element is focusable or clickable. The 10-foot reading distance is not a target (HUD OQ14). |
| **Console or TV** | would need 10 % | not a target | pad | Not in scope. If adopted: raise the text floors (the panel suggested 24 px), raise the safe margin, re-run F1 (HUD OQ14). |
| **Touch or mobile** | n/a | not a target | none | No touch target exists. |

**Scaling mechanism (ADR-0021 §6).** One custom DPI rule returns `viewport height / 1080` only; the client UI scale `s_ui` (75 to 150 %, fixed 100 % in MVP) is applied once by each layer root and by the L0 leaf; text sizes follow `max(floor_px · H/1080, base_px · u · s_text)` so the 18, 22 and 28 px floors ignore UI scale; `ApplicationScale` stays 1.0. At 720p `u = 0.667`, so the 18 px floor is 12 physical pixels; the supported minimum is a PC window, not a handheld.

**Bound-input glyphs by device.** Every prompt shows the **currently bound** key or button for the device the player last used and swaps at once on a device change; an unbound action shows "Unbound". Defaults (IIF R15, for reference only; the HUD reads bindings, it never hard-codes them):

| Label or prompt | Keyboard and mouse | Gamepad |
|---|---|---|
| Brace (E01 label) | Q | left trigger |
| Strike (E14) | left mouse button | right trigger |
| Tend (E13) | E | X |
| Slip in, Slip out (E13) | F | B |
| Ping (no on-screen prompt in MVP) | G | D-pad up |

Gamepad glyphs carry the button letter or shape, never colour alone.

**Layout test matrix** (AC-HUD-15, extended here): 1280 x 720, 1280 x 800, 1920 x 1080, 1920 x 1200, 2560 x 1440, 3440 x 1440 and 3840 x 2160; UI scale 75, 100 and 150 %; text scale 100, 125, 150 and 200 %. For each: no element rect crosses the safe rectangle (L4 plate and fade exempt), the centre box holds no persistent element, the Result and Notice slots do not overlap, the rejection plate does not overlap the Notice slot, and `A_persist` at 100 % and 16:10 is at most `HudPersistentAreaLimitPct`.

---

## Accessibility

**Committed tier: Standard, with named elevations from Comprehensive** (`design/accessibility-requirements.md`, Committed 2026-10-25; Standard is a **Release** commitment). Row ids below (V-, M-, C-, A-, LIM-, T-, F-) are that document's. **Delivery phasing:** Settings & Accessibility (system 24) is a Vertical Slice system and is not designed yet. The MVP HUD ships fixed defaults (UI scale 100 %, text scale 100 %, Reduce motion off, Reduce screen effects off, high contrast off, read-hold multiplier 1.0, accent Turquoise, marker scale 100 %) **but already honours every hook**, so the Vertical Slice only exposes them (HUD-R16). Every setting is client-local, is never replicated, and changes only size, local colour, timing of presentation or effects, never the set of facts shown (IVP R15; law L6; test T-10).

### Requirements by tier (what the HUD does for each)

| Tier | Row | HUD requirement | MVP behaviour | Control arrives | Verified by |
|---|---|---|---|---|---|
| Basic | V-03, V-04 | Text floors 18 / 20 / 22 / 28 px; contrast 4.5:1 below 28 px, 3:1 for larger text and graphics, measured on the plate over the brightest ground | Fixed in the style resolver; all plates opaque | n/a | AC-HUD-15, T-03 |
| Basic | V-08 | No meaning by colour alone | Every row of the audit below has a shape or text backup | n/a | T-01, T-02 |
| Basic | V-13 | No flashing; pulsing at most 0.5 Hz and never saturated red; removal sequence budgeted as one sequence | No element pulses or flashes; removal sequence per VA-HUD-4 | first-launch notice is unowned (F8) | AC-HUD-18, T-07 |
| Basic | A-04 | Independent volume buses; owner-only cues on the UI bus | Cues on the UI bus, each text-twinned | VS | T-09 |
| Standard | V-09 | UI scale 75 to 150 %, text scale 100 / 125 / 150 / 200 %, separate HUD and menu scale | Hooks honoured at 100 % | VS | AC-HUD-15 |
| Standard | V-06, V-07 | Colour-vision presets and ally accent (HUD signal colours only); no world post-process filter | Turquoise default; shapes and keylines never change | VS | T-02, T-13 |
| Standard | C-04 | Objective text zero presses away | Stage card, own and rival steps always on screen while Active | n/a | AC-HUD-23 |
| Standard | C-06 | Reading time for auto-dismissing plates | **Documented deviation**, see below | `HudReadHoldMult` to 2.0 (VS) | AC-HUD-09 |
| Standard | M-02, M-11 | Device switching; menus pointer-free | Glyphs follow the active device; the HUD itself takes no input | menus: VS | T-04, T-11 |
| Standard | C-10 | Plain, consistent language | Every string in `ST_Hud`, language-linted | n/a | AC-HUD-12 |
| Elevation | V-15 | **Reduce motion** | Mapping table below | VS | T-06 |
| Elevation | V-11 | **High contrast mode** | Defined below (closes F4) | VS | T-03, T-06 |
| Elevation | V-10 | **Marker scale** 100 or 150 %, never reduced by UI or text scale | Honoured at 100 % | VS | AC-TMK-17 |
| Elevation | V-14 | **Reduce screen effects** | The HUD has no screen effects; the setting scales Camera's grades and steps the ping exit | VS | T-06 |
| Elevation | V-22, A-02 | Captions for owner-only gameplay cues: the cue's own plate is its caption | Every cue in Feedback has a text twin | n/a | T-09 |
| Elevation | A-03, A-07 | Mono audio; gamepad haptic twins for owner-only outcome cues | Not HUD; haptics never before contact | VS or later | n/a |
| Out of scope | M-10, LIM-M4 | HUD element repositioning | Not offered: the HUD is a Pillar 1 layout contract (one layout for every player) | n/a | n/a |
| Out of scope | A-05, LIM-A1 | Any directional audio or off-screen indicator | **Prohibited**: it would identify (IVP R15) | n/a | n/a |

### Colour-as-only-indicator audit (HUD elements)

All entries are **Specified**; each becomes **Verified** only after the greyscale and protan, deutan and tritan simulations (T-01, T-02) pass on captured screens.

| Element | Colour signal | Non-colour backup | Status |
|---|---|---|---|
| Teammate marker | Turquoise | down-pentagon with stepped notch; 3 px keyline; label by focus; accent presets | Specified |
| Marker states | solid, hatch, outline | fill pattern; opacity encodes nothing; must separate in greyscale at 24 px | Specified |
| Tally squares and triangle | Cochineal fill, Bone outline | filled versus outlined squares; numeral; upward triangle from `n >= L - 1`; tally and threshold ticks | Specified |
| Result plate | Cochineal triangle versus Bone stamp versus none | four texts; stamp, triangle or empty glyph slot; distinct cue id per outcome | Specified |
| Removal card | Cochineal Deep bar | title text "Informant", cause subtitle, upward triangle, the second line | Specified |
| End banner | Bone bar versus Cochineal Deep bar | the words "Victory" and "Defeat" plus the reason line | Specified |
| Interact ring | Bone versus Marigold | four cardinal ticks on the live state; at least 3:1 luminance contrast between the states | Specified (contrast to be measured, HUD-UX-12) |
| Objective steps | Marigold fill | discrete steps with outlines; step count; labels "Your house" and "Rival house" | Specified |
| Ping ring | Marigold | open ring with a 60 degree gap; stepped stroke weight; 32 px minimum; edge variant; tick twin | Specified |
| Brace indicator | none (Bone, Bone Dim) | outline versus inner square versus sweep; numeral; persistent "Brace" label | Specified (no colour dependence) |
| Informant brackets | Bone | four corner brackets; frozen tally; plate on a refused press | Specified |
| Team feed line | Cochineal triangle on removal lines | text; reconnecting and back lines carry no colour | Specified |
| Latency icon | none (Bone) | three-step stair with a hollow top step (**closes P10**; art-director to approve, HUD-UX-11) | Specified |
| Returning countdown | none | text; never flashes, no colour near zero | Specified |

### Text scaling narrative (10.2)

At **text scale 200 %** text doubles, plates grow with it and never truncate; geometry (rings, squares, the marker, the 2 px stroke) keeps its 100 % size; names ellipsise. The tally, brace label, stage card and feed reflow **up and outward**; the stage card is capped at 40 % of the safe width from 150 % and wraps (title one line, card line and clue two lines each within the string budgets); the feed takes the width to the right of the card and wraps each entry to at most two lines; the Result plate grows to 450 x 73. Paper-derived worst case: the top-centre cluster ends at y = 418, clearing the centre box (top 432) by 14 px, and no cluster overlaps another (table in Layout Zones). The 8 % area cap may be exceeded from 125 % (art bible 7.4); **the centre-empty rule and the safe rectangle never are**. Widths are not measured until the face is chosen (HUD-UX-8); the layout resolver is authoritative.

### Reduce motion mapping (10.3)

| Motion | Severity | Under Reduce motion |
|---|---|---|
| Non-urgent plate entrance (400 ms ease, 8 px settle) and exit (600 ms) | mild | instant, no settle |
| Result and defence plate stepped entrance (250 ms) and exit | mild | instant; **hold unchanged** (3.0 s) |
| Removal card enter (600 ms) and collapse (800 ms) | moderate | instant; **holds and the F4 beat unchanged** (the hold-back is a timing, not a motion) |
| Tally square fill (300 ms stepped) | mild | instant |
| Step fill (500 ms), stage-card turn-over | mild | instant swap; the F10 banner extras keep their timing |
| Brace End exit (250 ms) | mild | instant |
| Brace Protected segments | state, not motion | unchanged (they track `DefenceDurationSec`) |
| Cooldown sweep | real-time progress | **remains**, advancing in 1 s steps (F5) |
| Interact ring sweep | real-time progress | **remains** (a stepped "no sweep" option is a post-VS candidate, F12) |
| Marker enter and exit (300 ms) | mild | 0 ms; the retire hold stays |
| Ping ring enter (300 ms) and exit (600 ms) | mild | instant; with Reduce screen effects the exit is stepped, not faded |
| Countdown | none | none |
| Banner and summary entrance | mild | instant |
| Camera fades (neutral, cut, elimination) | Camera-owned | stay fades (a cut to black would add a sudden luminance change); Reduce camera motion does not remove them |

### High contrast mode (confirms accessibility F4; V-11)

When on: **Bone Dim text and graphics are promoted to Bone** (plates are already opaque); every **stroke and keyline grows one step inward** (plate stroke 2 to 3 px; keylines on coloured glyphs 2 to 3 px; the marker, whose keyline is already 3 px, is unchanged; ring track and empty-square outlines 2 to 3 px; informant brackets 2 to 3 px; empty-step outlines 2 to 3 px). Growth is **inward, so no footprint or F1 value changes**. Pentagon, notch, hatch, outline, ticks and all text are unchanged, and **no fact is added or removed** (T-10). OS high-contrast detection is **NOT SOURCEABLE** for UE 5.8 in `docs/engine-reference/**` (accessibility, Platform table), so the mode is manual.

### Reading time: the documented 5 s deviation (answers accessibility F2 and interaction-patterns P8)

The Standard tier asks that no actionable notification auto-dismiss in under 5 s. The result plate holds 3.0 s, the rejection plate 1.5 s, the informant plate 2.0 s, the third-person hint 4.0 s, the removal card 4.0 s. **Rationale:** a longer hold would cover the next strike hint (the hint returns at 1.75 s, the plate lasts to 3.85 s in a separate slot), and would delay the end banner for the attacker (F10, capped at 1.6 s). **Mitigations:** every verdict is also recorded by a persistent element (tally, objectives cluster, brackets, the cooling numeral), so a missed plate loses no information; `HudReadHoldMult` (1.0 to 2.0, tied to text scale in the Vertical Slice) gives 6 s on the result plate; a rejection plate re-triggers on the next press; the third-person line lives permanently in the controls help (C-03). Recorded as LIM-C2.

### Cognitive load (C-07)

The player tracks at most **seven concurrent groups** on the HUD (bottom-left, top-centre, top-right, Result slot, Notice slot, rejection plate, world layer) against a review trigger of four; it stays within the trigger only by grouping (three corner clusters, two shared slots, one rejection plate, one world layer), and the HUD is silent until the owner acts or a team event occurs. This is recorded as a review item for `/ux-review`, not a defect (accessibility C-07). No navigation aid exists by design (C-08, LIM-C3); the clue on the stage card is the only wayfinding text.

### Screen readers and non-text indicators

Screen-reader support for menus, OS high-contrast detection, a mono downmix, the haptics API and adaptive-controller behaviour are **NOT SOURCEABLE** in `docs/engine-reference/unreal/**` (accessibility F16); no HUD requirement depends on them, and **T-12 stays NOT ASSESSED**. The in-world HUD is not screen-readable by design (LIM-V1: reading a figure's gait aloud would identify tells). For future support, every non-text indicator (marker fill, ring state, triangle, brackets, latency icon) carries a **text twin in the view-model dump** (its element id and state name), which is also what the leak suite compares; no extra on-screen element is needed.

### Input accessibility

The HUD has **no focus order, no pointer behaviour and no input of its own** (every layer below ZOrder 100 is `HitTestInvisible`), so keyboard-only, gamepad-only and pointer-free play are unaffected by it. Hold-to-toggle for Run, Tend and Brace (M-04, VS) changes nothing the HUD shows. Remapping (M-01, VS) is reflected immediately by the glyph plates. The strike hint is the only attack affordance; it follows rebinding, and no aim assist exists or may be added (M-07, LIM-M3).

---

## Tuning Knobs

Three kinds of adjustable value exist and they never mix: **player settings** (client-local, never replicated, never tuning keys, TDC CR-14), **designer keys** (data file, one value for every client; the HUD GDD's Tuning Knobs table owns them) and **spec constants** (owned by this document; a change is a spec change plus an F1 re-run).

### Player settings (client-local; MVP ships the defaults, Settings & Accessibility exposes them in the Vertical Slice)

| Setting | Default | Range | What it changes | What it must not change |
|---|---|---|---|---|
| UI scale | 100 % | 75 to 150 % | plate and non-text geometry; applied once by each layer root | text floors, the 2 px stroke, the 24 x 28 marker and the ring minima never go below their 100 % values |
| Text scale | 100 % | 100, 125, 150, 200 % | text size; plates grow, never truncate | the set of facts shown |
| Reduce motion | off | on or off | instant entrances and exits, no settle, 1 s cooldown steps (mapping in Accessibility) | holds, the F2 and F4 hold-backs, real-time sweeps |
| Reduce screen effects | off | on or off | Camera's grades (snap eased, saturation drop capped at 30 %, vignettes off); stepped ping exit | any fact |
| Reduce camera motion | off | on or off | Camera options (CS-R13); an MVP options-panel entry owned by Camera, listed here for completeness | permissions or anything sent |
| High contrast | off | on or off | Bone Dim to Bone; strokes and keylines one step thicker, inward | footprints, facts |
| Read-hold multiplier (`HudReadHoldMult`) | 1.0 | 1.0 to 2.0 | multiplies every read hold (plates, card hold, feed life and dwell); tied to text scale in the VS | the F2 and F4 hold-back caps (those are timings of the contact frame, not read holds) |
| Ally accent | Turquoise | Turquoise, warm white, magenta, ultramarine (each to pass protan, deutan and tritan simulation first) | local fill colour of the marker and the ping label stroke | shape, keyline, notch, the allies-only rule |
| Colour-vision preset | none | Protan, Deutan, Tritan | selects an accent and, where simulation shows a collision, alternates for the Cochineal and Marigold UI colours | no world filter exists |
| Marker scale | 100 % | 100 or 150 % | marker glyph size (never reduced by UI or text scale) | the hatch landing on whole pixels |
| Streamer alias mask | off | on or off | names in labels, feed lines and ping labels become aliases | art, shapes, markers (the identity layer only) |

**Deliberately not offered** (each would break Pillar 1 or the contrast contract): global HUD opacity (opacity is never a state encoding; plates are 100 % opaque), per-element show or hide, HUD repositioning (LIM-M4), a strike-hint off switch (deferred by the user until first-playtest data, HUD OQ12), any distance, occlusion or enemy-related option, a subtitle size (no spoken dialogue exists in MVP). The safe margin stays a designer key in MVP (Open Question HUD-UX-9).

### Designer keys this spec depends on (owned and ranged by the HUD GDD; not restated)

| Key (default) | What in this spec depends on it |
|---|---|
| `HudSafeMarginPct` (5) | safe rectangle, cluster anchors, L0 edge clamp, marker and ring clamp |
| `HudPersistentAreaLimitPct` (8) | the F1 lint: 8 >= 7.12 at 16:10 |
| `HudFeedLineCount` (3), `HudFeedLineLifeSec` (6.0), `HudFeedMinDwellSec` (2.0) | feed box 420 x 100 (each added line adds 420 x 35 = 14,700 reference px², about 0.79 points at 16:10) |
| `HudResultPlateInSec` (0.25), `HudResultPlateHoldSec` (3.0), `HudResultPlateOutSec` (0.6), `HudResultPlateMaxHoldBackSec` (1.25), `HudPreemptExitSec` (0.25) | E07, E08 timelines |
| `HudRemovalCardInSec` (0.6), `HudRemovalCardHoldSec` (4.0), `HudRemovalCardOutSec` (0.8), `HudRemovalCardDelaySec` (0.8), `HudOwnRemovalHoldCapSec` (1.25) | E10 timeline |
| `HudObjectiveStepSec` (0.5) | E03, E04, E05 fills and turn-over |
| `HudBannerMinPlateSec` (1.0), `HudBannerMaxDelaySec` (1.6) | E18 show time |
| `HudReplayWindowSec` (1.0) | rebuild behaviour of all transients |
| `HudReadHoldMult` (1.0) | every hold above |
| `HudLatencyWarnRttSec` (0.20) | E19 |
| `TeammateMarkerFadeSec`, `TeammateMarkerRetireHoldSec`, `TeammateLabelFocusSize`, `TeammateMarkerAnchorCm` (Teammate Markers) | (17), (17b) |
| `PingLifetimeSec` (8) (Informant Pings) | E22 stroke thirds |
| `TendCueDelaySec`, `DefenceDurationSec`, `DefenceCooldownSec`, `NpcKillStrikeLimit`, `ExileBeatSec`, `RejectPlateCooldownSec`, `InformantPlateCooldownSec` (other owners) | E12, E01, E02, E10, E15, E16 |

### Spec constants (a change is a spec change and an F1 re-run)

| Constant | Value | Why fixed |
|---|---|---|
| Cluster inset from the safe rectangle | 12 px | keeps brackets and clusters apart in every state |
| Footprints | tally 244 x 80; brace ring plate 56 x 56; brace label plate 232 x 40; own steps 161 x 56; rival steps 108 x 56; stage card 460 x 92; feed 420 x 100; latency plate 40 x 40; brackets 4 x 32 x 32 | F1 table; the lint reads them |
| Result plate 300 x 64; removal card 640 x 160; banner stack 720 x 238; centre line 560 x 44; status plate top y 236 | as stated | transient geometry, below 30 % |
| Interact ring 48 px; ring tick 8 px; strike hint plate 40 high; prompt plate 32 high | as stated | art bible 7.3 sizes |
| Marker 28 x 32 (floor 24 x 28); ping ring minimum 32 px; stroke 6, 4, 3 px | Teammate Markers and Pings | art bible constants |
| String budgets | title 27, card line 48, clue 43 characters at 100 % | keeps the stage card on its footprint |
| Text-scale card cap | 40 % of the safe width from 150 % | keeps the feed beside the card |
| Ultrawide clamp | centred 16:9 region of the safe rectangle for corner clusters | keeps eyes off far edges |
| Text floors 18 / 20 / 22 / 28 px; stepped entrance k = 4 | art bible 7.4, 7.6 | not a key |

---

## Acceptance Criteria

Each criterion is verifiable by a QA tester on a build (or, where marked, by the layout and presenter specs) without reading another document. `/story-done` closes HUD stories against these. Ids `UXA-HUD-nn` are this spec's; they refine, never replace, the GDD's `AC-HUD-nn` (cross-referenced). Evidence is retained screenshots in `production/qa/evidence/` named `hud-<case>-<resolution>.png` (testing standard: a parse check is not a run).

**Layout and safe zone**
- [ ] **UXA-HUD-01** At 1920 x 1080, 1920 x 1200, 2560 x 1440, 3440 x 1440, 1280 x 720 and 3840 x 2160, at UI scale 75, 100 and 150 % and text scale 100 and 200 %, no L1, L2 or L3 element crosses the safe rectangle (96 x 54 px at 1080p; 5 % of each dimension elsewhere). Screenshots retained. (AC-HUD-15)
- [ ] **UXA-HUD-02** At every case in UXA-HUD-01 the centred 20 % x 20 % box holds no persistent element, with every stage-card string at its budget (title 27, line 48, clue 43 characters). At 1080p and 200 % text the top-centre cluster ends at or above y = 432.
- [ ] **UXA-HUD-03** At 1080p, 100 % scales, each zone rectangle is within 2 px of the Layout Zones table: bottom-left cluster `(108, 870)` to `(408, 1014)`; stage card `(730, 128)`, 460 x 92; feed `(1392, 114)`, 420 x 100; latency plate `(1772, 66)`; Result slot centre `(960, 756)`; Notice slot top `y = 800`.
- [ ] **UXA-HUD-04** At 3440 x 1440 the bottom-left cluster's left edge is at x = 584 (within 2 px) and the informant brackets are at x = 172 (the full safe rectangle); at 5120 x 1440 the corner clusters stay inside the centred 16:9 region.
- [ ] **UXA-HUD-05** Resizing the window or changing UI scale mid-round relayouts every element without replaying any animation or cue.

**States and contexts**
- [ ] **UXA-HUD-06** Self-Active shows exactly the brace indicator with its "Brace" label and bound glyph, the tally, the stage card, the own steps and the rival steps as persistent elements (plus ally markers); nothing else is persistent. Retained screenshot. (AC-HUD-23)
- [ ] **UXA-HUD-07** An informant shows the frozen tally **without** the danger triangle, the objectives cluster, the four corner brackets and (when a feed line exists) the feed, and **no** brace indicator, prompt, ring, strike hint or result plate; the brace slot stays empty and the tally does not move. (AC-HUD-21)
- [ ] **UXA-HUD-08** For each viewer state (Lobby, Briefing, Self-Active, Eliminating, Informant Awaiting, Informant Bodied, Ended, Called off) the visible elements equal the visibility matrix; the Briefing shows no HUD element at all.
- [ ] **UXA-HUD-09** While the owner's action slot is `Striking` (1.75 s) the strike hint is hidden; it returns when the lockout ends if the aim ray still hits a rendered non-ally pawn in reach.
- [ ] **UXA-HUD-10** Opening the in-session menu leaves every HUD element unchanged and adds no scrim, dim, blur or desaturation; the menu panel's bottom edge (at most 70 % of height) clears the bottom-left cluster at every text scale.

**Budget**
- [ ] **UXA-HUD-11** `A_persist` computed by the layout resolver from the real widgets at 100 % scales and 16:10 is at most 8 % for the Self-Active state and for the informant state, and the lint `HudPersistentAreaLimitPct >= A_persist` passes. With the reference footprints it is 7.12 % and 6.68 %. (AC-HUD-15)
- [ ] **UXA-HUD-12** No state shows more than 12 plate-class elements in L1 and L2, and a lower-centre column never shows more than two text blocks (Result and Notice); after a removal card entrance, exactly one text block is on screen in the central region once 0.25 s have passed. (AC-HUD-13)
- [ ] **UXA-HUD-13** The removal card and the banner stack each cover at most 30 % of the screen (reference values 4.9 % and 8.3 %) and are never full-bleed.

**Notifications and queueing**
- [ ] **UXA-HUD-14** When a removal card enters, a plate in the Result slot leaves over 0.25 s in steps (not a cross-fade); an informant plate arriving while the Result slot is occupied is dropped and never appears later; a third-person hint arriving while the Notice slot is busy is dropped and shows on the next third-person entry.
- [ ] **UXA-HUD-15** Four ally removals in one update show three lines at once and the fourth after at least 2.0 s (evicting the oldest); a marker retiring without a state change creates no line; nothing is shown to a player of the other team. (AC-HUD-06, AC-HUD-16)
- [ ] **UXA-HUD-16** A rejection plate shows at most once per 3 s per code, a `Cooldown` rejection shows no plate, and a different code arriving while a plate shows replaces it in one step; the plate sits to the right of the brace row and does not overlap the tally or the Notice slot.
- [ ] **UXA-HUD-17** A rival-steps rise of 2 in one update fills both steps in one 0.5 s fill with one tick; the displayed rival count never decreases within a round.
- [ ] **UXA-HUD-18** Every result plate (Target exiled, Wrong target, No contact, Reversed, Strike reversed) has the same size, slot, entrance (four steps in 250 ms), hold (3.0 s) and exit (600 ms), and appears no earlier than the owner's own contact frame; only the text and the glyph slot differ. (AC-HUD-01, AC-HUD-05, AC-HUD-09)

**Wording** (strings must match exactly)
- [ ] **UXA-HUD-19** The removal card title is "Informant" for every cause; the subtitle is "Too many wrong targets" after an exile and "Eliminated for the round" after an assassination or a reversal; the second line is "You watch for your team now."; a rival kill shows the plate "Target exiled"; the word "Exiled" appears on no card. (AC-HUD-13)
- [ ] **UXA-HUD-20** The end banner shows "Victory" or "Defeat", exactly one of "completed the final objective" or "every rival eliminated for the round", and "Your house {a} · Rival house {b}"; the round summary is exactly the two approved lines, own house first, and is absent before `Ended` and in a called-off round. (AC-HUD-17, AC-HUD-27)
- [ ] **UXA-HUD-21** Over a 3-minute Active round with eliminations on both teams, no displayed string matches a time format outside "Brace in m:ss" and the returning countdown, an opponent count, "left", "remaining", or a banned word (dead, lives, revive, spectator, ghost, tracked, registered, detected, spotted). (AC-HUD-12)

**Accessibility (tier: Standard with elevations)**
- [ ] **UXA-HUD-22** Measured on captured pixels, text below 28 px has at least 4.5:1 contrast on its plate and larger text and graphics at least 3:1 (Bone Dim included); effective sizes are at least 18 px (20 px for actionable text, 22 px for changing numerals, 28 px for Cochineal Bright text) at 1080p-equivalent. (AC-HUD-15, T-03)
- [ ] **UXA-HUD-23** In greyscale, a tester identifies every HUD state (marker fills, tally, triangle, result outcomes, ring states, steps, brace states, brackets, latency icon) with 100 % accuracy, and in protan, deutan and tritan simulation no two semantic elements are confused. (T-01, T-02)
- [ ] **UXA-HUD-24** With Reduce motion on, no entrance or exit lasts longer than one frame, the result-plate hold is still 3.0 s and the card hold still 4.0 s, and the cooldown sweep advances in 1 s steps. (AC-HUD-09, T-06)
- [ ] **UXA-HUD-25** With High contrast on, every stroke and keyline is one step thicker without changing any footprint, Bone Dim is promoted to Bone, and the view-model dump shows the same facts as with it off. (T-10)
- [ ] **UXA-HUD-26** At text scale 200 % no message is truncated (names may ellipsise), no cluster overlaps another, the Result and Notice slots do not overlap, and the feed keeps three entries of at most two lines each.
- [ ] **UXA-HUD-27** The interact ring's live state (Marigold with four cardinal ticks) and its plain state differ by at least 3:1 in luminance and are distinguishable in greyscale by the ticks alone. (CR-HUD-8 item 11)
- [ ] **UXA-HUD-28** The latency icon is the three-step stair with a hollow top step, appears above 0.20 s RTT, hides only below 0.16 s after at least 2 s shown, and never appears on the listen-server host.
- [ ] **UXA-HUD-29** No HUD layer can be focused or clicked on keyboard-mouse or gamepad; a tester can complete a round using only the keyboard or only a gamepad with no HUD element capturing input.

**Layers, lifecycle and leaks**
- [ ] **UXA-HUD-30** The camera fade covers an ally marker, an interact ring and a ping ring and leaves the feed line, the card and the countdown fully visible; the layers sit at ZOrder 10, 20, 30, 40, 50, 60 and 100. (AC-HUD-24)
- [ ] **UXA-HUD-31** After a disconnect and a rejoin inside the grace, no plate, card, feed line, ping ring or cue is replayed, the tally equals the server's value, and a first strike afterwards shows its plate. (AC-HUD-19)
- [ ] **UXA-HUD-32** The strike hint is identical for an NPC, an enemy player and an enemy informant, absent for a concealed pawn, a bush and an empty spot, and a strike aimed at the viewer changes no HUD element before the viewer's own contact frame. (AC-HUD-01, -04, -05, -07)
- [ ] **UXA-HUD-34** At 1920 x 1080 on the reference GPU, with the cooldown sweep running, a result plate entering and three feed lines shown, profiling 600 frames shows the HUD at no more than 0.3 ms game thread and 0.2 ms render thread (0.5 ms with markers) and UI GPU at no more than 0.5 ms. ADVISORY until profiled. (AC-HUD-22)
- [ ] **UXA-HUD-35** At text scales 100, 125, 150 and 200 % a rejection plate sits 10 px right of the bottom-left cluster's actual right edge and overlaps neither the brace label plate nor the Notice slot.
- [ ] **UXA-HUD-33** Retained screenshots exist for: Self-Active with every persistent cluster, each result plate, the defence plate, the exile card, the assassination card, Informant Awaiting with the countdown, Informant Bodied, an ally feed line, the live-site ring with its ticks, a ping ring with its label and an edge ring, the Victory and Defeat banners with the summary, the called-off line, and each at 1080p and 16:10. (AC-HUD-23)

---

## Open Questions

> No GDD, art bible, pattern library, ADR or accessibility document was edited. **CONFLICT** is a contradiction between documents; **GAP** is a requirement or fact with no owner or no source; **RULING** needs a creative or technical decision; **NOTE** is information for the next reviewer. Every ruling this spec made on its own authority is logged in `production/session-logs/decision-log.md` (skill `ux-design`, points starting "hud:").

**Cross-reference check (skill Phase 5)**

- GDD requirements: **14 of 14** GDDs with HUD-relevant requirements covered; 5 of 19 confirmed headless by reading (Information Architecture).
- Patterns: **no new pattern invented**. Three existing patterns need updates (HUD-UX-20); flagged as gaps, library not edited (logged).
- Navigation mismatches: none. The pause menu's open item PM-13 is answered (Layout Zones: the panel clears the bottom-left cluster by at least 76 px at every text scale).
- Accessibility gaps: the concealed-state non-visual twin (F11) is not a HUD matter and stays open with Concealment; T-12 stays NOT ASSESSED; screen reader, OS high contrast, mono downmix, haptics API and adaptive-controller behaviour are NOT SOURCEABLE for UE 5.8 in the engine reference; the 5 s reading-time deviation is documented (Accessibility).
- Missing empty states: none. Every data-dependent element has a collapsed or empty state in the visibility matrix (feed, pings, objectives in `Preparing`, rival steps at 0, summary absent before `Ended`).
- Journey phases: `design/player-journey.md` does not exist (HUD-UX-14).

| # | Type | Question or follow-up | Owner | Resolve by |
|---|---|---|---|---|
| HUD-UX-1 | CONFLICT | HUD-R5 says the bottom-left slot stacks "brace indicator, tally, rejection plate (stacked upward)"; HUD-R13, HUD-E15 and IIF UI 5 say the rejection plate sits **beside** the brace indicator (interaction-patterns P5). This spec places it beside the brace row (right of it, outside the persistent footprint). Confirm, and correct HUD-R5's wording. | ux-designer, HUD GDD owner | HUD re-review |
| HUD-UX-2 | CONFLICT | The user-ratified removal wording (D11: title "Informant"; subtitles "Too many wrong targets" and "Eliminated for the round"; plate "Target exiled") is not yet in downstream documents: art bible 7.3 row 11 and 4.7 (still "Exiled"), `accessibility-requirements.md` (Colour audit row "Removal card", C-10 "the 'Exiled' wording collision must be resolved") and `interaction-patterns.md` PAT-CRD-1 ("Exiled"). This spec uses D11 throughout. **D17 (2026-10-25) applied** to `accessibility-requirements.md` and `interaction-patterns.md`; the art bible sweep remains with its own reviewer. | art-director | art bible next edit |
| HUD-UX-3 | CONFLICT | HUD VA-HUD-1 says "Turquoise never appears in any HUD element"; Informant Pings VA-PNG-3 gives the ping label plate a Turquoise keyline and cites art bible 4.6, whose table does permit "ally-only chat/ping chrome". This spec follows the art bible and the Pings GDD (2 px Turquoise stroke on the ping label, following the local accent). Reword VA-HUD-1 to "except ally chrome (marker, ping label)", or change the ping label to a Bone stroke. | art-director, creative-director | HUD re-review |
| HUD-UX-4 | CONFLICT | The ping is MVP and team-wide since 2026-10-25 (no feed line, no spatial audio), but PAT-WLD-5 and its Gaps row still say "Vertical Slice", art bible 7.3 row 9 and 7.7 still say "informant viewers only", a feed line "Sighting near [landmark]" and a "spatial audio tick", and the accessibility document still lists the sighting ring and callout cue as VS and Informant Pings as "No (Not Started)". **D17 applied** to `accessibility-requirements.md` and `interaction-patterns.md` (pings are MVP, team-wide; PAT-WLD-5 updated); the art bible rows remain with their reviewer. This spec follows the Pings GDD (PNG-R7, PNG-R8). Accessibility F3 (a team channel for players who cannot use voice) is only partly answered: a ping is a place-mark with no text. | ux-designer, art-director, game-designer | next edit of each document |
| HUD-UX-5 | RULING | (a) The approved banner reasons are lowercase fragments ("completed the final objective"); rendered after "Victory" or "Defeat" they need an initial capital or a joining word. (b) "every rival eliminated for the round" read by the **defeated** viewer sounds as if they eliminated every rival; the GDD allows no other phrase. The spec shows the approved phrase verbatim under the bar word and the score line; a viewer-relative phrasing needs Round's and the narrative-director's approval. | narrative-director, Round owner | HUD re-review |
| HUD-UX-6 | CONFLICT | IIF UI 5 specifies a quiet 2D tone with the rejection plate; HUD VA-HUD-6 omits it; the accessibility SFX audit assumes it (interaction-patterns P4). The informant plate has no sound. The spec lists the tone as an IIF-specified cue. | audio-director | HUD cue list (HUD OQ11) |
| HUD-UX-7 | RESOLVED (D16, 2026-10-25) | **F1 convention and margin.** **Resolved by D16**: count each plate's reserved rectangle (art bible 7.1 "corner-plate area"); `HudPersistentAreaLimitPct` stays 8 %; Self-Active at 7.12 % passes. The strict one-box-per-cluster reading is rejected. (Original ruling request follows.) The spec counts reserved plate rectangles (7.12 % at 16:10, margin 0.88 points). The strict one-box-per-cluster reading gives 9.44 % and fails the 8 % cap. Confirm the plate convention (art bible 7.1 says "corner-plate area"), or keep the strict reading and raise `HudPersistentAreaLimitPct` to 9 % (inside its safe range 8 to 9; art bible 7.1 says 8). Two wrapped stage-card lines at 100 % would breach (8.40 %). | art-director, creative-director | Closed by D16 |
| HUD-UX-8 | GAP (deferred by D16, 2026-10-25) | **Deferred to the first UI implementation story**: the art-director picks a licensed face then and the widths are re-measured. Until then every text width is an estimate. Every text width in this spec is **NOT MEASURED**: the functional face is not chosen (art bible Section 8 licence check) and widths assume 0.5 em (Regular) and 0.56 em (Semibold, tabular). Re-run F1, the text-scale reflow table and the string budgets with the real face and a real layout resolver. | art-director, ui-programmer | first UI implementation story (D16) |
| HUD-UX-9 | RULING | Answers HUD OQ9 on this spec's authority: clamp corner clusters to a centred 16:9 region on wider aspects (brackets and L0 edge clamp use the full safe rectangle); the safe margin stays a key in MVP and is not a player setting (if exposed later it leaves the key table, TDC CR-14); 4:3 and 5:4 are unsupported (cap not enforced). Confirm; HUD OQ9 can then close. | creative-director | HUD re-review |
| HUD-UX-10 | GAP | Text floors scale with `H/1080`: at 1280 x 800 (handheld-PC class) the 18 px floor is 13.3 physical pixels, and for a TV the HUD GDD's OQ14 asks for 24 px floors. Neither is a stated target, but 1280 x 800 is in the layout test matrix. Decide whether a handheld-PC target is adopted (then floors and F1 change). | producer, accessibility-specialist | before any handheld or TV target |
| HUD-UX-11 | RULING | The latency icon is a three-step stair with a hollow top step on a 40 x 40 plate (closes interaction-patterns P10 and the accessibility audit row "Needs confirmation"). Art-director to approve the glyph and add it to the art bible 3.4 table. | art-director | HUD asset spec |
| HUD-UX-12 | GAP | (a) The interact ring's live and plain states must differ by at least 3:1 luminance (art-director confirmation, CR-HUD-8 item 11); measure on captured frames over the brightest ground. (b) HUD OQ12's strike-hint visible share is a **client-local** quantity: ADR-0023's telemetry is server-side and `NeverReleased`, so the first playtest needs a client-local counter in a playtest build (never read by gameplay, never replicated). | art-director, analytics-engineer | first playtest build |
| HUD-UX-13 | CONFLICT | Art bible 7.3 row 15 puts the returning countdown in the "result slot"; HUD-E11 moved it to the Notice slot at design review. Other rows of CR-HUD-8 not yet verified as applied: items 11 to 14 (ring ticks, "Brace" label, no triangle for a frozen tally, contrast ratios and the PC-first floor note). | art-director | art bible next edit |
| HUD-UX-14 | GAP | `design/player-journey.md` does not exist; journey phases in this spec are inferred from the GDDs. Author it from `.claude/docs/templates/player-journey.md`. No skill writes it. | producer, ux-designer | before `/ux-review` of the full UX set |
| HUD-UX-15 | RULING | Localisation: plate sizes that must be outcome-independent (result plate, removal card) are computed from the widest string of the **active locale**; stage-card strings have character budgets because wrapping at 100 % costs F1 margin. Owner to confirm the budgets are enforceable in the string pipeline and that the pseudo-locale test is part of `/localize`. | localization-lead | before string freeze |
| HUD-UX-16 | RULING | The GDD and art bible say the removal card "collapses into the informant brackets" over 800 ms. This spec reads that as a **fade-out** (alpha 1 to 0, ease-in-out) with the brackets already presented at the card entrance (F4 `t_removal = t_card`), because a spatial collapse toward four corners would be a large motion. Confirm the reading. | art-director | HUD re-review |
| HUD-UX-17 | RULING | Four UX decisions made here that the GDD does not state and that need a GDD acknowledgement: (1) the context prompt collapses while an Interact hold is active and the ring takes its place; (2) a different rejection code replaces a showing rejection plate in one step; (3) informant brackets fade with the other L1 clusters at the end banner; (4) the prompt is hidden, not greyed, while the owner is running or traversing (Concealment UI-1 wording). All depend only on the owner's own state and are leak-neutral. | ux-designer, HUD GDD owner | HUD re-review |
| HUD-UX-18 | NOTE | The owner's concealed state has no HUD element by rule (Concealment UI-2: "no icon, meter, timer"); with Reduce screen effects the vignette is disabled and the player has only the camera inside foliage and the "Slip out" prompt (accessibility F11, interaction-patterns P9). This spec adds nothing; the owner is Concealment, with the art-director and audio-director. | art-director, audio-director | Concealment re-review |
| HUD-UX-19 | NOTE | This spec supplies answers the accessibility document and pattern library still list as open: F2 and P8 (the 5 s deviation and its rationale, Accessibility), F4 (high contrast defined), P10 (latency shape), P5 (rejection plate position), PM-13 of the pause menu (clearance). Update those rows when this spec passes `/ux-review`. | ux-designer | `/ux-review` |
| HUD-UX-20 | GAP | Pattern library updates still needed: PAT-PLT-4 gains the reconnecting and back line types and the removal triangle only on removal lines; PAT-CRD-2 gains the round summary plate (E21) and the called-off and lobby plates. (PAT-WLD-5 became an MVP, team-wide pattern in the D17 sweep, 2026-10-25; the edge ring is still to be added to it.) | ux-designer | next edit of `interaction-patterns.md` |
| HUD-UX-21 | NOTE | ADR-0021's L1 widget list names an "objectives cluster"; this spec splits it into three plates (own steps, rival steps, stage card) as the HUD re-review suggested. Widget names in the component inventory (`W_ObjectivesCluster` and children) are layout detail, not an ADR change. | ui-programmer | implementation |
| HUD-UX-22 | GAP | The "Ping" key has no on-screen prompt in MVP (PNG-UI-4 is a controls reference only), so discoverability relies on the controls reference and the playtest briefing (PNG OQ3, Onboarding is Alpha). Decide after the first playtest whether a one-time hint (like E17) is needed. | ux-designer | first playtest |
| HUD-UX-23 | CONFLICT | The rejection plate (E15) is on layer **L2** in the HUD GDD's inventory (HUD-E15) but in the **L1** persistent list of ADR-0021 §1 and HUD-R4's layer table. This spec follows the ADR (L1; it is transient but beside the brace cluster). Confirm and correct HUD-E15's layer in the GDD. Behaviour is unaffected (both layers are `HitTestInvisible`). | HUD GDD owner, ui-programmer | HUD GDD next edit |
| HUD-UX-24 | RULING | The centre-empty rule ("no persistent element in the 20 % x 20 % centre box") is written for plates in L1 to L3. Ally markers (Must Show, L0) are anchored to world positions and can legitimately cross the centre. Confirm that L0 world-anchored glyphs are outside the centre rule (as they are outside F1), and state it in HUD-R5. | creative-director, HUD GDD owner | HUD GDD next edit |

**Handoffs.** `art-director`: HUD-UX-2, -3, -8 (deferred, D16), -11, -12a, -13, -16. `ui-programmer` and `ue-umg-specialist`: the component inventory, the layout resolver and the F1 lint (HUD-UX-8, -21). `audio-director`: cue list and HUD-UX-6. `qa-lead`: the UXA-HUD acceptance criteria and the evidence list. **Cross-links to add (named, not edited):** `design/ux/interaction-patterns.md` ("Used In" entries that say `hud.md (planned)` now have a file), `design/ux/pause-menu.md` (PM-13 is answered here), `design/gdd/hud.md` (its UX Flag and OQ9 and OQ14 point here). Next steps: run `/ux-review hud`; then `/asset-spec system:hud` once the art bible items above are settled.
