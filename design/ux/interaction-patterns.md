# Interaction Pattern Library

> **Status**: In Design (authored in an unattended run, 2026-10-25; not yet reviewed with `/ux-review`)
> **Author**: ux-designer (autonomous run; no user session)
> **Last Updated**: 2026-10-25
> **Version**: 0.1
> **Engine**: Unreal Engine 5.8 (installed 5.8.4)
> **UI Framework**: UMG widgets driven by a plain-C++ view-model, one custom Slate leaf for the world-anchored layer, Common UI for menus (ADR-0021)
> **Platform / Input**: PC (Windows); keyboard and mouse primary, gamepad partial (`.claude/docs/technical-preferences.md`; `project.yaml` has no `platform:` block)
> **Related Documents**:
> - `design/art/art-bible.md` — visual standards (3.4 shape grammar, 4.6 palette, 4.7 colour-blind safety, 7.1 to 7.7 HUD)
> - `design/accessibility-requirements.md` — accessibility commitments (tier: Standard; row ids V-, M-, C-, A- are cited below)
> - `design/gdd/hud.md`, `design/gdd/input-interaction-framework.md`, `design/gdd/teammate-markers.md`, `design/gdd/camera-system.md` — the sources of nearly every pattern here
> - `docs/architecture/adr-0021-ui-hud-layering-commonui.md` — layer order and widget rules the patterns must obey
> - `design/ux/[screen].md` — screen specs that reference these patterns. **None exist yet** (`design/ux/` held no file when this library was written), so every "Used In" entry names a GDD element id or a planned spec, and each is marked "(planned)".
>
> **Template**: Interaction Pattern Library

> **Why this document exists**: Every UI screen spec should be able to say "uses Result Plate pattern" rather than re-specifying entrance timing, slot priority, text twin and accessibility from scratch. This library is the single source of truth for reusable interaction behaviours. When a screen spec references a pattern name, the programmer looks it up here. When the behaviour changes, it changes here and applies everywhere.
>
> This is a living document. Do not design a new interaction without checking here first. If a new pattern is needed, add it here before writing the first screen spec that uses it.
>
> **Status definitions**:
> - **Draft**: Interaction specified but not yet implemented or validated
> - **Stable**: Implemented, tested, and validated in at least one shipped screen
> - **Deprecated**: Being phased out; do not use in new screens
>
> Every pattern below is **Draft**. Nothing in this project has been built to the UI stage.

---

## Overview

### What is different about this library

Most pattern libraries describe how a control feels. In Smoking Mirror the dominant risk is not feel, it is **leakage**: the interface is the one layer that is identical in structure for every player, so any element that appears, moves, sounds or is timed differently depending on *who is in front of you* would identify an enemy and break Pillar 1 ("The game never tells you who an enemy is"). The patterns are therefore written around six laws that every pattern inherits. A pattern that cannot obey them is not added to the library.

| Law | Statement | Source |
|-----|-----------|--------|
| **L1 Subject-kind parity** | Before the owner's own result is due, the view-model and widget tree are identical whether the figure in front of the viewer is an NPC, an enemy player or an informant. Afterwards only the result plate text and stamp differ, and never between an NPC and an informant victim. | HUD-R2, HUD-R3, art bible 7.1 |
| **L2 No warning, no early outcome** | No widget, sound, haptic or layout change precedes the local contact frame of a strike aimed at the viewer. Outcome-caused owner data is held and presented at the viewer's own contact or fall, never on receipt. | ASN-R6, HUD-R6a |
| **L3 Self-only means not sent** | Private state (tally, cooldown, result, removal card) is rendered from the owning client's own data and sent to no one else. The same layout, fonts and animation apply to every player. | art bible 7.7 rules 1 and 2 |
| **L4 No signal by hue or opacity alone** | Every colour has a shape or fill and, where urgent, a non-visual twin. Opacity is never a state; plates are 100 % opaque; fades are transitions only. Turquoise means "ally" and nothing else; Cochineal means danger or finality and nothing else. | art bible 3.4, 4.7, 7.2 |
| **L5 Slow, level, quiet** | One-shot stepped entrances for urgency, no loop faster than 0.5 Hz, no pulse, scale-pop, shake, bounce or overshoot. Real-time progress sweeps are the only linear motion. | art bible 7.6 |
| **L6 Settings never add information** | Every accessibility setting is client-local and changes size, local colour, timing of presentation or effects, never the set of facts shown. | IVP R15, accessibility doc |

### Layout and layer facts every pattern depends on

- **Safe margin 5 %** (96 x 54 px at 1080p). Anchors: top-centre objectives, bottom-left brace indicator and tally, top-right team feed, lower-centre column (Result slot centred at 70 % height, Notice slot below it), four frame corners for informant brackets. **The screen centre is always empty** of persistent elements at every text scale. Persistent corner area at most 8 % of the screen at 100 % scales (HUD-R5, F1).
- **Layers** (ADR-0021 §1): L0 world-anchored 10, fade 20, L1 persistent plates 30, L2 lower-centre 40, L3 cards and banners 50, L4 status overlays 60, menus 100. Every layer below 100 is hit-test-invisible, has no input bindings and is never a Common UI activatable. `AddToViewport` is banned for game UI.
- **One layout, no reflow**: an element that does not apply is collapsed and its slot is never reused by a different element. The two shared transient slots (Result, Notice) are the only exception (HUD-R5).
- **Widgets are dumb**: a plain-C++ presenter produces a view-model; widgets apply it. No UMG property bindings, no widget animation assets, no Blueprint graph logic; all timelines are sampled from the presenter clock (ADR-0021 §4).
- **Palette** (art bible 4.6): Obsidian Ink `#1C1917` plate (opaque), Obsidian Raised `#2B2622`, Bone `#EFE8D6`, Bone Dim `#B9B09C`, Turquoise `#0FC2B8`, Cochineal Bright `#E04A62` (never text below 28 px), Cochineal Deep `#B01E3C` (on Bone or as a bar), Marigold `#E3A72F`. Stepped 4 px corners, 2 px stroke, 2 px Obsidian keyline on coloured glyphs (3 px on the marker and ping).

### How to Use This Library

**If you are designing a screen**: Browse the Pattern Catalog Index before inventing a new interaction. Reference patterns by name (for example "uses Result Plate and Slot Priority"). When none fits, propose a new one here first.

**If you are implementing a screen**: Find the pattern for the complete specification. The Implementation Notes carry the UE 5.8 guidance; the Accessibility lines are non-negotiable.

**If you are reviewing a screen spec**: Verify every interactive or informational element references a pattern here or carries a full specification. "Standard button" is not a reference. Also verify the spec against the six laws: any element that depends on a non-ally figure, or on being targeted, fails review.

**If you are updating a pattern**: Changing a Stable pattern affects every screen that uses it. Audit usages, update this document before or with the code change, and log the change in the Catalog "Used In" column.

---

## Pattern Catalog

Patterns are grouped by what they are for, not by widget type. Groups: **FND** foundations, **WLD** world-anchored (layer L0), **PLT** persistent plates (L1), **TRN** transient slots (L2), **CRD** cards, banners and overlays (L3, L4), **FBK** full-screen feedback, **MNU** menu controls and navigation (layer 100; Vertical Slice except the MVP host/join menu).

### Pattern Catalog Index

> Add a row every time a pattern is added. Update "Used In" when a screen adopts a pattern. HUD element ids (HUD-Exx) come from `design/gdd/hud.md`; "(planned)" marks a screen spec that does not exist yet.

