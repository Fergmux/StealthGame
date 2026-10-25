# Story 042: Bush and planting value band — leg-and-forearm read test (OI-2)

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: art bible `design/art/art-bible.md` (3.6 bush silhouette specification, 4.1 Law 3b and the leg-and-forearm read test, 4.2, 8.x.17 open item OI-2); `design/gdd/level-socket-contract.md` (V-BUSH-2, V-BUSH-3, AC-45)
**Requirement**: `TR-lsc-???` (no registry entry matches a bush value-band test; traced to art-bible open item OI-2)
*(OI-2: "Law 3b: P4 Canal Green (L* 51), P5 Foliage Green (L* 43) and bush leaf tips (L* 42 to 48) fall in the excluded band as leg and forearm backdrops" — owner art-director with technical-artist; closes with "Run the leg-and-forearm read test (4.1) on water edges, planting and the bush; adopt dark-value variants (L* 38 or lower) if it fails"; blocks "bush sign-off; at the latest the graybox greyscale tests before the vertical slice".)*

**ADR Governing Implementation**: N/A — measurement of art-bible targets on the graybox; no architectural decision governs
**ADR Decision Summary**: N/A
**ADR Version**: N/A

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (rendered L* measured in Lumen; ADR-0022 budgets)
**Engine Notes**: Uses the standard pose set (walk at mid-stride, idle with arms hanging, one arm raised) for the mannequin-variant pool at 20 m and 30 m in sun and shade; rendered L* from linear captures with the T1 audit script (OI-3 re-measures hexes).

**Why this story**: The bush must stay readable as a bush without hiding a figure's legs and forearms, the very tells players read. Read first: art bible 3.6 (value structure, dome-to-planter separation), 4.1 Law 3b and its test, OI-2 rows in 8.x.17.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Bushes are identified by shape and value structure, never a unique colour; occupied and empty bushes are identical (art bible 3.6, 2.8)
- Forbidden: Never give decorative plants the dome silhouette (V-BUSH-2 Dome Ban) (GDD R7)
- Guardrail: n/a

---

## Acceptance Criteria

*From art bible 4.1 (leg-and-forearm read test), 3.6 and OI-2 (revised after QL-STORY-READY: tester counts and a completion rule defined), scoped to this story:*

- [ ] GIVEN the standard pose set at 20 m and 30 m in sun and shade against water edge, planting and bush planter backdrops, THEN the median rendered L* difference between skin pixels (legs, forearms) and the adjacent backdrop is at least 10 for the pool and at least 6 for every individual model
- [ ] THEN, with at least 10 testers, at least 100 trials per distinction per distance, 2 s exposure, greyscale, randomised, stride-vs-stand is identified in at least 90% and arm raised-vs-hanging in at least 85%, pooled at 20 m and at 30 m
- [ ] GIVEN the bush at 25 m in sun and shade, THEN the dome's lowest tier median L* is at least 3 above the planter face, the Limewash cap line is visible, and overhanging leaves hide at most 25% of the cap front length
- [ ] THEN this story is complete only when the three criteria pass on the final assets and OI-2 is recorded closed in the art-bible register; if the first or second fails, the dark-value variants (L* at most 38) for water-edge and low planting and the bush's lowest 1.2 m are adopted in a follow-up change and the three criteria re-run

---

## Implementation Notes

- Backdrops: dado, door leaf, upper wall, limewash, paving, water edge, planting, bush planter; the failing combinations are the ones OI-2 names (P4 Canal Green at water edges, P5 Foliage Green in the lowest 1.2 m, bush leaf tips L* 42-48 in the lowest 1.2 m).
- Planter L* 30-33 plus lowest tier 36-38 leaves only 3-8 L* of separation, so the Limewash cap (L* about 80) carries the boundary: a median of exactly 3 L* is the pass edge; overhanging leaves must never hide more than 25% of the cap's front length.
- Human test is a small-sample greyscale study (clusters of testers, randomised options); keep raw trial sheets with the evidence. The same 25 m capture also feeds story 043.
- On failure, the change request goes to the art director and technical-artist (dark variants L* <= 38 or L* >= 69 per Law 3b); rerun everything and record the closure date in the art-bible register.
- Evidence under `production/qa/evidence/level-socket/`: captures, L* measurement sheet, trial sheets, sign-off by art-director and technical-artist.

---

## Out of Scope

- Story 043: bush identification tests (AC-45/46); Law 3 whole-world value audit; character pool Meshy conformity (OI-9/10)

---

## Files

- `production/qa/evidence/level-socket/bush-value-band-*.png`
- `production/qa/evidence/level-socket/bush-value-band-evidence.md`
- `design/art/art-bible.md` (OI-2 register update, via an art-bible revision)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** measured L*
  - Setup: standard pose set at 20 m and 30 m in sun and shade against water edge, planting and bush planter.
  - Verify: median rendered L* difference between skin pixels and backdrop.
  - Pass condition: >= 10 for the pool and >= 6 for every model.
- **AC-2** human greyscale
  - Setup: >= 10 testers, >= 100 trials per distinction per distance, 2 s exposure, greyscale, randomised.
  - Verify: stride vs stand; arm raised vs hanging.
  - Pass condition: >= 90% and >= 85% pooled at 20 m and at 30 m.
- **AC-3** bush dome
  - Setup: the bush at 25 m in sun and shade.
  - Verify: lowest-tier median L* vs planter face; Limewash cap line; cap occlusion.
  - Pass condition: >= 3 L* above the planter; cap line visible; overhanging leaves hide <= 25% of the cap front.
- **AC-4** completion rule
  - Setup: final assets.
  - Verify: AC-1..3 results and the art-bible register.
  - Pass condition: AC-1..3 pass on final assets and OI-2 recorded closed; on failure the dark variants (L* <= 38) are adopted and AC-1..3 re-run.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures in `production/qa/evidence/level-socket/` plus sign-off in `bush-value-band-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
- Cross-epic: `production/epics/character-pool-appearance/EPIC.md` (mannequin-variant pool), `production/epics/concealment-bushes/EPIC.md` (bush asset family)
- Unlocks: `production/epics/level-socket-contract/story-043-bush-greyscale-and-swatch-tests.md`
