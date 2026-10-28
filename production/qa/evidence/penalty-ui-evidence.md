# Penalty UI evidence and sign-off (story nkp-018)

Story: `production/epics/npc-kill-penalty-informants/story-018-tally-countdown-and-exile-card-ui.md` (UI, BLOCKING).
Captured 2026-10-27, 3-player listen-server PIE on L_Prototype (host #0 and #2 Jaguar / team 0, #1 Eagle / team 1),
floating 640x480 PIE windows, driven through the Unreal MCP (`StealthPlayToolset.RunPlayConsoleCommand` with
`StealthBot`), windows captured with `tools/dev/capture-window.ps1` (and a burst variant of it). Every image was looked at;
none is black. Images: `production/qa/evidence/npc-kill-penalty-informants-016-022-2026-10-27/`.

Widgets: the minimal production Slate overlay `MakePenaltyHudOverlay` (StealthPresentation, `View/PenaltyHudViewModel.h`)
painting `FPenaltyTallyView`, `FPenaltyExileCardView` and `FPenaltyCountdownView`, fed by
`AStealthPlayerController::BuildPenaltyHudFrame`. The HUD epic restyles them (CommonUI, layout); the wording is the GDD's
(D11).

## AC1 / AC-NKP-19: tally line (L = 3)

| Capture | Shows |
|---|---|
| `nkp-19-0.png` | "Wrong targets: 0", three empty Bone squares, no triangle |
| `nkp-19-1.png` | after an NPC kill: 1 square filled (Cochineal), "Wrong targets: 1", no triangle; the "Wrong target" plate |
| `nkp-19-2.png` | after a second NPC kill: 2 filled, the static danger triangle (n >= L - 1) |
| `nkp-19-3-informant-kill.png` | the solo player at tally 3 through an INFORMANT kill (k stays 2, still Active): 3 filled, triangle |
| `nkp-19-4-informant-kill.png` | tally 4 while Active (second informant kill): 3 filled (min(n, L)), numeral 4, triangle, no overflow glyph |
| `nkp-19-frozen.png` | exiled at tally 3: frozen 3 squares, numeral 3, NO triangle (ruling D4), "INFORMANT" panel |

The limit is never written. Widget-state parity of the two victim kinds at the same tally: the dump
(`FPenaltyTallyView::Dump`: text, squares, triangle, frozen, colours) is byte-equal for an NPC kill and an informant kill
(`StealthGame.Penalty.PIE.TallyWidgetParity.NpcKillAndInformantKillDumpEqualAtSameTally`), and the view-model takes no victim
kind (`StealthGame.Penalty.HudViewModel`).

## AC2 / AC-NKP-20: the exile moment (frame strip)

Screen capture of a background PIE window refreshes at about 3 fps, so the strip shows the order and the states, not the
exact frames; the frame-exact times are the PIE test's (`StealthGame.Penalty.PIE.ExileBeatTiming.*`: the plate at its F2 show
time, the card at s + ExileBeatSec within one frame, for RPC lateness 0 / 300 / 900 ms).

| Capture | Shows |
|---|---|
| `nkp-20-1-plate-at-s.png` | the "Wrong target" plate at the local contact frame, tally ticked to 3 with the triangle |
| `nkp-20-2-plate-before-card.png` | the plate still shown, no card (the held breath of ExileBeatSec) |
| `nkp-20-3-card-entering-plate-pushed-off.png` | the card entering ("Informant" / "Too many wrong targets" / "You watch for your team now."), the plate gone from the Result slot (pushed off, no cross-fade) |
| `nkp-20-4-card-hold.png` | the card holding (later build: no other text block behind it) |
| `nkp-20-5-collapse-start.png`, `nkp-20-6-collapsing-to-brackets.png`, `nkp-20-7-collapse-end.png` | the card collapsing towards the informant panel (top left) and gone |

The card is about 560 x 200 at 1080p (well under 30 % of the screen) and is shown once per exile
(`ExileCardEnteredAt`, reset only when the presented role returns to Active). Not observed: the "disconnect and reclaim
during the card's hold" clause (Session's PIE rejoin is not built; a rebuilt controller starts with no card and no result
is replayed, so the card cannot re-show, by construction).

Found and fixed during the run: the first build showed the countdown behind the card during its hold (two text blocks at
once): the countdown now waits for the card's collapse (`nkp-20-1..3` are from that first build; the card and plate frames
are unaffected).

## AC3 / AC-NKP-31: returning countdown

| Capture | Shows |
|---|---|
| `nkp-31-owner-returning-in-6.png`, `nkp-31-owner-returning-in-5.png` | the exiled owner: "Returning in 6", then "Returning in 5" (whole seconds, Bone on Obsidian, Notice slot), the view still on the released body |
| `nkp-31-returning-static-at-0.png` | at T_due while the takeover picks: the static "Returning" (no ellipsis, no count-up, no "searching") |
| `nkp-31-ally-no-countdown.png` | the ally (host) at the same moment: no countdown |
| `nkp-31-opponent-no-countdown.png` | the opponent (Client 1) at the same moment: no countdown |
| `nkp-44-1-killed-informant-falling-body-countdown.png`, `nkp-44-2-killed-informant-countdown.png` | a killed informant: no card, the countdown after the fall and fade gate |

After a reclaim at time t the display is ceil(T_due - t) because it is a pure function of the carried owner-part T_due and
now (`StealthGame.Penalty.HudViewModel` checks ceil, the static "Returning" at <= 0 and the reclaim case; the countdown appears
only after the fade, `StealthGame.Penalty.PIE.InformantKilledView`).

## Photosensitivity (OQ-NKP-6, art bible 7.6 rule 7)

Not analysed: still open (no flash analysis tool in the project). The sequence has no flash: the plate is pushed off, the
card eases in over 0.6 s, holds, and collapses over 0.8 s.

## Sign-off

| Role | Verdict | Date |
|---|---|---|
| UI evidence captured (dev-story, unattended) | Captured | 2026-10-27 |
| UX / HUD lead review | [ ] Pending (no human reviewer in the unattended run) | |