| Pattern Name | Category | Description | Used In | Status |
|-------------|----------|-------------|---------|--------|
| **PAT-FND-1** Opaque Stepped Plate | Foundation / Layout | The one container: opaque Obsidian, stepped 4 px corners, 2 px stroke, text on plate only. | Every plate, card and menu panel; `hud.md` (planned) | Draft |
| **PAT-FND-2** Bound-Input Glyph | Foundation / Input | Key glyph plate showing the currently bound key or button for the active device. | Context prompt, strike hint, brace label, third-person hint, menu hints; `hud.md` (planned) | Draft |
| **PAT-FND-3** Signal Glyph Set | Foundation | The shape vocabulary: down pentagon (ally), up triangle (danger), square (tally, stamp), circle (duration or place), stepped bar (progress). | All signal elements | Draft |
| **PAT-FND-4** Slot Priority and Drop | Foundation / Feedback | Transient slots hold one message; a lower-priority arrival is dropped, never queued. | Result slot, Notice slot; `hud.md` (planned) | Draft |
| **PAT-FND-5** Contact-Gated Presentation | Foundation / Feedback | Outcome-caused presentation is held until the viewer's own contact frame, fall or removal. | Result plate, defence plate, brace End, rejection replies, removal card, hold cancel | Draft |
| **PAT-WLD-1** Ally Marker | Game-Specific / World | Down-pentagon marker above an ally; filled, hatched or hollow; edge-clamped variant. | HUD-(17); `hud.md` (planned) | Draft |
| **PAT-WLD-2** Ally Name Label | Game-Specific / World | Opaque name plate beside a marker, by focus or by hold. | HUD-(17), TM-R11 | Draft |
| **PAT-WLD-3** Context Action Prompt | Game-Specific / World | Bound glyph plus verb anchored to a court fixture or bush, never a person. | HUD-E13 | Draft |
| **PAT-WLD-4** Predicted Hold Ring | Game-Specific / World | Linear progress ring at the interacted object, predicted from the press, clamped at 0.98. | HUD-E12 | Draft |
| **PAT-WLD-5** Sighting Ping (VS) | Game-Specific / World | Static open ring on the ground with stepped stroke weight for age. | Informant Pings (planned) | Draft |
| **PAT-PLT-1** Brace Indicator | Game-Specific / Ability | Ready, pending, Protected, End and Cooling states of the defence; never closes or flashes. | HUD-E01 | Draft |
| **PAT-PLT-2** Wrong-Target Tally | Game-Specific / Resource | Stepped squares plus a numeral line plus a threshold triangle. | HUD-E02 | Draft |
| **PAT-PLT-3** Step Bar and Stage Card | Feedback / Layout | Discrete stepped progress (own 3, rival 2) with a stage card under it. | HUD-E03, E04, E05 | Draft |
| **PAT-PLT-4** Team Feed Line | Feedback | Up to three plain lines, top-right, allies only. | HUD-E06 | Draft |
| **PAT-PLT-5** Informant Frame | Game-Specific | Four stepped corner brackets for the informant state. | HUD-E09 | Draft |
| **PAT-PLT-6** Latency Icon | Feedback | Owner-only icon above the RTT threshold, with hysteresis. | HUD-E19 | Draft |
| **PAT-TRN-1** Result Plate | Feedback | Outcome of the viewer's own strike or reversal; one timeline for every outcome. | HUD-E07, E08 | Draft |
| **PAT-TRN-2** Rejection Plate | Feedback | Short refusal text beside the brace indicator, rate-limited per code. | HUD-E15 | Draft |
| **PAT-TRN-3** Strike Hint | Game-Specific / Input | Screen-fixed "[glyph] Strike", identical for every rendered non-ally in reach; no reticle. | HUD-E14 | Draft |
| **PAT-TRN-4** One-Time Hint | Feedback / Onboarding | Once-per-profile plate that must also live in Help. | HUD-E17 | Draft |
| **PAT-TRN-5** Returning Countdown | Feedback | Whole-second plate on the neutral view while an informant awaits a body. | HUD-E11 | Draft |
| **PAT-CRD-1** Removal Card | Modal / Overlay | One-time central band on own removal; preempts the plate; collapses into the informant frame. | HUD-E10 | Draft |
| **PAT-CRD-2** End Banner | Overlay | Victory or Defeat bar, one reason line, a match-score line. | HUD-E18 | Draft |
| **PAT-CRD-3** Status Overlay | Feedback / Modal | Connection and loading text, owned by Session, hosted on L4. | HUD-E20; Session (planned) | Draft |
| **PAT-FBK-1** Neutral View and Fade | Feedback | Camera-driven black layer above L0 and below L1. | Camera CS-R12, CS-R15 | Draft |
| **PAT-FBK-2** Per-Viewer Grade | Feedback | Post-process mood grade on the owner's frame only, with Reduce screen effects and a non-visual twin. | Camera, Concealment UI-2, art bible 2.6 to 2.9 | Draft |
| **PAT-MNU-1** Menu Button (Primary, Secondary, Destructive) | Input | Three weights; Destructive always opens a confirmation. | Host/Join, Lobby, Pause, Settings (planned) | Draft |
| **PAT-MNU-2** Toggle | Input | Binary setting, state shown by fill and text. | Settings (planned) | Draft |
| **PAT-MNU-3** Slider | Input | Continuous setting with a numeric value. | Settings (planned) | Draft |
| **PAT-MNU-4** Dropdown / Select | Input | Pick one of 3 to 15. | Settings (planned) | Draft |
| **PAT-MNU-5** Binding Row | Input | A remappable action row with capture, conflict warning and reset. | Settings (planned) | Draft |
| **PAT-MNU-6** List Item | Layout / Input | Selectable row in a vertical list. | Lobby participants (planned), Settings | Draft |
| **PAT-MNU-7** Tab Bar | Navigation | Same-screen sections. | Settings (planned) | Draft |
| **PAT-MNU-8** Input Field | Input | Text entry (join address, display name). | Host/Join (planned) | Draft |
| **PAT-MNU-9** Modal and Confirmation Dialog | Feedback / Layout | Blocking decision with a focus trap; Cancel is the default focus. | Leave round, reset settings (planned) | Draft |
| **PAT-MNU-10** Tooltip | Feedback | Contextual text on hover or focus; never the only place a fact lives. | Settings (planned) | Draft |
| **PAT-MNU-11** Screen Push / Pop / Replace | Navigation | Stack navigation between menu screens. | All menus (planned) | Draft |
| **PAT-MNU-12** Focus Management | Navigation | Where focus goes on open, close, disable and destroy. | All menus (planned) | Draft |
| **PAT-MNU-13** Escape / Cancel | Navigation | Universal back; in a round the menu key opens an overlay and never pauses. | All menus, Pause (planned) | Draft |
| **PAT-MNU-14** Loading, Empty and Error State | Feedback | How menu content states are shown. | Lobby, Join, Settings (planned) | Draft |

**Template patterns deliberately not adopted** (so a reviewer does not read the omission as an oversight):

| Template pattern | Why not |
|------------------|---------|
| Health / Resource Bar, Damage Number, Status Effect Icon | No health, damage or status effects exist (anti-pillar "NOT a combat game"). The tally and the brace indicator cover the only two private counters. |
| Minimap | Prohibited: a radar is an identification aid (anti-pillar). |
| Quest / Objective Tracker with waypoint | Replaced by Step Bar and Stage Card; no waypoint, compass, arrow or distance read-out may exist (OC-15). |
| Dialogue Box | No spoken dialogue or authored plot in MVP. Revisit if voice ships (accessibility V-19, V-20). |
| Inventory Slot, Grid Item, Ability / Skill Icon | No inventory or loadout. The brace indicator is the only ability readout and is its own pattern. |
| Toast / Notification Banner (generic) | The game's notifications are specific, owner-only and slot-bound: Result Plate, Rejection Plate, Team Feed Line, One-Time Hint. A free-floating toast would break the slot rule. |
| Optimistic UI | Used in a controlled form only inside Predicted Hold Ring and the brace "pending" emphasis, where the GDD defines exactly what may be predicted. |

---

## Patterns

### Foundations

---

#### PAT-FND-1 Opaque Stepped Plate

**Category**: Foundation / Layout
**Status**: Draft
**Used In**: Every plate, card, banner and menu panel.

**Description**: The only container in the interface. A rectangle in Obsidian Ink at 100 % opacity with stepped (two-step) 4 px corners and a 2 px stroke. All text sits on a plate, never bare over the world, so contrast does not depend on the sun.

**Specification**:
- Fill Obsidian Ink `#1C1917`, 100 % opaque, never semi-transparent (art bible 7.2 item 1). Raised variant Obsidian Raised `#2B2622` for the own objective bar and menu hover.
- Corners: two steps of 4 px at 1080p; no rounded corners, no drop shadows, no gradients, no glow (art bible 3.4).
- Plates size to their text and grow with text scale; they never truncate a message. Names ellipsise at the plate width cap.
- Text: Bone `#EFE8D6` or Bone Dim `#B9B09C` (Bone Dim only at 18 px minimum labels). Floors at 1080p: 18 px minimum, 20 px for text the player must act on, 22 px for changing numerals, 28 px for Cochineal Bright text and display headings.
- Danger text is Bone beside a Cochineal glyph or on a Cochineal Deep bar; never Cochineal Bright below 28 px.

**When to Use**: Any container that holds text or an interactive element.
**When NOT to Use**: Glyphs that float on the world (marker, ping, ring) use the keyline treatment of PAT-FND-3 instead of a plate.

**Accessibility**: Contrast at least 4.5:1 for text below 28 px and 3:1 for larger text and graphics, measured on the plate over the brightest ground (V-04). Minimum sizes scale with UI scale but never fall below their 100 % values (V-09). High contrast mode promotes Bone Dim to Bone and thickens keylines to 3 px (V-11).

**Implementation Notes**: One shared UMG style asset (brush with stepped corners, nearest filtering, no mips) used by every layer. Opacity is set once and never animated except as a transition fade.

---

#### PAT-FND-2 Bound-Input Glyph

**Category**: Foundation / Input
**Status**: Draft
**Used In**: Context Action Prompt, Strike Hint, Brace Indicator label, One-Time Hint, menu hints.

**Description**: A stepped-square key glyph plate, Bone outline, showing the key or button currently bound to the action on the device the player last used. It follows rebinding and device switching.

**Specification**:
- Shows the glyph for keyboard-mouse or gamepad according to the last input device; switches at once on a device change (M-02).
- Text label is the verb, never the glyph alone ("Tend", "Slip in", "Slip out", "Strike", "Brace").
- A held-input action shows the same glyph in hold or toggle mode; the label does not change (the accessibility toggle never changes what is shown).
- An unbound action shows "Unbound" in Bone Dim; it never shows a stale glyph.

**When to Use**: Any prompt that names an input.
**When NOT to Use**: Never hard-code a key name in a string.

**Accessibility**: Key names are text-readable at 20 px or more. Glyphs for gamepad buttons carry the button letter or shape as well as colour (no A/B/X/Y colour dependence).

**Implementation Notes**: Resolve through Enhanced Input (`UEnhancedInputUserSettings` keys, ADR-0011 E4) and Common UI input-method detection; verify the input-method callback on 5.8.4 (ADR-0021 E8). The glyph widget reads only local bindings.

---

#### PAT-FND-3 Signal Glyph Set

**Category**: Foundation
**Status**: Draft
**Used In**: All signal elements.

**Description**: The fixed shape vocabulary. Shape is the meaning; colour reinforces it.

| Meaning | Shape | Colour | Keyline |
|---------|-------|--------|---------|
| Ally | Downward pentagon with a centred two-step top notch (the only downward pentagon) | Turquoise (ally only) | 3 px Obsidian |
| Danger or finality | Upward triangle with a two-step base | Cochineal | 2 px Obsidian |
| Wrong-target count | Stepped square, filled or outline | Cochineal filled, Bone outline | 2 px |
| Successful own action (stamp) | Stepped square with one horizontal bar | Bone | 2 px |
| Duration or place | Circle (interaction ring, cooldown ring, ping) | Bone, Bone Dim, Marigold | 2 px (ping 3 px) |
| Progress | Stepped bar of discrete steps | Marigold | 2 px |
| Informant state | Four stepped corner brackets | Bone | none |

