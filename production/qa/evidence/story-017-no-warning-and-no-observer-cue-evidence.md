# Evidence: story asn-017 The target gets no warning and no observer gets a cue

Story: `production/epics/assassination-defence-reversal/story-017-no-warning-and-no-observer-cue.md`
Date: 2026-10-27. Build: `tools/dev/cycle.ps1 -Tests -NoLaunch`, `Result: Succeeded`, 387 / 387 `StealthGame.` tests
passed (`story-017-no-warning-and-no-observer-cue/cycle-tests.txt`). The measurements are in
`story-017-no-warning-and-no-observer-cue/pie-observations.txt`. The files are stamped in `stamp.txt`.

## AC1 `StealthGame.Assassination.PIE.NoWarningToVictim`

Setup: four players (2v2), a fixed 60 Hz frame rate, no emulated lag, and each case in a fresh session with its own control leg.

| Case | Entries the control lacks before contact | Displacement vs control at the last consumed move before the death write | Property changes on the victim's and the bystander's copies |
|---|---|---|---|
| (a) walking, from behind | none | 92.168 vs 92.167 cm (0.002 cm) | none outside movement (ReplicatedBasedMovement, as in the control) |
| (a) walking, from the front | none | 92.168 vs 92.167 cm (0.002 cm) | as above |
| (b) standing, from behind / front | none | 0 cm | none |
| (c) braced defender, from behind / front | none | 0 cm | none |

NEGATIVE CONTROL `NegativeControlWidgetOnRosterReceiptIsReported`: the seam's HUD line, shown on the arrival of the
owner's outcome state, is reported 133 ms before contact. Check 1 therefore fails under the seam, as it must.

Defects this story found and fixed, each first seen failing in the run logs and then passing:
1. A round-ending kill's "win" line reached the victim 150 ms before its contact. Server lines are now released by the
   per-frame hold buffer.
2. The defender's own brace crouch dropped at the Counter's staged write, before contact. It now holds until the contact on the owner and on the server.
3. The HUD role, defence and objective lines switched on the Roster informant state's arrival. They now follow the presented role.

## AC2 `StealthGame.Assassination.PIE.NoObserverCue`

Four players, events in order: a whiff, a kill, a reversal, and the deciding strike.
- The observers (the allies of both participants, the rival bystander and the listen host) logged no HUD, widget,
  camera or marker cue. They received no replicated change beyond FActionState, FDeathState, movement, the body release
  and the prototype crouch tell.
- Round end:
  - bystanders: at or after the deciding death write;
  - the attacker: on its own plate frame;
  - the eliminated victim: on its own fall frame (DeathServerTime).

No owner audio bus or haptics exist yet, so there is nothing to record for them.

## Run and observe

Run result: OBSERVED, in a 3-player PIE (captures retained under story 024's folder).
`asn-49-victim-before-contact.png` shows the victim's screen with nothing changed before contact.
`asn-49-victim-contact.png` shows the card, the role line and the round end entering together at its fall.

Self-reviewed (unattended batch).
