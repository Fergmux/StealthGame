# UX Spec: Pause Menu (in-session Escape menu, does not pause)

> **Status**: In Design (authored in an unattended run, 2026-10-25; every choice is logged in `production/session-logs/decision-log.md`; reviewed with `/ux-review` 2026-10-25: NEEDS REVISION, revisions applied in place, report in `production/design-closeout/ux-review-pause-menu.md`)
> **Author**: ux-designer (autonomous run; no user session)
> **Last Updated**: 2026-10-25
> **Journey Phase(s)**: Any phase of a session: Lobby, Preparing, Active, Ended, as an informant, in the neutral view (no `design/player-journey.md` exists; see Open Question PM-10)
> **Platform Target**: PC (Windows). Keyboard and mouse primary, gamepad partial (`project.yaml` has no `platform:` block; source is `.claude/docs/technical-preferences.md`). Touch: none.
> **Scope tier**: MVP (Resume, Settings with the minimal options panel, Leave, Quit). Help and a host-controls entry are Vertical Slice candidates.
> **Template**: UX Spec

> **Naming note.** The file is called "pause menu" because the project calls the key "pause/menu" (input-interaction-framework `IMC_Common`). **Nothing is paused.** Smoking Mirror is a server-authoritative 10-player round; pause is an intentional, documented limitation (`design/accessibility-requirements.md` C-02, LIM-C1). This menu is an overlay on a world that keeps running. Player-facing text never says "pause".

---

## Purpose & Player Need

**Player goal.** While a session keeps running, do one of four things and get back quickly: **resume**, **change a setting**, **leave the session**, or **quit the game**.

**Complete the sentence.** *The player arrives at this screen wanting to* **step out of the way of the game for a moment, or get out of it, without the game punishing the way they do it.**

**What goes wrong without it.** There is no other way to reach Settings, to leave a session, or to quit from inside a session except Alt+F4, which is a disconnect with no explanation. C-02 (pause anywhere) is waived on the strength of exactly this menu: "the menu/pause key opens an overlay that does not stop the world... reachable in one press".

**The design problem unique to this game.** A pause overlay is normally a safe, neutral surface. Here it cannot be, for two reasons:

1. **The simulation does not stop.** The player's character stays in the crowd while their hands are on a menu. They cannot act (no Move, Look, Strike, Prepare) but can still be struck, and the menu must say so honestly.
2. **The character is the thing the game hides.** Pillar 1 ("the game never tells you who an enemy is") means any difference between a player-driven pawn and an NPC is a tell. A menu is the one moment a player's pawn has no input at all. So the menu carries a **binding design rule**:

> **PM-R1. Opening, using or closing the menu must not create a visual (or audible, or network-observable) tell on the player's character.** Observable pawn behaviour while a menu is open stays inside the crowd baseline (Session & Travel rule 25f is the existing binding outcome for connection events; this extends it to the menu).

The rest of this spec derives from PM-R1. It is **honest about the one residual tell the documents accept**: after D14 (PM-1, 2026-10-25) the frozen-head tell is closed by the shared idle gaze layer and only a stand longer than `CrowdMaxIdleSec` stays a player-chosen risk (see "Pillar 1 rules").

**Sources.** `design/accessibility-requirements.md` (C-02, LIM-C1, M-01, M-11, V-01, V-15); `design/ux/interaction-patterns.md` (PAT-MNU-1, -9, -11, -12, -13; laws L1 to L6); `docs/architecture/adr-0021-ui-hud-layering-commonui.md` §1, §7, §8, §9; `docs/architecture/adr-0011-pawn-command-interface-input.md` (§9, E11); `design/gdd/input-interaction-framework.md` (R4, input-layer edge cases); `design/gdd/shared-character-locomotion.md` (R5, R8.4, R9, R15); `design/gdd/crowd-npc-behaviour.md` (CNB-R6 gaze, `CrowdMaxIdleSec`, T14); `design/gdd/session-travel.md` (rules 4, 22, 24, 25f, 25g); `design/gdd/round-match-framework.md` (grace expiry, R22); `design/gdd/hud.md` (HUD-R5).

---

## Player Context on Arrival

| Question | Answer |
|---|---|
| When is it first seen? | The first time the player presses the menu key in a session; most likely in the Lobby (to change a setting) or at the first reason to leave. |
| What came immediately before? | Anything. In `Active` the player may be mid-walk, mid-Run, mid-Tend (3 s hold) or mid-Brace (4 s hold). In `Lobby` they are standing in the lobby. As an informant they are watching. In the neutral view they see a black screen with plates. |
| Emotional state | Split. **Relaxed** in Lobby, `Ended` and as an informant. **Time-pressured and a little anxious** in `Active`: "the round is still going and I have just taken my hands off the controls". The design assumes the anxious case. |
| Voluntary? | Yes, always. The game never opens this menu itself. (It may close it: see Entry & Exit Points.) |
| Journey phase | Unknown: no journey map (PM-10). |

Consequence for design: in `Active` the menu must be **one press to open, one press to close, Resume focused by default**, must state plainly that the world is still running, and must never ask the player to read a paragraph before they can get back.

---

## Navigation Position

