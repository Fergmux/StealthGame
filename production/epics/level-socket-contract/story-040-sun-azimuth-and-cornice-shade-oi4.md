# Story 040: Graybox sun azimuth lock and cornice shade bands (OI-4)

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md` (Open Question 4, V-POOL); art bible `design/art/art-bible.md` (2.1 baseline lighting, 6.8 objective sites, 8.x.17 open item OI-4)
**Requirement**: `TR-lsc-???` (no registry entry matches a lighting sign-off; this story is traced to art-bible open item OI-4 and LSC Open Question 4)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — none exists for this item; flagged. OI-4: "Sun azimuth of 45 degrees from the grid X axis is a working value" — owner art-director with level-design; closes with "Graybox: confirm cornice shade bands in every street orientation and alley sun slivers"; blocks the sun direction lock "before the generator's layout rules are frozen".)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (section 5.3: V-POOL sun raycast at the fixed sun direction on the `ViewBlock` channel)
**ADR Decision Summary**: The V-POOL sun-pool predicate is a geometric raycast at one fixed sun direction (not Lumen); the project's single sun azimuth must therefore be one constant read by both the level lighting and the predicate.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (rendering/Lumen lighting measured; ADR-0022 owns budgets)
**Engine Notes**: One directional sun (5,600 K, elevation 65 +/- 3 degrees, azimuth 45 degrees from +X of the 2.0 m plan grid), one sky (9,000 K), no other light. Captures use the graybox from story 028, debug draw off, fixed exposure.

**Why this story**: It turns the azimuth from a working value into a locked project constant before anything (V-POOL goldens, the Generator's shadow-trace check) depends on it. Read first: art bible 2.1 (baseline lighting table), 6.8, 8.x.17 (OI-4 rows); LSC V-POOL, D9, Open Question 4, AC-48.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The V-POOL sun raycast is geometric on the `ViewBlock` channel at the fixed sun direction (ADR-0014)
- Forbidden: Never add per-area, per-site or per-team lights or sun overrides (art bible 4.5)
- Guardrail: n/a

---

## Acceptance Criteria

*From art bible OI-4 and 2.1/6.8, LSC Open Question 4 (revised after QL-STORY-READY: working thresholds defined for the art director to confirm), scoped to this story:*

- [ ] GIVEN graybox captures at 1.5 m under the project sun (45 degrees from +X, 65 degrees elevation, 5,600 K), THEN each of the 4 street orientations shows a cornice shade band with sun:shade at least 2:1 and band height at least 0.5 m, no street axis lies within 15 degrees of the sun azimuth, and each alley orientation shows a sun sliver at least 0.3 m wide (thresholds are working values the art director confirms in the sign-off sheet)
- [ ] GIVEN the project, THEN `SunAzimuthDeg` is one constant read by the level lighting and by the V-POOL sun raycast, identical on server and clients (`Config.SunAzimuthSingleSource`); and LSC Open Question 4 is closed in the GDD naming the owner of `SunAzimuthDeg` and classing `SunPoolRatioMult` as a lighting value or a tuning key
- [ ] THEN art-director and level-design sign-off is retained in `production/qa/evidence/level-socket/`; if the azimuth changed, art bible 2.1 / 6.8 and the V-POOL goldens are updated

---

## Implementation Notes

- Capture set: street axes along +X and +Y in both directions (the 4 orientations) and each alley orientation present in the graybox; record sun azimuth/elevation from the level, exposure and capture camera in the sheet.
- If the working 45 degrees fails (a street runs parallel to the sun, or a band/sliver is missing), propose the replacement azimuth, update the constant everywhere and re-capture; the art bible 2.1 text and the V-POOL goldens change with it.
- `SunAzimuthDeg` is read from one data key (owner decided here per Open Question 4); the level's directional light and the layout predicate must read the same value; the Logic test asserts a level cannot override it.
- Lock rule: the Generator never rotates the sun or the grid; OI-4 must close before the Generator's layout rules freeze.

---

## Out of Scope

- Story 041: per-area shade ratios (OI-6); story 044: sun-pool beacon test (AC-48); Generator shadow-trace calibration (OI-5)

---

## Files

- `production/qa/evidence/level-socket/sun-azimuth-*.png` (captures)
- `production/qa/evidence/level-socket/sun-azimuth-evidence.md` (sign-off sheet)
- `Source/StealthGameplay/Private/Tests/Layout/SunAzimuthConfigTest.cpp` (`StealthGame.Layout.Config.SunAzimuthSingleSource`)
- `design/gdd/level-socket-contract.md` (Open Question 4 closure, via a GDD revision)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** cornice shade and slivers
  - Setup: graybox captures at 1.5 m, sun 45 degrees from +X, 65 degrees elevation, 5,600 K, only sun.
  - Verify: each of the 4 street orientations, street axes vs azimuth, each alley orientation.
  - Pass condition: cornice shade band with sun:shade >= 2:1 and height >= 0.5 m on each orientation; no street axis within 15 degrees of the azimuth; sliver >= 0.3 m wide per alley orientation (working values confirmed in the sign-off sheet).
- **AC-2** `Config.SunAzimuthSingleSource` (Logic) and document closure
  - Given: the level lighting and the V-POOL raycast.
  - When: both are inspected on server and clients.
  - Then: both read one `SunAzimuthDeg` constant, identical everywhere; a level overriding it fails. LSC Open Question 4 is closed naming the owner and classing `SunPoolRatioMult`.
- **AC-3** sign-off
  - Setup: the retained captures.
  - Verify: art-director and level-design sign-off; whether the azimuth changed.
  - Pass condition: sign-off in `production/qa/evidence/level-socket/`; if changed, art bible 2.1/6.8 and V-POOL goldens updated.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures in `production/qa/evidence/level-socket/` plus sign-off in `sun-azimuth-evidence.md`; `StealthGame.Layout.Config.SunAzimuthSingleSource` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
- Unlocks: `production/epics/level-socket-contract/story-041-per-area-shade-ratio-grey-card-oi6.md`, `production/epics/level-socket-contract/story-044-static-figure-beacon-and-paired-capture-tests.md`, Generator layout-rule freeze (`production/epics/procedural-city-generator/EPIC.md`)
