# Accessibility Requirements

> **Status**: In Design. The tier is committed (see below) but was chosen autonomously during the 2026-10-25 unattended run (`modes.automation: autonomous`); it awaits user confirmation. Until confirmed, treat the tier as the working commitment, not a user-ratified one.
> **Author**: ux-designer (autonomous run; no user session)
> **Last Updated**: 2026-10-25
> **Accessibility Tier Target**: **Standard**, with named elevations from Comprehensive (see "This Project's Commitment")
> **Platform(s)**: PC (Windows). Storefront TBD (game concept, Core Identity). Input: keyboard and mouse primary, gamepad partial (`.claude/docs/technical-preferences.md`, Input & Platform; `project.yaml` has no `platform:` block, so that file is the source).
> **External Standards Targeted**:
> - WCAG 2.1 Level **AA** (contrast and text sizing only; the game is not a web page)
> - AbleGamers CVAA Guidelines: **Partial** (no in-engine voice or text chat in MVP, so CVAA communication duties are not triggered yet; they trigger the day an in-game chat or ping channel ships, see GDD follow-up F3)
> - Xbox Accessibility Guidelines (XAG): **No** (PC only; used as a reference checklist, not a certification target)
> - PlayStation Accessibility (Sony Guidelines): **No** (not a target platform)
> - Apple / Google Accessibility Guidelines: **N/A** (no mobile target)
> - Game Accessibility Guidelines (gameaccessibilityguidelines.com): **Yes**, as the working checklist for Basic and Intermediate items
> **Accessibility Consultant**: None engaged
> **Linked Documents**: `design/gdd/systems-index.md`, `design/ux/interaction-patterns.md`, `design/gdd/game-concept.md`, `design/art/art-bible.md` (4.7, 7.4, 7.6, 7.7, 7.x), `design/gdd/hud.md` (HUD-R16), `design/gdd/input-interaction-framework.md` (R15, R16), `design/gdd/teammate-markers.md` (TM-R12, VA-4), `design/gdd/camera-system.md` (CS-R13)
> **Template**: Accessibility Requirements

> **Why this document exists**: Per-screen accessibility annotations belong in UX specs. This document captures the project-wide accessibility commitments, the feature matrix across all systems, the test plan, and the audit history. If a feature conflicts with a commitment made here, this document wins: change the feature, not the commitment, unless the producer approves a formal revision. **One exception is structural:** the four pillars and the anti-pillars (game concept) and the Information Visibility Policy guard (IVP R15: "no accessibility or settings feature may give directional or player-only information beyond the default presentation") outrank any row below. Where a row cannot be met without breaking a pillar it is recorded under Known Intentional Limitations with its mitigation, not silently dropped.
>
> **When to update**: After each `/gate-check` pass, after any accessibility audit, and whenever a new game system is added to `systems-index.md`.

> **Delivery phasing (read this before the tables).** `Settings & Accessibility` (system 24) is a Vertical Slice system and is **not yet designed**. The MVP ships fixed defaults (HUD-R16: UI scale 100 %, text scale 100 %, Reduce motion off, high contrast off) but its HUD, markers and camera must already *honour* every hook so that the Vertical Slice only has to expose them. The `Scope` column names the phase in which each requirement becomes player-visible: **MVP** (default behaviour or design constraint), **VS** (Vertical Slice settings screen), **Alpha**, **Release**. The Standard tier is a **Release** commitment; the Pre-Production gate needs only this document and the tier.

---

## Accessibility Tier Definition

### Tier Definitions

| Tier | Core Commitment | Typical Effort |
|------|----------------|----------------|
| **Basic** | Critical player-facing text is readable at standard resolution. No feature requires color discrimination alone. Volume controls exist for music, SFX, and voice independently. The game is completable without photosensitivity risk. | Low — primarily design constraints |
| **Standard** | All of Basic, plus: full input remapping on all platforms, subtitle support with speaker identification, adjustable text size, at least one colorblind mode, and no timed input that cannot be extended or toggled. | Medium — requires dedicated implementation work |
| **Comprehensive** | All of Standard, plus: screen reader support for menus, mono audio option, difficulty assist modes, HUD element repositioning, reduced motion mode, and visual indicators for all gameplay-critical audio. | High — requires platform API integration and significant UI architecture |
| **Exemplary** | All of Comprehensive, plus: full subtitle customization (font, size, color, background, position), high contrast mode, cognitive load assist tools, tactile/haptic alternatives for all audio-only cues, and external third-party accessibility audit. | Very High — requires dedicated accessibility budget and specialist consultation |

### This Project's Commitment

**Target Tier**: **Standard** (Release), plus the elevations listed below.

**Rationale**: Smoking Mirror is a 10-player online PvP stealth game whose entire skill is *reading* a crowd of near-identical figures by how they move. That makes it low in reflex and timing demands (anti-pillar "NOT a reflex game"; aim and reaction speed never decide an encounter; every defence is chosen by anticipation) and high in visual attention. The motor barriers are therefore modest and mostly about sustained holds (Run, a 3 s Interact hold, a 4 s Prepare hold) and a single-press, irreversible Strike. The visual barrier is large and cannot be solved with the usual toys, because the game forbids every identification aid (no outlines, radar, nameplates, audio direction, aim assist) and forbids any accessibility feature from adding directional or player-only information (IVP R15). The design already delivers most of Standard by construction: every HUD plate is opaque, no signal hue works alone (art bible 4.7), text floors and a 100 to 200 % text scale are specified (HUD-R16, art bible 7.4), and every owner-only audio cue has a text twin (art bible 4.7). Standard is what a solo developer with AI agents can finish and verify; the Comprehensive items that need platform screen-reader integration have no sourced UE 5.8 support in this repo (see Platform Accessibility API Integration), and three further Comprehensive items (directional audio indicators, difficulty assist modes, HUD repositioning) are structurally blocked by pillars or by PvP fairness (see Known Intentional Limitations). Dropping to Basic would remove input remapping, hold-to-toggle and the colour-vision options, which affect an estimated 8 to 12 % of any PC audience (colour-vision deficiency alone is about 8 % of men) and would break the HUD-R16 commitments the GDDs already made.

