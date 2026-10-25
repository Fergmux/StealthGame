# Story 041: Graybox per-area shade ratios by grey card (OI-6)

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: art bible `design/art/art-bible.md` (4.5 per-area colour temperature, 8.x.17 open item OI-6); `design/gdd/level-socket-contract.md` (R10, V-ZONE parity, V-POOL)
**Requirement**: `TR-lsc-???` (no registry entry matches a lighting measurement; traced to art-bible open item OI-6)
*(OI-6: "Per-area shade ratio and bounce outcomes are targets, not yet measured" — owner technical-artist; closes with "Grey-card capture at 1.5 m, solar noon, no grade, per area; adjust albedo within band if outside tolerance"; blocks per-area albedo and shade-ratio sign-off "at the first lighting pass of each area type".)*

**ADR Governing Implementation**: N/A — measurement of art-bible targets on the graybox; no architectural decision governs (ADR-0014 5.3 only fixes the sun predicate, covered in story 040)
**ADR Decision Summary**: N/A
**ADR Version**: N/A

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (Lumen lighting; ADR-0022 budgets)
**Engine Notes**: One sun and one sky; nothing in the per-area table is a light input. If an area measures outside tolerance the fix is an albedo or geometry change inside the 4.2 and 3.3 bands, never a per-area light, sun colour or fill override. An area's profile must be identical whichever team's objective it hosts.

**Why this story**: Per-area lighting must come out of geometry and albedo, and must never become a team signal. Read first: art bible 4.5 table and implementation path for the sun pool; 8.x.17 (OI-6, OI-5 neighbour).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One directional sun and one sky; courts identical in fixture class and sun-pool presence (ADR-0014, GDD R9.6)
- Forbidden: Never add per-area lights, sun colour or fill overrides (art bible 4.5)
- Guardrail: n/a

---

## Acceptance Criteria

*From art bible 4.5 and OI-6, scoped to this story:*

- [ ] GIVEN an 18% grey card at 1.5 m, solar noon, no grade, THEN sun:shade measures Causeway 2.5 +/- 0.3 at 9,000-9,500 K shade, Market 3 +/- 0.3 (open) and 2.2 +/- 0.3 (awning) with awning shade 4,300-5,000 K, Temple 4 +/- 0.4 at 9,000 +/- 500 K
- [ ] THEN Residential alleys measure 2 +/- 0.3 with shade 5,000-7,200 K and mean scene luminance at least 70% of the plaza's
- [ ] THEN the objective-site pool measures 5 +/- 0.5 on every court (identical across courts, role-blind), and any out-of-tolerance area is fixed by albedo or geometry inside the art bands, never a per-area light, sun colour or fill override

---

## Implementation Notes

- Rig: single sun and sky, no grade, fixed exposure; 18% grey card at 1.5 m in sun and open shade in each area; read linear pixels; record sun elevation/azimuth, exposure and area in the measurement sheet.
- Objective sites (art 4.5 table row): pool to surround 5 : 1 +/- 0.5, pool at 5,600 K, surround about 7,500 K +/- 500; max - min across courts at most 1.0. Court fixtures and sun pools are identical on every court (GDD R9.6).
- Open Market shade about 9,000 K is stated in the art bible without a tolerance; record the measured value and ask the art director to set one in the sign-off.
- Fixes go through albedo/geometry only and re-measure; the sheet records before/after values.
- Retain captures and the sheet in `production/qa/evidence/level-socket/`; technical-artist and art-director sign-off.

---

## Out of Scope

- Story 040: sun azimuth; OI-5 generator shadow-trace calibration (Generator epic); the full art-bible albedo audit

---

## Files

- `production/qa/evidence/level-socket/shade-ratio-*.png`
- `production/qa/evidence/level-socket/shade-ratio-evidence.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE, provisional TR).*

- **AC-1** per-area ratio
  - Setup: single sun, no grade, fixed exposure; 18% grey card at 1.5 m in sun and shade per area.
  - Verify: ratio and shade temperature.
  - Pass condition: Causeway 2.5 +/-0.3 at 9,000-9,500 K; Market 3 +/-0.3 open, 2.2 +/-0.3 under awnings at 4,300-5,000 K; Temple 4 +/-0.4 at 9,000 +/-500 K.
- **AC-2** Residential alleys
  - Setup: same rig in alleys and the plaza.
  - Verify: ratio, tint, mean scene luminance.
  - Pass condition: 2 +/-0.3; 5,000-7,200 K; alley mean >= 70% of the plaza's.
- **AC-3** objective sites
  - Setup: same rig on every court.
  - Verify: ratio on every court; lighting setup.
  - Pass condition: 5 +/-0.5 on every court, identical across courts; any fix by albedo or geometry within art bands, with no per-area light, sun colour or fill override. Capture retained in `production/qa/evidence/level-socket/` with technical-artist and art-director sign-off.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures in `production/qa/evidence/level-socket/` plus sign-off in `shade-ratio-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`, `production/epics/level-socket-contract/story-040-sun-azimuth-and-cornice-shade-oi4.md`
- Unlocks: Generator albedo/shade constants (`production/epics/procedural-city-generator/EPIC.md`)