This screen lives at: **[in-session] -> Pause Menu** (overlay on layer 100, ADR-0021 §1) **-> Settings (pushed) | Leave dialog | Quit dialog**.

It is an **overlay on a live round, never a replacement for the HUD** (PAT-MNU-11): L0 to L4 keep rendering and updating beneath it (the menu draws at ZOrder 100, above all of them). Its counterpart outside a session is `design/ux/main-menu.md`: same dock, same controls, same Settings screen.

Context-dependent, reachable from every in-session state (see States). Reachable from one place only: the menu key.

---

## Entry & Exit Points

| Entry Source | Trigger | Player carries this context |
|---|---|---|
| Gameplay (any phase, any role) | Menu key: **Esc** or gamepad **Start/Menu** (default; reserved from remapping, accessibility M-01) | Possibly held keys (all Ended on open), the current HUD state (unchanged) |
| Neutral view (black screen, informant awaiting a body) | Same key | Plates L1 to L4 visible; no world |
| Loading overlay ("Loading the city...", Preparing) | Same key | The overlay stays beneath; menu drawn above it (decision logged; PM-5) |
| Session overlay is up (connecting, connection lost, session ended) | None: the Session overlay owns input; the menu cannot be opened over it | n/a |

| Exit Destination | Trigger | Notes |
|---|---|---|
| Back into the session | Menu key again, **Esc**, **B**, **Start**, or the **Resume** button | Instant; gameplay keys need a **fresh press** (ADR-0021 §7, E10); no camera jump (Interaction Map) |
| Settings (pushed screen) | Settings button | Esc or B pops back to this menu with focus on Settings (PAT-MNU-12). `design/ux/settings.md` does not exist yet (spec dependency) |
| Main menu | Leave the session, after confirmation | One-way for this attempt: the client disconnects. Rejoin is possible while the round is in progress (Session rule 8: a known key is admitted as a rejoin) |
| Application exit | Quit the game, after confirmation | One-way. Same session consequence as Leave, then the process closes |
| **System dismissal** | The owner's removal card or the end banner begins to present; a Session status overlay takes over (connection lost, session ended) | The menu closes without changing anything (any open dialog is cancelled, any in-progress setting edit is discarded unless already applied live). Happens at **presented** time only, so it adds no pre-contact signal (PAT-FND-5, ADR-0021 §8) |

One-way exits: Leave and Quit (both behind a confirmation whose default focus is Cancel).

---

## Layout Specification

All sizes are px at the 1080p reference (UI scale = screen height / 1080); the safe margin is 5 % (96 x 54 px).

### ASCII Wireframe

```
+------------------------------------------------------------------------------+
|  [objectives, top-centre]                                  [team feed, TR]   |
|                                                                              |
|  +-------------------------------+                                           |
|  | MENU                          |                                           |
|  |                               |        (the world keeps running here;     |
|  | The round continues while     |         screen centre stays empty;        |
|  | this menu is open. You        |         NO dim, blur or desaturation)     |
|  | cannot act.                   |                                           |
|  |                               |                                           |
|  | [ Resume                    ] |  <- Primary, initial focus                |
|  | [ Settings                  ] |                                           |
|  | [ Leave the session  (up-tri) ] <- Destructive: opens a dialog            |
|  | [ Quit the game      (up-tri) ] <- Destructive: opens a dialog            |
|  +-------------------------------+                                           |
|                                                                              |
|  [brace, tally: bottom-left HUD]                      [result slot, 70 %]    |
+------------------------------------------------------------------------------+
```

Leave dialog (PAT-MNU-9), drawn over the panel, focus trapped, default focus on Cancel:

```
  +------------------------------------------------+
  | Leave the session                              |
  | The round continues without you. You can       |
  | rejoin while the round is in progress.         |
  |                                                |
  | [ Leave the session ]          [ Cancel ]      |
  +------------------------------------------------+
```

### Layout Zones

| Zone | Position (1080p) | Content |
|---|---|---|
| Panel | **Left dock**: x 96, width 520 (right edge 616 px, left of the central third that starts at 640 px), top at 20 % height (216 px), bottom at most 70 % height (756 px) **at every text and UI scale**: when the content is taller the top rises to the safe margin (54 px) and then the panel scrolls (focus keeps the focused control in view) | One PAT-FND-1 plate: heading, notice, four buttons |
| Dialog | Centred over the panel's own footprint (never over the screen centre): the dialog plate is anchored to the panel, x 96, width 520 | PAT-MNU-9 |
| Scene | Everything else | **The live world and the live HUD, untouched** |

Why left-docked, not centred (decision logged): the screen centre is "always empty" and the removal card, result slot (centre at 70 % height) and end banner use the central band and lower-centre column (HUD-R5); a centred panel would hide the very plates the player must see. At text scale 125 % and above the panel grows in width (to at most 45 % of the screen) but its **bottom edge stays at or above 70 % of the height (y = 756)**, so it never covers the bottom-left HUD cluster, whose top is at y >= 832 at every text scale (HUD spec, Clearance with the pause menu; PM-13 answered).

### Component Inventory

