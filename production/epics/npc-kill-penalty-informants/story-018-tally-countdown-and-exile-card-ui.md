# Story 018: Tally line, returning countdown and exile card

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Blocked — BLOCKED only for AC2's reconnect clause ("after a disconnect and reclaim during the card's hold, the card is not shown again"): Session's PIE rejoin (session-travel 026-027) is not built, so it is shown by construction, not observed. AC1 and AC3 pass with retained screenshots; the UX / HUD lead review in `production/qa/evidence/penalty-ui-evidence.md` is pending (no human reviewer).
> **Layer**: Feature
> **Type**: UI
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-28

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§2.2, §6: the data Penalty supplies), with ADR-0001 (owner-only audiences) and ADR-0021 (HUD layering and CommonUI; owns the presentation)
**ADR Decision Summary**: Penalty supplies owner-only data to HUD (`TallyAfter` per strike in `FStrikeResult`, the latched `L`, `bExiled`, and `T_due` on the C-TeamView owner part) and draws nothing: every element is screen-space and laid out by the HUD GDD. The card, the plate push-off and the countdown follow HUD's own rules; the countdown is a live count from the carried `T_due` that reads a static "Returning" at 0.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Risk is the highest of the governing ADRs (ADR-0001 HIGH). The widgets are CommonUI/UMG under ADR-0021, which `StealthPresentation` builds from `View/` interfaces only (it can never include Penalty's server headers: IVP AC-IVP-17). Engine 5.8 unifies Enhanced Input and Common UI (VERSION.md): follow the project's UI engine notes before implementing. The cited `design/ux/hud.md` UX spec is not yet authored at the time of writing: stories cite it when it exists (the GDD's UX flag says to run `/ux-design` for the HUD before HUD epics).

**Control Manifest Rules (this layer)**:
- Required: HUD presentation builds only from `View/` interfaces; owner-only data is consumed only on the owning client — source: ADR-0007, ADR-0021
- Forbidden: presentation modules never include Penalty's server headers; no element relies on colour alone (squares plus numeral plus triangle) — source: GDD Accessibility, IVP AC-IVP-17
- Guardrail: the dumped widget state for an NPC kill and an informant kill at the same tally is byte-equal — source: GDD AC-NKP-19

These three UI elements are the only places the player meets the penalty. They must read identically for every victim kind, never state the strike limit in words, never count up or say "searching" while a pick is retrying, and show the exile card exactly once.

**Prototype replaced:** the prototype's `AStealthHUD` banner messages (`MessagePlayer` text such as "A civilian. Civilians killed: n/limit." and "Exiled for killing n civilians. You are now an informant: watch, and report to your team."). The prototype's wording names a strike counter and the exile cause in a way the GDD replaced; the HUD text is not promoted.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-19, AC-NKP-20, AC-NKP-31), scoped to this story:*

- [x] **AC1** (AC-NKP-19 as clarified at QL-STORY-READY, [UI/BLOCKING] tally line, PEN-R15): **GIVEN** L = 3 **WHEN** the owner's tally is 0, 1, 2, 3 and 4 while Active, and frozen at the exile tally of 3 for the rest of the round as an informant **THEN** the counter reads per UI-NKP-1 at each value, the `t >= L - 1` danger triangle appears identically for both victim kinds (and is absent while frozen as an informant), and the dumped tally widget state (text, filled squares, triangle visibility, colours) is equal between an NPC-kill and an informant-kill run at the same tally (retained screenshots `nkp-19-*.png` of each value as evidence).
- [ ] **AC2** (AC-NKP-20 as clarified at QL-STORY-READY, [UI/BLOCKING] exile moment, PEN-R16): **GIVEN** an exiling kill **WHEN** captured on the owner's client at a fixed capture schedule (plate shown at `s`; `s + ExileBeatSec - 1 frame`; `+1 frame`; `+HudPreemptExitSec`; during the card's hold; at the end of the collapse) **THEN** the `WrongTarget` plate is shown, then the plate is pushed off as the removal card ("Informant" / "Too many wrong targets") enters (no cross-fade), then the card collapses into the informant frame, and the card is shown once; **AND** after a disconnect and reclaim during the card's hold, the card is not shown again (retained frame-strip screenshots `nkp-20-*.png`, one file per capture point).
- [x] **AC3** (AC-NKP-31 as clarified at QL-STORY-READY, [UI/BLOCKING] returning countdown, UI-NKP-4, PEN-R15/R16): **GIVEN** `T_due` 15 s ahead **WHEN** the awaiting informant's screen is captured **THEN** "Returning in {s}" counts down in whole seconds, reads a static "Returning" (no ellipsis, no count-up, no "searching" wording; UI-NKP-4) at 0 while picking; after a reclaim at time t the display equals `ceil(T_due - t)` whole seconds, or the static "Returning" when that is <= 0; **AND** captures of every other connection show no countdown text (retained screenshots `nkp-31-*.png`).

