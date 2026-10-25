# UX Spec: Main Menu

> **Status**: In Design (authored in an unattended run, 2026-10-25; every choice is logged in `production/session-logs/decision-log.md`; reviewed with `/ux-review` 2026-10-25: NEEDS REVISION, revisions applied in place, report in `production/design-closeout/ux-review-main-menu.md`)
> **Author**: ux-designer (autonomous run; no user session)
> **Last Updated**: 2026-10-25
> **Journey Phase(s)**: Front end, before the first round and between sessions (no `design/player-journey.md` exists; see Open Question MM-1)
> **Platform Target**: PC (Windows). Keyboard and mouse primary, gamepad partial (`project.yaml` has no `platform:` block; source is `.claude/docs/technical-preferences.md`). Touch: none.
> **Scope tier**: **MVP**. Lobby UI proper is Vertical Slice (`design/gdd/systems-index.md` row 19); this spec ends at the hand-off into the minimal lobby panel.
> **Template**: UX Spec

---

## Purpose & Player Need

**Player goal.** Get from "I launched the game" to "I am in my group's session" in one or two actions, with no matchmaking and no surprises. The game has no session browser, no matchmaking and no NAT relay in the MVP (Session & Travel rule 5): a group of up to ten friends agrees an address out of band, one person hosts, the others type the address.

**Complete the sentence.** *The player arrives at this screen wanting to* **start or enter tonight's session with the people they came with.**