**Specification**:
- Functional icons are at least 32 px at 1080p; the marker is 28 x 32 px (minimum 24 x 28) and constant pixel size.
- A full circle in the UI means duration or place; never use a circle for a person or a status.
- No arrow, eye, crosshair or human silhouette anywhere (art bible 3.4, TM VA-7).
- Every coloured glyph has a shape cue and, where urgent, a non-visual cue (L4).

**When to Use**: Any new signal. Add a row here first.
**When NOT to Use**: Do not give a non-ally figure any glyph.

**Accessibility**: Greyscale test (100 % identification) and protan, deutan and tritan simulations are required before a glyph or colour change ships (accessibility tests T-01, T-02).

**Implementation Notes**: Glyph brushes at integer sizes or hard-stepped UI materials, nearest filtering, no mips, so stair edges stay crisp at non-integer DPI scale.

---

#### PAT-FND-4 Slot Priority and Drop

**Category**: Foundation / Feedback
**Status**: Draft
**Used In**: Result slot, Notice slot.

**Description**: The two shared transient slots (Result, Notice) hold one message at a time. A lower-priority arrival while a slot is occupied is **dropped, never queued**.

**Specification**:
- Result slot priority: removal card (its own band, preempts) > result plate or defence plate > informant plate.
- Notice slot priority: returning countdown (only in `InformantAwaiting`) > third-person hint > strike hint.
- A preempted plate leaves over `HudPreemptExitSec` (0.25 s, stepped); after 0.25 s only one text block is on screen.
- Rate limits bound repeats (rejection plate once per `RejectPlateCooldownSec` per code; informant plate once per `InformantPlateCooldownSec`).

**Alternatives considered**: A queue was rejected because a queued plate would arrive late and could correlate with a different encounter, and because a backlog of stale text is worse than a missed repeat. A single combined slot was rejected because the removal card must not be delayed.

**When to Use**: Any transient message in the lower-centre column.
**When NOT to Use**: Persistent elements have fixed slots and never share (no reflow).

**Accessibility**: Dropping is safe only because every dropped message is repeated by a persistent element (tally, step bar, informant frame, cooldown numeral); the accessibility document's LIM-C2 depends on this. Extended read time (`HudReadHoldMult`) lengthens holds but does not queue.

**Implementation Notes**: The presenter owns slot arbitration in `FHudPresenterCore`; widgets only apply the winner.

---

#### PAT-FND-5 Contact-Gated Presentation

**Category**: Foundation / Feedback
**Status**: Draft
**Used In**: Result Plate, brace End, Rejection Plate replies, Removal Card, hold cancellation.

**Description**: Any presentation caused by an outcome is held until the viewer's own contact frame, fall or removal, never shown on network receipt. Until then the HUD keeps running exactly as before, so nothing can announce the outcome early.

**Specification**:
- All HUD predicates about the owner's role read one local value, `PresentedOwnState`, never the raw replicated state (HUD-R6a).
- A result plate shows at the later of the owner's own contact frame (mapped to local time) and RPC arrival, capped by `HudResultPlateMaxHoldBackSec`; a plate is never dropped for lateness.
- A reply (`Busy`, `NotNow`) to a command pressed between a reversal's slot and contact is held to the defender show time.
- A server hold-cancel of a tend or bush entry is held until the own fall; the ring keeps its prediction until then.
- Exception (ruling D1): when the owner's own fall starts, hint, prompts and ring hide at once (a live prompt on a falling pawn is a false affordance).

**When to Use**: Any element whose appearance or change is caused by a strike, reversal, exile or removal.
**When NOT to Use**: Purely local feedback (the press itself, bound-glyph change) is immediate.

**Accessibility**: Timing is identical for all players; Reduce motion shortens animation, not hold-back timing. The 1.25 s hold-back cap is a timing, not a motion, and stays under every setting.