| # | Component | Type | Content | Interactive | Pattern |
|---|---|---|---|---|---|
| P1 | Panel | Container | Opaque Obsidian Ink, stepped 4 px corners, 2 px Bone Dim stroke; **100 % opaque** | No | PAT-FND-1 |
| P2 | Heading | Static text | "Menu" (Heading, 28 px, display accent) | No | Art bible 7.4 |
| P3 | Notice line | Static text | In a round phase (`Preparing`, `Active`, `Ended`): "The round continues while this menu is open. You cannot act." In `Lobby`/`Reset`: "The session continues while this menu is open." Chosen from the **public** phase, identical for every player and role | No | PAT-MNU-9 (overlay says so) |
| P4 | Resume | Button | "Resume" | Yes | PAT-MNU-1 **Primary** |
| P5 | Settings | Button | "Settings" | Yes | PAT-MNU-1 Secondary; opens PAT-MNU-11 Push |
| P6 | Leave | Button | "Leave the session" with the upward-triangle glyph | Yes | PAT-MNU-1 **Destructive** (opens PAT-MNU-9) |
| P7 | Quit | Button | "Quit the game" with the upward-triangle glyph | Yes | PAT-MNU-1 **Destructive** (opens PAT-MNU-9) |
| P8 | Leave dialog | Modal | See wireframe; host variant text in States | Yes | PAT-MNU-9 confirmation form |
| P9 | Quit dialog | Modal | Title "Quit the game"; the same consequence sentence as Leave; confirm "Quit the game", Cancel | Yes | PAT-MNU-9 |
| P10 | Help (VS) | Button | "Help" (holds every one-time hint and the controls, accessibility C-03) | Yes | PAT-MNU-1 Secondary; not in MVP |

New patterns introduced: **none**. Not included on purpose: a player list, scores, a timer, a count of any kind, a team or role readout, a latency readout (Pillar 1; Session rule 24; HUD-R14).

### Information Hierarchy

1. **Resume**: the single most important thing: get back. Primary plate, initial focus.
2. **The world is still running** (P3): second, so nobody reads "pause" into the overlay.
3. **Settings**: a normal, reversible task.
4. **Leave / Quit**: last, marked with the danger-class glyph, each behind a dialog.

---

## States & Variants