**What goes wrong without it.** There is no other way to host or join: without this screen the MVP is console commands only. A hard-to-use address field, an unreadable rejection or an unlabelled wait would make the first thirty seconds of every playtest evening a support call. Pillar 1 adds a negative duty: the front end must say nothing about who else is online, in a round, or on which team beyond the six Session-defined rejection sentences (the `InProgress` sentence is the one place a round's existence is stated, and Session mandates it); it is the only screen a not-yet-admitted person sees, and it never shows a count or a name of anyone else.

**What this screen is not.** It is not a lobby (participants, teams, ready state and Start belong to the minimal lobby panel now and to Lobby UI in the Vertical Slice), not a settings screen (Settings is a pushed screen), not a help screen (Vertical Slice) and not a server browser (Matchmaking, Full Vision).

**Sources.** Session & Travel UI Requirements ("Main menu: Host and Join, an address field plus a Join button; the field remembers the last address"), rules 1, 4-6, 10, 23, 24; accessibility C-02, M-01, M-11, V-01, V-13; interaction-patterns PAT-MNU-1, -8, -9, -11 to -14.

---

## Player Context on Arrival

| Question | Answer |
|---|---|
| When is it first seen? | On launching the game, after the first-launch photosensitivity notice (Vertical Slice, Layout Specification, Component M10). |
| What came immediately before? | Process start and the engine splash; or the end of a session (the host left, "The session has ended." acknowledged), a deliberate Leave from the in-round menu, a cancelled connection attempt, or "Quit to menu" after a failed join. |
| Emotional state to assume | Calm and social, a little impatient: a group is waiting on a chat channel for the address. Not time-pressured by the game itself. The art bible sets the target for menus: "composed anticipation. The player is being seated, not hurried" (art bible 2.2). |
| Voluntary? | Yes. The only involuntary arrivals are the session-end and connection-failure returns, where the player arrives mildly annoyed and needs the cause in words and the next action at hand. |
| Journey phase | "Arrive" only; the journey map does not exist yet (MM-1). |

Frequency: once per evening per person, plus a return after any drop that did not auto-recover. Typing an address is therefore the dominant interaction and the remembered address (Session rule 23) removes it for every session after the first.

---

## Navigation Position

This screen lives at: **[application root] -> Main Menu**. It is the root of the front end and is never pushed onto anything; every other front-end screen is pushed onto it (PAT-MNU-11).

Alternate entry paths: session end, deliberate Leave, connection failure, cancel of a connection attempt. It is the one screen reachable with no session, and the only menu not drawn over a round.

It is a top-level destination, not context-dependent. The in-round Escape menu (`design/ux/pause-menu.md`) is its in-session counterpart and uses the same panel geometry, the same controls and the same Settings screen.

---

## Entry & Exit Points

| Entry Source | Trigger | Player carries this context |
|---|---|---|
| Game launch | Process start | Locally stored display name and last address (both may be absent on first run) |
| Session end overlay | Player presses OK on "The session has ended." (Session UI) | Last address and display name; no session |
| In-round Escape menu, Leave | Confirmed "Leave the session" | Last address (so Join is one press for a rejoin, Session rule 23), display name |
| Connection overlay | Cancel on "Connecting...", "Loading the city..." or "Connection lost. Reconnecting..." | The address that was attempted, still in the field |
| Join rejection | Session rejection reason code | The attempted address; the reason sentence is shown on this screen (State: Join rejected) |
| In-round Escape menu, Quit | Not an entry: Quit closes the application | n/a |

| Exit Destination | Trigger | Notes |
|---|---|---|
| Status overlay "Connecting..." then the Lobby (minimal lobby panel) | Host (button) or Join (button, or Enter in the address field) | Not reversible without a Cancel; Cancel on the overlay returns here. The Lobby is a map travel, so this screen is destroyed; it is rebuilt on return. |
| Settings (pushed screen) | Settings button | Reversible: Back or Esc pops to this screen with focus on the Settings button (PAT-MNU-12). `design/ux/settings.md` does not exist yet (spec dependency). |
| Application exit | Quit button | One-way. No confirmation (decision logged; nothing is at risk on this screen). |

Illegal exits: there is none by Esc. At the root, Esc never quits and never navigates (Interaction Map, Esc).

---

## Layout Specification

All sizes are px at the 1080p reference (UI scale = screen height / 1080, art bible 7.4); the safe margin is 5 % (96 x 54 px).

### ASCII Wireframe

```
+------------------------------------------------------------------------------+
|                                                                              |
|  SMOKING MIRROR                                  (right of the screen is     |
|  ----------------                                 an empty Obsidian Ink      |
|                                                   field in the MVP; the      |
|  +----------------------------------------+       art bible 2.2 courtyard     |
|  | Your name                              |       scene is a VS decision)    |
|  | [ Citizen 4821                      ]  |                                  |
|  |                                        |                                  |
|  | [ Host a session                    ]  |   <- Secondary                   |
|  |                                        |                                  |
|  | Join a session                         |                                  |
|  | Host address                           |                                  |
|  | [ 192.168.0.5:7777                  ]  |                                  |
|  | e.g. 192.168.0.5 or 192.168.0.5:7777   |   <- help / inline error line    |
|  | [ Join                              ]  |   <- Primary (inverted plate)    |
|  |                                        |                                  |
|  | [ Settings ]            [ Quit ]       |   <- Secondary, Secondary        |
|  +----------------------------------------+                                  |
|                                                              Build 0.1.0     |
+------------------------------------------------------------------------------+
```

Status overlays ("Connecting...", "Loading the city...", "Connection lost. Reconnecting...", "The session has ended.") are Session-owned L4 content drawn over this screen (PAT-CRD-3); the wireframe shows the screen beneath them.

### Layout Zones

| Zone | Position (1080p) | Content |
|---|---|---|
| Title block | Top-left at the safe margin: x 96, y 54 to about 170 | Title (Display, 48 px, display accent face) on an opaque plate |
| Menu panel | Left-docked column: x 96, width 560, vertically below the title with a 48 px gap | One PAT-FND-1 plate holding every control, in the order of Component Inventory. Max height: the safe area; scrolls (PAT-MNU-6 rules) only at text scale 150 % and above if the content exceeds it |
| Build label | Bottom-right at the safe margin | "Build {version}", 18 px, Bone Dim, on a plate |
| Scene area | The rest of the screen | Flat Obsidian Ink in the MVP. No element, no animation, no input |

Arrangement chosen (logged): **left-docked single column**, over a centred column or a split title-and-menu. Reasons: one geometry for the front end and the in-round menu (PAT-MNU-11: "a menu opened during a round is an overlay on layer 100 and never replaces the HUD"), a reading path that is one vertical line for keyboard and gamepad, and a free right-hand side for the art bible 2.2 courtyard in the Vertical Slice. Menu panel width scales with text scale up to 45 % of the screen width; beyond that, labels wrap (MM-12, P12).

### Component Inventory

| # | Component | Type | Content | Interactive | Pattern |
|---|---|---|---|---|---|
| M1 | Title plate | Static text | "SMOKING MIRROR" (string-table; the title is a working name, game concept title note) | No | PAT-FND-1; Display typography (>= 28 px) |
| M2 | Menu panel | Container | Opaque Obsidian Ink, stepped 4 px corners, 2 px Bone Dim stroke | No | PAT-FND-1 |
| M3 | Name field | Text input | Label "Your name" above the field (not placeholder-only); value = locally stored display name; limit `DisplayNameCharLimit` (20, Session rule 10) with the 80 % counter | Yes | PAT-MNU-8 |
| M4 | Host button | Button | "Host a session" | Yes | PAT-MNU-1 Secondary |
| M5 | Join heading | Static text | "Join a session" (Primary text size, Semibold) | No | n/a |
| M6 | Address field | Text input | Label "Host address"; value = last address; accepts `ip` or `ip:port` (IPv4) or a hostname (Session rule 5); help line beneath: "Example: 192.168.0.5 or 192.168.0.5:7777" (from the string table); Clear control | Yes | PAT-MNU-8 |
| M7 | Join button | Button | "Join" | Yes (disabled only while M6 is empty, with the reason in the help line; an invalid address leaves M7 enabled and is reported by M12 on press, see State: Invalid address) | PAT-MNU-1 Primary |
| M8 | Settings button | Button | "Settings" | Yes | PAT-MNU-1 Secondary; opens PAT-MNU-11 Push |
| M9 | Quit button | Button | "Quit" | Yes | PAT-MNU-1 Secondary (not Destructive; see Entry & Exit Points) |
| M10 | First-launch notice (VS) | Modal | Photosensitivity and Reduce screen effects notice; single Primary "Continue"; link to the Reduce screen effects setting; no auto-dismiss | Yes | PAT-MNU-9 (notice form) |
| M11 | Build label | Static text | "Build {version}" | No | n/a |
| M12 | Inline message plate | Text plate | Join rejection, invalid address, host failure; replaces the help line text in M6's slot (it never adds a second line) | No | PAT-MNU-14 Error |

New patterns introduced: **none**. The title block and the build label are one-off statics, not interaction patterns.

### Information Hierarchy

1. **The two ways in** (Host, Join with its address): the single most important thing; Join is the one Primary (inverted plate), Host is the other large control.
2. **What is wrong** (M12): when present it replaces the help line directly under the field the player just used, so it is read second and never missed. Never colour-only (words plus the upward-triangle glyph from PAT-FND-3 for finality-class errors only; routine "enter an address" hints carry no glyph).
3. **Who I am** (M3): visible but quiet; the name is used in the lobby.
4. **Settings and Quit**: discoverable, last in order.
5. **The title and build label**: orientation only.

Nothing on this screen shows other players, counts, sessions in progress, latency or the state of any round (Session rule 24; Pillar 1).

---

## States & Variants

| State / Variant | Trigger | What Changes |
|---|---|---|
| Default (first run) | First launch | M3 holds the generated default "Citizen NNNN"; M6 empty; M7 disabled; help line reads the example; initial focus on M6 |
| Default (returning) | Launch with a stored address | M6 holds the last address; M7 enabled; initial focus on M7 (Join) |
| Join disabled | M6 empty | M7 is disabled (Bone Dim text, no marker); help line: "Enter the host's address to join." (the reason is shown in text beside it, PAT-MNU-1 Disabled) |
| Invalid address | Validation fails on edit-commit or Join press | M12 replaces the help line: "That address doesn't look right. Example: 192.168.0.5:7777" (never a colour change alone); M7 stays enabled but activation re-focuses M6 and shows M12 |
| Name empty or over limit | M3 cleared | On blur the field restores the previous valid name; at the limit a counter shows at 80 % and further input is blocked (PAT-MNU-8) |
| Connecting | Host or Join pressed and accepted | Session status overlay "Connecting..." with Cancel (PAT-CRD-3, L4). This screen stays loaded beneath it so Cancel returns instantly |
| Join rejected | Session reason code on the same attempt | Overlay closes; M12 shows the reason sentence (list below); focus returns to M7 (the trigger). The sentence stays until the player edits M6 or presses a button; it never auto-dismisses (accessibility C-06) |
| Retryable `Busy` | Session returns `Busy` | Not a menu state: the overlay shows "The session is changing cities. Trying again..." and the client retries (Session UI Requirements) until it succeeds or the player cancels |
| Connection lost, reconnecting | Client loses the host mid-session | Session overlay "Connection lost. Reconnecting..." with Cancel; Cancel returns here with M6 intact |
| Session ended | Host left or crashed | Session overlay "The session has ended." with OK; OK reveals this screen in Default (returning) |
| Host failed (gap) | The listen socket could not be opened | M12: "Could not start a session. Try again, or restart the game." (Session defines no host-failure reason code; Open Question MM-4) |
| Settings pushed | Settings pressed | PAT-MNU-11 Push; this screen is not destroyed; focus restored on pop |
| First-launch notice (VS) | First ever launch, before any control is operable | M10 modal; the screen beneath is inert; focus trapped on "Continue" (PAT-MNU-9) |
| Loading (boot) | Process start to first interactive frame | The title block and an unlabelled empty panel are not shown; the engine splash covers it. No spinner: boot is not a menu wait (PAT-MNU-14 applies to waits after a player action) |
| Platform variant | Gamepad active | Glyphs follow the active device (PAT-FND-2); M6 and M3 text entry opens the OS on-screen keyboard if the OS provides one (availability under the unified 5.8 input is NOT SOURCEABLE here, MM-7) |

**Rejection sentences** (one per Session reason code, verbatim from Session & Travel UI Requirements; shown only to the rejected client, rule 24):

| Code | Sentence |
|---|---|
| `Full` | "This session is full." |
| `InProgress` | "This round has already begun." |
| `Busy` | "The session is changing cities. Trying again..." (retry, not a final state) |
| `VersionMismatch` | "Your game version does not match the host's." |
| `ParamsMismatch` | "Your game could not match the host's city. Please restart the game." |
| `Refused` | "You cannot rejoin this session." |

---

## Interaction Map

Mapping interactions for **keyboard and mouse (primary) and gamepad (partial)**. Menu input is the `IMC_Menu` context, held by the persistent menu root, containing only navigation actions (navigate, accept, back, tab); no gameplay action is mapped (ADR-0021 §7).

| Component | Action | Keyboard / Mouse | Gamepad | Immediate feedback | Outcome |
|---|---|---|---|---|---|
| All controls | Move focus | Tab / Shift+Tab (reading order), Up/Down arrows; mouse hover | D-pad / left stick (spatial) | Focus: 4 px Bone stroke plus stepped-square marker, instant; quiet UI tick | Focus moves; never wraps between unrelated regions (PAT-MNU-12) |
| M3 Name field | Edit | Click or Enter to focus, type; Esc clears focus without discarding | A to focus (OS keyboard if available), B to blur | Visible caret; counter at 80 % | Value held locally; committed on blur or Enter |
| M4 Host | Activate | Enter / Space / click | A | Pressed state (plate change, no scale); confirm cue | Session `HostSession` -> overlay "Connecting..." (Events Fired) |
| M6 Address field | Edit / activate Join | Type; **Enter in the field activates Join**; Esc blurs; Clear control | A to focus; keyboard via OS | Caret; inline validation line | Join attempt if valid, else M12 and focus stays in M6 |
| M7 Join | Activate | Enter / Space / click | A | Pressed; confirm cue | Session `JoinSession(address)` -> overlay "Connecting..." |
| M8 Settings | Activate | Enter / Space / click | A | Pressed | Push Settings (200 ms stepped slide of at most 8 px, instant under Reduce motion) |
| M9 Quit | Activate | Enter / Space / click | A | Pressed | Application exits at once. No confirmation |
| Screen | Esc / back | Esc | B | None at the root | **No-op at the root** (decision logged). Esc only blurs a field, closes a dropdown or dialog, or pops a pushed screen |
| Screen | Right-click | no-op (defined, not undefined) | n/a | none | none |
| Screen | Scroll wheel | Scrolls the panel only if it overflows (text scale 150 % and above); otherwise no-op | Right stick scrolls the same | Panel content moves | none |
| Screen | Device change | Last device decides glyphs; focus is kept (PAT-MNU-12) | | Glyphs swap instantly | none |

Activation timing follows PAT-MNU control baseline: pointer activates on release, keyboard and gamepad on press. Every state is distinguishable in greyscale (stroke weight and marker, never hue). Menus use plates, Bone, Bone Dim and Obsidian only; **no signal hue appears** (interaction-patterns "Menu look").

---

## Data Requirements

| Data | Source System | Read / Write | Notes |
|---|---|---|---|
| Display name | Local profile (client-local storage) | Read and Write | Default "Citizen NNNN" (4 random digits generated once, then persisted). Sent to the host as the requested name (Session rule 6); the host trims, strips, truncates and suffixes duplicates (rule 10), so the lobby may show a name different from the one typed. Never used for identity |
| Last address | Local profile; written by Session's join code (rule 23: "written to local storage when the client joins") | Read here; write by Session | Specified: the value that was last attempted. Open: whether a failed attempt overwrites a working address (MM-9) |
| Build version label | Build constant (the displayed form of the project's protocol and build identity) | Read | Display only; the gate itself is `ProtocolVersion` (Session rule 7); which string is shown is MM-13 |
| Connection status and reason codes | Session & Travel (own connection only) | Read | Rejection code, `Busy` retry, session ended, connection lost. Only the local client's own status is readable (Session rule 24) |
| Settings values | Local settings (client-local; never replicated, TDC CR-14) | Read in the Settings screen, not here | This screen reads none |
| `IdentityKey` | Local profile | Neither | Generated once by Session; the UI never displays it (it is a shared secret, rule 6) |

Architecture concerns flagged (not decided here): (1) two local-profile values (name, address) need a defined local store and a defined write time (MM-9); (2) the host path calls Session directly from UI; Session exposes no `HostSession` API in the GDD text I read, only "when a host chooses Host" (rule 1), so the entry-point name is a placeholder for the architecture team. The UI holds no game state.

---

## Events Fired

| Player Action | Event Fired | Payload / Data |
|---|---|---|
| Press Host | Session host request (game-state change: opens a listen session and travels to the Lobby) | Display name; no address |
| Press Join / Enter in M6 | Session join request (game-state change) | Display name, address, `IdentityKey` (never shown) |
| Cancel on a connection overlay | Session cancel (game-state change) | none |
| Press Settings | none (local navigation) | none |
| Press Quit | none (process exit) | none |
| Edit name or address | none (local profile write on commit, see Data Requirements) | none |
| Any menu navigation | **none, deliberately** | Clients record and write no telemetry (ADR-0023: "Clients record and write nothing"); menu events are not analytics |

Flags: the two local-profile writes are persistent state and need the architecture team's attention (MM-9). No action here touches server persistent data.

---

## Transitions & Animations

Values from the interaction-patterns Animation Standards; the art bible (7.6) is authoritative. Transition length is independent of frame rate. **Reduce motion** (accessibility V-15): every fade 0 ms, no settle, push and pop become cuts.

| Transition | Default motion | Reduce motion |
|---|---|---|
| Screen appear (first interactive frame) | Plates fade in 400 ms ease-out with an 8 px settle in whole 4 px steps (non-urgent plate appear) | Instant |
| Push to Settings / pop back | 200 ms stepped slide of at most 8 px with a fade; not a full-width slide | Instant cut |
| Host or Join pressed -> overlay | Replace: 200 ms fade out, 200 ms fade in (to the L4 overlay) | Instant |
| Overlay Cancel -> back to this screen | Replace in reverse, 200 + 200 ms | Instant |
| Inline message plate (M12) appear | Non-urgent plate appear 400 ms | Instant |
| First-launch modal open / close | 200 ms ease-out / 150 ms ease-in, no scale-pop | Instant |
| Focus and hover | 0 ms, state change only | Same |

State-change animation (minimum for a multi-state screen): M12 appearing under the field is the one in-screen state animation; M7's enabled/disabled change is instant. Nothing loops, pulses or spins; the "Connecting..." spinner (if Session uses one) is replaced by a static equivalent under Reduce motion (PAT-MNU-14). Motion-sickness risk: none (no camera, no parallax); the VS courtyard scene must keep its mirror-disc smoke loop at 10 s or longer (art bible 2.2).

---

## Input Method Completeness Checklist

Touch is not a supported input method for this game; its block is dropped.

**Keyboard**
- [x] All interactive elements reachable with Tab and arrows alone (Interaction Map, focus order below)
- [x] Tab order follows reading order: M3, M4, M6, M7, M8, M9
- [x] Every mouse action is achievable by keyboard
- [x] Focus visible at all times (4 px stroke plus marker, never suppressed)
- [x] Focus trapped inside modals (M10, any dialog); returns to the trigger on close
- [x] Esc closes or cancels and does **not** quit the game from within this screen (no-op at root)

**Gamepad**
- [x] All elements reachable with the D-pad and left stick
- [x] Face-button mapping consistent with platform convention: A confirm, B back (menus fixed, not remappable, PAT-MNU-1)
- [x] No action requires analog precision
- [x] Trigger and bumper shortcuts: none used on this screen (nothing to document); tab switching is Settings'
- [ ] Controller disconnected while this screen is open: **not specified by any source**; listed under Open Questions MM-7 as an implementation question, not ticked
- [ ] Typing an address with a gamepad needs an OS on-screen keyboard: availability NOT SOURCEABLE (MM-7)

**Mouse**
- [x] Hover states defined (Obsidian Raised plate; no hover-only information)
- [x] Hit targets at least 48 px at 1080p (the template floor is 32 px; PAT baseline is 48)
- [x] Right-click defined as a no-op
- [x] Scroll wheel defined (scrolls the panel only on overflow)

---

## Accessibility

Committed tier: **Standard** with named elevations (`design/accessibility-requirements.md`). Rows cited by id.

| Requirement | How this screen meets it | Phase |
|---|---|---|
| **V-01** menu text 24 px minimum at 1080p; **V-02** n/a; WCAG 1.4.4 | All body, label and button text is at least 24 px at 1080p; the build label is 18 px Bone Dim and is the one exception: it is secondary orientation, equal to the art bible's 18 px floor (accessibility V-03 floor for secondary labels); flagged for review (MM-12) | MVP |
| **V-04** contrast | Text below 28 px at least 4.5:1, large text and graphics at least 3:1, measured on the opaque plate. Bone on Obsidian Ink expected well above; verify on final values (T-03) | MVP |
| **V-08** colour not the only indicator | No hue is used at all on this screen. States are shape, stroke weight and words (focus = stroke plus marker; disabled = Bone Dim plus a reason line; error = words plus a glyph where the pattern requires one) | MVP |
| **V-09** UI scale 75 to 150 %, text scale 100 to 200 % | Layout holds at both extremes on 1920x1080, 1920x1200, 2560x1440, 3440x1440; plates size to text; no truncation of any message; the stroke and text floors never shrink | VS (MVP honours hooks at 100 %) |
| **V-11** high contrast | Every interactive element outlined; selection never colour-only | VS |
| **V-13** photosensitivity | First-launch notice M10 before the menu is operable; no flashing anywhere on this screen | VS (no MVP owner for the notice, accessibility F8) |
| **V-15** Reduce motion | Every transition in the table above has an instant variant | VS toggle; MVP hooks |
| **M-01 / M-11** | Fully operable without a pointer; gamepad and keyboard complete; every control states its function in text | MVP (operability); remap UI is VS |
| Mis-press safety (no accessibility row applies: M-12 concerns Strike only) | Quit is the only irreversible control; it sits last in the focus order and is never reachable by Esc. It is a Secondary button with no confirmation because exiting from the root menu loses nothing (an exception to PAT-MNU-1's Destructive rule, MM-15) | MVP |
| **C-06** reading time | A rejection sentence never auto-dismisses | MVP |
| **C-10** plain language | Strings obey Session's language rule: never "server", "map", "level", "seed", "travel", "player count" or "waiting for players"; "session" and "city" only | MVP |
| **T-12 / LIM-V2** screen reader | Name, role and state are exposed if the F16 spike resolves; **NOT ASSESSED** until then (engine support is NOT SOURCEABLE in `docs/engine-reference/unreal/**`). Until then, visible focus and text labels only | Alpha |

Keyboard-only path: Tab through M3, M4, M6, M7, M8, M9; Enter activates; Esc blurs or closes. Gamepad order is the same, by D-pad.

---

## Localization Considerations

All strings come from string tables (ADR-0021: no text on the wire). Plates size to their text so expansion is absorbed, not truncated (art bible 7.4); a control with a long label needs a wrapping rule that does not yet exist (P12, MM-12).

| Element | English | Max planned | 40 % expansion risk |
|---|---|---|---|
| M1 title | SMOKING MIRROR | 14 chars | Low; display face has no fallback script (art bible 7.4: header uses the functional face at Semibold in uncovered locales) |
| M4 | Host a session | 14 | Medium: wraps to two lines at text scale 200 % with German or French; the 48 px minimum height must grow, not clip. **HIGH PRIORITY for localization** |
| M3 label | Your name | 9 | Low |
| M5 heading | Join a session | 14 | Medium (same rule as M4) |
| M6 label and help | Host address / Example: 192.168.0.5 or 192.168.0.5:7777 | 55 | **HIGH PRIORITY**: the help line is the longest static string; must wrap, never truncate; the example numerals are locale-neutral and not translated |
| M12 sentences | See the rejection table | up to 70 | **HIGH PRIORITY**: `ParamsMismatch` is the longest; wraps inside the panel |
| M7, M8, M9 | Join / Settings / Quit | 8 | Low |
| M11 | Build {version} | 16 | Low; the version string is not localised |

Numbers, dates and currencies: none shown. The address field accepts only ASCII host characters (IPv4, ports, hostnames); the display-name field must accept the full Latin Extended set, macrons and the saltillo (art bible 7.4 test string) and its limit counts characters, not bytes (Session rule 10 `DisplayNameCharLimit`). Right-to-left layout: out of scope (no RTL target).

---

## Acceptance Criteria

All verifiable by a tester without any other document.

- [ ] **Performance**: from the first rendered frame, the screen is operable (focus visible, Tab works) within **5 s** of process start on the reference machine (RTX 3060-class, 1080p) [proposed; ADR-0022 sets no boot budget, MM-9a], and a return from Settings, Cancel or session end is operable within **250 ms** [proposed]
- [ ] **Navigation (Host)**: pressing Host with a valid name shows "Connecting..." and then the Lobby; Cancel on the overlay returns to this screen with the name and address unchanged
- [ ] **Navigation (Join)**: with a reachable host, typing its address and pressing Enter in the field (without touching the mouse) shows "Connecting..." and reaches the host's Lobby
- [ ] **Navigation (Settings)**: Settings pushes the Settings screen; Esc or B pops it and focus returns to the Settings button
- [ ] **Navigation (Quit)**: Quit closes the application immediately with no dialog; Esc at the root never quits and never navigates
- [ ] **Empty state**: on first launch the address field is empty, Join is disabled, and the help line states why in words
- [ ] **Error state**: entering "banana!!" and pressing Join shows the invalid-address sentence in the help slot, keeps focus in the field, and changes no colour (the message is words)
- [ ] **Rejection (one per code)**: for each of `Full`, `InProgress`, `VersionMismatch`, `ParamsMismatch` and `Refused` the exact sentence in the table appears under the address field within one round trip of the decision, stays until the player acts, and appears **only** on the rejected client
- [ ] **Session end**: after the host leaves, "The session has ended." with OK appears on every client, OK returns to this screen, and the address field still holds the last address
- [ ] **Remembered address**: after one join attempt, relaunching the game shows that address in the field and Join enabled with initial focus on Join
- [ ] **Name rules**: a name over 20 characters cannot be entered (counter at 16 characters, input blocked at 20); the lobby shows the name after the host's trimming and duplicate suffixing
- [ ] **Pillar 1 negative**: with a round in progress on the host, a client attempting to join sees no count, name, team or state of any other participant on this screen, only the `InProgress` sentence
- [ ] **Accessibility (committed tier)**: every interactive element has a visible focus indicator that survives greyscale; at text scale 200 % and UI scale 150 % on 1920x1080 no message is truncated and every control is still reachable by Tab; all text on the panel measures at least 4.5:1 on its plate
- [ ] **Busy retry**: with the host mid-travel (`Busy`), pressing Join shows "The session is changing cities. Trying again..." on the status overlay (not on this screen), retries without player input, and reaches the Lobby when the host is ready; Cancel returns here with the address intact
- [ ] **Resolution**: at 1280 x 720, 1920 x 1080, 1920 x 1200, 2560 x 1440 and 3440 x 1440 the panel stays inside the 5 % safe margin, every control is reachable, and no text is clipped
- [ ] **Input parity**: the same journey (name, Host, Settings, Back, Join, Cancel) can be completed with keyboard only and with gamepad only (OS keyboard permitting)
- [ ] **Screenshot evidence**: a retained screenshot of the Default, Invalid address and Join-rejected states exists in `production/qa/evidence/` (UI story evidence)

---

## Open Questions

**Cross-reference check (autonomous run).**
- GDD UI requirements: Session & Travel "Main menu" (Host, Join, address field remembering the last address) covered; "Status overlays" and "Rejection messages" covered as states, with their drawing owned by Session on L4.
- New patterns to add to the library: none.
- Navigation mismatches: none found; the in-round counterpart (`design/ux/pause-menu.md`) shares geometry and Settings.
- Accessibility gaps: screen reader NOT ASSESSED (F16); first-launch notice has no owner (F8).
- Missing empty states: none (every data-dependent control has an empty or disabled state).

| ID | Type | Question | Owner | Resolution |
|----|------|----------|-------|-----------|
| MM-1 | GAP | Player journey map not yet created. Author it from the template at `.claude/docs/templates/player-journey.md` to establish player context for this screen. | producer, ux-designer | Open |
| MM-2 | RULING | Join is the one Primary and Host is Secondary, because most players join. Confirm, or swap if playtests show the host path dominates. | ux-designer, creative-director | Proposed |
| MM-3 | RULING | Display name: field on the main menu, default "Citizen NNNN", persisted. No GDD owns name entry (Session only receives a requested name). Settings & Accessibility may move it. | game-designer | Proposed |
| MM-4 | GAP | Session defines no reason code or sentence for "the host could not open a session" (port in use, no network). The sentence in States is a placeholder. | network-programmer | Open |
| MM-5 | CONFLICT | PAT-CRD-3 status overlays need Cancel and OK input on layer L4, but ADR-0021 forbids input and activatables below ZOrder 100 (interaction-patterns P7). This spec assumes the overlays route their input through the menu root; the ADR needs to say so. **Resolved by D15 (ADR-0021 L4 split, 2026-10-25):** layers below ZOrder 100 stay display-only; an overlay that needs buttons (Cancel, OK) pushes a Session-owned modal at ZOrder 100, so the status overlays' text stays on L4 and their buttons live on that modal. | ue-umg-specialist, technical-director | Resolved by D15 (ADR-0021 L4 split) |
| MM-6 | RULING | The MVP background is flat Obsidian Ink. Art bible 2.2 describes a lit courtyard with an obsidian mirror disc as the "lobby and menus" scene; deciding whether it is a menu map, a lobby-only motif (its text says lobby-only) or a Vertical Slice item is the art-director's. | art-director | Proposed |
| MM-7 | GAP | Gamepad text entry: whether the OS on-screen keyboard appears under the 5.8 unified input stack, and how a controller disconnect mid-screen is handled, are NOT SOURCEABLE in this repo (`docs/engine-reference/unreal/**`). Needs a spike (accessibility F16 class). Alternative to consider: a recent-addresses list so a gamepad player never types. | ue-umg-specialist | Open |
| MM-8 | RULING | After a deliberate Leave, the way back is Join with the remembered address (Session admits a known key as a rejoin). Whether to add a one-line "Rejoin your last session" hint or a dedicated button is undecided and not in any GDD. | game-designer | Open |
| MM-9 | GAP | (a) No boot-time budget exists in ADR-0022; the 5 s and 250 ms figures are proposals. (b) Where the local profile (name, address) lives and when the address is written (on Join press versus on success; a failed typo must not overwrite a working address) is unspecified. | technical-director, performance-analyst | Open |
| MM-10 | GAP | The first-launch photosensitivity notice (M10) has no owning system or story (accessibility F8, V-13). This spec reserves its place; Settings & Accessibility (VS) owns the content. | producer | Open |
| MM-11 | NOTE | Lobby UI, the minimal lobby panel, the connection status overlays and the host panel have no UX spec yet (Session & Travel UX flag; Round & Match UX flag). The next screens to spec: minimal lobby panel plus overlays, Settings (MVP options panel per camera CS-R13: default camera mode, FOV per mode, sensitivity, invert Y, stick dead zone and curve, Reduce camera motion). | ux-designer | Open |
| MM-12 | GAP | No wrapping rule for a localisation-expanded control label (P12). The build label's 18 px size equals the art bible floor but sits below the 24 px menu minimum of V-01; confirm it is a "secondary label". | localization-lead, art-director | Open |
| MM-13 | GAP | Which string the build label shows (engine build, `ProtocolVersion`, or both) is undefined; it must be comparable between two players to diagnose `VersionMismatch`. | network-programmer | Open |
| MM-15 | CONFLICT | PAT-MNU-1 says an irreversible button is Destructive and always opens a confirmation ("no exceptions"); this screen's Quit is irreversible but Secondary with no dialog, while the pause menu's Quit is Destructive with a dialog. The exception here is deliberate (no session or state is at risk from the root menu). Confirm, and either word the pattern as "irreversible loss of session or state" or give Quit a dialog here. **Resolved by the pattern library review (2026-10-25):** PAT-MNU-1 now defines Destructive as irreversible for the session or saved state, and names Quit from the front-end root as Secondary. | ux-designer, art-director | Resolved in PAT-MNU-1 |
| MM-14 | NOTE | Menu motion, hover and focus values are proposals shared with interaction-patterns P1, P2, P3; the art-director confirms them for all menu specs together. | art-director | Proposed |
