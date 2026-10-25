## UX Review: HUD Design
**Date**: 2026-10-25
**Reviewer**: ux-review skill
**Document**: design/ux/hud.md
**Checklist**: 3B (Template line "HUD Design")
**Platform Target**: PC (Windows); keyboard and mouse primary, gamepad partial, touch none (source: .claude/docs/technical-preferences.md; project.yaml has no platform block)
**Accessibility Tier**: Standard with named Comprehensive elevations (design/accessibility-requirements.md, committed 2026-10-25)

### Completeness: 10/10 sections present
Philosophy, Information Architecture (22 elements + markers, 14 of 14 GDDs, 5 of 19 headless), Layout Zones, per-element specs E01-E22, States by Context (the four guide contexts mapped), Information Hierarchy (every element tiered), Visual Budget (F1 re-run; arithmetic re-checked: 132,920 ref px2 = 7.12 % at 16:10, margin 16,379 ref px2 - correct), Platform Adaptation, Tuning Knobs, Acceptance Criteria (33 at review time).

### Quality Issues: 2 blocking, 5 advisory
1. **Rejection plate position only valid at 100 % text scale** BLOCKING. Layout Zones and E15 fixed x = 418, but the Z1 cluster grows right to x = 444 / 486 / 568 at 125 / 150 / 200 %, so the plate overlaps the brace label plate, contradicting the layout test matrix. Fix: x = cluster right edge + 10 px. APPLIED.
2. **E15 layer disagreement undocumented** BLOCKING (spec rule: GDD/spec disagreements must be in Open Questions). HUD GDD HUD-E15 says L2; ADR-0021 s1 and this spec say L1. Fix: Open Question HUD-UX-23. APPLIED.
3. Centre-empty rule vs L0 ally markers ambiguous ADVISORY. Added HUD-UX-24 (ruling for CD / GDD owner). APPLIED as open question.
4. No performance acceptance criterion ADVISORY (AC-HUD-22 only cited in the budget table). Added UXA-HUD-34 and UXA-HUD-35. APPLIED.
5. Stale status line and HUD-UX-20 (PAT-WLD-5 already updated by D17) ADVISORY. APPLIED.
6. All text widths NOT MEASURED (HUD-UX-8) ADVISORY. Deferred to first UI implementation story by D16.
7. design/player-journey.md absent (HUD-UX-14) ADVISORY; journey phases inferred.

### GDD Alignment: ALIGNED
- 14 of 14 GDDs with HUD requirements covered; HUD GDD E01-E22 all present with matching anchors, wording (D11) and timings. Art-director handoffs (HUD-UX-3, -11, -12, -13, -16) and art-bible rows remain open but are owned elsewhere.

### Accessibility: COMPLIANT (at Standard + named elevations, as specified; Verified status awaits T-01/T-02/T-03 captures)
- No colour-only indicator (audit table); contrast and text floors specified; reduce-motion mapping and high-contrast defined; 5 s reading-time deviation documented (LIM-C2); no focus order needed (HitTestInvisible).

### Pattern Library: N/A - no pattern library checklist for this document type

### Verdict: NEEDS REVISION
**Blocking issues**: 2 (both revised in place after the verdict; verdict not changed)
**Advisory issues**: 5
**Dimensions not assessed**: 0

Revisions applied to design/ux/hud.md: rejection plate x rule (Layout Zones + E15), HUD-UX-23 and -24 added, UXA-HUD-34/35 added, HUD-UX-20 and status line refreshed.
