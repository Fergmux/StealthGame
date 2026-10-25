## UX Review: Interaction Pattern Library
**Date**: 2026-10-25
**Reviewer**: ux-review skill
**Document**: design/ux/interaction-patterns.md
**Checklist**: 3C (Template line "Interaction Pattern Library")
**Platform Target**: PC (Windows); keyboard/mouse primary, gamepad partial (technical-preferences.md; project.yaml has no platform block)
**Accessibility Tier**: Standard with named elevations (design/accessibility-requirements.md, committed)

### Completeness: 7/7 checklist items assessed; 3 met at review time, 4 met after revision
- [x] Catalog index current: 40 pattern sections, 40 rows (41 after PAT-MNU-15).
- [ ] Standard controls: button, toggle, slider, dropdown, list, modal/dialog, tooltip, progress, input field, tab bar present; toast, grid, minimap etc. deliberately not adopted with reasons; **scroll missing** (two specs relied on "PAT-MNU-6 rules"). FIXED: PAT-MNU-15.
- [x] Game-specific patterns needed by the current specs present (HUD E01-E22, markers, rings, cards).
- [ ] Each pattern has When to Use, When NOT to Use, states, accessibility, implementation notes: PAT-MNU-7 to -14 lacked several of these. FIXED.
- [x] Animation Standards table present. [x] Sound Standards table present.
- [ ] No conflicting behaviours: see issues 2-4. FIXED.

### Quality Issues: 4 blocking, 3 advisory
1. **No Scroll Container pattern** BLOCKING. Added PAT-MNU-15 (wheel and stick, focus scroll-into-view, stepped thumb, no inertia) and a catalog row. APPLIED.
2. **Menu patterns incomplete** BLOCKING. PAT-MNU-7, -8, -9, -10, -11, -12, -13, -14 lacked When to Use / When NOT / States / Accessibility / Implementation Notes (as applicable). Added from existing content. APPLIED.
3. **Back and Destructive rules contradict the built specs** BLOCKING. PAT-MNU-13 said Esc closes the menu at the root, but the main menu's root Esc is a no-op; PAT-MNU-1 said every irreversible button is Destructive "no exceptions" while the main menu's Quit is Secondary. Fix: root exception added; Destructive defined as irreversible for the session or saved state (resolves main-menu MM-15). APPLIED.
4. **PAT-CRD-3 contradicted D15** BLOCKING. The spec said the L4 overlay takes input and hosts a focus trap; D15 makes L4 display-only with a Session-owned modal at ZOrder 100. Aligned. APPLIED.
5. Stale entries ADVISORY: "(planned)" Used-In references to specs that now exist, "None exist yet", the MNU scope note, PAT-CRD-1 "collapses into the informant frame" (HUD-UX-16 reads it as a fade), PAT-PLT-4 missing reconnecting and back lines, PAT-PLT-4 reserved sighting line (PNG-R8 says none), PAT-CRD-2 missing the round summary, PAT-WLD-5 missing edge ring and sender label, PAT-MNU-9 "Leave the round" (specs say "Leave the session"). APPLIED (HUD-UX-20).
6. Open questions P5, P8, P10, P11 answered by hud.md / pause-menu.md ADVISORY. Updated. APPLIED.
7. Menu look, motion and sound values remain proposals (P1, P2, P3) ADVISORY. Art-director and audio-director to confirm.

### GDD Alignment: ALIGNED
- Patterns trace to HUD, IIF, Teammate Markers, Camera, Session and the Pings GDD; six laws (L1-L6) match IVP R15 and the art bible. No pattern displays or modifies game state.

### Accessibility: COMPLIANT at Standard
- Every pattern carries an accessibility line; no hue-only signal; 48 px targets and 24 px menu text stated; reduce-motion variants in the Animation Standards. Screen-reader announcements remain NOT ASSESSED (F16, Comprehensive elevation only).

### Pattern Library: CONSISTENT after revision (INCONSISTENCIES FOUND before)

### Verdict: NEEDS REVISION
**Blocking issues**: 4 (revised in place after the verdict; verdict not changed)
**Advisory issues**: 3
**Dimensions not assessed**: 0

Revisions applied to design/ux/interaction-patterns.md (and the MM-15 resolution row in design/ux/main-menu.md): see issues 1-6.