**Features explicitly in scope (beyond tier baseline)** (each is already specified in a GDD or the art bible, so these are commitments to deliver, not new design):
- **Reduce motion, Reduce camera motion and Reduce screen effects** as three independent client settings (HUD-R16, CS-R13, art bible 7.6 rules 5 and 8). Elevated from Comprehensive because the game has a controlled grade-plus-card sequence at every removal and first-person camera motion (motion-sickness exposure).
- **High contrast mode** for HUD and menus (HUD-R16, TM-R12). Elevated from Exemplary because the HUD is already opaque and the extra work is small.
- **Ally accent colour** with four options, shape and keyline unchanged (art bible 4.7, 7.7 rule 3). The marker is the one element whose hue cannot be fully backed up by shape alone at 24 px against bright ground, so a luminance-safe alternative is a core requirement, not a nicety.
- **Text scale to 200 %** on the HUD as well as menus, and **ally marker scale 100 to 150 %** (HUD-R16, TM-R12 `k_large`).
- **Extended read time** (`HudReadHoldMult` up to 2.0, tied to text scale; HUD-R16 ruling D11) to offset the plate holds that fall below the 5 s Standard guideline (see LIM-C2).
- **Hold-to-toggle for Run, Interact and Prepare** (IIF R16), delivered at the Vertical Slice together with the settings screen.
- **Mono audio**, **independent volume buses** and **captions for every owner-only gameplay cue** (the cue's own plate is its caption, see the SFX audit).
- **Haptic twins for owner-only cues on gamepad** (Exemplary item, scoped to owner-only outcome cues, never before contact; ASN-R6). Optional, VS or later.

**Features explicitly out of scope** (each is also a row in Known Intentional Limitations):
- Screen reader for menus (no sourced UE 5.8 support; see platform table). Revisit at Alpha after a spike.
- Screen reader or text-to-speech for the in-game world (impossible without breaking Pillar 1).
- HUD element repositioning.
- Difficulty assist modes and granular difficulty sliders (PvP; every rule parameter is a server-wide tuning value).
- Aim assist of any kind, input timing multipliers, and any directional audio visualisation (anti-pillars and IVP R15).
- Full subtitle customisation (the MVP has no spoken dialogue; reconsider if voice is added).
- External third-party accessibility audit (no budget; the Test Plan carries internal audits and user-testing placeholders).

### Autonomous decisions recorded in this run

Logged to `production/session-logs/decision-log.md` (skill `ux-design`): the tier choice (Standard with elevations), that Status is "In Design" pending user confirmation, the three-setting split of "reduce motion", the colour-vision approach (shape redundancy plus accent and signal-colour presets rather than a world post-process filter), the display brightness guard, and the 24 px menu text floor. Each can be reversed by the user in this file.

---

## Visual Accessibility

**Context from the game.** The world carries no gameplay information in hue: "No hue on any world character is unique to a team or role" (concept, Visual Identity Anchor), pool models are uniform by contract, and there are only three UI signal colours (Turquoise = ally, Cochineal = danger or finality, Marigold = progress or intel), each with a shape backup (art bible 4.7). That is why this project does not need a full-screen colourblind post-process: no information in the world frame depends on colour. The risk is confined to the HUD signal colours and to the marker's luminance against bright ground.

Targets use px at the 1080p reference (UI scale = screen height / 1080, art bible 7.4). "Scope" gives the phase in which the feature is player-visible.

| Feature | Target Tier | Scope | Status | Implementation Notes |
|---------|-------------|-------|--------|---------------------|
| **V-01** Minimum text size, menus | Standard | All menu screens (lobby, settings, pause, help). VS | Not Started | **24 px minimum body text at 1080p** (the template default), scaling with UI and text scale. The art bible's hierarchy (7.4) does not set menu sizes (GDD follow-up F13); this row sets them. WCAG 2.1 SC 1.4.4: text scalable to 200 % without loss of content, met by text scale 200 % (V-09). |
| **V-02** Minimum text size, subtitles and captions | Standard | Any spoken-dialogue subtitle and any caption line. Alpha (conditional) | Not Started | **28 px minimum at 1080p** on an opaque plate (PC desk distance; the template's 32 px is a TV figure, OQ14 in the HUD GDD reopens this if a couch target is added). No spoken dialogue exists in MVP, so this applies from the first VO. |
| **V-03** Minimum text size, HUD | Standard | In-game HUD. MVP | Not Started | Already specified: **18 px floor, 22 px for changing numerals, text the player must act on at least 20 px, Cochineal Bright text at least 28 px** (HUD-R16, art bible 7.4). Satisfies the template's 20 px "critical information" row, because actionable text is already at 20 px and the 18 px floor is reserved for secondary labels. Never reduced below 100 % values by UI scale. |
| **V-04** Text contrast, UI text on backgrounds | Standard | All UI text. MVP | Not Started | Text below 28 px at least **4.5:1**; larger text and non-text graphics at least **3:1**, Bone Dim included, measured on the plate over the brightest ground (HUD-R16, art bible 7.4 and 7.x C2). Plates are 100 % opaque, which makes the measurement independent of the world. Automated check: AC-HUD-15 style contrast pass over final colour values. |
| **V-05** Text contrast, subtitles and captions | Standard | Any subtitle or caption. Alpha (conditional) | Not Started | At least **7:1** on an opaque plate (Bone on Obsidian is expected to land well above this; verify on final values). No bare text over the world, ever (art bible 7.4). |
| **V-06** Colour-vision modes (protan, deutan, tritan) | Standard | HUD signal colours only (Turquoise, Cochineal, Marigold). VS | Not Started | Delivered as **three presets** (Protan, Deutan, Tritan) that select an *ally accent* (from V-07) and, where simulation shows a collision, an alternative for the Cochineal and Marigold UI colours. **No post-process filter on the world frame**, because the world carries no hue-coded information; an engine-level colour-vision filter is also NOT SOURCEABLE for 5.8 in `docs/engine-reference/unreal/**`. Candidate values for the danger and progress alternatives are not yet chosen (art-director; each must pass the three simulations and the world-palette collision test in art bible 4.7 before adoption). Presets change local rendering only and are never replicated (art bible 7.7 rule 3). |
| **V-07** Ally accent colour | Standard (elevated) | Teammate marker, allies only. VS | Not Started | Four options from art bible 4.7 / 7.7: (a) Turquoise `#0FC2B8` default, (b) warm white (art bible lists `#FFFFFF`, see F5), (c) magenta `#E454C8`, (d) ultramarine `#5B8CFF`. Pentagon, notch, 3 px keyline and the allies-only rule are unchanged. Each option must pass protan, deutan and tritan simulation against the world palette (4.2, 4.3) before adoption (VA-4 of the Teammate Markers GDD). Never replicated, so an accent can never become a player identifier. |
| **V-08** Color-as-only-indicator audit | Basic | All UI and gameplay. MVP | Not Started | Completed below. Every row already has a shape or text backup in the GDDs; the open work is *verification* (greyscale and CVD simulation tests, art bible 4.7 Tests 1 to 3). |
| **V-09** UI scale and text scale | Standard | All UI. VS (MVP honours hooks at 100 %) | Not Started | **UI scale 75 to 150 %** (default 100), **text scale 100 / 125 / 150 / 200 %** (HUD-R16, art bible 7.4). Plates grow with text and never truncate a message; names ellipsise at the plate width. The 2 px stroke, text floors, the 24 x 28 marker and ring minima never shrink below their 100 % values. **HUD scale and menu scale are separate controls** (the template asks for independence); the mechanism must be verified against UE 5.8.4 DPI scaling (`UUserInterfaceSettings`, ADR-0021 E5) [verify]. Layout must hold at 75 % and 150 % on 1920x1080, 1920x1200, 2560x1440 and 3440x1440 (AC-HUD-15). |
| **V-10** Ally marker scale | Standard (elevated) | Teammate marker. VS | Not Started | 100 or 150 % (`TeammateMarkerLargeMult`, TM-R12). Never reduced by UI or text scale; "shrink everything else before the marker" (art bible 4.8). |
| **V-11** High contrast mode | Comprehensive (elevated) | Menus and HUD. VS | Not Started | HUD: Bone Dim text and graphics promoted to Bone, keylines at 3 px on every coloured glyph (the marker already uses 3 px), ring and brace outlines thickened one step. Menus: every interactive element outlined, selection state never colour-only. Plates are already opaque. OS high-contrast detection is NOT SOURCEABLE for UE 5.8 in this repo; the setting is therefore manual. |
| **V-12** Display brightness and gamma | Basic | Global. VS | Not Started | A calibration screen with a barely-visible reference symbol. **Range limited to -25 % to +25 %** of default (the template's -50 to +50 would let a player lift shadows far past what the art direction allows; the Lighting Law makes shade deliberately bright, art bible 2.1). Gamma only: **no per-viewer exposure, shadow lift or contrast boost**, because that would change tell legibility between players (Pillar 1 fairness). Art-director to confirm the range (F15). |
| **V-13** Screen flash and strobe warning | Basic | First launch and settings. VS (warning), MVP (VFX budget) | Not Started | (1) A **first-launch photosensitivity notice** before gameplay. No GDD owns this yet (F8). (2) No signal element flashes; pulsing is capped at **0.5 Hz** and never saturated red (art bible 4.7, 7.6 rule 1). (3) The removal sequence (grade, plate, preempt exit, card, fade) is one budgeted sequence inside 3 luminance reversals per second and 10 % relative luminance change over a large area, checked with a photosensitivity analyser (PEAT or Harding FPA) before the Vertical Slice (AC-HUD-18, OQ-NKP-6). (4) **Reduce screen effects** is the flash-reduction mode (V-14). |
| **V-14** Reduce screen effects | Standard (elevated) | Per-viewer grades: assassination snap (2.7), concealment grade (2.8), informant grade (2.9), pursuit grade (2.6 if built). VS | Not Started | Art bible 7.6 rule 8: the 2.7 snap eases over 0.5 s, saturation drop capped at -30 %, vignettes disabled, ping exit stepped instead of faded. Default off with a warning on first launch. **Gap:** where a grade carries information (concealment, informant) a non-visual twin is required (7.6 rule 9); see F11. |
| **V-15** Reduce motion (UI) | Standard | All UI transitions. VS | Not Started | Every fade 0 ms, stepped entrances instant, no 8 px settle (art bible 7.6 rule 5; TM-R12 "fades 0 ms"). **Cannot be removed:** the cooldown sweep and interaction ring are real-time progress (linear motion by definition); under Reduce motion they remain, as a stepped fill if the user also selects "no sweep" (post-VS option). |
| **V-16** Reduce camera motion | Standard | Camera. MVP (options panel) | Not Started | CS-R13: toggle becomes a 0.08 s crossfade with no dolly, no traversal pivot lag, shoulder swap and boom recovery become cuts, FOV changes apply instantly. The body-driven view turn while steering stays (the body really turns). Changes no permission and nothing sent. There is no camera shake, FOV punch or zoom anywhere in the design (CS-R9). |
| **V-17** Field of view | Standard | Camera. MVP | Not Started | One slider per mode, 55 to 75 degrees vertical (shown as horizontal degrees), capped at 108 degrees horizontal for every aspect (CS-R13, F5). A wider FOV is the main comfort lever for motion-sensitive players, and it is already inside the fairness cap. Optional view bob defaults to 0 (VS). |
| **V-18** First or third person | Standard (inherent) | Camera. MVP | Not Started | The toggle (V / Y) is local, has identical permissions in both modes, and is never sent as a command (IIF R14, CS-R5). It is a free comfort and visibility option: a player who gets motion sick or loses track of their body in one mode can use the other at no competitive cost. Default is third person. |
| **V-19** Subtitles, on/off | Basic | Any spoken dialogue. Alpha (conditional) | Not Applicable (MVP) | No spoken dialogue, narration or authored VO in MVP ("no authored plot"). If voice is added: default **off**, offered at first launch. UE 5.8 Audio Subtitles is **Beta** (`docs/engine-reference/unreal/modules/audio.md`): verify before relying on it. |
| **V-20** Subtitles, speaker identification | Standard | Any spoken dialogue. Alpha (conditional) | Not Applicable (MVP) | Speaker name before the line; never colour-coded by hue alone (art bible 4.7). Must not identify an NPC versus a player (Pillar 1). |
| **V-21** Subtitle style customisation | Comprehensive | Not committed | Out of scope | See Known Intentional Limitations (LIM-V3). Two presets (default, high-readability) are the planned partial mitigation if voice ships. |
| **V-22** Sound-effect captions | Comprehensive (elevated for owner-only cues) | Every owner-only gameplay cue. VS | Not Started | Satisfied by construction for owner-only cues: each cue's **text twin is its own plate, card or line** (art bible 4.7, HUD accessibility paragraph). Captions for *diegetic world audio* are a separate decision (F9, LIM-A1). |

### Color-as-Only-Indicator Audit

> A resolved entry has a non-color backup that works in all three colorblind presets. Status "Specified" means the GDD or art bible already defines the backup; it becomes "Verified" only after the greyscale and simulation tests (Accessibility Test Plan, rows T-01 and T-02) pass.

| Location | Color Signal | What It Communicates | Non-Color Backup | Status |
|----------|-------------|---------------------|-----------------|--------|
| Teammate marker | Turquoise | "This figure is on my team" | Downward pentagon with a stepped notch (the only downward pentagon in the UI); 3 px Obsidian keyline; name label on focus or hold; accent presets (V-07) | Specified |
| Marker fill states | Turquoise fill vs hatch vs hollow | Teammate Active, Concealed, Informant | Solid, two-band stepped hatch, outline-only. Opacity encodes no state (art bible 7.x C1). Must separate in greyscale at 24 px (AC-TMK-21) | Specified |
| Wrong-target tally | Cochineal filled vs Bone outline squares | Wrong strikes so far | `L` stepped squares filled versus outlined; numeral line "Wrong targets: {n}"; static upward danger triangle at n at or above L-1; audio twin (tally tick, threshold tick) | Specified |
| Danger triangle | Cochineal | Finality or danger (tally threshold, "Wrong target", "Reversed", removal card) | Upward triangle with stepped base; every instance carries a text label | Specified |
| Result plate stamp | Stamp vs triangle vs none | Target exiled / Wrong target / No contact / Reversed | Four plate texts; stamp shape differs; distinct result cue id per outcome | Specified |
| Removal card | Cochineal Deep title bar | Own elimination | Title text "Exiled" or "Eliminated for the round", upward triangle, "You watch for your team now." | Specified |
| End banner | Bone bar (Victory) vs Cochineal Deep bar (Defeat) | Round outcome | The words "Victory" and "Defeat" plus the reason line | Specified |
| Interact ring | Bone vs Marigold | The tended court is one of my team's live sites | Four short outward ticks at the cardinal points on the live state, at least 3:1 luminance contrast against the plain ring (HUD-R13, ruling D6) | Specified |
| Objective steps | Marigold fill | Team progress | Discrete stepped bar, step count, labels "Your house" and "Rival house" (no "n/5") | Specified |
| Sighting ring and ping (VS) | Marigold | A place an informant saw something | Open broken ring on the ground; age shown by stepped stroke weight, never fade; screen-space minimum 32 px with keyline; edge indicator (art bible 7.x C3) | Specified |
| Brace indicator | Bone, no signal hue | Ready, Protected, Cooling | Outline vs inner square vs sweep plus the "Brace in m:ss" numeral and a persistent "Brace" label | Specified (no colour dependence) |
| Informant frame | Bone brackets | Own state is Informant | Four stepped corner brackets; "Informants can only observe" plate on a refused press | Specified |
| Concealed view | Per-viewer grade and leaf-edge vignette | The owner's own character is hidden in a bush | None specified beyond the camera being inside the foliage and the "Slip out" prompt. **Open: F11** | OPEN |
| Latency icon | Not specified | Own round-trip above `HudLatencyWarnRttSec` | Icon shape; colour must not be the only cue. Confirm in the HUD UX spec | Needs confirmation |

---

## Motor Accessibility

**Context from the game.** Default bindings (IIF R15): Move WASD / left stick, Look mouse / right stick, Run Left Shift / L3 **(hold)**, Traverse Space / A, Interact E / X **(hold, about 3 s)**, Prepare Q / LT **(hold, up to 4 s)**, Attack left mouse / RT **(press, irreversible)**, Hide F / B **(press)**, Camera toggle V / Y, Shoulder swap X (or middle mouse) / D-pad right. Two structural facts help: **the brace is planted** (no translation, ASN-R3) and **a tend is cancelled by commanded displacement** (IIF F6), so the two long holds never overlap with Move; and **nothing in the design is a rapid-input, rhythm or QTE input**.

| Feature | Target Tier | Scope | Status | Implementation Notes |
|---------|-------------|-------|--------|---------------------|
| **M-01** Full input remapping | Standard | Every gameplay and menu input, keyboard, mouse and gamepad. VS (MVP ships defaults; glyphs already follow bindings) | Not Started | Remap UI over `UEnhancedInputUserSettings` (ADR-0011 E4; `UPlayerMappableInputConfig` is deprecated and must not be used). Bindings are client-local and never replicated (TDC CR-14). Conflict warning on duplicate bindings; persist to profile; reset-to-default per action and global. **Gamepad remap is required at Standard** (IIF OQ10; the GDD currently defers it to VS, which this row confirms as a Release requirement). Reserve only the menu/pause key. |
| **M-02** Input method switching | Standard | PC. MVP | Not Started | Switching between keyboard-mouse and gamepad mid-session with no restart; every prompt shows the bound glyph for the active device (HUD-R12, HUD-R13, IIF UI-3). Common UI routing needs `UCommonGameViewportClient` (ADR-0021 E8). |
| **M-03** One-hand mode | Comprehensive | Not committed | Out of scope (audited) | Audit in the table below. Keyboard-mouse needs two hands for Move plus Look; a left-hand-only path is not feasible without a non-analog Look. Gamepad play is possible one-handed only with external adaptive hardware and full remap. See LIM-M1. |
| **M-04** Hold-to-press alternatives (toggle) | Standard | Run, Interact (Tend), Prepare (Brace). VS | Not Started | IIF R16: a client-local setting turns press/press into Begin/End. **The second press sends End**, so early release stays available and durations are identical; caps and cooldowns are unchanged; the server stream equals hold mode (gate `ToggleStreamParity`). Per-action toggles (not all-or-nothing). The 0.10 s server release debounce already merges a double-tap into one brace, and a double-tap never burns the 60 s cooldown (IIF edge case). The Hide, Traverse and Attack inputs are presses and need none. |
| **M-05** Rapid input alternatives | Standard | Any input over 3 presses per second | Not Applicable | No such input exists. A QA audit confirms none is introduced (test T-08). Attack is rate-limited server-side anyway (token bucket). |
| **M-06** Input timing adjustments | Standard | Timed inputs | Not Applicable (documented) | The only timed inputs are server-fixed rules: the 4 s Prepare window, the 60 s cooldown, the 3 s Interact duration and the 0.10 s release debounce. **None may be scaled**, because scaling would alter a defence or objective duration (IIF R16 guard; ASN-R3), which is a fairness change, not an accessibility one. The Standard tier's "no timed input that cannot be extended or toggled" is met through M-04 (the *hold* is toggleable; the *duration* is a game state, not a reflex window). See LIM-M2. |
| **M-07** Aim assist | Standard | Strike targeting | **Waived by pillar** | **Not offered**: anti-pillar "NOT a reflex game" and IIF R15, CS-R9 ("no aim assist ever"; no snapping, magnetism or slowdown). Granular **non-assist** aim comfort is in scope instead (CS-R13, CS-R14): look sensitivity split by X/Y and by mouse/stick, invert Y, stick dead zone and curve preset (1.0 / 1.5 / 2.0). The Strike aims along the camera-centre ray at a pawn in reach with no timing pressure. See LIM-M3. |
| **M-08** Auto-sprint and movement assists | Standard | Movement. VS | Not Started | Run toggle (M-04). Auto-walk (a toggle that holds Move) is **not committed**: it is a client-local translation to the same command stream and is likely legal under the IIF guard, but it needs a parity gate like `ToggleStreamParity` first (F7). |
| **M-09** Platforming and traversal assists | Standard | Traverse | Not Applicable | Traverse is one contextual press with a small set of motion-matched vaults and climbs; there is no precision jump, timing window or ledge-grab input to assist. |
| **M-10** HUD element repositioning | Comprehensive | Not committed | Out of scope | See LIM-M4: the HUD is a Pillar-1 layout contract (same layout for every player, centre kept empty, corner-area budget) and a fixed safe margin. UI scale, text scale and marker scale cover most of the need. |
| **M-11** Menus fully operable without a pointer | Standard | All menus. VS | Not Started | Every menu reachable and operable by keyboard alone and by gamepad alone; focus visible at all times (V-11 outlines); no hover-only information. Common UI input routing (ADR-0021 §3). |
| **M-12** Mis-press protection for Strike | Standard | Attack. Playtest | Open | One press costs a strike or a life and no confirm step is allowed (IIF OQ6). Mitigations in scope: remap Strike to any key or button (M-01) and a larger, more distant default key on request. A "strike on release" option is **not committed** (F7). |

**One-hand and simultaneity audit** (feeds M-03):

| Command | Hands needed on keyboard and mouse | Hands needed on gamepad | Can the action be done without Move held? |
|---|---|---|---|
| Move plus Look | Two (left WASD, right mouse) | Two sticks | n/a |
| Run | Left (Shift) while Move held; toggleable | L3 click while the stick is held (awkward; toggle recommended, see F7) | No, Run is a gait of Move |
| Interact (Tend) 3 s | One hand, stationary | One | Yes (and must be: movement cancels it) |
| Prepare (Brace) 4 s | One hand, stationary | One trigger held | Yes (planted by design) |
| Attack | Right (mouse) while Look is steady | RT | Yes |
| Hide, Traverse, Camera toggle, Shoulder swap | One key press | One button press | Yes |

---

## Cognitive Accessibility

**Context from the game.** The game's core skill is sustained visual attention across about 60 figures, and the design deliberately offers no assist for it (anti-pillar "NOT an identification-tools game"). The design instead keeps *everything else* quiet: the HUD shows only the owner's own legitimate knowledge, never opponent counts, elapsed time or timers other than the owner's own cooldown and respawn (HUD-R14), and its motion budget is slow and level (art bible 7.6).

| Feature | Target Tier | Scope | Status | Implementation Notes |
|---------|-------------|-------|--------|---------------------|
| **C-01** Difficulty options | Standard | All difficulty parameters | Not Applicable (documented) | Online PvP: every rule parameter (defence duration, cooldown, NPC count, tally limit) is a server-wide tuning value (TDC) that must be equal for all players. **Fixed, with justification: fairness.** A private-lobby *house rules* screen (host sets `NpcCount` and similar for everyone) is a proposed assist, owned by Lobby UI (system 19, VS); not committed (F14). A pausable, NPC-only practice mode is owned by Onboarding & Practice Mode (Alpha). |
| **C-02** Pause anywhere | Basic | All gameplay states | **Waived (documented)** | A 10-player server-authoritative round cannot pause. The menu/pause key opens an overlay that does not stop the world. Mitigations: menu reachable in one press; a player who leaves can rejoin within the rejoin window (Session & Travel); an eliminated player becomes an informant with no gameplay inputs, so stepping away after elimination costs nothing; **practice mode must support a true pause** (Onboarding, Alpha). See LIM-C1. |
| **C-03** Tutorial persistence | Standard | All hints and help text. Alpha (Onboarding), VS (help screen) | Not Started | Every one-time hint must be retrievable: the third-person hint (HUD-E17, shown once per profile), the first-round primer card ("Walk like them. Watch for who doesn't."), the controls list and the tell glossary all live permanently in a Help screen. Camera CS UI Requirements already require the 3P line in "the controls help and a loading tip". |
| **C-04** Objective clarity | Standard | Objectives cluster | Not Started | The current stage card (title, one line, clue "A court [ClueTag phrase]"), "Your house" (3 steps) and "Rival house" (2 steps) are **always on screen** while Active (HUD-E03 to E05), so objective text is zero presses away, not two. The clue is text. |
| **C-05** Visual indicators for audio-only information | Standard | All SFX carrying gameplay information | Not Started | See the SFX audit. Owner-only cues are all text-twinned. The one unresolved class is **diegetic world audio** (F9, LIM-A1). Directional indicators are prohibited (IVP R15). |
| **C-06** Reading time for UI | Standard | All auto-dismissing plates | **Deviation (documented)** | Standard says no actionable notification auto-dismisses in under 5 s. Current holds: result plate **3.0 s** (`HudResultPlateHoldSec`, range 1.0 to 6.0), rejection plate 1.5 s, informant plate 2.0 s, defence plate as result plate, third-person hint 4 s, removal card 4 s, feed line life per F8. Mitigations: `HudReadHoldMult` 1.0 to 2.0 exposed in settings and tied to text scale (HUD-R16, D11) gives 6 s on the result plate at 2.0; the persistent tally, objective cluster and informant frame record every outcome after the plate leaves, so nothing is lost; a rejection plate re-triggers on the next press. See LIM-C2. |
| **C-07** Cognitive load documentation | Comprehensive | Per game system | Partial | See the Per-Feature Accessibility Matrix, which counts the simultaneous items each system asks the player to track. A player tracks at most: own tally, brace state, two step bars, one stage card and up to three feed lines on the HUD (7 elements), against about 60 figures in the world. The HUD side is within the review trigger of 4 only by grouping (clusters); recorded as a review item for the HUD UX spec, not a defect. |
| **C-08** Navigation assists | Standard | World navigation | **Intentionally omitted** | **No waypoint, compass, arrow, distance read-out or map exists** (HUD-R11, OC-15; anti-pillar). Fast travel is meaningless (compact city, no travel mechanic). The clue text on the stage card is the only wayfinding aid. See LIM-C3. |
| **C-09** Motion-sickness and overload comfort | Standard | Camera and grades | Specified | V-14 to V-18 together; plus the 3P default and the free mode toggle. |
| **C-10** Plain, consistent language | Standard | All player-facing strings | Not Started | Short, concrete strings already in the HUD GDD; language rule bans "dead", "lives", "tracked", "spotted" and similar (IIF, Roster). The "Exiled" wording collision in HUD OQ5 must be resolved before text freeze. |

---

## Auditory Accessibility

**Context from the game.** Audio is deliberately sparse and diegetic ("dry, close, short, then silence"). **Nothing in the design plays a warning when a strike is aimed at you**, and there is no owner-only audio, haptic or visual cue of any kind before contact (ASN-R6), so a deaf player has no information deficit there by construction. All owner-only HUD cues are 2D UI-bus sounds that fire *with* their plate and are therefore redundant with a visible element.

| Feature | Target Tier | Scope | Status | Implementation Notes |
|---------|-------------|-------|--------|---------------------|
| **A-01** Subtitles for all spoken dialogue | Basic | All voiced content. Alpha (conditional) | Not Applicable (MVP) | No spoken dialogue in MVP. In-engine voice and chat are disabled; the MVP uses external voice (Session & Travel). If voiced content or intelligible NPC speech is added, 100 % coverage is required. |
| **A-02** Closed captions for gameplay-critical SFX | Comprehensive | Owner-only cues: VS. Diegetic world cues: undecided | Partial | Owner-only cues: satisfied by the plate twins. **Diegetic world audio: undecided**, see F9 and LIM-A1. |
| **A-03** Mono audio option | Comprehensive (elevated) | Global output. VS | Not Started | Fold to mono at the output stage so volume balance is preserved. Local only; it adds or removes no information (positional audio is already parity). The UE mechanism is NOT SOURCEABLE in `docs/engine-reference/unreal/**` (the audio module doc covers sound classes and submixes, not a mono downmix); verify before committing an implementation. |
| **A-04** Independent volume controls | Basic | Master, Music, SFX, Voice or Chat, UI buses. VS | Not Started | Four buses minimum (the template's Music / SFX / Voice / UI), default 80 %, range 0 to 100 %, persisted, exposed in settings and in the pause menu. Owner-only 2D UI cues sit on the **UI bus** (VA-HUD-6), so a player can mute them knowing every cue has a text twin. |
| **A-05** Visual representation of directional audio | Comprehensive | Off-screen audio events | **Prohibited (pillar)** | A screen-edge indicator for footsteps, strikes or voices would be a directional identification aid (Pillar 1; IVP R15; anti-pillar "NOT an identification-tools game"). Not offered. See LIM-A1. |
| **A-06** Hearing aid compatibility | Standard | High-frequency cues | Not Started | Audit every owner-only 2D cue for energy above about 4 kHz; each already has a visual twin, so the audit confirms no cue is high-frequency-only. Diegetic foley follows the same audit when Diegetic Audio (system 22) is written. |
| **A-07** Haptic twins | Exemplary (scoped) | Owner-only outcome cues on gamepad. VS or later | Not Started | Result, tally threshold and removal cues may have a short rumble twin, **outcome-uniform and never before the local contact frame** (ASN-R6, Assassination accessibility note). The UE 5.8 haptics API is NOT SOURCEABLE in this repo. |

### Gameplay-Critical SFX Audit

> Sources: HUD VA-HUD-6, art bible 4.7 and 7.3, Assassination VA-ASN-5, Crowd and NPC Behaviour VA, IVP L12. Diegetic Audio (system 22) and Event Feedback and Cues (system 23) are not written: this table is the requirement they must satisfy, and any cue they add is added here.

| Sound Effect | What It Communicates | Visual Backup | Caption Required | Status |
|-------------|---------------------|--------------|-----------------|--------|
| Result cue, one id per outcome (Target exiled, Wrong target, No contact, Reversed) | Outcome of my own strike | Result plate (HUD-E07) with distinct text and stamp | No: the plate is the caption | Specified |
| Defence result cue ("Strike reversed") | My brace reversed an attack | Defence plate (HUD-E08) | No | Specified |
| Tally tick and threshold tick | A wrong-target square filled; one strike from removal | Tally squares, numeral, danger triangle | No | Specified |
| Exile or removal cue | I have been removed from active play | Removal card (HUD-E10), informant frame | No | Specified |
| Stage tone | My team completed a counted stage | Stage card turns over, own step fills | No | Specified |
| Rival tick (team only) | A rival stage count rose | Rival step fills | No | Specified |
| Brace ready-again tone | The 60 s cooldown ended | Cooling numeral resolves to Ready outline | No | Specified |
| Brace protected and protection-ended cues (art bible 4.7) | Server confirmed my brace / it ended | Inner square appears / stepped exit | No | Specified |
| Rejection tone | A command was refused | Rejection plate ("Not now", "Busy", "Too far") | No | Specified |
| Teammate callout and sighting tick (VS, Informant Pings) | A teammate or informant called something | Feed line and sighting ring, stepped age | **Yes**, text line is the caption | Planned |
| Being targeted | Nothing, by design | None, by design (no cue of any kind) | No cue exists | Specified (ASN-R6, IVP R15) |
| Lunge foley, contact impact, body fall (positional world sound) | A strike happened near me (bystander information) | Visible only if the strike is in my frame | **Undecided** (F9, LIM-A1) | OPEN |
| Footsteps and crowd murmur (diegetic) | Ambience and parity | Not a decision channel by design; no unique footstep set for players (Pillar 1) | No | Specified |

---

## Platform Accessibility API Integration

> Rule applied: where this repo pins external facts (`docs/engine-reference/**`) a gap is **not filled from training data**. A row marked `NOT SOURCEABLE` has no sourced answer for UE 5.8.4 in `docs/engine-reference/unreal/**`; it is not a statement that the feature is absent. **Any NOT SOURCEABLE item that is unresolved keeps this document from advancing past "Committed"** (it cannot be marked Audited or Certified), and the Polish gate must treat it as NOT ASSESSED rather than as a pass.

| Platform | API / Standard | Features Planned | Status | Notes |
|----------|---------------|-----------------|--------|-------|
| Steam (PC) | Steam Input and Steam accessibility features | In-game remap (M-01) is the commitment; Steam Input is a free bonus | Not Started | **Storefront is TBD**, so no store-specific requirement applies yet. Sourcing for Steam Input behaviour is not in this repo. |
| PC input hardware | Enhanced Input user settings; Game Input for Windows (`GameInputWindows`, **Beta**) | Remap storage (M-01), device switching (M-02), adaptive-controller compatibility | Not Started | Sourced: `UEnhancedInputUserSettings` is the remap API (ADR-0011 E4); Raw Input is deprecated in 5.8 in favour of `GameInputWindows` (Beta) (`modules/input.md`). **NOT SOURCEABLE:** adaptive-controller behaviour under `GameInputWindows`. |
| PC (Screen Reader) | Windows Narrator, NVDA, JAWS via a UE accessibility layer | Menu navigation announcements (out of scope until verified) | NOT SOURCEABLE | `docs/engine-reference/unreal/**` has no coverage of UE's accessible-widget or screen-reader support in 5.8. A spike is required at Alpha before any commitment (F16). Until then, menus ship keyboard- and gamepad-complete (M-11) with visible focus. |
| PC (OS high contrast, text size) | Windows contrast themes, system text scale | Honour OS settings | NOT SOURCEABLE | No engine-reference coverage; the in-game settings (V-09, V-11) are the commitment. |
| PC (Colour-vision filter) | Engine post-process colour-vision simulation | Not used by design (V-06) | NOT SOURCEABLE | Irrelevant to the chosen approach, recorded so nobody assumes it exists. |
| Audio | Mono downmix, subtitles | A-03; V-19 | Partial | Sourced: Audio Subtitles **Beta** in 5.8 (`modules/audio.md`). **NOT SOURCEABLE:** a mono downmix mechanism. |
| UI scaling | `UUserInterfaceSettings` DPI scale rule, `UIScaleCurve`, `ApplicationScale` | V-09 | Sourced, needs spike | ADR-0021 E5 lists the fields; separate HUD and menu scale is [verify] (ADR-0021 UI-V2). |
| Xbox, PlayStation 5, iOS, Android | XAG, Sony, UIAccessibility, TalkBack | None | N/A | PC only. Revisit if a console port is ever scoped (the HUD text floors would then rise, HUD OQ14). |

---

## Per-Feature Accessibility Matrix

> One row per system in `systems-index.md` (30 systems, read 2026-10-25). "Addressed" is **Yes** (nothing outstanding), **Partial** (specified but an item is open or unbuilt), **No** (a concern is not yet handled) or **N/A (no player-facing surface)**. A system with a "No" cannot be marked Approved in the systems index.

| System | Visual Concerns | Motor Concerns | Cognitive Concerns | Auditory Concerns | Addressed | Notes |
|--------|----------------|---------------|-------------------|------------------|-----------|-------|
| 1 Tuning Data & Config | None directly; client-local settings are not tuning keys (CR-14) | None | None | None | N/A | Owns the rule that accessibility settings are client-local, never tuning. Holds the fairness values that make C-01 and M-06 non-adjustable. |
| 2 Participant State & Roster | Marker fill states derive from `State` | None | "Informant" role must be explained in Help | None | Partial | Language rule bans stealth wording; Help text owed (C-03). |
| 3 Input & Interaction Framework | Prompts must show bound glyphs | Three holds (Run, Interact 3 s, Prepare 4 s), one irreversible press (Attack) | Press-and-hold semantics | None | Partial | Toggle (R16), remap (OQ7, ADR-0011) specified; delivery at VS (M-01, M-04). Gamepad default Run on L3 hold is a concern (F7). |
| 4 Character Pool & Appearance | Uniform figures; no hue marks team or role (colour-vision safe by construction) | None | Reading 60 near-identical figures is the core load | Footsteps at parity | Partial | Cognitive load is the product; no assist is permitted (LIM-C4). |
| 5 Session & Travel | Loading and connection overlays are text on plates | None | Seamless travel removes menu navigation | In-engine voice and chat disabled (MVP) | Partial | External voice is the only team channel in MVP (F3). Overlays must respect text scale. |
| 6 Round & Match Framework | End banner text and bar | None | Briefing screen is neutral; no clock or opponent count | None | Partial | End banner has text plus bar (colour-independent). Briefing text must scale. |
| 7 Information Visibility Policy | Sets the "no directional or player-only information" guard (R15) | None | None | Sets audio parity (L12) | Yes | The reason A-05 and M-07 are prohibited. |
| 8 Level Socket Contract | Court fixtures and bush silhouettes must be readable with no hue dependence | None | None | None | Partial | Bush silhouette is a learnable shape (concept); verify at greyscale (T-01). |
| 9 Shared Character & Locomotion | First-person motion exposure; ground view at pitch limits | Run hold; Traverse press; walking turn-rate 180 degrees in about 1.8 s | Gait is a tell | Footsteps at parity | Partial | Reduce camera motion, FOV, 3P default (V-16 to V-18). Turn-rate comfort is a playtest item (Camera OQ3). |
| 10 Camera System | Motion, FOV, boom; no shake or zoom | Look sensitivity, dead zone, curve; no aim assist | One-time 3P hint | None | Partial | MVP options panel covers FOV, sensitivity, invert Y, dead zone, Reduce camera motion (CS-R13). |
| 11 Crowd & NPC Behaviour | Tells are motion and posture, not colour | None | Red herrings add load; "never a coin flip" | Reactions are motion only; no VO in MVP | Partial | No assist possible without breaking Pillars 1 and 2 (LIM-C4). |
| 12 Concealment (Bushes) | Concealed grade and leaf vignette carry state; hatched marker for allies | Hide is a press | Rule: no Prepare or Interact inside a bush | Concealed pawn audio culled | **No** | Owner's concealed state has no non-visual twin and the vignette is disabled by Reduce screen effects (F11). |
| 13 Assassination & Defence Reversal | Result and defence plates; strike hint; no reticle | Attack is a single irreversible press; Prepare is a 4 s hold with a 60 s cooldown | Anticipation-based, not reflex | World foley positional; owner cues 2D with plate twins | Partial | No warning cue by design (ASN-R6). Strike mis-press risk (M-12). World foley captions undecided (F9). |
| 14 NPC-Kill Penalty & Informants | Tally squares, triangle, removal card, informant grade and brackets | None | Wrong-target tally must be understood once | Tally ticks, removal cue, all twinned | Partial | Removal sequence needs photosensitivity analysis (V-13). Card holds 4 s and scales with `HudReadHoldMult`. |
| 15 Objectives Chain | Ring colour (Bone vs Marigold) backed by four ticks; step bars | Interact is a 3 s stationary hold | Clue is text; no waypoint | Stage tone twinned | Partial | Toggle for Interact at VS (M-04). Navigation omission documented (LIM-C3). |
| 16 Procedural City Generator | Layout varies per round; no recolour of hue-coded info | None | Fresh layout each round raises wayfinding load | None | N/A | No UI. Wayfinding effect noted under C-08. |
| 17 Teammate Markers | Turquoise marker on bright ground; keyline 3 px; three fill states | None (markers are not focusable) | Three states to learn | None; callouts need a caption (VS) | Partial | Accent presets, 150 % scale, shape plus fill (V-07, V-10). Simulation sign-off outstanding (F5). |
| 18 HUD | All HUD-R16 floors, contrast, scales | Prompts follow bindings | Seven concurrent groups; slots drop rather than queue | All cues twinned | Partial | Matrix home for V-03, V-04, V-09. Result-plate hold below 5 s is a documented deviation (LIM-C2). |
| 19 Lobby UI (VS) | Menu text 24 px floor, high contrast | Pointer-free operation required | House rules could act as an assist (F14) | None | No (Not Started) | Design must cite M-01, M-11, V-01. |
| 20 Informant Pings & Team Comms (VS) | Sighting ring: shape plus stepped weight, 32 px minimum | Ping input must be remappable | Pings are places, not people | Callout cue needs a caption | No (Not Started) | **Highest-priority gap**: a text-capable, accessible team channel (F3). CVAA applies when chat ships. |
| 21 Post-Round Reveal & Results (VS) | Reveal rows, staggered 500 ms; no colour-only roles | None | Many rows at once | None | No (Not Started) | Reveal must respect Reduce motion and text scale. |
| 22 Diegetic Audio (VS) | None | None | None | Defines the world-audio information problem | No (Not Started) | Must resolve F9 and add every cue to the SFX audit. |
| 23 Event Feedback & Cues (VS) | Cue twins | Haptic twins optional (A-07) | None | Owns cue ids and timing | No (Not Started) | Every cue it adds needs a text twin and a row in the SFX audit. |
| 24 Settings & Accessibility (VS) | Owns V-06 to V-15 settings | Owns M-01, M-04 | Owns help and read-time setting | Owns A-03, A-04 | No (Not Started) | **This document is its requirements input.** Its GDD must map every row here to a setting key and resolve F1. |
| 25 Onboarding & Practice Mode (Alpha) | Hint plates | None | Owns C-03 and a pausable practice mode | None | No (Not Started) | Must support true pause (C-02). |
| 26 Telemetry & Analytics (Alpha) | None | None | None | None | N/A | Should log which accessibility settings are on (no personal data) to size the audience. |
| 27 Anti-Cheat & Info-Leak Security (Alpha) | None | None | None | None | N/A | Must treat accessibility settings as client-local and never as a leak channel. |
| 28 Dedicated Server (Alpha) | None | None | None | None | N/A | No player surface. |
| 29 Matchmaking (Full Vision) | Menu text | Pointer-free | Wait-state text | None | No (Not Started) | Add a row to the test plan when designed. |
| 30 Progression & Cosmetics (Full Vision) | **Any cosmetic must not create a hue-coded identifier** | None | None | None | No (Not Started) | Cosmetics enter the NPC appearance deck at equal weight or are cut (IVP R15; CPA). |

---

## Accessibility Test Plan

> Contrast and size checks are automated where a logic test can compute them (AC-HUD-15, AC-TMK-17, AC-TMK-21). Simulation and user-testing rows are manual and cannot be run headlessly. Evidence for the visual rows is a **retained screenshot** in `production/qa/evidence/` per the testing standard.

| Feature | Test Method | Test Cases | Pass Criteria | Responsible | Status |
|---------|------------|------------|--------------|-------------|--------|
| **T-01** Greyscale legibility (art bible 4.7 Test 1) | Manual: greyscale screenshots of every HUD state | Ally, danger, tally, sighting, progress, brace, ring states; marker fills at 24 px on brightest and darkest ground | A tester identifies each semantic element with **100 %** accuracy | ux-designer | Not Started |
| **T-02** Colour-vision simulation (art bible 4.7 Tests 2 and 3) | Manual: protan, deutan, tritan simulation (Coblis or equivalent) of the same screens, each ally accent and each preset | All semantic pairs; signal colours against world palette (cochineal vs Tezontle, turquoise vs Canal Green, marigold vs ochre, Causeway Dust, market goods) | No pair of semantic elements confused; no signal colour confused with a world surface in any simulation | art-director with ux-designer | Not Started |
| **T-03** Text contrast and floors | Automated logic test over final style values (AC-HUD-15 style) | All text and graphics at 1920x1080, 1920x1200, 2560x1440, 3440x1440; UI scale 75, 100, 150 %; text scale 100 and 200 % | Body text at least 4.5:1, large text and graphics at least 3:1, caption plates at least 7:1; floors 18 / 22 / 28 px honoured; no truncated message | ui-programmer | Not Started |
| **T-04** Remapping | Manual and logic: rebind every action to a non-default key, then complete a round | All gameplay and menu actions, keyboard, mouse, gamepad | All actions reachable; duplicate-binding warning works; bindings persist across restart; glyphs follow | qa-tester | Not Started |
| **T-05** Hold toggles | Logic gate `ToggleStreamParity` plus manual | Scripted press/press versus press/release timings for Run, Interact, Prepare; double-tap within 0.10 s | Identical server command stream; no cooldown burned by a double-tap; every hold completable in toggle mode | gameplay-programmer, qa-tester | Not Started |
| **T-06** Reduce motion, camera motion, screen effects | Manual with each setting on, all combinations | Menus, HUD, toggle, traversal, removal sequence, assassination snap | No fade above 0 ms under Reduce motion; no dolly or lag under Reduce camera motion; no vignette, snap eased, saturation drop at most 30 % under Reduce screen effects | ux-designer | Not Started |
| **T-07** Photosensitivity | Analyser (PEAT or Harding FPA) on captured sequences (AC-HUD-18) | Exile and assassinated-victim removal sequences; assassination snap | At most 3 luminance reversals per second and 10 % relative luminance change over a large area, as one sequence | technical-artist | Not Started |
| **T-08** No rapid or timed input audit | Manual input audit | List every input in IIF R15 | No input needs more than 3 presses per second; every hold has a toggle; no input window scales | qa-lead | Not Started |
| **T-09** Text twin coverage | Manual cue audit | Every row of the SFX audit with audio muted | The player can act correctly on every owner-only cue from the visual twin alone | qa-tester | Not Started |
| **T-10** Pillar guard (no information added by any setting) | Logic diff: view-model and widget tree with each accessibility setting on versus off | All settings in this document | The set of facts shown is identical; settings change only size, colour (local), timing of presentation, or effects | gameplay-programmer | Not Started |
| **T-11** Pointer-free menus | Manual | Every menu with keyboard only and gamepad only | Every control reachable and operable; focus always visible | qa-tester | Not Started |
| **T-12** Screen-reader (conditional on F16) | Manual with an OS screen reader | Main menu, settings, lobby | Announcements and logical order, or the row is recorded NOT ASSESSED | ux-designer | NOT ASSESSED (engine support unsourced) |
| **T-13** User testing, colour-vision deficiency | User testing | A full session with each colour-vision preset | No session-stopping confusion | producer | Not Started |
| **T-14** User testing, motor and low-vision | User testing with players using toggles, remap, 150 to 200 % scale | A full session | Participants complete a round with no help | producer | Not Started |

---

## Known Intentional Limitations

> Every entry is a risk. "Approval" says who owns reversing it.

| Feature | Tier Required | Why Not Included | Risk / Impact | Mitigation |
|---------|--------------|-----------------|--------------|------------|
| **LIM-A1** Visual or caption equivalent for diegetic world audio (off-screen lunge foley, impact, fall; footsteps) | Comprehensive | A directional or distance indicator is an identification aid (Pillar 1, anti-pillar, IVP R15). The positional strike audio is a bystander channel that hearing players get for free. | **Medium.** A deaf or hard-of-hearing player misses an off-screen "something happened nearby" that a hearing player gets. | Strikes in the frame are visible; no decision depends on audio alone (no warning cue exists, ASN-R6). **Proposal for a creative-director ruling (F9):** a non-directional, range-bounded caption such as "[A struggle nearby]" that fires only for sounds the player would hear and gives no direction, identical for every kind of victim. |
| **LIM-V1** Screen reader for the in-game world | Exemplary | Reading a figure's gait aloud would identify tells and enemies (Pillar 1). | Blind players cannot play the core loop. This is a genre limit. | Menus and text surfaces aim to be screen-reader-friendly (conditional on F16). State plainly in the store page that the game is visual. |
| **LIM-V2** Screen reader for menus | Comprehensive | UE 5.8 support is NOT SOURCEABLE in this repo. | Blind players cannot navigate menus unaided. | Keyboard- and gamepad-complete menus with visible focus; spike at Alpha (F16). |
| **LIM-V3** Full subtitle customisation | Comprehensive | No spoken dialogue in MVP. | Affects only a later voiced build. | Two presets (default and high-readability) if voice ships. |
| **LIM-M1** One-hand mode | Comprehensive | Move plus Look needs two analog inputs. | Single-handed players need external adaptive hardware. | Full remap on all devices, toggles for every hold, Strike remappable to any input. |
| **LIM-M2** Input timing multiplier | Standard | Every timed value is a server-wide fairness rule (defence 4 s, cooldown 60 s, interact 3 s, debounce 0.10 s). | Players who need longer than a 4 s hold or a precisely released brace use the toggle instead. | Toggle (M-04): second press ends the hold, so early release is possible with one extra press. |
| **LIM-M3** Aim assist | Standard | Anti-pillar "NOT a reflex game"; IIF R15 and CS-R9 forbid it. | Players with tremor or low precision find the camera-centre Strike harder. | Sensitivity, dead zone, curve, invert Y; reach is generous and untimed; Strike remappable. Revisit with data from the first playtest if hit rates split by input device. |
| **LIM-M4** HUD repositioning | Comprehensive | The HUD is a Pillar-1 layout contract: same layout for every player, centre empty, corner-area budget, fixed safe margin (art bible 7.1, 7.7 rule 1). | Players with a restricted field of view cannot move plates. | UI scale 75 to 150 %, text scale to 200 %, marker scale to 150 %. |
| **LIM-M5** "Strike on release" and auto-walk | Standard (optional) | Each is a client-local translation that needs a parity gate before it can be called safe (F7). | Some motor users would benefit. | Candidates for after the Vertical Slice. |
| **LIM-C1** Pause anywhere | Basic | Server-authoritative 10-player round. | A player who must stop leaves the round. | One-press menu; rejoin window; informant state removes cost after elimination; **true pause in practice mode**. |
| **LIM-C2** Plate holds under 5 s | Standard | Result plate 3.0 s and rejection and informant plates 1.5 to 2.0 s are tuned so a plate does not cover the next strike hint or delay the end banner (HUD tuning knob notes). | Slow readers may miss a plate. | `HudReadHoldMult` to 2.0; persistent tally, step bars and informant frame record every result. |
| **LIM-C3** Navigation assists | Standard | No waypoint, compass or map (OC-15; anti-pillar). | Players with spatial-memory or wayfinding difficulty have only a text clue. | Clue always on screen; compact city; consider a re-readable clue log (not committed). |
| **LIM-C4** Cognitive assists for reading the crowd | Exemplary | The crowd reading *is* the game; any highlight is an identification aid. | The core loop is cognitively demanding and cannot be softened per player. | House-rules lobby settings (crowd size) that apply to everyone, proposed (F14); a pausable NPC-only practice mode (Alpha). |
| **LIM-X1** Third-party accessibility audit | Exemplary | No budget. | Unvalidated by disabled players until user testing. | Test rows T-13 and T-14; AbleGamers Player Panel as a later option. |

---

## Audit History

> No audit has been performed. This document was authored from the GDDs and art bible in an unattended run; nothing here has been implemented or tested.

| Date | Auditor | Type | Scope | Findings Summary | Status |
|------|---------|------|-------|-----------------|--------|
| 2026-10-25 | Internal, ux-designer (autonomous) | Authoring review against the six input documents (concept, art bible, HUD, IIF, Teammate Markers, Camera) | Requirements only; no build exists to audit | 16 follow-ups recorded below (F1 to F16); 2 cross-document conflicts (F2, F3), 1 art-bible inconsistency (F5), 1 requirement gap with no owner (F8), 1 information-channel conflict needing a creative ruling (F9, F11) | Open |

---

## External Resources

| Resource | URL | Relevance |
|----------|-----|-----------|
| WCAG 2.1 (Web Content Accessibility Guidelines) | https://www.w3.org/TR/WCAG21/ | Contrast ratios and text resizing used for V-03 to V-05, V-09 |
| Game Accessibility Guidelines | https://gameaccessibilityguidelines.com | Working checklist for Basic and Intermediate items |
| AbleGamers Player Panel | https://ablegamers.org/player-panel/ | Future user testing (T-13, T-14) |
| Xbox Accessibility Guidelines (XAG) | https://docs.microsoft.com/gaming/accessibility/guidelines | Reference checklist only (PC target) |
| PlayStation Accessibility Guidelines | https://www.playstation.com/en-us/accessibility/ | Reference only |
| Colour Blindness Simulator (Coblis) | https://www.color-blindness.com/coblis-color-blindness-simulator/ | T-02 simulation |
| Accessible Games Database | https://accessible.games | Prior art |
| CVAA | https://www.fcc.gov/consumers/guides/21st-century-communications-and-video-accessibility-act-cvaa | Triggered when in-game chat or voice ships (F3) |
| Photosensitive Epilepsy Analysis Tool (PEAT) and Harding FPA | https://trace.umd.edu/peat/ | T-07 photosensitivity analysis |
| UE 5.8 release notes | https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes?application_version=5.8 | Source for Audio Subtitles (Beta) and Game Input for Windows (Beta) |

---

## Open Questions / GDD follow-ups

> No GDD was edited. Items marked **CONFLICT** are contradictions between documents; **GAP** is a requirement with no owner; **RULING** needs a creative or user decision.

| ID | Type | Question or follow-up | Owner | Deadline | Resolution |
|----|------|-----------------------|-------|----------|-----------|
| F1 | GAP | Settings & Accessibility (system 24) has no GDD. It must map every V-, M-, C-, A- row to a client-local setting key. Three "reduce" settings exist under different names: **Reduce motion** (HUD-R16, TM-R12, art bible 7.6 rule 5: UI fades and steps), **Reduce camera motion** (CS-R13), **Reduce screen effects** (art bible 7.6 rule 8: grades). Recommendation: keep them as three toggles plus one convenience "Reduce all motion" that sets all three. | game-designer, ux-designer | Before the Vertical Slice | Unresolved |
| F2 | CONFLICT | Plate holds (result 3.0 s, rejection 1.5 s, informant 2.0 s) are below the Standard-tier 5 s guideline for actionable notices; the HUD tuning notes argue against longer holds. Recorded as LIM-C2 with `HudReadHoldMult` as the mitigation. The HUD GDD should state the 5 s deviation and its rationale so a reviewer does not read it as an oversight. | ux-designer, systems-designer | HUD re-review | Unresolved |
| F3 | CONFLICT, GAP | The concept (Pending Decision 4) says informant communication is "pings + team chat", but Session & Travel disables in-engine voice and chat in the MVP (external voice only), and Informant Pings is a Vertical Slice system. A deaf, hard-of-hearing or non-speaking player has **no team channel** in the MVP. CVAA duties apply the day a chat or voice channel ships. A text channel and a pre-set ping set must be built accessible from the start. | game-designer, network-programmer | Before the Vertical Slice | Unresolved |
| F4 | GAP | High-contrast mode is a setting hook in HUD-R16 and TM-R12 but neither GDD defines what it changes in the HUD beyond "keylines already 3 px". V-11 proposes a definition; the HUD UX spec must confirm it. | ux-designer | HUD UX spec | Proposed |
| F5 | CONFLICT (art bible internal) | Art bible 4.7 and 7.7 list the ally accent "(b) warm white `#FFFFFF`". `#FFFFFF` is neutral white, not warm, and the palette's off-white is Bone `#EFE8D6`. Decide which is meant; a pure white may also be confused with the Bone plate text. All four accents still need protan, deutan and tritan simulation before adoption. | art-director | Before V-07 ships | Unresolved |
| F6 | NOTE | User decisions 2026-10-25 declined a tap or hold camera toggle and left-handed shoulder default for the MVP (Camera OQ12, IIF OQ1). This document schedules both for the Vertical Slice and does not contradict the decision. | user | n/a | Consistent |
| F7 | RULING | (a) Gamepad default Run is an L3 **hold** while the left stick is held, which is awkward; consider a gamepad default of toggle or another button. (b) Auto-walk and "strike on release" need a parity gate (like `ToggleStreamParity`) before they can be called safe under IVP R15. | game-designer, ux-designer | VS design | Unresolved |
| F8 | GAP | No document owns the **first-launch photosensitivity and Reduce screen effects notice**. HUD says it "belongs to the front end"; art bible 7.6 says "a warning is shown on first launch"; Lobby UI and Settings are unwritten. Assign to Settings & Accessibility plus a boot-screen story. | producer | Before the Vertical Slice | Unresolved |
| F9 | RULING | Off-screen diegetic strike audio is a bystander information channel that a deaf player lacks (LIM-A1). Decide between (a) accept the asymmetry, or (b) a non-directional, range-bounded, kind-blind caption. Must satisfy the Pillar-1 design test and IVP R15 before adoption. | creative-director | Diegetic Audio GDD | Unresolved |
| F10 | GAP | Audio Subtitles in UE 5.8 is **Beta** (`modules/audio.md`). If voice or captions are built on it, verify stability on 5.8.4 first. | ue-umg-specialist | Before any VO | Unresolved |
| F11 | GAP | The owner's **concealed** state is shown only by a grade and a leaf-edge vignette (Concealment UI-2: "no icon, meter, timer"). Reduce screen effects **disables vignettes**, and art bible 7.6 rule 9 requires a non-visual twin for any grade that carries information. As written, a player with that setting on has no cue that they are concealed other than the camera being inside foliage. Options: a quiet owner-only 2D cue, or accept that the "Slip out" prompt and the view suffice. | art-director, audio-director | Concealment re-review | Unresolved |
| F12 | NOTE | The brace indicator, cooldown sweep and interaction ring are real-time progress and cannot be removed by Reduce motion. A "stepped, no sweep" option is listed as a post-VS candidate. | ux-designer | Post-VS | Open |
| F13 | GAP | Art bible 7.4 sets HUD, plate and reveal sizes but no menu type sizes. V-01 sets 24 px menu body at 1080p; art-director to confirm it fits the menu design language. | art-director | Menu UX specs | Proposed |
| F14 | RULING | Private-lobby house rules (host sets `NpcCount` and similar for everyone) could act as a cognitive-load assist without a per-player advantage. Needs a decision from the Lobby UI GDD and a check against tuning fairness (TDC). | game-designer | Lobby UI GDD | Unresolved |
| F15 | RULING | Brightness and gamma range of -25 % to +25 % (V-12) is a proposal; the Lighting Law in art bible 2.1 may justify a narrower or wider range. | art-director | Settings GDD | Proposed |
| F16 | GAP | UE 5.8 accessibility support is **not covered** by `docs/engine-reference/unreal/**`: runtime screen-reader or accessible-widget support, OS high-contrast and text-size detection, a mono downmix mechanism, the haptics API, and adaptive-controller behaviour under `GameInputWindows`. Run a documented research or spike task and add the findings to the engine reference **before** any Platform API row advances past NOT SOURCEABLE. Until then T-12 is NOT ASSESSED. | technical-director | Alpha | Unresolved |
