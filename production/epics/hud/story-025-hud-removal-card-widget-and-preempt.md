# Story 025: Removal card widget, approved wording and the stepped preempt of the Result slot

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: The removal card lives on L3 (`UStealthHudCardLayer`, ZOrder 50) and the Result slot on L2 (`UStealthHudTransientLayer`, ZOrder 40), both `UUserWidget` layers created once at HUD init, collapsed until needed and added with `AddToPlayerScreen`. Widgets are dumb: they receive a view-model sub-struct and apply it only when it differs from the last applied value. Card and plate timelines are sampled in C++ from `now - t0` by `FHudPresenterCore` and never from widget animation assets.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 engine fact E1: `AddToPlayerScreen` widgets sit in the Player slot (100), below `AddToViewport` content, so the card must never be added with `AddToViewport`. E2: a camera fade is applied inside the scene render and can never cover UMG, which is why the card (L3, ZOrder 50) sits above the HUD-owned fade layer (ZOrder 20) and stays readable while the world goes black. Common UI is not involved: the card is a plain `HitTestInvisible` overlay, never a `UCommonActivatableWidget`. The display face is the art-director's pick from story 017 (HUD-UX-8); the card size is computed from the widest subtitle so it is cause-independent. `UE_LOGF` is preferred over `UE_LOG` in 5.8 for any debug log.

**Control Manifest Rules (this layer)**:
- Required: Widgets are dumb (no UMG property bindings, no Blueprint logic beyond layout and style); `FHudViewModel::Dump()` is deterministic — source: ADR-0021
- Forbidden: Never sample plate or card timelines from widget animation assets — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story is the visual half of the removal card. Story 024 decides when the card enters, holds and collapses and fires `OnRemovalCardEntered`; this story draws it with the user-ratified wording of 2026-10-25 (D11, D29) and makes the Result-slot plate leave in steps when the card enters, so that after 0.25 s exactly one text block is on screen in the central region.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (HUD-R9, F3, F4) and UX spec `design/ux/hud.md` (UXA-HUD-12, -13, -14, -19), scoped to this story:*

- [ ] **AC1** (UXA-HUD-19; D11, D29; retained screenshots `hud-card-exile-1080p.png`, `hud-card-assassinated-1080p.png`): **GIVEN** an exile card, an assassination card, a reversal card and a rival-kill plate **WHEN** each is shown **THEN** the card title is "Informant" for every cause; the subtitle is "Too many wrong targets" after an exile and "Eliminated for the round" after an assassination or reversal; the second line is "You watch for your team now."; the title bar is opaque Cochineal Deep with Bone text and an upward stepped triangle; the word "Exiled" appears on no card; a rival kill shows the plate "Target eliminated".
- [ ] **AC2** (UXA-HUD-12, -13, -14; the central band is a derived definition for the QA oracle; frame-stepped capture at 60 Hz): **GIVEN** an exile with a plate in the Result slot **WHEN** the card enters **THEN** the card covers at most 30 % of the screen (reference 4.9 %) and is never full-bleed; it enters in 0.6 s, holds 4.0 s and collapses into the informant brackets over 0.8 s; the Result-slot plate leaves over 0.25 s in steps (4 equal alpha steps, not a cross-fade); and from `t_card + 0.25 s` the dump lists exactly one text-bearing element intersecting the central band (x 20-80 %, y 30-70 % of the screen; y 324-756 at 1080p): the card.

---

## Implementation Notes

*Derived from ADR-0021 Decision §1 and §4, GDD HUD-R9 / F3 / F4 and UX element E10:*

**What to build.** Two pieces of dumb widget code in `StealthPresentation`, both applying view-model sub-structs produced by `FHudPresenterCore` (stories 016 and 024):

1. The removal card inside `UStealthHudCardLayer` (L3, ZOrder 50), created once at HUD init and collapsed until the card state is `Entering`. Per UX E10: one card 640 x 160 centred at `(960, 464)` at 1080p, a title bar 640 x 56 in opaque Cochineal Deep holding an upward stepped triangle (40 px, Bone) and the title (Heading 28 px, Bone), beneath it an Obsidian body plate 640 x 104 with the cause subtitle (Primary 22 px Semibold Bone) and the second line (Body 20 px Bone). No striped band. Plate style (4 px stepped corners, 2 px stroke, 2 px Obsidian keyline) and the display face come from story 017's shared style asset; do not restyle here. The card rectangle is computed once from the widest subtitle ("Too many wrong targets" vs "Eliminated for the round") so its size never depends on the cause; the 4.9 % reference is 640 x 160 over 1920 x 1080.
2. The Result-slot preempt in `UStealthHudTransientLayer` (L2, ZOrder 40). When the card state enters `Entering`, the presenter core sets the plate's `t_out := t_preempt` and `T_out := HudPreemptExitSec` (story 016's F3 branch). This story's widget only applies the resulting render opacity from the view model; the stepped curve `alpha = a0 x (1 - floor(4x/T)/4)` is computed in the core, never in the widget. The widget must therefore apply opacity changes only when the value differs and must not interpolate.

**Strings.** All text from the `ST_Hud` string table (story 005): `Hud.Card.Title` ("Informant"), `Hud.Card.Sub.Exile` ("Too many wrong targets"), `Hud.Card.Sub.Removed` ("Eliminated for the round"), `Hud.Card.Line2` ("You watch for your team now."), `Hud.Result.RivalEliminated` ("Target eliminated", renamed from the earlier rival-kill wording by D29: "exile" is reserved for the NPC-kill removal). The view model carries the subtitle choice as an enum, not text (never carry text on the wire; the same applies inside the presenter: the widget resolves `FText` from the table). The subtitle follows the cause carried by F4 (exile versus assassinated or reversed); the title never changes (D11).