---

## Implementation Notes

*Derived from GDD UI Requirements UI-NKP-1 to UI-NKP-5 and ADR-0019 §2.2, §6:*

HUD owns the widgets; this story's deliverable is the verified *data contract and capture evidence* for the three Penalty-fed elements, plus any glue the HUD epic's stories leave to Penalty. The UI elements per the GDD:
- **UI-NKP-1 tally**: the line "Wrong targets: {n}" with `L` stepped squares filled up to `min(n, L)` (Cochineal filled, Bone outline); at `n ≥ L − 1` a static upward danger triangle that stays for `n > L`; numeral and fill are the non-colour backups; the limit is never written in words; no overflow glyph; frozen without the triangle while Informant (HUD-R7 ruling D4). Cue: one tick per increment, a threshold variant once `n ≥ L − 1`, chosen by `n` alone.
- **UI-NKP-3 exile card**: "Informant" / "Too many wrong targets" / "You watch for your team now." at the plate's show time plus `ExileBeatSec`; the plate is pushed off (HUD-R9, no cross-fade), the card is ≤ 30 % of the screen and collapses into the informant brackets; once, never replayed on reconnect; a killed informant gets no card.
- **UI-NKP-4 countdown**: Bone-on-Obsidian "Returning in {s}", whole-second steps from the carried `T_due` (story 011), resuming after reclaim or rejoin; at 0 a static "Returning"; shown on the neutral view only after the fade completes; no pulse, no colour change near zero.

Evidence procedure follows `.claude/docs/run-and-observe.md` for Unreal: launch PIE 2-client, drive the states through the dev bot driver (`StealthBot <player#> <strike_npc|strike_enemy|strike_informant ...>` exists in the prototype), capture owner-client screenshots at each tally value 0-4, the exile sequence, the returning countdown, and the ally/opponent connections, and retain them under `production/qa/evidence/` with the story's sign-off file. The widget-state dump equality (NPC kill vs informant kill) is a client-side test accessor assertion on the view model, not a pixel diff.

Photosensitivity: the sequence grade + plate + push-off + fade + card falls within 1.4 s and must fit the art bible 7.6 rule 7 budget (OQ-NKP-6, shared with HUD VA-HUD-4); note the analysis result in the sign-off file if available, or record it as still open.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- hud epic: the widgets, layout, scaling and CommonUI wiring themselves.
- `story-017-owner-presentation-timing.md`: timestamp timing of the same sequence.
- `story-011-t-due-owner-carrier-adapter.md`: the `T_due` data path.
- Art bible change requests CR-16 (art-director): applied in the art bible, not here.

---

## Files

*Files changed by /dev-story (2026-10-27). Minimal production widgets (Slate, C++), restyled later by the HUD epic; the screenshots are in the batch evidence folder, not loose in `production/qa/evidence/`.*

