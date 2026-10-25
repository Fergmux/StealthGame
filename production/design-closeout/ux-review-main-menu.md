## UX Review: Main Menu
**Date**: 2026-10-25
**Reviewer**: ux-review skill
**Document**: design/ux/main-menu.md
**Checklist**: 3A (Template line "UX Spec")
**Platform Target**: PC (Windows); keyboard/mouse primary, gamepad partial, touch none (technical-preferences.md Input Methods; project.yaml has no platform block)
**Accessibility Tier**: Standard with named elevations (design/accessibility-requirements.md, committed)

### Completeness: 15/15 sections present
Header, Purpose, Arrival context, Navigation, Entry/Exit, Layout (wireframe, zones, 12-row inventory, hierarchy), States (loading, empty, error, rejection x6), Interaction Map, Data, Events, Transitions, Input Method checklist, Accessibility, Localization, Acceptance Criteria (15 at review time).

### Quality Issues: 2 blocking, 5 advisory
1. **Join (M7) enablement contradicts itself** BLOCKING. Component row said disabled while M6 is "empty or invalid"; the Invalid-address state and the Error AC require M7 enabled and the press to show M12. Fix: disabled only while empty. APPLIED.
2. **Mis-cited M-12 and undocumented Quit exception** BLOCKING. Accessibility M-12 is Strike mis-press protection, not Quit; and Quit is irreversible yet Secondary with no dialog, against PAT-MNU-1 ("Destructive, no exceptions") and the pause menu's Quit. Fix: citation corrected, exception stated, MM-15 opened. APPLIED.
3. Purpose said the screen "says nothing about ... in a round" while the mandated `InProgress` sentence says a round has begun ADVISORY. Wording qualified. APPLIED.
4. No resolution AC and no Busy-retry AC ADVISORY. Both added. APPLIED.
5. Unticked gamepad "none used" item ADVISORY. Ticked. APPLIED.
6. Build label 18 px vs V-01 24 px menu minimum ADVISORY (existing MM-12).
7. Player journey map absent ADVISORY (existing MM-1).

### GDD Alignment: ALIGNED
- Session & Travel UI Requirements: main menu, status overlays and all six rejection sentences covered; sentences verified verbatim against the GDD. Minimal lobby panel and overlay specs are still unwritten (MM-11, outside this screen).

### Accessibility: GAPS (documented, owned elsewhere)
- Standard tier met on focus order, contrast, no hue use, reduce motion hooks. Open: gamepad text entry / controller disconnect (MM-7), first-launch photosensitivity notice has no owner (MM-10, accessibility F8), screen reader NOT ASSESSED (Comprehensive elevation only).

### Pattern Library: INCONSISTENCIES FOUND
- Quit vs PAT-MNU-1 (MM-15). Otherwise PAT-MNU-1, -8, -9, -11 to -14 and PAT-CRD-3 used as defined; D15 resolves the L4 input conflict (MM-5).

### Verdict: NEEDS REVISION
**Blocking issues**: 2 (revised in place after the verdict; verdict not changed)
**Advisory issues**: 5
**Dimensions not assessed**: 0

Revisions applied to design/ux/main-menu.md: M7 row, Quit/M-12 row, Purpose sentence, gamepad tick, MM-15, two new ACs, status line.