**Timeline numbers (read from tuning, never hardcoded).** `HudRemovalCardInSec` 0.6 (eased fade in), `HudRemovalCardHoldSec` 4.0 (multiplied by `HudReadHoldMult`), `HudRemovalCardOutSec` 0.8 (alpha 1 to 0, ease-in-out), `HudPreemptExitSec` 0.25. These are story 001's keys; this story reads them through the client tuning face. With Reduce motion the entrance and exit last at most one frame (story 016); the hold is unchanged.

**Layering and visibility.** The card is L3, above the HUD fade layer (ZOrder 20) and below the L4 status overlays, so it stays readable when Camera's fade reaches black (I-HUD-6 guarantees the card is fully in before the screen is black). The card is `HitTestInvisible`, not focusable, never an activatable. It is created once; toggling is by visibility only. "Entering", "Holding" and "Exiting" are view-model phases; the widget never infers phase.

**Dump and the central band.** `FHudViewModel::Dump()` and the widget-tree capture used by the leak suite must list text-bearing elements with their screen rectangles, so the QA oracle can count elements intersecting the central band (x 20-80 %, y 30-70 %; y 324-756 at 1080p). This definition is derived for this story (the UX text says "exactly one text block is on screen in the central region once 0.25 s have passed"); record it in the evidence sign-off. The informant-plate and banner cases are not part of this oracle.

**Traps.**
- Never replay the card: shown once per removal, never after a reconnect (story 024 owns the once-only rule; the widget must not show on rebuild).
- The word "Exiled" must appear on no card, string table entry or dump: the language lint of story 005 enforces it, and the screenshot confirms it.
- The Result-slot plate leaves in four alpha steps, never a cross-fade (changes Penalty's earlier "cross-fade" wording, CR-HUD-6).
- No field names the killer, its kind or its team; there is no kill cam.

**Prototype.** The prototype `Source/StealthGame/Private/StealthHUD.cpp` canvas (`AHUD`) draws no card; nothing is promoted. Removal of the prototype is story 059.

**Decisions applying:** D11 (removal wording), D29 ("Target eliminated"), D16 (display face and re-measured widths belong to story 017).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`: F4 card time, gating, `OnRemovalCardEntered`, never-again rule (logic).
- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`: the result and defence plate widgets and their text, stamp and cue id.
- `production/epics/hud/story-026-hud-removal-card-pie-timing.md`: PIE proof of card timing, the informant-killed case and the hold cap.
- `production/epics/hud/story-034-hud-informant-presentation.md`: informant brackets, the frozen tally and the countdown slot the card collapses into.
- `production/epics/hud/story-055-hud-photosensitivity-removal-sequence.md`: the budgeted removal sequence (grade, plate, preempt exit, card, camera fade).
- `production/epics/hud/story-059-hud-retire-prototype-canvas-and-message-plumbing.md`: deleting the prototype HUD.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudCardLayer.h` and `.cpp` (L3 layer, removal card sub-widget; the card Widget Blueprint is layout and style only, named in prose, no `Content/` path)
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudTransientLayer.cpp` (apply the Result-slot preempt opacity from the view model; widget created by story 022)
- `Source/StealthPresentation/Private/Hud/HudViewModel.h` (card sub-struct fields consumed by the widget, if not already present from story 024)
- `production/qa/evidence/hud-card-exile-1080p.png`, `production/qa/evidence/hud-card-assassinated-1080p.png`, `production/qa/evidence/hud-removal-card-widget-and-preempt-evidence.md` (retained evidence, not code)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Card wording (Manual check)
  - Setup: trigger an exile card, an assassination card and a reversal card, plus a rival-kill plate.
  - Verify: the title is "Informant" for every cause. The subtitle is "Too many wrong targets" after an exile and "Eliminated for the round" after an assassination or reversal. The second line is "You watch for your team now." The title bar is opaque Cochineal Deep with Bone text and an upward stepped triangle. "Exiled" appears on no card. The rival-kill plate reads "Target eliminated".
  - Pass condition: all wording matches exactly in `hud-card-exile-1080p.png` and `hud-card-assassinated-1080p.png`.
- **AC-2**: Card size, timing and preempt (Manual check)
  - Setup: trigger an exile with a result plate in the Result slot, and record a frame-stepped capture at 60 Hz.
  - Verify: the card covers at most 30 % of the screen (reference 4.9 %) and is not full-bleed. It enters in 0.6 s, holds 4.0 s and collapses into the brackets over 0.8 s. The plate leaves in 4 equal alpha steps over 0.25 s (no cross-fade). From `t_card + 0.25 s` the dump lists exactly one text-bearing element intersecting the central band (x 20-80 %, y 30-70 %; y 324-756 at 1080p): the card.
  - Pass condition: all four checks hold.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots `production/qa/evidence/hud-card-exile-1080p.png` and `production/qa/evidence/hud-card-assassinated-1080p.png`, with sign-off in `production/qa/evidence/hud-removal-card-widget-and-preempt-evidence.md` (gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`, `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`
- Unlocks: `production/epics/hud/story-026-hud-removal-card-pie-timing.md`, `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`, `production/epics/hud/story-055-hud-photosensitivity-removal-sequence.md`, `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`