- `Source/StealthPresentation/Public/View/PenaltyHudViewModel.h`, `Source/StealthPresentation/Private/View/PenaltyHudViewModel.cpp` (FPenaltyTallyView / FPenaltyExileCardView / FPenaltyCountdownView and the Slate overlay `MakePenaltyHudOverlay`)
- `Source/StealthPresentation/StealthPresentation.Build.cs` (SlateCore public, Slate private)
- `Source/StealthPresentation/Private/Tests/PenaltyHudViewModelTest.cpp` (`StealthGame.Penalty.HudViewModel`)
- `Source/StealthGame/Public/StealthHUD.h`, `Source/StealthGame/Private/StealthHUD.cpp` (the overlay added to the owner's viewport; the canvas tally line removed; the plate pushed off by the card; the HUD element list carries the Penalty elements)
- `Source/StealthGame/Private/StealthPlayerController.cpp` (`BuildPenaltyHudFrame`; the countdown gate)
- `Source/StealthGameplay/Features/Assassination/Public/View/OwnResultPlateSettings.h` (STAND-IN HUD keys HudPreemptExitSec, HudRemovalCardInSec / HoldSec / OutSec)
- `Source/StealthTestHarness/Private/Tests/PIE/Penalty/TallyWidgetParityPieTest.cpp` (`StealthGame.Penalty.PIE.TallyWidgetParity.NpcKillAndInformantKillDumpEqualAtSameTally`)
- `Source/StealthTestHarness/Private/Tests/PIE/Assassination/NoObserverCuePieTest.cpp` (an awaiting informant observer's own countdown ticks are allowed: time-driven, never caused by the observed event)
- `production/qa/evidence/penalty-ui-evidence.md` (sign-off), `production/qa/evidence/npc-kill-penalty-informants-016-022-2026-10-27/nkp-19-*.png`, `nkp-20-*.png`, `nkp-31-*.png`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-19) Manual check: tally line
  - Setup: PIE with L = 3 on the owner client. Script tally 0, 1, 2, 3 and 4 as an Active player, once with NPC kills and once with informant kills (tally 4 while Active needs informant kills so k stays below L). Exile at tally 3 for the frozen case.
  - Verify: capture each value (`nkp-19-0..4.png`, `nkp-19-frozen.png`) and dump the tally widget state.
  - Pass condition: text and filled squares match UI-NKP-1 at each value; the triangle shows at t >= 2 and is absent at 0, 1 and when frozen; the dumped text, squares, triangle visibility and colours are equal between the NPC and informant runs at the same tally; after the exile it stays frozen at 3.
- **AC2** (AC-NKP-20) Manual check: exile moment
  - Setup: an exiling kill on the owner client, captured at a fixed schedule: `s`, `s + ExileBeatSec - 1 frame`, `+1 frame`, `+HudPreemptExitSec`, during the card hold, end of the collapse. A second capture follows a disconnect and reclaim during the hold.
  - Verify: the `nkp-20-*.png` frame strip, one file per capture point.
  - Pass condition: at `s` the plate is shown; just before the beat ends the plate is still shown and there is no card; at `+1 frame` the card is entering as the plate is pushed off with no frame showing both semi-transparent (no cross-fade); at `+HudPreemptExitSec` the card reads "Informant" / "Too many wrong targets"; at the end of the collapse the informant frame; the card appears once and not again after the reclaim.
- **AC3** (AC-NKP-31) Manual check: returning countdown
  - Setup: an awaiting informant with `T_due` 15 s ahead, captured after the fade completes, on its own client and on an ally's and an opponent's. Include picking at 0 (fake LSC returns `None`) and a disconnect with a reclaim at time t.
  - Verify: captures at several remaining times (`nkp-31-*.png`).
  - Pass condition: the text is "Returning in {s}" in whole seconds and never counts up; at <= 0 it is exactly "Returning" (no ellipsis, count-up or "searching" wording); after the reclaim it equals `ceil(T_due - t)`; the ally and opponent captures show no countdown text.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen touched, in `production/qa/evidence/` (`nkp-19-*`, `nkp-20-*`, `nkp-31-*`), with the sign-off file `production/qa/evidence/penalty-ui-evidence.md`

**Status**: Created 2026-10-27: retained screenshots `nkp-19-*`, `nkp-20-*`, `nkp-31-*` (evidence folder below), `production/qa/evidence/penalty-ui-evidence.md`; tests `StealthGame.Penalty.HudViewModel`, `StealthGame.Penalty.PIE.TallyWidgetParity.*` pass. Evidence: `production/qa/evidence/npc-kill-penalty-informants-016-022-2026-10-27/evidence.md`.

Run result: OBSERVED — tally 0-4 and frozen, the exile plate / card / collapse strip and the returning countdown (owner only) in 3-player PIE — `production/qa/evidence/npc-kill-penalty-informants-016-022-2026-10-27/nkp-19-2.png` and the files listed in `penalty-ui-evidence.md`.

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-011-t-due-owner-carrier-adapter.md`, `story-017-owner-presentation-timing.md`, `story-003-tally-rule-and-strike-result-supply.md`; cross-epic: hud (UI-NKP-1 to UI-NKP-4 widgets, HUD-R7, HUD-R9, HUD-E11), camera-system `story-017-view-state-neutral-view-fade.md`, `story-018-dying-hold-and-view-release.md`
- Unlocks: None