**Implementation Notes**: Time from `EstimatedServerNow` (ADR-0002) and one slewed offset; until `IsServerClockSynced()` persistent clusters show but timed plates are deferred per F2. Automated check: AC-HUD-05 (a target's dump before contact equals a control run).

---

### World-Anchored Patterns (layer L0, ZOrder 10)

> All L0 patterns are drawn by one volatile Slate leaf, in the paint pass, after post-process, with no depth test, at constant pixel size, and collapse while the camera view state is Neutral.

---

#### PAT-WLD-1 Ally Marker

**Category**: Game-Specific / World
**Status**: Draft
**Used In**: HUD element (17); `design/ux/hud.md` (planned).

**Description**: A Turquoise down-pentagon with a stepped top notch hangs above each teammate, always on top. Fill encodes the marked player's state independent of the viewer.

**Specification**:

| State | Fill | Trigger | Visual | Duration |
|-------|------|---------|--------|----------|
| Active | Solid | Ally `State` Active | Solid Turquoise inside a 3 px Obsidian keyline | Appear 300 ms ease |
| Concealed | Hatched | Ally in a bush | Solid fill with two Obsidian stair bands across the lower half, ends offset by one 4 px step | Instant on change |
| Informant | Outline only | Ally is an informant | 3 px Turquoise stroke, hollow, Obsidian keyline outside and inside the stroke | Instant on change |
| Edge (off-screen ally) | Current fill, stepped base | Anchor outside the safe rectangle | Same pentagon clamped to the edge; no rotation, no arrow | Edge de-overlap per TM F4 |
| Retiring | Hold, then exit | Ally removed | Marker holds until the blow, then exits over 300 ms; the team feed line enters with the exit | 300 ms ease |

- Size 28 x 32 px default, 24 x 28 minimum, 150 % large option; constant pixel size, never scaled by distance, FOV or text scale.
- No sound. No fade by distance or occlusion. No blink, bob, pulse or scale pop.
- Exists only on teammates' clients, built only from the viewer's own C-TeamView. A marker never reacts to a non-ally.

**When to Use**: Marking teammates only.
**When NOT to Use**: Never on an enemy, NPC or informant of the other team, in the lobby, the briefing or any menu. In the round-end reveal only, the same silhouette is used for everyone (allies filled, opponents Bone-outlined), shown after the round is decided.

**Accessibility**: State is shape plus fill, never opacity (art bible 7.x C1); separation survives greyscale at 24 px (AC-TMK-21). Ally accent presets (V-07) and marker scale (V-10) change local fill colour and size only. Markers are not focusable or clickable. No audio, so no caption is required; the VS callout cue carries its own caption (SFX audit).

**Implementation Notes**: `SStealthWorldAnchorLeaf`, `ForceVolatile(true)`, projection inside `OnPaint` from the frame's `ULocalPlayer::GetProjectionData`; apply DPI scale once (ADR-0021 §2). Never a `UWidgetComponent` on a pawn (TM-R1).

---

#### PAT-WLD-2 Ally Name Label

**Category**: Game-Specific / World
**Status**: Draft
**Used In**: HUD element (17), TM-R11.

**Description**: An opaque Bone-on-Obsidian plate with the ally's display name beside a marker, shown on **one** marker by focus (nearest the screen centre), or on **every** marker while a hold input is pressed.

**Specification**:
- 22 px text at 100 % text scale; scales with text scale and never below its 100 % size.
- Focus hysteresis: the focus moves only when another marker is at least `round(8 * u * s_ui)` px nearer the centre; the entrance and exit fades of old and new labels overlap for one permitted moment of two labels.
- Names come from the public display name by handle; streamer mode substitutes an alias.
- The hold input `ShowTeamNames` is **pending user decision** (TM CR-4, scoped to Vertical Slice); until then focus alone shows labels.

**When to Use**: Allies only.
**When NOT to Use**: A label never appears for a non-ally and never responds to one.

**Accessibility**: Focus-based labels need no held input, which helps motor accessibility; if `ShowTeamNames` ships it needs a toggle alternative (M-04) and must be remappable (M-01).

**Implementation Notes**: Text from `ST_Hud` string table or the public name; long names ellipsise at the plate cap; duplicate names carry Session's sanitised suffix.

---

#### PAT-WLD-3 Context Action Prompt

**Category**: Game-Specific / World
**Status**: Draft
**Used In**: HUD-E13.

**Description**: A square plate with a Bone outline, the bound-input glyph and a verb, anchored to an interactable **object** (court fixture or bush), never a person. A hint, never a gate.

**Specification**:

| State | Visual | Trigger | Response | Duration |
|-------|--------|---------|----------|----------|
| Hidden | none | Not in reach, or not Active | n/a | n/a |
| Shown | Plate, glyph, verb ("Tend", "Slip in", "Slip out") | Object in reach, owner Active, not running or traversing | Appears; command is valid from the moment of reach whether or not the fade has finished | 400 ms ease-out with 8 px stepped settle |
| Hidden by own fall | none | `OnRoleChangePresented` | Hides at once (PAT-FND-5 exception) | Instant |

- **"Tend" is identical at every court fixture in reach**, live or not, own, rival or hidden final alike, so it confirms nothing (OC-6). The only live-site cue is the ring colour after `TendCueDelaySec` (PAT-WLD-4).
- Computed from public fixture geometry and `PresentedOwnState` only; never from team live-site data; never shown to informants.

**When to Use**: Any world object the owner can act on.
**When NOT to Use**: Never anchored to a person; never a per-press server query.

**Accessibility**: Verb text always shown with the glyph; glyph follows device (PAT-FND-2). Plate scales with UI and text scale.

**Implementation Notes**: L0 leaf; object positions from the locally built layout components.

---

#### PAT-WLD-4 Predicted Hold Ring

**Category**: Game-Specific / World
**Status**: Draft
**Used In**: HUD-E12.

**Description**: A 48 px circle at the interacted object showing hold progress with a linear sweep, predicted from the local press and re-based on server hold status. Never at the screen centre and never on the owner's own body.

**Specification**:

| State | Visual | Input | Response | Duration |
|-------|--------|-------|----------|----------|
| Idle | none | none | none | n/a |
| Predicting | Bone ring fills linearly | Interact press (hold, or press and press in toggle mode) | Starts at the press; re-based on `FHoldStatus` | Linear, hold duration (about 3 s) |
| Own live site | Marigold fill **and four short outward ticks at the cardinal points** | Hold continues past `TendCueDelaySec` (1.0 s) at an own live site | Colour and ticks appear together; non-live courts stay plain Bone | Instant at the delay |
| Clamped | Ring at 0.98 | No server completion yet | Never shows completion before `OnActionCompleted` | Until completion, then fills |
| Cancel or reject | Ring empties in one step | Release, displacement, rejection, own fall (held per PAT-FND-5) | Empty in one step | Instant |

- The live and plain states differ by at least 3:1 luminance contrast (art-director to confirm, CR-HUD-8). Off-screen target: ring clamps to the screen edge at constant size.

**When to Use**: Any timed hold at a world object.
**When NOT to Use**: Brace uses PAT-PLT-1 because it is self-only and not object-bound.

**Accessibility**: The ticks are the non-colour backup. Under Reduce motion the sweep remains (real-time progress); a stepped "no sweep" option is a post-VS candidate (accessibility F12). Hold-to-toggle applies (M-04).

**Implementation Notes**: Prediction per IIF F4 (ring in `[0, 0.98]`, monotonic; hides at `T_ack` if no confirmation).

---

#### PAT-WLD-5 Sighting Ping (Vertical Slice)

**Category**: Game-Specific / World
**Status**: Draft
**Used In**: Informant Pings and Team Comms (planned; the system is not designed).

**Description**: A Marigold open ring (60 degree gap) on the ground marking a **place**, never a person.

**Specification**:
- World radius at least 4 m; screen-space minimum 32 px; static after placement; no centre mark.
- Mandatory Obsidian keyline, 3 px (4 px beyond 20 m).
- Age is shown by stepped stroke weight (6, 4, 3 px at thirds of its life, *proposed*), never by fade; hard expiry (starts at 8 s). Entrance 300 ms stepped; exit 600 ms ease or stepped under Reduce screen effects.
- Edge-clamped ring at 32 px when off-screen.
- Visible to informant viewers on the pinging team; whether active allies also see it is an open design question.

**When to Use**: Place sightings only. **When NOT to Use**: Never follows or attaches to anything; never on a person.

**Accessibility**: Shape and stepped stroke carry meaning without colour; every ping needs a feed line and a text caption twin for its cue (SFX audit row "Teammate callout"). The ping input must be remappable.

**Implementation Notes**: Placeholder only; finalise when the Informant Pings GDD exists.

---

### Persistent Plates (layer L1, ZOrder 30)

---

#### PAT-PLT-1 Brace Indicator

**Category**: Game-Specific / Ability
**Status**: Draft
**Used In**: HUD-E01.

**Description**: A 56 px bottom-left circle with a persistent "Brace" label and bound glyph, in Bone with no signal hue. It shows the defence's state truthfully and never creates a timing window to "hit".

**Specification**:

| State | Visual | Input | Response | Duration |
|-------|--------|-------|----------|----------|
| Ready | Empty outline ring | none | none | n/a |
| Pending | Local outline emphasis only (no fill, no square) | Prepare press | Acknowledged within one frame; protection not yet shown | until confirmation |
| Protected | Inner stepped square appears at once; ring fills in 4 stepped segments | Server confirmation (`FHoldStatus`) | Segments of `DefenceDurationSec` / 4 (1 s each at the 4 s default) | Window length |
| End | Square removed with a stepped exit | Release, cap, cancel or reversal consuming the brace (all identical) | Presented when estimated server time reaches `ServerEndTime` (on a reversal, the contact time), never on receipt | 250 ms stepped |
| Cooling | Bone Dim linear sweep plus numeral "Brace in m:ss" | Counts from activation + `DefenceCooldownSec` (60 s) | Numeral holds at "0:01" until the estimated server end plus half the own RTT, then Ready | Real time, linear |

- It never shrinks, narrows, closes in on a mark or flashes (no parry-window reading). Hidden while the owner is an informant. No ready-again cue is audible or visible to anyone else; the owner's own ready-again tone is a soft 2D UI-bus tone.
- No cue of any kind when a strike begins against the viewer (L2).

**When to Use**: The owner's own defence only. **When NOT to Use**: Never for another player's brace; never styled as an enemy warning.

**Accessibility**: Label, glyph, numeral and shape carry state without colour; every cue has a visible twin (the state change itself). Hold-to-toggle available (M-04); the second press ends the hold, so early release stays available and durations are unchanged. The cooldown sweep is real-time progress and survives Reduce motion.

**Implementation Notes**: Presenter F5 formulas; sampled from `now - t0`, never a UMG animation asset.

---

#### PAT-PLT-2 Wrong-Target Tally

**Category**: Game-Specific / Resource
**Status**: Draft
**Used In**: HUD-E02.

**Description**: A bottom-left plate above the brace indicator: a line "Wrong targets: {n}" with `L` stepped squares above it, filled up to `min(n, L)`, and a static upward danger triangle once `n >= L - 1`. The limit is never written as a number.

**Specification**:

| State | Visual | Trigger | Duration |
|-------|--------|---------|----------|
| Zero | `L` outline squares (Bone), numeral 0 | Round start | n/a |
| Increment | One square fills (Cochineal), numeral changes in **one step**, one tick | Result plate show time (never before it) | 300 ms stepped fill |
| Threshold | Static upward triangle added | `n >= L - 1` | Instant, never pulses |
| Frozen (informant) | Squares and numeral frozen at last value, **no triangle** | `PresentedOwnState = Informant` | n/a |

- Identical for an NPC and an informant victim (the true strike count never reaches a client). A re-push is held until its plate shows and never lowers the displayed value within a round.

**When to Use**: The owner's own tally. **When NOT to Use**: Never show another player's tally; never write the limit.

**Accessibility**: Squares, numeral and triangle carry the count without colour; audio twins (tally tick, threshold tick) each have this plate as their text twin. Numeral at least 22 px Semibold tabular.

**Implementation Notes**: Display cache keyed by `RoundEpoch` in `UStealthHudLocalStore`.

---

#### PAT-PLT-3 Step Bar and Stage Card

**Category**: Feedback / Layout
**Status**: Draft
**Used In**: HUD-E03, E04, E05.

**Description**: Top-centre team progress. Own bar "Your house" has exactly 3 discrete steps (Stage 1, Stage 2, Final); rival bar "Rival house" has exactly 2 steps showing the rival preliminary count, never a final step, never a time. A stage card under them shows title, one line and the clue "A court [clue phrase]".

**Specification**:
- Steps fill Marigold in 500 ms ease-out; a rival rise of two fills both steps in one 500 ms fill with one tick. The stage card turns over in 500 ms on a counted completion.
- Own bar is drawn at 1.25x scale on Obsidian Raised; **no "n/5" label**; never a Turquoise own-team bar (Turquoise is the marker's only).
- Team audience only (active and informant viewers); empty during `Preparing`.
- **No waypoint, compass, arrow, distance read-out, map or world plate exists** (OC-15).
- On an own objective win the Final step fills over `HudObjectiveStepSec` before the banner (F10).

**When to Use**: Team objective progress. **When NOT to Use**: Never show an opponent count, elapsed time or countdown.

**Accessibility**: Step count and labels carry quantity without colour; the clue is always on screen, so objective text is zero presses away (C-04). Text 22 px Primary title at 100 % scale.

**Implementation Notes**: Invalidation panel per cluster; `rival_shown` is monotonic within a `RoundEpoch` (F9).

---

#### PAT-PLT-4 Team Feed Line

**Category**: Feedback
**Status**: Draft
**Used In**: HUD-E06.

**Description**: Up to three plain lines top-right, allies only. In MVP: "{Ally name} — eliminated for the round", identical for every removal reason, shown at the later of the state's arrival and the marker's retire event so the line enters with the marker's exit.

**Specification**: Opaque plate, 20 px Bone text, upward triangle for removal; life and dwell per F8 and multiplied by `HudReadHoldMult`. Opponents receive nothing (no removal notice for the enemy team). Marker retirement alone (a release to the crowd on disconnect) never creates a line.

**When to Use**: Team-visible facts. **When NOT to Use**: Joins, leaves and connection states of others are never shown in a round (Session rule 24).

**Accessibility**: Text, not colour; line life is extendable (V-09 text scale and `HudReadHoldMult`); a reserved line type for informant sightings (VS) needs a caption twin.

**Implementation Notes**: Slots drop rather than queue beyond the three-line cap (PAT-FND-4).

---

#### PAT-PLT-5 Informant Frame

**Category**: Game-Specific
**Status**: Draft
**Used In**: HUD-E09.

**Description**: Four 2 px stepped Bone corner brackets framing the screen while the viewer is an informant. The removal card collapses into it over 800 ms.

**Specification**: Static; no animation after entry; hides the brace indicator, prompts, ring, strike hint and result plates. The informant grade (art bible 2.9) is Camera's post-process, not a HUD element.

**When to Use**: Own informant state only. **When NOT to Use**: Never visible to any other viewer.

**Accessibility**: Shape carries the state without the grade (so Reduce screen effects loses nothing); "Informants can only observe" plate appears on a refused press.

**Implementation Notes**: Appears at `PresentedOwnState = Informant`, never on receipt (PAT-FND-5).

---

#### PAT-PLT-6 Latency Icon

**Category**: Feedback
**Status**: Draft
**Used In**: HUD-E19.

**Description**: A small owner-only icon at the top-right safe edge, shown when own round-trip exceeds `HudLatencyWarnRttSec` (0.20 s). Hysteresis: hidden only below 0.8 x the threshold and after at least 2 s shown. Never shown on the listen-server host.

**Specification**: Shape on the 32 px icon grid, Bone. Driven only by own RTT from the owner's net connection, never by anything else.

**When to Use**: Own connection warning. **When NOT to Use**: Never reflect another player's connection.

**Accessibility**: Shape, not colour (to be confirmed in the HUD UX spec; accessibility audit row "Latency icon").

**Implementation Notes**: Smoothed lag from the owner's net connection, not the replicated ping byte.

---

### Transient Slots (layer L2, ZOrder 40)

---

#### PAT-TRN-1 Result Plate

**Category**: Feedback
**Status**: Draft
**Used In**: HUD-E07 (attacker), HUD-E08 (reversing defender).

**Description**: Tells the owner the outcome of their own strike, or that their brace reversed a strike. One size, slot, entrance, hold and exit for every outcome, so the plate's timing leaks nothing.

**Specification**:

| Outcome | Text | Glyph | Notes |
|---------|------|-------|-------|
| Rival eliminated | "Target exiled" | Stepped-square stamp | Bone plate |
| Wrong target | "Wrong target" | Upward Cochineal triangle | Identical for an NPC and an informant victim |
| No contact | "No contact" | none | Bone only; a miss is not finality |
| Reversed (attacker) | "Reversed" | Upward triangle | Removal card follows and preempts |
| Strike reversed (defender) | "Strike reversed" | Stepped-square stamp | Never names the attacker or its team |

| State | Visual | Timing |
|-------|--------|--------|
| Pending | Not shown | Until the matching action state is observed and show time is reached |
| Entering | One-shot stepped entrance | 250 ms (`HudResultPlateInSec`) |
| Holding | Static | 3.0 s (`HudResultPlateHoldSec`, range 1.0 to 6.0) |
| Exiting | Fade out, no movement | 600 ms (`HudResultPlateOutSec`) |
| Preempted | Stepped exit | 0.25 s (`HudPreemptExitSec`) |

Total on screen 3.85 s at defaults. Shown at the local contact frame (PAT-FND-5). The tally changes with the plate, never before it. Owner only; no observer sees anything.

**When to Use**: The owner's own action outcomes. **When NOT to Use**: Never for any event the viewer did not cause; never names a person, count of opponents or kind.

**Accessibility**: Text and shape, never colour alone; every result has a distinct audio cue id with this plate as its text twin (SFX audit). Hold is multiplied by `HudReadHoldMult`; **the 3.0 s default is below the 5 s Standard guideline and is a documented deviation** (accessibility LIM-C2).

**Implementation Notes**: Presenter F2 matching on the pawn handle captured at the press; late results are never dropped (PEN-R16).

---

#### PAT-TRN-2 Rejection Plate

**Category**: Feedback
**Status**: Draft
**Used In**: HUD-E15, HUD-E16 (informant plate).

**Description**: A short refusal beside the brace indicator when a command is rejected: "Not now", "Busy", "Too far". A `Cooldown` rejection shows no plate because the Cooling numeral already says it. The informant plate "Informants can only observe" uses the same pattern in the Result slot.

**Specification**:

| Plate | Text | Timing in / hold / out | Rate limit |
|-------|------|------------------------|------------|
| Rejection | "Not now", "Busy", "Too far" | 400 ms / 1.5 s / 600 ms | Once per `RejectPlateCooldownSec` per code (3 s) |
| Informant | "Informants can only observe" | 400 ms / 2 s / 600 ms | Once per `InformantPlateCooldownSec` (3 s) |

- 20 px Bone text on Obsidian. The ring empties on a rejection. A press before the removal card has collapsed shows nothing (dropped, not deferred). The informant plate is caught locally; nothing is sent to the server.
- A reply to a pawn struck in the same drain is presented no earlier than its contact time (PAT-FND-5).
- Sound: IIF UI-5 specifies a quiet level 2D owner-only tone on the UI bus for the rejection plate; the informant plate has no sound (see Open Questions P4).

**When to Use**: Own command rejection. **When NOT to Use**: Never explain *why* with another player's state.

**Accessibility**: Text carries the message; hold is extendable; the plate is re-triggerable on the next press, which is the mitigation for its short hold (LIM-C2).

**Implementation Notes**: Reply code from the owner reply sink (ADR-0011).

---

#### PAT-TRN-3 Strike Hint

**Category**: Game-Specific / Input
**Status**: Draft
**Used In**: HUD-E14.

**Description**: A screen-fixed lower-centre plate "[bound glyph] Strike" in Bone on Obsidian, shown whenever the camera aim ray hits **any rendered non-ally pawn in reach**, NPC or player alike. **There is no reticle, outline, highlight, colour or sound that differs by target class.**

**Specification**:
- Hidden while the owner's action slot is `Striking` (the 1.75 s lockout), while any card is up, from the owner's own fall, for a spot, a bush, a marked ally and any concealed pawn (the actor is not rendered).
- Predicate reads no team, controller, kind or concealment field.
- Notice slot priority: lowest (PAT-FND-4).
- Fallback if playtests show aiming fails: a static 4 px Bone stepped square at the screen centre that never changes state (art bible 7.1 amendment; IIF OQ9).

**When to Use**: The attack affordance. **When NOT to Use**: Never as a lock-on or a target marker.

**Accessibility**: Text and glyph follow rebinding. No aim assist exists or may be added (accessibility M-07, LIM-M3). The single irreversible press is a motor risk; mitigation is remapping (M-12).

**Implementation Notes**: Computed locally from `GetAimRay()` and the mask-rendered flag (CS-R8, R10); its frequency is a playtest check (HUD OQ12).

---

#### PAT-TRN-4 One-Time Hint

**Category**: Feedback / Onboarding
**Status**: Draft
**Used In**: HUD-E17 (third-person hint).

**Description**: A once-per-profile plate shown on the first live third-person frame: "Third person shows only what your character can see." (4 s). The flag is consumed only when the hint is actually shown; if its slot is busy it retries on the next third-person entry. It is **never triggered by masking itself**.

**Specification**: Notice slot, 4 s hold. The same line must live permanently in the controls Help screen and a loading tip (Camera UI Requirements; accessibility C-03).

**When to Use**: A rule the world cannot teach. **When NOT to Use**: Never trigger on an event that reveals a hidden fact.

**Accessibility**: Retrievable from Help so a reflexively dismissed hint is not lost (C-03).

**Implementation Notes**: Profile flag in `UStealthHudLocalStore`.

---

#### PAT-TRN-5 Returning Countdown

**Category**: Feedback
**Status**: Draft
**Used In**: HUD-E11.

**Description**: On the neutral view, a plate "Returning in {s}" in whole-second steps with tabular numerals; at 0 it reads a static "Returning" while the takeover is still picking.

**Specification**: Bone on Obsidian, no animation, no count-up, no "searching" wording, no colour change near zero, never flashes. It resumes correctly from the carried due time after a reclaim or rejoin. Notice slot, highest priority, only in `InformantAwaiting`.

**When to Use**: Own respawn wait only. **When NOT to Use**: Never for any other countdown (no round clock exists).

**Accessibility**: Whole-second text steps are screen-reader and Reduce-motion safe. Never colour-coded.

**Implementation Notes**: Presenter F7.

---

### Cards, Banners and Overlays (layers L3 and L4)

---

#### PAT-CRD-1 Removal Card

**Category**: Modal / Overlay
**Status**: Draft
**Used In**: HUD-E10.

**Description**: A one-time central band shown to the removed player: "Exiled" or "Eliminated for the round", both with "You watch for your team now." (wording provisional, HUD OQ5). An opaque Cochineal Deep title bar with Bone text and an upward stepped triangle; at most 30 % of the screen; never full-bleed.

**Specification**:

| State | Timing | Notes |
|-------|--------|-------|
| Wait | Exile: plate show time + `ExileBeatSec` (0.8 s). Other removals: own fall start + `HudRemovalCardDelaySec` (0.8 s) | Fall is visible at least 0.8 s before the card |
| Enter | 600 ms fade | The plate in the Result slot leaves over 0.25 s (stepped, never a cross-fade) |
| Hold | `HudRemovalCardHoldSec` 4 s | Static |
| Collapse | 800 ms into the informant frame | Ease-in-out |

- Shows once; never replayed on reconnect or rejoin after the grace. A killed informant gets no card. If the removal ends the round, no card shows (or a showing card exits over 0.25 s as the banner enters).
- No field names the killer, its kind or its team; no kill cam. The world fades to black over 0.5 s from the card entrance.

**When to Use**: Own removal. **When NOT to Use**: Never on another player's screen (allies get a one-line feed entry).

**Accessibility**: Title text plus triangle, never colour alone; one text block on screen after 0.25 s. The removal sequence (grade, plate, preempt exit, card, fade) is one photosensitivity-budgeted sequence checked with an analyser before the Vertical Slice (accessibility V-13, T-07). Hold scales with `HudReadHoldMult`.

**Implementation Notes**: Fires a client-local `OnRemovalCardEntered` event that Camera uses for the exile view release.

---

#### PAT-CRD-2 End Banner

**Category**: Overlay
**Status**: Draft
**Used In**: HUD-E18.

**Description**: On `Phase = Ended`: "Victory" (Bone bar) or "Defeat" (Cochineal Deep bar) in the viewer's own terms, exactly one approved reason line, and a score line "Your house {a} · Rival house {b}". No duration, no per-participant list, no opponent colours.

**Specification**:
- Waits for an own pending result plate to show for `HudBannerMinPlateSec`, and for an own pending removal to reach its fall start; total wait capped by `HudBannerMaxDelaySec`; never shows before the own plate or fall (F10).
- Persistent clusters fade out over 600 ms as the banner enters. Winners' Final step fills first (0.5 s).
- Variants on `Reset`: "The round was called off" (clears every transient element at once); lobby line "The host will begin the next round" (non-host; no count beyond `n / capacity`).
- The post-round reveal (art bible 7.3 row 13) is a separate Vertical Slice screen.

**When to Use**: Round end. **When NOT to Use**: Never before `Ended`; never show elapsed time or opponent counts.

**Accessibility**: The words "Victory" and "Defeat" carry the outcome, not the bar colour. Display accent is only at 28 px or above, with the functional face as fallback.

**Implementation Notes**: Presenter F10.

---

#### PAT-CRD-3 Status Overlay

**Category**: Feedback / Modal
**Status**: Draft
**Used In**: HUD-E20; Session & Travel surfaces (planned).

**Description**: Local connection and loading text on layer L4: "Connecting...", "Loading the city...", "Connection lost. Reconnecting..." with a Cancel option, "The session has ended." with an OK button. Rejection messages are one plain sentence per reason code ("This session is full.", "This round has already begun.", and so on).

**Specification**: Opaque plate, text from a string table, one message at a time. The overlay sits over any HUD state and is orthogonal to it. No in-round element may show joins, leaves, connection states, pings or counts of other participants (Session rule 24). Cancel and OK are Menu Buttons (PAT-MNU-1) and make the overlay the one HUD layer that takes input; it hosts a focus trap while a button is showing.

**When to Use**: Own connection state. **When NOT to Use**: Never show another participant's connection.

**Accessibility**: Text, not colour or spinner alone; a spinner, if used, has a static equivalent under Reduce motion. Messages persist until resolved (no timed auto-dismiss of actionable text).

**Implementation Notes**: Note the exception to ADR-0021's "no input below 100": buttons on this overlay need Common UI routing; the ADR must state how input is routed on L4 [verify] (Open Questions P7).

---

### Full-Screen Feedback

---

#### PAT-FBK-1 Neutral View and Fade

**Category**: Feedback
**Status**: Draft
**Used In**: Camera CS-R12, CS-R15; removal sequence; informant takeover.

**Description**: A full-screen black UMG layer (ZOrder 20) driven by the camera, covering the world and the L0 markers but not the plates, cards or menus. Used for the neutral view, exile fade, takeover fade-in and cuts.

**Specification**: Camera decides when and how fast and publishes per-frame alpha and colour; the HUD only renders it. Timings: exile fade 0.5 s from card entrance; takeover fade-in 0.5 s ease; neutral view renders only black and the HUD's plates. Under Reduce camera motion, boom recovery and shoulder swap become cuts (CS-R13); fades remain fades because a cut to black would add a sudden luminance change.

**When to Use**: Camera-owned transitions. **When NOT to Use**: The engine's `StartCameraFade` is not used (it cannot cover L0 while leaving plates visible).

**Accessibility**: Counted in the removal photosensitivity sequence budget (at most 3 luminance reversals per second, 10 % relative luminance change over a large area).

**Implementation Notes**: ADR-0021 §3; requires the ADR-0015 amendment `GetScreenFade()`.

---

#### PAT-FBK-2 Per-Viewer Grade

**Category**: Feedback
**Status**: Draft
**Used In**: Concealment UI-2, assassination snap (art bible 2.7), informant grade (2.9), pursuit grade (2.6, built only if a legitimate signal exists).

**Description**: A post-process mood grade applied to the owner's frame only. It never darkens the HUD (UMG is composited after post-process).

**Specification**: Values per art bible 2.6 to 2.9 (for example concealed grade 0.6 s in, 0.4 s out). **Reduce screen effects**: the 2.7 snap eases over 0.5 s, saturation drop capped at -30 %, vignettes disabled (art bible 7.6 rule 8). Any grade that carries information needs a non-visual twin (rule 9).

**When to Use**: Own-state mood only. **When NOT to Use**: Never on a non-ally's presence, proximity or action; never saturated red.

**Accessibility**: **Open gap**: the concealed grade has no non-visual twin and its vignette is disabled by Reduce screen effects (accessibility F11). The informant grade is twinned by PAT-PLT-5.

**Implementation Notes**: Camera's `ApplyTransientGrade`; verify TSR, Lumen and MegaLights interaction (Camera OQ5).

---

### Menu Controls and Navigation (layer 100, Common UI)

> **Scope.** The MVP has one minimal menu (Host / Join with an address field, a minimal lobby panel, status overlays). Lobby UI, Settings & Accessibility and Pause are Vertical Slice and are not designed. These patterns exist so those screens inherit one behaviour. **Values marked *proposed* are not in any GDD and need art-director confirmation** (Open Questions P1, P2, P3).
>
> **Menu look.** Menus use plates, Bone, Bone Dim and Obsidian only. **No signal hue appears in a menu**: Turquoise is the ally's, Cochineal is danger and finality, and Marigold is progress or intel, so a saturated colour on screen always means a message (art bible 4.6).

#### Shared control state baseline

All MNU controls share these states. Per-pattern entries below list only their differences.

| State | Visual (proposed) | Input | Response | Duration | Audio |
|-------|-------------------|-------|----------|----------|-------|
| Default | Obsidian Ink plate, 2 px Bone Dim stroke, Bone text | none | none | n/a | none |
| Hovered (pointer) | Obsidian Raised plate | Pointer over | Plate change only | Instant | none, or a very quiet 2D tick (menu only) |
| Focused (keyboard, gamepad) | 4 px Bone stroke plus a Bone stepped-square focus marker at the leading edge; **always visible**, never suppressed | Tab, D-pad, arrow | Focus moves; selected element scrolls into view | Instant | Same quiet tick |
| Pressed | Obsidian Raised plate, 4 px stroke retained; **no scale or movement** | Confirm key, click | Activates on release for pointer, on press for keyboard and gamepad | Instant | Confirm cue |
| Disabled | Bone Dim text, 2 px Bone Dim stroke, no marker; label stays | none | Not focusable; the reason is shown in text beside it | n/a | none |

Minimum control height 48 px at 1080p; minimum menu text 24 px (accessibility V-01); everything operable without a pointer (M-11); every state distinguishable in greyscale (focus is stroke weight and marker, not colour).

---

#### PAT-MNU-1 Menu Button (Primary, Secondary, Destructive)

**Category**: Input
**Status**: Draft
**Used In**: Host/Join, Lobby, Pause, Settings, Status Overlay (all planned).

**Description**: Primary is the screen's one main action; Secondary is cancel or an alternative; Destructive is irreversible.

**Specification**:
- Primary: Bone plate with Obsidian text (inverted), at most one visible per screen. Secondary: baseline plate. Destructive: baseline plate with an **upward triangle glyph and the specific verb as label**; **no Cochineal** (reserved for danger and finality; *proposed*).
- **A Destructive button never executes directly; it always opens PAT-MNU-9.** No exceptions.
- Activation inputs: Enter or Space or click; gamepad A (confirm) and B (cancel) per the active platform mapping; both remappable where they are gameplay-adjacent, fixed for menus.

**When to Use**: Menu actions. **When NOT to Use**: Gameplay commands are never menu buttons; no gameplay command may be a Common UI action (ADR-0011 §9).

**Accessibility**: Name, role and state exposed to a screen reader if F16 resolves; until then, visible focus and text labels. Hit target 48 px minimum.

**Implementation Notes**: `UCommonButtonBase`; input mode Menu blocks gameplay input and sends Ends synchronously before the switch (ADR-0011 §9).

---

#### PAT-MNU-2 Toggle

**Category**: Input
**Status**: Draft
**Used In**: Settings (planned): hold/toggle per action, Reduce motion, Reduce camera motion, Reduce screen effects, High contrast, subtitles, streamer mask.

**Description**: Binary setting. Current state must be readable at a glance without colour.

**Specification**: A stepped square, **filled when On and outline when Off**, plus the words "On" and "Off" beside the label. Activates on Enter, Space, click, or gamepad A; one press flips. Applies immediately where safe and shows the effect; settings that change layout relayout without replaying animations. Under Reduce motion the flip is instant.

**When to Use**: Two states. **When NOT to Use**: Three or more options (use PAT-MNU-4).

**Accessibility**: State is shape plus text. For hold/toggle input settings, the label names the action ("Brace: hold / toggle") and the help line states that early release remains possible in toggle mode (IIF R16).

**Implementation Notes**: Persist client-local, never replicated (TDC CR-14).

---

#### PAT-MNU-3 Slider

**Category**: Input
**Status**: Draft
**Used In**: Settings (planned): volumes, UI scale, text scale, FOV, sensitivity, brightness.

**Description**: Continuous or stepped value with a **numeric readout always shown**.

**Specification**: Left/Right (or A/D, D-pad) change one step; Page Up/Down or shoulder buttons change a large step; Home/End jump to min/max. Fill is Bone on an outline track; **no hue**. Text scale is a stepped slider with 100, 125, 150, 200 %. UI scale 75 to 150 %. Volume 0 to 100 % default 80 %. Values apply live; a preview line (for example a sample plate) shows the effect for scale settings.

**When to Use**: Range selection. **When NOT to Use**: Precise numeric entry (use PAT-MNU-8) or fewer than about five options.

**Accessibility**: Numeric value on screen at all times; keyboard step and large step; works with gamepad D-pad. Dragging is never the only way to change a value.

**Implementation Notes**: Layout must survive UI scale 75 to 150 % and text scale 200 % at 1920x1080, 1920x1200, 2560x1440 and 3440x1440.

---

#### PAT-MNU-4 Dropdown / Select

**Category**: Input
**Status**: Draft
**Used In**: Settings (planned): ally accent colour, colour-vision preset, resolution, language.

**Description**: Selection from 3 to 15 options; only the selection is visible at rest.

**Specification**: Opens a list of PAT-MNU-6 rows beneath the control, focus trapped inside; Esc or B closes without change; Enter or A selects. The ally accent list shows each option as a **live marker swatch drawn with its shape and keyline**, not a bare colour chip. The colour-vision preset list shows its name and a one-line description.

**When to Use**: A discrete list. **When NOT to Use**: Binary (toggle) or comparison-heavy choices.

**Accessibility**: Opened/closed state announced in text; options are readable without colour (name plus swatch).

**Implementation Notes**: Positions to stay on screen at 150 % UI scale.

---

#### PAT-MNU-5 Binding Row

**Category**: Input
**Status**: Draft
**Used In**: Settings (planned) remapping screen.

**Description**: One row per action: label, current binding glyph for each device, a capture control and a reset control.

**Specification**:
- Activating "Rebind" enters a capture state with the text "Press a key or button. Esc cancels." Capture accepts one key, mouse button or gamepad button for the **active device**; Esc or B cancels.
- **Conflict**: if the input is already bound, show an inline plate "Already used by {action}" with two actions, "Swap" and "Cancel"; never bind silently to two actions. The menu key is reserved.
- Reset per action and "Reset all" (Destructive pattern, confirmation required).
- Applies to every gameplay action in IIF R15, including Camera toggle and Shoulder swap; hold/toggle (PAT-MNU-2) sits on the Run, Interact and Prepare rows.
- A rebinding updates every on-screen glyph at once (PAT-FND-2). Bindings persist to the profile and are never replicated.

**When to Use**: Remapping. **When NOT to Use**: Never expose a server tuning value here.

**Accessibility**: Required by M-01 and M-04. Fully keyboard- and gamepad-operable. The capture state is announced in text and times out after 10 s back to idle (a timeout never changes a binding).

**Implementation Notes**: `UEnhancedInputUserSettings` (ADR-0011 E4); `UPlayerMappableInputConfig` must not be used. Gamepad remap UI is a Standard-tier requirement (IIF OQ10).

---

#### PAT-MNU-6 List Item

**Category**: Layout / Input
**Status**: Draft
**Used In**: Lobby participants (planned), dropdown options, settings rows.

**Specification**: One row per item, at least 48 px; baseline states; Up/Down moves focus with scroll-into-view; the focused row is stroke-weight plus marker. A participant row shows the display name and, in the lobby only, the team and ready state **as text** ("Team A", "Ready"). A list is never a place where roles or kinds are revealed mid-round.

**When to Use**: Vertical lists. **When NOT to Use**: Two-dimensional grids (none exist).

**Accessibility**: Position announced ("3 of 10") in text for screen readers; focus never wraps between unrelated regions.

**Implementation Notes**: Display names come from the public name with Session's sanitised suffix; streamer mode substitutes an alias.

---

#### PAT-MNU-7 Tab Bar

**Category**: Navigation
**Status**: Draft
**Used In**: Settings (planned): Controls, Display, Audio, Accessibility, Help.

**Specification**: A row of Menu Buttons; the active tab has a filled square marker and a 4 px stroke (state by shape, not colour). Left/Right or Q/E (menu only) or LB/RB switches tab; Tab key moves into the content. Switching is instant; under default motion a 150 ms cross-fade is the maximum.

**Accessibility**: Active state is shape plus text; each tab's content is reachable by keyboard and gamepad; Help tab holds the permanent tutorial text (C-03).

---

#### PAT-MNU-8 Input Field

**Category**: Input
**Status**: Draft
**Used In**: Host/Join address field (MVP), display name (planned), chat if it ever ships.

**Specification**: Baseline plate with a visible caret; the Join address field remembers the last address. Validation shows an inline text line beneath, never a colour change alone ("Enter an address like 192.168.0.5:7777" style example text from the string table). A character limit shows a counter at 80 % and blocks further input. Escape clears focus without discarding text; a Clear control is provided.

**Accessibility**: Label above the field, not placeholder-only; errors are text; works with an on-screen keyboard if the OS provides one. If in-game chat or voice is ever added, CVAA duties apply (accessibility F3).

---

#### PAT-MNU-9 Modal and Confirmation Dialog

**Category**: Feedback / Layout
**Status**: Draft
**Used In**: Leave round, reset settings, reset bindings, photosensitivity notice (planned).

**Description**: A blocking decision. The confirmation form is always triggered by a Destructive button.

**Specification**:
- **Focus trap**: focus cycles inside the dialog only and returns to the trigger on close.
- Structure: title (the action), one sentence stating the consequence, confirm button (Primary, **labelled with the specific action**, never "OK" or "Yes"), Cancel (Secondary). **Default focus is Cancel.**
- In-round "Leave the round" confirms with the consequence in text ("The round continues without you. You can rejoin while the round is in progress.") because the game cannot pause (accessibility C-02, LIM-C1). Opening any menu in a round **does not pause the world** and the overlay says so.
- Esc or B equals Cancel. The first-launch **photosensitivity notice** uses the modal form with a single Primary "Continue", a link to the Reduce screen effects setting, and no timed auto-dismiss.
- Open 200 ms ease-out, close 150 ms ease-in under default motion; instant under Reduce motion; no scale-pop.

**Accessibility**: Dialog title and body are text; no timed dismissal; screen reader announcement as an alert when F16 resolves.

**Implementation Notes**: Common UI activatable on layer 100; input mode Menu; Ends sent before the switch.

---

#### PAT-MNU-10 Tooltip

**Category**: Feedback
**Status**: Draft
**Used In**: Settings (planned).

**Specification**: Appears on hover after 400 ms or on keyboard or gamepad focus after 400 ms; plate, 20 px text. **Never the only place a fact lives**: anything required to use a control is in its visible label or help line. Repositions to stay on screen; disappears on blur or any input. Appearance 120 ms ease-out, none under Reduce motion.

**Accessibility**: Reachable by keyboard and gamepad (no hover-only information).

---

#### PAT-MNU-11 Screen Push / Pop / Replace

**Category**: Navigation
**Status**: Draft
**Used In**: All menus (planned).

**Specification**: Push (deeper), Pop (back), Replace (peer, or to the loading screen). Default motion: Push and Pop are a 200 ms **stepped** slide of at most 8 px with a fade, not a full-width slide (a full-width slide breaks L5); Replace is a 200 ms fade out plus 200 ms fade in. **Reduce motion: instant cut or 100 ms fade at most.** Focus rules in PAT-MNU-12. A menu opened during a round is an overlay on layer 100 and never replaces the HUD.

**Implementation Notes**: `UCommonActivatableWidgetStack` (ADR-0021 §7).

---

#### PAT-MNU-12 Focus Management

**Category**: Navigation
**Status**: Draft
**Used In**: All menus (planned).

| Rule | Behaviour |
|------|-----------|
| Screen open | Focus goes to the most logical interactive element (Primary button, first list item, or the last-focused element on a revisit); never a non-interactive element |
| Screen close | Focus returns to the element that triggered it, or the nearest preceding interactive element |
| Modal open or close | Trapped inside; returns to the trigger |
| Element disabled or destroyed | Focus moves to the next available element |
| Tab and D-pad | Tab moves in reading order; D-pad moves spatially; focus never wraps between unrelated regions |
| Visibility | The focus indicator is **always** visible for keyboard and gamepad; never suppressed |
| Device change | The last input device decides glyphs; focus is kept |

---

#### PAT-MNU-13 Escape / Cancel

**Category**: Navigation
**Status**: Draft
**Used In**: All menus (planned).

**Specification**: Esc and gamepad B (platform mapping) go back one level; at the root of a menu they close it; in a round the menu key opens the pause overlay and pressing it again closes it. The overlay never pauses the world and never changes any HUD value. Cancelling a capture, a dropdown or a dialog never changes a setting. In a Confirmation dialog Esc equals Cancel.

**Accessibility**: Reachable in one press from any gameplay state (accessibility C-02 mitigation).

---

#### PAT-MNU-14 Loading, Empty and Error State

**Category**: Feedback
**Used In**: Lobby, Join, Settings (planned).

**Specification**:
- **Loading**: plate with a text line ("Loading the city..."); a determinate Step Bar if progress is known, otherwise text only; a spinner, if used, is replaced by a static equivalent under Reduce motion. Text never counts or names other players.
- **Empty**: a plain sentence and the next action ("No participants yet. Share your address."); never an empty void.
- **Error**: one plain sentence per reason code (Session's list); the cause in words, the next action as a button; no colour-only error; no harsh buzzer (a short dull UI tone is acceptable).
- Success confirmation for settings is the control's own state change plus a one-line "Saved" in the plate for 3 s; never the only record.

**Accessibility**: All states are text; none auto-dismisses actionable text under 5 s.

---

## Animation Standards

> These timing values apply to ALL patterns. **They replace the template's defaults**, which include scale-pop, bounce and overshoot feel ("button press scale up... slightly bouncy", "value flash 100 ms on / 100 ms off") that art bible 7.6 forbids. The art bible is authoritative; where this table and the art bible differ, the art bible wins and this table is corrected.

| Animation Type | Duration (ms) | Easing | Notes |
|---------------|--------------|--------|-------|
| Plate appear (non-urgent) | 400 | Ease-out | Fade in plus 8 px settle in whole 4 px steps |
| Plate disappear | 600 | Ease-in-out | Fade out, no movement |
| Urgent entrance (result plate, tally square fill, threshold triangle) | 250 (tally square fill 300) | Stepped | One-shot; the tally numeral changes in one step, no roll-up |
| Preempt exit | 250 | Stepped | Never a cross-fade |
| Objective step added | 500 | Ease-out | Plate unchanged |
| Stage card turn over | 500 | Ease-out | On a counted completion |
| Brace Protected | `DefenceDurationSec` / 4 per segment | Stepped | Never shrinks, closes or flashes; End exit 250 |
| Cooldown ring and interaction ring | real duration | **Linear** | The **only** linear motion |
| Removal card | 600 in, 4000 hold, 800 out | Ease-in-out | Shown once |
| Returning countdown | 1000 steps | Stepped | No motion |
| Takeover fade-in | 500 | Ease | |
| Ally marker appear / disappear | 300 | Ease | Only on state change, removal or the view gate; no bob, pop or smoothing lag |
| Ping | 300 in, hold, 600 out | Stepped / Ease | Stroke steps at thirds of life |
| Menu hover / focus | 0 | none | State change only, instant |
| Menu push / pop | 200 | Stepped, at most 8 px | Not a full-width slide |
| Menu replace | 200 + 200 | Fade | |
| Modal open / close | 200 / 150 | Ease-out / ease-in | No scale-pop |
| Tab switch | up to 150 | Cross-fade | |
| Tooltip appear | 120 after 400 delay | Ease-out | |

**Rules (art bible 7.6)**: (1) no flashing or pulsing faster than 0.5 Hz, no saturated-red pulsing; (2) no scale-pop, shake, spin, overshoot or bounce; (3) nothing moves more than 8 px except cards and the reveal; (4) no persistent looping animation except the cooldown sweep, ambient loops at least 4 s; (5) Reduce motion; (6) nothing animates on an element attached to a non-ally figure because none exists; (7) the removal sequence is one photosensitivity-budgeted sequence; (8) Reduce screen effects; (9) a grade that carries information needs a non-visual twin. Transition length is independent of frame rate.

**Reduce motion override** (accessibility V-15): every fade 0 ms, stepped entrances instant, no 8 px settle, menu push and pop become cuts. Real-time progress (rings, sweeps) and timing holds remain. Reduce camera motion (V-16) and Reduce screen effects (V-14) are separate settings (accessibility F1).

---

## Sound Standards

> Sound is a primary feedback channel for **owner-only** events. Specific audio assets belong to the audio-director; this table maps events to categories so the audio and UI programmers share vocabulary. **No sound bible exists** (`design/audio/sound-bible.md` and `design/gdd/sound-bible.md` were not present when this was written), and Diegetic Audio and Event Feedback & Cues (Vertical Slice) are not designed, so every row is provisional.
>
> **Rules**: every HUD cue is owner-only (or team-only for team lines), **2D on the UI bus**, never spatialised and never played on a pawn (VA-HUD-6); every cue has a **text twin** (its plate, card or line); **no cue of any kind on being targeted**; mood is dry, short, then silence, with no buzzer, fanfare or musical sting. In-round UI sound is **only** the cues below; menu sounds (outside a round) may be more conventional but stay quiet.

| Interaction Event | Sound Category | Text twin | Notes |
|------------------|---------------|-----------|-------|
| Result plate, per outcome (Target exiled, Wrong target, No contact, Reversed) | UI Result, one id per outcome | The plate | Fires at the plate show time, never before contact; `WrongTarget` is identical for any victim kind |
| Defence result ("Strike reversed") | UI Result, defence | The plate | Fires with the plate |
| Tally increment and threshold | UI Tally Tick, Threshold Tick | Tally squares, numeral, triangle | Chosen by `n` alone; fired with the plate |
| Exile or removal | UI Removal | The card | Fired with the card, never with the plate |
| Stage completion | UI Stage Tone | Stage card turns over, own step fills | Once per own counted completion |
| Rival rise | UI Rival Tick (team only) | Rival step fills | Once per update |
| Brace protected and protection ended | UI Brace | Indicator state | Server-confirmed |
| Brace ready-again | UI Ready (owner only, soft, 2D) | Cooling numeral resolves to Ready | Must stay unobservable to anyone else |
| Rejection | UI Reject (quiet, level, 2D) | Rejection plate | Specified by IIF UI-5; not listed in HUD VA-HUD-6 (Open Questions P4); informant plate has no sound |
| Teammate callout, sighting tick (VS) | UI Callout | Feed line, ping | Caption required |
| Being targeted | none | none | By design (ASN-R6) |
| Menu hover / focus | UI Hover (menu only) | State change | Very short, non-fatiguing on rapid navigation |
| Menu confirm / back | UI Confirm / UI Cancel (menu only) | State change | |
| Menu modal open | UI Modal (menu only) | Dialog | |
| Menu error | UI Error (menu only) | Error text | A short dull tone, never a harsh buzzer |

All UI sounds sit on the UI bus, which the player can mute knowing every in-round cue has a text twin (accessibility A-04).

---

## Gaps & Patterns Needed

> Patterns that planned screens or Vertical Slice systems will need and that are not yet specified.

| Gap | Needed by | Notes |
|-----|-----------|-------|
| Ping layer slot and sighting feed line | Informant Pings and Team Comms (VS) | PAT-WLD-5 is a placeholder; the HUD reserves a slot inside L0 and a feed line type. |
| Team text channel and ping set | Informant Pings (VS) | Accessibility F3: the MVP has no in-game team channel. Needs an input-field and a message-line pattern. |
| Round-end reveal rows | Post-Round Reveal and Results (VS) | Staggered rows one per 300 ms, each 500 ms ease-out (art bible 7.6); one pentagon silhouette for everyone. |
| Lobby list with team and ready state | Lobby UI (VS) | PAT-MNU-6 covers the row; the screen is unspecified. |
| Host panel | Round (MVP optional) | Start and Reset buttons; console commands are the MVP path. |
| Briefing screen | Round (MVP) | One neutral screen for every participant; not designed. |
| Help screen | Settings and Onboarding | Must hold every one-time hint, the tell glossary, the controls (accessibility C-03). |
| Brightness calibration screen | Settings (VS) | A reference symbol; range limits per accessibility V-12. |
| First-launch photosensitivity notice | Settings or front end (VS) | Uses PAT-MNU-9; no owner yet (accessibility F8). |
| Onboarding primer card and practice mode | Onboarding (Alpha) | "Walk like them. Watch for who doesn't." Practice mode needs a true pause. |
| Streamer alias mask | Settings (VS) | Replaces names in labels, feed, lobby and reveal with aliases; art unchanged. |
| Key rebinding conflict and capture | Settings (VS) | PAT-MNU-5; needs a screen spec. |
| Re-readable notice log | Optional | A way to recall a missed plate without adding information; not committed (accessibility LIM-C2). |

**Inconsistencies noticed between existing sources** (not edited; see Open Questions): the rejection-plate stacking wording (P5), the missing rejection tone in the HUD cue list (P4), and the missing `ShowTeamNames` binding (P6).

---

## Open Questions / GDD follow-ups

> No GDD or art-bible file was edited. **CONFLICT** is a contradiction between documents; **GAP** is a requirement with no owner; **RULING** needs a creative decision.

| ID | Type | Question or follow-up | Owner | Deadline | Resolution |
|----|------|-----------------------|-------|----------|-----------|
| P1 | RULING | Menu control look (focus stroke 4 px plus marker, Primary inverted plate, Destructive with an upward triangle and no Cochineal) is **proposed here**; no GDD or art-bible section specifies menus. Does Destructive use Cochineal Deep (spending a reserved colour) or not? | art-director | Before the first menu spec | Proposed |
| P2 | GAP | Menu motion values (push and pop 200 ms stepped, at most 8 px) are proposed to keep L5; the template's 250 ms full slide breaks the art bible rule. Confirm. | art-director, ux-designer | Menu specs | Proposed |
| P3 | GAP | Menu hover and focus sounds are not in any audio document. Confirm they are allowed outside a round and stay on the UI bus. | audio-director | Event Feedback and Cues GDD | Unresolved |
| P4 | CONFLICT | IIF UI-5 says a quiet 2D owner-only tone plays with a rejection plate; HUD VA-HUD-6 lists every HUD cue and omits a rejection tone. Decide whether the tone exists. The accessibility SFX audit assumes it does and lists the plate as its twin. | audio-director, ux-designer | HUD re-review | Unresolved |
| P5 | CONFLICT (wording) | HUD-R5 says the bottom-left slot stacks "brace indicator, tally, rejection plate (stacked upward)"; HUD-R13 and HUD-E15 say the rejection plate sits "beside" the brace indicator. The HUD UX spec must pick one layout. | ux-designer | HUD UX spec | Unresolved |
| P6 | GAP | The `ShowTeamNames` hold input (TM CR-4) is pending user decision and is absent from IIF R15's default-binding table. PAT-WLD-2 works by focus alone until it exists. If added it needs a default binding, a toggle alternative and a place in the binding screen. | user, game-designer | Before Vertical Slice | Pending user |
| P7 | GAP | ADR-0021 states every layer below 100 is hit-test-invisible with no input, but PAT-CRD-3 (status overlay with Cancel and OK buttons, owned by Session on L4) needs input. State how input is routed on L4 and whether a Common UI activatable is permitted there, given the ban on activatables below 100. | ue-umg-specialist, technical-director | ADR-0021 amendment | Unresolved |
| P8 | CONFLICT (accessibility) | The result plate default hold (3.0 s) is below the Standard-tier 5 s guideline; the library records it as a documented deviation mitigated by `HudReadHoldMult`. The HUD GDD should state the rationale. See accessibility F2. | ux-designer | HUD re-review | Unresolved |
| P9 | GAP | The concealed grade has no non-visual twin and Reduce screen effects disables its vignette (accessibility F11). PAT-FBK-2 carries the gap until the Concealment GDD answers. | art-director, audio-director | Concealment re-review | Unresolved |
| P10 | GAP | HUD-E19's latency icon and the `HudLatencyWarnRttSec` threshold specify no colour-independent shape; confirm in the HUD UX spec. | ux-designer | HUD UX spec | Unresolved |
| P11 | RULING | In-round "Leave the round" has no GDD owner. The library specifies a confirmation that says the round continues without the player; confirm the wording and whether leaving as an Active player should become an informant or release the pawn to the crowd (Roster and Session decide). | game-designer | Lobby UI and Pause design | Unresolved |
| P12 | GAP | No pattern exists for a localisation-expanded string (plates absorb expansion, but a menu control with a long label needs a wrapping rule). Define when `/localize` first runs. | localization-lead | Before string freeze | Unresolved |
