# Evidence: story asn-024 Result plates and the brace square at the local contact frame

Story: `production/epics/assassination-defence-reversal/story-024-owner-result-matcher-and-plate-timing.md`
Date: 2026-10-27. Build: `tools/dev/cycle.ps1 -Tests -NoLaunch`, `Result: Succeeded`, 387 / 387 `StealthGame.` tests
passed (`story-024-owner-result-matcher-and-plate-timing/cycle-tests.txt`). The PIE measurements are in
`story-024-owner-result-matcher-and-plate-timing/pie-observations.txt`. The files are stamped in `stamp.txt`.

## Automated

| AC | Test | Result and measurement |
|---|---|---|
| AC1 | `StealthGame.Assassination.PIE.KillPlateTellsOneThing.FivePlatesOnTheirContactFrameWithOneTimeline` | S: "Wrong target" +5 ms after contact, "No contact" +4 ms, "Strike reversed" +3 ms, "Wrong target" +3 ms, "Target eliminated" +3 ms (frame 20 ms). P2: "Reversed" at its contact, with the removal card on the same frame and the plate first. The two "Wrong target" strings are byte-identical. One timeline (`FOwnResultPlateTimeline`) for all five plates. |
| AC2 | `StealthGame.Assassination.PIE.OutcomePresentedAtContact.BraceEndsAndDefencePlateAndRemovalCardStartAtContactNeverOnReceipt` (PktLag 75 ms) | The defender's FDefenceResult arrived 426 ms before contact and the Counter write 177 ms before it. At both arrivals the square was still up and no plate was shown. The square was removed and "Strike reversed" entered +7 ms after contact (frame 20 ms). The victim's Roster informant state arrived 150 ms before its DeathServerTime with the card not up. The removal card and the role-change dispatch (Camera Dying) came at +0 ms. |
| AC2 negative control | `...OutcomePresentedAtContact.ReceiptSeamPutsThePlateUpBeforeContact` | With `SetPresentResultsOnReceipt` (the prototype's on-receipt behaviour) the plate is up at receipt, 433 ms before contact, and the contact-frame check FAILS, as it must. This is the regression test of the "Reversal! before contact" bug, seen failing. |
| AC3 | `StealthGame.Assassination.PIE.NoBushTargetingUi.OccupiedAndEmptyBushShowTheUnaimedWidgetListBeforeAndAfterAStrike` | The element lists for occupied, empty and baseline are identical before and after a whiff into the bush, with no hint, prompt, reticle or highlight element. |
| AC4 | `StealthGame.Assassination.OwnResultMatcher` | Covers both arrival orders, the dropped write (cap at t_arrival + 1.25 s, hard floor 1.25 s), duplicate and older-sequence drops, a late result shown on arrival, the press baseline, the reversed attacker gated on its role change, the F3 timeline and the brace end at contact. |

## Run and observe (3-player PIE, floating windows, StealthBot drivers)

Run result: OBSERVED. Each plate appears on the attacker's own screen in the lower-centre Result slot with one size,
one glyph slot and one entrance, and only the text and the glyph differ:

- `asn-16-npc.png`: "Wrong target" with the cochineal triangle; the tally ticks to 1 with the plate.
- `asn-16-whiff.png`: "No contact", with an empty glyph slot and no cochineal.
- `asn-16-reversed.png` (host, the reversed attacker): "Reversed" and the removal card "You have been eliminated" on one frame.
- `asn-16-defender-strike-reversed.png`: the defender's "Strike reversed" plate; the square is gone and the line reads "recovering".
- `asn-16-informant.png`: "Wrong target", byte-identical to the NPC plate; the tally reads 2.
- `asn-16-rival.png`: "Target eliminated"; the round banner is released with it.
- `asn-49-defender-before-contact.png`: braced, after the strike executed, "Defence: PREPARED" and no plate.
- `asn-49-defender-contact.png`: on the contact frame, the plate is entering (stepped) and the square is removed.
- `asn-49-victim-before-contact.png`: the victim is still "ACTIVE", the round is Active and no card is shown.
- `asn-49-victim-contact.png`: at its fall, the removal card, the INFORMANT line and the round end enter together; none of them came earlier.
- `asn-40-occupied.png` / `asn-40-empty.png`: the camera is on the same bush with an enemy hidden in it and then empty. The HUD is identical, with no hint, prompt or reticle. The world exposure differs between the two captures (the sample's auto-exposure), not the HUD.

Note: in the editor's unfocused PIE windows the frame rate is low (about 4 fps; "Use Less CPU when in Background"),
so the log shows plates about 0.27 s after contact. That gap is the next rendered frame after contact. The
frame-accurate timing is measured in the automated PIE tests above at 50 fps.

The receipt-time captures cannot be timed by hand inside the 66 ms window between arrival and contact. The arrival
states are asserted frame by frame in `OutcomePresentedAtContact`.

## Sign-off

Self-reviewed (unattended batch). The UI is the canvas stand-in for the HUD epic's UMG plates (hud stories 015, 016 and
022); layout follows `design/ux/hud.md` E07 and E08.
