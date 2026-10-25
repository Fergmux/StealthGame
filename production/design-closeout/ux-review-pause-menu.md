## UX Review: Pause Menu (in-session Escape menu)
**Date**: 2026-10-25
**Reviewer**: ux-review skill
**Document**: design/ux/pause-menu.md
**Checklist**: 3A (Template line "UX Spec")
**Platform Target**: PC (Windows); keyboard/mouse primary, gamepad partial, touch none (technical-preferences.md; project.yaml has no platform block)
**Accessibility Tier**: Standard with named elevations (design/accessibility-requirements.md, committed)

### Completeness: 15/15 sections present (plus a Pillar 1 rules section)
All required sections present; error and empty states stated as n/a with reasons; loading handled as the overlay-beneath variant; 17 acceptance criteria at review time including performance.

### Quality Issues: 2 blocking, 5 advisory
1. **PM-R1/PM-1 still read as unresolved after D14** BLOCKING. The Purpose said the documents "cannot yet deliver" the no-tell rule, PM-R1 said "not fully achievable", and PM-1 was typed "RULING (blocks the tell criterion)", while PM-R3, the AC and D14 say it is resolved. Fix: Purpose, PM-R1 and PM-1 synced to D14 (head scan after `IdleGazeDelaySec` 3 s; a stand over 20 s is the accepted player-chosen risk). APPLIED.
2. **Panel overlap rule contradicts the HUD spec** BLOCKING. This spec accepted the panel overlapping the bottom-left cluster from text scale 125 %, while hud.md proves a 76 px clearance at every scale assuming the panel bottom is at or above 70 % height. Fix: panel bottom capped at y = 756 at every scale (top may rise to the safe margin, then scroll); PM-13 closed; clearance AC added. APPLIED.
3. Mis-cited accessibility M-12 (it concerns Strike) ADVISORY. Corrected. APPLIED.
4. PM-4 stale (D15 resolved the L4 input conflict) ADVISORY. Updated. APPLIED.
5. No resolution AC; no-tell AC named parameters without values ADVISORY. Added AC and values (3 s, 3 to 7 s, 20 s). APPLIED.
6. Unticked gamepad "none used" item ADVISORY. Ticked. APPLIED.
7. Player journey map absent ADVISORY (PM-10, open).

### GDD Alignment: ALIGNED
- Accessibility C-02 mitigation, interaction-patterns PAT-MNU-9/-13, Session rules 4, 19, 24, 25f, IIF/ADR-0021 s7 menu-open sequence, SCL R8.9 / AC-SCL-49 (D14) all reflected. No UI requirement left uncovered; no game state shown.

### Accessibility: COMPLIANT at Standard (Comprehensive screen-reader row T-12 NOT ASSESSED, engine support not sourceable)
- Focus order, contrast, no hue, reduce-motion variants, no timed dismissal, pointer-free operation all specified. Open: controller disconnect with menu open (PM-14).

### Pattern Library: CONSISTENT
- PAT-MNU-1 Destructive for Leave and Quit with PAT-MNU-9 confirmations (default focus Cancel); PAT-MNU-11/-12/-13 used as defined. (Contrast: main-menu's Quit is Secondary, tracked as MM-15.)

### Verdict: NEEDS REVISION
**Blocking issues**: 2 (revised in place after the verdict; verdict not changed)
**Advisory issues**: 5
**Dimensions not assessed**: 0

Revisions applied to design/ux/pause-menu.md: Purpose/PM-R1/PM-1 wording, panel clearance rule, PM-4 and PM-13 closed, M-12 citation, gamepad tick, two AC edits, status line.
