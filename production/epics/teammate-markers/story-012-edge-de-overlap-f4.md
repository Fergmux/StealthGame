# Story 012: Edge de-overlap F4 (cuttable)

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

When several allies are off-screen on the same side, their clamped markers would stack. F4 spreads them along the safe-rectangle boundary: it measures each marker's arc-length position clockwise from the top-left corner, orders them cyclically (ties by lower `PublicHandle`), opens the cycle at the midpoint of the largest gap so the first and last markers are the two farthest apart, then relaxes symmetrically so no neighbours are closer than `L = W + H + e` (across a corner the arc splits so either the horizontal part is at least `W` or the vertical part at least `H`, so the glyphs cannot overlap). The user made F4 cuttable on 2026-10-25 (TM OQ12): if the schedule slips, edge markers may fall back to plain overlap with far-to-near drawing while F3, F5 and F6 stay. Build F4 behind a single code seam so that fallback is a switch, not a rewrite.

**GDD**: `design/gdd/teammate-markers.md` (F4, TM-R9; OQ12)
**Requirement**: `TR-tmk-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 2)
**ADR Decision Summary**: The leaf applies the edge de-overlap in its paint pass after the edge clamp, over at most four markers.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure 2D math over at most four points; deterministic (no random, no unordered iteration), zero per-frame heap allocation (fixed-size arrays).

**Control Manifest Rules (this layer)**:
- Required: Edge markers never rotate and are not arrows — source: art bible 7.5, GDD TM-R9
- Forbidden: Never use a non-deterministic ordering (hash-map iteration) for the cyclic order — source: GDD F4 (identical result on every run)
- Guardrail: Each marker moves at most 1.5 `L` (96 px at 1080p) from its true clamped position

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-18, F4, OQ12), scoped to this story:*

- [ ] **AC1** (AC-TMK-18 a,b, [U] `EdgeDeOverlap`): **GIVEN** (a) four allies clamped to one edge within 10 px of each other and (b) two allies either side of each corner **WHEN** F4 runs **THEN** no two glyph rectangles overlap and the F4 examples are reproduced: (1810, 398) and (1810, 410) become (1810, 372) and (1810, 436), and (1800, 70) and (1810, 80) become (1778, 70) and (1810, 102).
- [ ] **AC2** (AC-TMK-18 c): **GIVEN** (c) two allies either side of the perimeter origin (top-left corner) **THEN** no two rectangles overlap, the cyclic order follows the true directions with ties broken by lower `PublicHandle`, and the result is identical on every run.
- [ ] **AC3** (OQ12, derived): **GIVEN** the user decision that F4 is cuttable **THEN** F4 sits behind one code seam so that, if cut, edge markers fall back to plain overlap with far-to-near drawing and the F3, F5 and F6 outputs are identical in both modes (a test runs both modes).

---

## Implementation Notes

*Derived from F4:*

`s_i = Perimeter(q'_i)`; order cyclically by `s_i` (ties: lower `PublicHandle`); open the cycle at the midpoint of the largest gap; relax each group of markers closer than `L` about its own mean: `s'_j = mean_G + (j - (n_G - 1)/2) * L`, merging groups and repeating until no neighbours are closer than `L` (at most `N_e - 1` merges); map back to the boundary. `L = W + H + e` with `e = TeammateMarkerEdgeGapSize` (default 4), so `L = 64` at 1080p, 52 to 286 across the size range; `P = 2 * ((x_max - x_min) + (y_max - y_min))` is 5,280 px at 1080p so four markers (256 px) always fit. With symmetric relaxation each marker moves at most `1.5 L`. Worked examples (GDD F4): two allies at (1810, 398) and (1810, 410) form one group with mean 404 and move to (1810, 372) and (1810, 436); two allies either side of the top-right corner at (1800, 70) and (1810, 80) (arc gap 20) spread to 64 px of arc about the corner, `|dx| = 32 >= 28`.

The seam is one function pointer or compile-time switch (`bEdgeSpreadEnabled`), selected by code (not by a tuning key; the GDD lists no key for it). The fallback draws the clamped positions as given with the far-to-near order of story 009.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-edge-clamp-f3.md`: the clamp that produces `q'`.
- `story-019-paint-markers-in-world-anchor-leaf.md`: drawing the spread positions.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerEdgeSpread.h` and `MarkerEdgeSpread.cpp`
- `Source/StealthPresentation/Private/Tests/Markers/MarkerEdgeSpreadTest.cpp` (name `StealthGame.Markers.EdgeDeOverlap`, class `FMarkersEdgeDeOverlapTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: (a) four allies clamped to one edge within 10 px, (b) two allies either side of each corner. When: F4 runs. Then: no two glyph rectangles overlap and the examples (1810, 372)/(1810, 436) and (1778, 70)/(1810, 102) are reproduced. Edge cases: all four allies at the same pixel.
- **AC2**: Given: two allies either side of the top-left perimeter origin, plus a tie in direction. When: F4 runs, repeated several times. Then: no overlap, cyclic order follows the true directions, ties go to the lower `PublicHandle`, and the output is identical every run. Edge cases: handles swapped.
- **AC3**: Given: the F4 seam with the F4 and plain-overlap implementations. When: the same edge scene runs in both modes. Then: cut mode gives plain overlap with far-to-near drawing, and the F3, F5 and F6 outputs are identical in both modes. Edge cases: the mode is switched at runtime.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerEdgeSpreadTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-011-edge-clamp-f3.md`, `story-009-fade-draw-order-no-motion.md`
- Unlocks: `production/epics/teammate-markers/story-019-paint-markers-in-world-anchor-leaf.md`