| State / Variant | Trigger | What Changes |
|---|---|---|
| Default (round phase) | Menu key in `Preparing`, `Active`, `Ended` | Panel as specified; P3 round text |
| Lobby variant | Menu key in `Lobby` or `Reset` | P3 session text; Leave dialog text per the Lobby row below |
| Informant | Own state is Informant | **Identical** to Default: no extra text, no hidden item. (An informant's gameplay keys already do nothing; the menu is unchanged. IMC_Informant is not touched, ADR-0021 §8) |
| Neutral view | Awaiting a body (black screen, L1 to L4 visible) | Identical to Default |
| Loading overlay beneath | Preparing, "Loading the city..." | Identical; menu drawn above the L4 overlay (PM-5) |
| Settings pushed | Settings pressed | PAT-MNU-11 Push of the Settings screen; Resume remains the way out of the stack |
| Leave dialog (round) | Leave pressed in a round phase | Title "Leave the session". Body: "The round continues without you. You can rejoin while the round is in progress." (PAT-MNU-9 text). Confirm "Leave the session", Cancel (default focus) |
| Leave dialog (Lobby) | Leave pressed in Lobby | Body: "You will leave the session. You can join again with the same address." Confirm and Cancel as above |
| Leave dialog (host) | The local client is the listen host (known locally) | Body replaces the sentence: "You are the host. If you leave, the session ends for everyone." (Session rule 4). Same buttons |
| Quit dialog | Quit pressed | Same body as the matching Leave variant, title "Quit the game", confirm "Quit the game" |
| Empty | n/a | The menu has no data-dependent content |
| Error | n/a | No control can fail in-menu; a failed disconnect is a Session overlay, not a menu state |
| System-dismissed | Removal card or end banner presents; Session overlay takes over | Menu closed; focus returns to the game; fresh key presses needed |
| Platform variant | Gamepad active | Glyphs follow the active device (PAT-FND-2); Resume hint "(B)"; D-pad navigation |

No control is ever disabled: Leave and Quit must always work, including during travel and in a stuck load.

---

## Interaction Map

Mapping interactions for **keyboard and mouse (primary) and gamepad (partial)**. While the menu is open the active input context is `IMC_Menu` (navigate, accept, back, tab only; a lint fails the build if it maps any `IA_*` gameplay action), held by the persistent menu root. Gameplay `IMC_*` contexts are never added or removed by Common UI (ADR-0021 §7).

### Opening (the critical sequence)

| Step | What happens | Source |
|---|---|---|
| 1 | Menu key pressed (`IMC_Common` menu action) | IIF `IMC_Common` |
| 2 | `UStealthMenuStack::PushMenu` first calls `UPawnCommandClient::EndAllOutstanding(EEndReason::MenuOpen)` **synchronously**: an End is sent for every outstanding Begin (Run, Interact, Prepare); Ends are idempotent and identical to a key release | ADR-0021 §7; ADR-0011 §9 |
| 3 | Then the activatable is pushed; input mode becomes `Menu`; the OS cursor appears | ADR-0021 §7 |
| 4 | The owner's pawn simply receives no input: **zero Move, zero Look, holds ended**, identical for every player. The local client keeps its normal command stream going (zero-acceleration moves); it does **not** go silent, so Session's silence clock never reaches `SessionSoftTimeoutSec` and Round never hands the pawn to AI because of a menu | ADR-0021 §7; Session rules 18-19, 25g |
| 5 | **Nothing else is sent.** No "menu opened" message, flag, RPC, replicated property, telemetry record or log line exists (clients record and write nothing, ADR-0023) | PM-R1 |

The panel's own 200 ms entrance (Transitions) never delays steps 2 to 4: input mode flips on the key frame.

### Controls

| Component | Action | Keyboard / Mouse | Gamepad | Immediate feedback | Outcome |
|---|---|---|---|---|---|
| Panel (open) | Open | Esc (menu key) | Start | Panel appears; cursor shown | As above |
| Panel (closed by key) | Close | Esc (again) | Start or B | Panel leaves; cursor hidden | Back to the game; **mouse and stick deltas accumulated while open are discarded (no camera jump)**; held keys ignored until released and pressed again |
| All controls | Move focus | Tab / Shift+Tab, Up/Down; mouse hover | D-pad / left stick | 4 px Bone stroke plus stepped-square marker, instant; quiet UI tick (PM-8) | Focus moves; never wraps between unrelated regions |
| P4 Resume | Activate | Enter / Space / click | A | Pressed state | Close (same as the key) |
| P5 Settings | Activate | Enter / Space / click | A | Pressed | Push Settings (200 ms stepped slide of at most 8 px; instant under Reduce motion) |
| P6 Leave | Activate | Enter / Space / click | A | Pressed | Opens the Leave dialog; **never leaves directly** (PAT-MNU-1) |
| P7 Quit | Activate | Enter / Space / click | A | Pressed | Opens the Quit dialog; never quits directly |
| Dialog | Confirm | Enter on the confirm button | A | Dialog closes | Leave: disconnect and show the main menu. Quit: disconnect and exit the process |
| Dialog | Cancel | Esc, or Enter on Cancel | B, or A on Cancel | Dialog closes | Returns to the panel with focus on the trigger; nothing changes |
| Screen | Esc / back | One level back: dropdown, dialog, Settings, then close | B | | PAT-MNU-13 |
| Screen | Right-click | no-op | n/a | | none |
| Screen | Scroll wheel | Scrolls the panel only if it overflows (text scale 150 % and above) | Right stick | | none |
| Screen | Device change | Last device decides glyphs; focus kept | | | none |

**Initial focus is Resume** (Primary), so the quickest sequence back into a round is **Esc, Esc**, or **Esc, Enter**. Enter can never leave the session by accident: the Destructive items are two buttons down and each opens a dialog whose default focus is Cancel.

**Window focus loss (Alt+Tab) behaves identically to opening the menu** for gameplay input: every outstanding Begin is Ended, no menu is opened (ADR-0011; IIF input-layer edge cases). Documented so nobody reads the shared behaviour as a menu feature.

---

## Data Requirements

| Data | Source System | Read / Write | Notes |
|---|---|---|---|
| Round phase (public) | Round & Match (replicated `FRoundPublicState`) | Read | Chooses P3 and the Leave text. Public, identical for all viewers |
| "Am I the listen host" | Session (local knowledge) | Read | Chooses the host variant of the Leave dialog; client-local, never sent |
| Own connection status | Session | Read | Triggers system dismissal when a Session overlay takes over |
| Presented own state / removal card / end banner | HUD presenter (`PresentedOwnState`, HUD-E10, HUD-E18) | Read | Triggers system dismissal at presented time |
| Settings values | Local settings (client-local; never replicated, TDC CR-14) | Read/Write in the Settings screen | This menu holds none |
| Participant lists, counts, teams, roles, scores, timers, latency | **None** | Never read | Absent by rule |

The menu writes no game state. Leave and Quit are requests to Session (explicit quit: `Disconnected` at once with that reason, Session rule 19). Architecture concern flagged: the Leave and Quit requests need a Session entry point that is not defined in the GDD text (only "an explicit quit"); placeholder for the architecture team.

---

## Events Fired

| Player Action | Event Fired | Payload / Data |
|---|---|---|
| Open the menu | **none** (deliberate). Side effect only: Ends for outstanding Begins, identical to key releases | none |
| Close / Resume | **none** (deliberate) | none |
| Settings pressed | none (local navigation) | none |
| Leave confirmed | Session explicit-quit request (game-state change): connection closes; Round applies its normal disconnect policy | Reason `Quit` (Session's reason vocabulary) |
| Quit confirmed | Same as Leave, then process exit | Reason `Quit` |
| Any dialog Cancel | none | none |

Flags: (1) **Persistent game state** is affected by Leave and Quit only, and only through Roster/Round's disconnect path (the leaver becomes `Disconnected`; after `DisconnectGraceSec` the active role becomes Informant, Round R17 disconnect policy, grace expiry; teammates see "{Ally} reconnecting" because the Team-audience `bReconnecting` bit cannot distinguish a deliberate leave: PM-6). (2) **No analytics event** for opening the menu: clients write no telemetry (ADR-0023).

---

## Transitions & Animations

Values from the interaction-patterns Animation Standards; the art bible (7.6) is authoritative. **Reduce motion** (V-15): every fade 0 ms, no settle, push and pop become cuts.

| Transition | Default motion | Reduce motion |
|---|---|---|
| Panel open | PAT-MNU-11 overlay appearance: 200 ms stepped slide of at most 8 px with a fade (no scale-pop). **Input mode flips on the key frame, not after the animation** | Instant |
| Panel close | 150 ms fade; **gameplay input is live from the key frame** (fresh press required) | Instant |
| Settings push / pop | 200 ms stepped slide of at most 8 px | Instant cut |
| Dialog open / close | 200 ms ease-out / 150 ms ease-in, no scale-pop | Instant |
| Focus, hover | 0 ms | Same |

What does **not** animate, ever (PM-R1 and art bible 7.6 rule 6): **nothing on or near the player's character**: no ring, marker, outline, glow, icon or label on the own body (art bible 7.1: "self-owned feedback never sits on or around the viewer's own body"), no camera move, no FOV change, no zoom, no blur, no dim, no vignette, no grade change. There is **no scrim**: the world is rendered exactly as when the menu is closed.

State-change animation (minimum for a multi-state screen): the panel appearing and the dialog appearing; nothing loops, pulses or spins. Motion-sickness risk: none (no camera effect).

---

## Input Method Completeness Checklist

Touch is not a supported input method; its block is dropped.

**Keyboard**
- [x] All interactive elements reachable with Tab and arrows alone
- [x] Tab order follows reading order: Resume, Settings, Leave, Quit
- [x] Every mouse action is achievable by keyboard
- [x] Focus visible at all times (4 px stroke plus marker, never suppressed)
- [x] Focus trapped inside the dialogs; returns to the trigger on close
- [x] Esc closes or cancels one level and **does not quit the game** (Esc from the panel only closes the panel; Quit exists only as a confirmed button)

**Gamepad**
- [x] All elements reachable with the D-pad and left stick
- [x] Face-button mapping consistent: A confirm, B back, Start opens and closes (menus fixed, not remappable)
- [x] No action requires analog precision
- [x] Trigger and bumper shortcuts: none used (nothing to document)
- [ ] Controller disconnect while the menu is open: **not specified by any source**; an open implementation question (PM-14), not ticked

**Mouse**
- [x] Hover states defined (Obsidian Raised plate; no hover-only information)
- [x] Hit targets at least 48 px at 1080p
- [x] Right-click defined as a no-op
- [x] Scroll wheel defined (panel overflow only)

---

## Accessibility

Committed tier: **Standard** with elevations (`design/accessibility-requirements.md`).

| Requirement | How this menu meets it | Phase |
|---|---|---|
| **C-02** pause anywhere (Waived, documented) | The mitigation set is **this menu**: reachable in one press from every state; states plainly the world is running (P3); a player who needs a real break uses Leave and rejoins (informants lose nothing by stepping away) | MVP |
| **M-11 / M-01** pointer-free; remap | Fully operable by keyboard or gamepad alone; **the menu key is the one reserved binding** (M-01); remap UI is VS | MVP / VS |
| Mis-press safety (no accessibility row applies: M-12 concerns Strike only) | Resume is the default focus; Leave and Quit are last, marked, and each behind a dialog whose default is Cancel; Esc never leaves or quits | MVP |
| **V-01 / V-04** text size, contrast | All text at least 24 px at 1080p, at least 4.5:1 on the opaque plate; heading 28 px | MVP |
| **V-08** colour not the only indicator | No hue: focus is stroke plus marker; Destructive is an upward-triangle glyph plus a specific verb; no Cochineal (PAT-MNU-1, interaction-patterns P1) | MVP |
| **V-09** scale | Holds at UI scale 75 to 150 % and text scale 200 % on 1920x1080, 1920x1200, 2560x1440, 3440x1440; plates grow, nothing truncates | VS (MVP hooks) |
| **V-15** Reduce motion | Every transition has an instant variant | VS toggle |
| **V-14 / V-16** | No screen effect and no camera motion is added by this menu, so Reduce screen effects and Reduce camera motion have nothing to turn off here | n/a |
| **C-06** reading time | The menu and dialogs never auto-dismiss on a timer; system dismissal is event-driven (card, banner, overlay) and the information they dismiss for is itself persistent on screen | MVP |
| **C-10** plain language | Language rule: never "dead", "lives", "tracked", "spotted", "pause", "spectator"; "session" and "round" only; no server wording | MVP |
| **T-12 / LIM-V2** screen reader | Exposed if the F16 spike resolves; **NOT ASSESSED** until then (engine support NOT SOURCEABLE in `docs/engine-reference/unreal/**`) | Alpha |
| **L6 / IVP R15** settings never add information | Every setting reachable here is client-local; the menu itself shows no fact about anyone else | MVP |

Keyboard-only path: Esc, then Tab or arrows to the target, Enter to activate, Esc to back out.

---

## Pillar 1 rules (the "no tell" rules)

These are the binding rules the Acceptance Criteria test. Each states what the documents already guarantee and what they do not.

| ID | Rule | Grounding | Status |
|---|---|---|---|
| **PM-R1** | Opening, using or closing the menu creates no tell on the player's character: no pose, animation, replicated property, network message, sound or timing difference attributable to the menu | Pillar 1; Session 25d/25f; SCL R8, R9 | Binding. **Achievable after D14 (PM-1, 2026-10-25)**: the head scan closes tell (a); a stand over `CrowdMaxIdleSec` is the accepted player-chosen residue (PM-R3) |
| **PM-R2** | Input while open is exactly "all input released": zero Move, zero Look, every hold Ended (a Run ends and decelerates by the baseline run-to-walk rule; a stop brakes by `WalkStopSec`). The command stream continues at its normal cadence; no "menu" message of any kind | ADR-0021 §7; ADR-0011 §9; SCL R15 baseline braking | Specified |
| **PM-R3** | While open, the pawn must remain inside the **crowd baseline**. Under PM-R2 alone it does not, in two ways: **(a) frozen head**: Look is zero, so `HeadLook` holds one yaw and pitch; NPC heads scan every 3 to 7 s while paused (CNB-R6 G1) and glance at passers-by (G2). **(b) long stand**: NPC stationary stretches never exceed `CrowdMaxIdleSec` (20 s); "a visible stand longer than 20 s is a player's choice: an intended P2 tell" (CNB T14). A menu left open longer than that is a statue | Crowd GDD; SCL R5.5, R8.4, R8.9 | **Resolved by D14 (PM-1, 2026-10-25):** (a) the shared idle gaze layer (SCL R8.9) scans the head after `IdleGazeDelaySec`; (b) a stand over 20 s stays a player-chosen risk |
| **PM-R4** | No visual change to the **owner's own view** that other viewers do not get by default: no scrim, blur, desaturation, vignette, grade, FOV or camera move; no ducking, low-pass or other filter on world audio. The world is rendered exactly as when the menu is closed. (Local-only, so not a leak, but it would blind the owner of a world that keeps running and would add a presentation difference the art bible forbids) | Art bible 7.1 (HUD drawn after post-process), 7.6; ADR-0021 §1 | Specified |
| **PM-R5** | Nothing is drawn on or around the own character (no ring, marker, label, "away" icon) | Art bible 7.1 | Specified |
| **PM-R6** | The menu shows no fact about anyone else and none that depends on role, team, state or phase beyond the public phase text | IVP; Session rule 24; HUD-R14 | Specified |
| **PM-R7** | The menu never delays, advances or hides an owner-only presentation: result plates, rejection plates, the removal card and the end banner present at their own contact-gated times (PAT-FND-5); the menu dismisses itself when the card or banner begins | ADR-0021 §8 | Specified |
| **PM-R8** | The menu is identical for every player and role (the same four buttons, the same text per public phase) | Law L1, L3 | Specified |

**PM-1 resolved by D14 (2026-10-25): neither Option B nor C as written.** No AI hand-over; the shared idle gaze layer (SCL R8.9) covers (a) for every pawn kind, and (b) is accepted as a player-chosen risk. The analysis below is kept as the record. **Original recommended resolution of PM-R3 (for ruling, PM-1).** The mechanism the project already designed for "a player pawn that must look like a citizen while its owner is absent" is the **Round hand-over**: Session raises `OnParticipantUnresponsive`, Round hands the pawn to crowd control through the same pawn command interface, and a rejoin hands it back with no visible change (Session rule 25f; Crowd takeover rule). **Option B** is to treat "menu open" as a client-declared absence that triggers that same hand-over (the AI holds a walk pace for `HandoverHoldSec`, then follows the catalogue, scans, glances and idles within `CrowdMaxIdleSec`). It would require one new owner-to-server message at open and close, which is server-only knowledge, and a ruling that menu use cannot become an exploit (an AI-driven pawn is a perfect citizen while its owner cannot act, which is also true of a disconnect). **Option A** (a client-side idle-gaze feed into the normal Look stream) fixes only (a). **Option C** is the current baseline: accept both residual tells and say so in the overlay. Chosen for this spec (decision logged): **C as the shipping-interim baseline, B recommended, and the gap wired into the acceptance criteria** so that it cannot pass silently.

---

## Localization Considerations

All strings come from string tables (ADR-0021). Plates size to their text so expansion is absorbed (art bible 7.4). No wrapping rule exists yet for long control labels (interaction-patterns P12).

| Element | English | Max planned | 40 % expansion risk |
|---|---|---|---|
| P2 | Menu | 4 | Low |
| P3 (round) | The round continues while this menu is open. You cannot act. | 62 | **HIGH PRIORITY**: wraps to three or four lines at 200 % text scale; must wrap, never truncate |
| P3 (lobby) | The session continues while this menu is open. | 46 | Medium |
| P4, P5 | Resume / Settings | 8 | Low |
| P6 | Leave the session (plus glyph) | 18 | **HIGH PRIORITY**: glyph plus label may need two lines in German or French; the 48 px minimum height must grow, not clip |
| P7 | Quit the game (plus glyph) | 13 | Medium |
| Dialog bodies | See States | up to 80 | **HIGH PRIORITY**: the round Leave body and the host body are the longest |

Numbers and dates: none shown. No names are shown, so the display-name Latin Extended requirement does not apply here. RTL: out of scope.

---

## Acceptance Criteria

All verifiable by a tester without any other document. "Observer" means a second client (or the host) watching the opener's pawn.

- [ ] **Performance**: pressing the menu key switches input to the menu (the pawn stops responding to Move) on the **same frame**, and the panel is fully visible within **250 ms** [proposed; no UI-open budget exists in ADR-0022]; closing restores gameplay input on the key frame
- [ ] **Navigation (open)**: the menu key opens the menu from each of: Lobby, Preparing (loading overlay), Active walking, Active in the neutral view, Ended, and as an informant
- [ ] **Navigation (close)**: Esc, Start, B and Resume each close it; afterwards held movement keys do nothing until released and pressed again, and **the camera does not jump** by the mouse or stick motion made while the menu was open
- [ ] **Holds**: with Prepare, Interact or Run held, opening the menu sends exactly one End per outstanding Begin before the next tick, and no second End follows when the key is released
- [ ] **Does not pause**: with the menu open for 10 s, an observer sees the opener's pawn in the world, the HUD unchanged (it shows no clock), other pawns moving, and a strike on the opener resolves normally
- [ ] **No scrim**: a screenshot with the menu open, cropped to the area outside the panel, equals the same frame with it closed except for the L0 to L4 HUD content (no dim, blur, desaturation, vignette)
- [ ] **Wire silence**: with the menu open and no key held, no RPC, replicated property or reflection-listed message exists that is specific to the menu (the static allowlist check `ReplicatedAllowlist` still passes; AC-SCL-43), and the opener never reaches `Unresponsive` during a 60 s open
- [ ] **Facts shown**: the menu shows no count, name, team, role, score, timer or latency; the view-model dump with the menu open equals the dump with it closed (settings-never-add-information diff T-10)
- [ ] **Leave (round)**: Leave opens the dialog with focus on Cancel; Esc cancels with no change; confirming returns the player to the main menu; teammates see only the existing reconnecting line and opponents see nothing
- [ ] **Leave (host)**: the host's dialog states that the session ends for everyone; confirming shows "The session has ended." on every client
- [ ] **Quit**: Quit opens its dialog with focus on Cancel; confirming disconnects and closes the process; Esc at the panel never quits
- [ ] **System dismissal**: when the owner's removal card begins presenting, the menu closes, any open dialog is cancelled with no setting changed, and the card shows in full; the same for the end banner
- [ ] **Preparing overlay**: during "Loading the city..." the menu opens above the overlay and Leave works
- [ ] **Accessibility (committed tier)**: focus is visible at all times and survives greyscale; at text scale 200 % and UI scale 150 % no message truncates and every control is reachable by Tab; all text on the panel is at least 4.5:1 on its plate; the whole menu is operable by keyboard alone and by gamepad alone
- [ ] **`PauseMenuNoTell` (observable tell, PM-R1 to PM-R3)**: GIVEN a player opens the menu and an observer watches the pawn for 30 s WHEN the observer's view is compared with an idle NPC (head motion distribution, stationary stretch length) THEN no statistical difference in head motion is detectable (testable against D14: the head scan starts `IdleGazeDelaySec` (3 s) after the last look input, then repeats every 3 to 7 s, SCL R8.9 and AC-SCL-49). **PM-1 resolved by D14 (2026-10-25).** A stationary stretch over `CrowdMaxIdleSec` (20 s) is outside this criterion: it is a player-chosen risk, recorded as a known, accepted tell like any away-from-keyboard stand
- [ ] **Resolution and panel clearance**: at 1280 x 720, 1920 x 1080, 1920 x 1200, 2560 x 1440 and 3440 x 1440, at text scale 100 and 200 %, the panel stays inside the safe margin, its bottom edge is at or above 70 % of the screen height, and it never covers the bottom-left HUD cluster; content that does not fit scrolls
- [ ] **Evidence**: retained screenshots in `production/qa/evidence/` of the Default panel in Active (HUD visible around it), the Lobby variant, the Leave dialog and the host Leave dialog

---

## Open Questions

**Cross-reference check (autonomous run).**
- GDD UI requirements: accessibility C-02 mitigation (one-press menu, honest text) covered; interaction-patterns PAT-MNU-9 text ("Opening any menu in a round does not pause the world and the overlay says so") covered by P3; PAT-MNU-13 covered.
- New patterns to add to the library: none.
- Navigation mismatches: none; shares dock, controls and Settings with `design/ux/main-menu.md` (panel width differs: 520 here to keep clear of the central band, 560 on the main menu).
- Accessibility gaps: screen reader NOT ASSESSED (F16); no pause is a waived requirement (LIM-C1).
- Missing empty states: none (no data-dependent content).

| ID | Type | Question | Owner | Resolution |
|----|------|----------|-------|-----------|
| **PM-1** | **RESOLVED (D14); original ruling request kept as the record** | **How is PM-R3 met?** Under the documents as written (ADR-0021 §7: zero Move, zero Look, holds ended, "identical for every player"), the pawn of a player in the menu has a frozen head (NPCs scan every 3 to 7 s, CNB-R6 G1) and, after `CrowdMaxIdleSec` (20 s), a stand the Crowd GDD itself classes as an intended tell (T14). Option A: client idle-gaze feed (fixes the head only). **Option B (recommended): menu open is a client-declared absence that uses the existing Round hand-over to crowd control** (needs one owner-to-server message, an exploit review, and edits to Round/Crowd/IIF). Option C: accept the tells and keep the honest overlay text (current baseline). ADR-0021 §7's "identical for every player" is true of the input and not of the observable behaviour relative to the crowd; the ADR should say so. **Resolved by D14 (2026-10-25, main-session decision):** no AI hand-over; the pawn keeps its position and last state, held actions end on open, and any pawn (menu-open, idle player or NPC) with no look input for more than `IdleGazeDelaySec` (3 s) gets the shared idle head-scan layer (SCL R8.9, AC-SCL-49; Crowd G1 shares the parameters). A stand longer than `CrowdMaxIdleSec` (20 s) stays a player-chosen risk, the same as being away from the keyboard. | creative-director, game-designer, network-programmer | Resolved by D14 |
| PM-2 | RULING | Opening the menu during a Prepare hold sends End (as any release does), so the brace ends early and the 60 s defence cooldown, stamped at execution, keeps running; a held Tend (3 s) is cancelled. This is a real gameplay cost of the one-press menu. Options: accept and teach it (Help), or defer menu-open until the brace ends (breaks "one press, always"). Not specified anywhere. | game-designer | Open |
| PM-3 | GAP | The Leave text cannot name times. `RejoinWindowSec` and `DisconnectGraceSec` are ServerOnly knobs, so the client cannot say "within 2 minutes" or "after 30 seconds you return as an informant". The text is generic. To show numbers one must become a public (audience All) value, a TDC and Session change. | game-designer, producer | Open |
| PM-4 | RESOLVED (D15, ADR-0021 L4 split, 2026-10-25) | ADR-0021 forbids input and activatables below ZOrder 100 (interaction-patterns P7), yet Session overlays on L4 need Cancel and OK. This spec assumes the menu closes when a Session overlay takes over. **D15:** layers below ZOrder 100 stay display-only; an overlay that needs buttons pushes a Session-owned modal at ZOrder 100, so the overlay's text stays on L4 and its buttons live on that modal. | ue-umg-specialist, technical-director | Resolved by D15 |
| PM-5 | GAP | Menu availability during travel and loading: this spec opens it above the L4 overlay (the only in-game way out of a stuck load). Whether the persistent menu root and `IMC_Menu` survive seamless travel, and whether a menu may be open during Session's neutral loading screen without revealing anything, is unverified. | ue-umg-specialist, network-programmer | Open |
| PM-6 | RULING | A deliberate Leave is handled as a disconnect (Session rule 19: quit moves to `Disconnected` at once). Consequences: teammates see "{Ally} reconnecting" (HUD-R10a) for someone who chose to leave; after `DisconnectGraceSec` the active role becomes Informant; the leaver may rejoin as an informant. Confirm that is wanted, or that Round distinguishes a deliberate leave (interaction-patterns P11). | game-designer | Open |
| PM-7 | RULING | Host leaving ends the session for everyone (Session rule 4, no migration). The host dialog says so. Consider a separate "End the session" wording and a host-controls entry (Round R22's optional host panel: Start, Reset, abort) in this menu for the host only; not committed. | game-designer, ux-designer | Open |
| PM-8 | CONFLICT | Sound Standards say in-round UI sound is only the listed cues, yet list menu hover and confirm sounds as "menu only". Whether the quiet tick plays when this in-round menu is open is unspecified (interaction-patterns P3). The tick is local 2D UI-bus audio and leaks nothing; the spec assumes it plays. | audio-director | Open |
| PM-9 | NOTE | Window focus loss has the same input effect as the menu (all Ends, zero input) and does not open the menu. Left as is; mentioned so the two are not confused in tests. | ux-designer | Info |
| PM-10 | GAP | Player journey map not yet created. Author it from the template at `.claude/docs/templates/player-journey.md` to establish player context for this screen. | producer, ux-designer | Open |
| PM-11 | NOTE | Streamers: a streamed frame of the opener shows the menu and a standing pawn. Name masking is the streamer alias mask (VS); a stream-sniping risk from a visible menu is out of scope here and belongs to art bible 7.7. | art-director | Info |
| PM-12 | GAP | System dismissal rules (removal card, end banner, Session overlay) are proposed here; they need HUD and Round to confirm, including the exact ordering against the ADR-0021 §8 `UseInformantContext` gate (the menu closing must not release a gameplay context early). | ue-umg-specialist | Proposed |
| PM-13 | RESOLVED | The HUD UX spec (`design/ux/hud.md`, Clearance with the pause menu) measures the bottom-left cluster top at y = 870 (100 %) to 832 (200 % text) and confirms a panel bottom at or above y = 756 clears it by at least 76 px at every text scale; this spec now caps the panel bottom at 70 % at every scale. | ux-designer | Resolved by hud.md |
| PM-14 | GAP | No GDD lists a default binding for the menu key (IIF `IMC_Common` lists "pause/menu" without one). Esc and Start are proposed; controller disconnect with the menu open is unspecified. | game-designer | Proposed |
