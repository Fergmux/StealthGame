# Story 010: Marker size formula F1

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

A marker is a constant pixel size: it depends only on the viewport height and the user's UI scale and large-marker setting, never on world distance or field of view. F1 gives the outer glyph size (keyline included) as `W = max(24, round(28 * u * s_ui * k_large))` and `H = max(28, round(32 * u * s_ui * k_large))` with `u = H_v / 1080`, a keyline of at least 3 px, and a two-band hatch height `b = max(2, round(2 * W / 28))` so the stair bands land on whole pixels. The 24 x 28 floors are absolute (not scaled by `u`) so the hatch stays legible in greyscale at 24 px. This story is the pure function; drawing it is story 019.

**GDD**: `design/gdd/teammate-markers.md` (F1, TM-R9)
**Requirement**: `TR-tmk-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 6, DPI and scale)
**ADR Decision Summary**: `u * s_ui` is applied exactly once: sizes are computed in physical pixels from F1, then divided by the leaf geometry scale; the DPI rule returns `H/1080` only and never contains `s_ui`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure integer and float math, no engine type. The DPI behaviour itself (UI-V4) is verified in the painted story 019, not here.

**Control Manifest Rules (this layer)**:
- Required: DPI comes only from `UStealthDpiScalingRule` (`H/1080`); `s_ui` is applied once by the layer roots and the L0 leaf; `ApplicationScale` stays 1.0 — source: ADR-0021
- Forbidden: Never call `SetApplicationScale`, apply `s_ui` inside the DPI rule, or use the engine DPI curve — source: ADR-0021
- Guardrail: Size never depends on distance or field of view

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-19, F1), scoped to this story:*

- [ ] **AC1** (AC-TMK-19, [U] `SizeFormula`, revised): **GIVEN** viewport heights 720, 1080, 1440, 2160, `s_ui` in {0.75, 1.0, 1.5} and `k_large` in {1.0, 1.5} **WHEN** F1 runs **THEN** the sizes equal the 24-cell table in Implementation Notes (including the F1 examples 720p 24 x 28, 1080p 28 x 32, 1440p 37 x 43, 4K 56 x 64 and 1080p large 42 x 48), no size is below 24 x 28 and the keyline is never below 3 px.
- [ ] **AC2** (F1): **GIVEN** the same inputs **THEN** the hatch band height is `b = max(2, round(2 * W / 28))` (1080p 2, 1440p 3, 4K 4, 1080p large 3) so the bands land on whole pixels, and `W x H` is the outer size including the keyline.
- [ ] **AC3** (TM-R9, derived): **GIVEN** any world distance or vertical field of view **THEN** the size is identical (it depends only on the viewport height and the user's two settings).

---

## Implementation Notes

*Derived from F1 and ADR-0021 §6:*

Constants: `W_0 = 28`, `H_0 = 32`, floors `W_min = 24`, `H_min = 28` (absolute), keyline `K = max(3, round(3 * u * s_ui))`. Output range 24 x 28 up to 126 x 144 px (4K, UI scale 150 %, large markers). Hand-computed expected sizes (W x H), `u` = 0.667, 1, 1.333, 2 for 720, 1080, 1440, 2160p:

| `s_ui` / `k_large` | 720p | 1080p | 1440p | 2160p |
|---|---|---|---|---|
| 0.75 / 1.0 | 24x28 | 24x28 | 28x32 | 42x48 |
| 1.0 / 1.0 | 24x28 | 28x32 | 37x43 | 56x64 |
| 1.5 / 1.0 | 28x32 | 42x48 | 56x64 | 84x96 |
| 0.75 / 1.5 | 24x28 | 32x36 | 42x48 | 63x72 |
| 1.0 / 1.5 | 28x32 | 42x48 | 56x64 | 84x96 |
| 1.5 / 1.5 | 42x48 | 63x72 | 84x96 | 126x144 |

(1080p at 0.75 / 1.5 is `28 * 1.125 = 31.5`, which rounds to 32.) `k_large` is 1.0 or 1.5 only (story 001). `s_ui` is 0.75 to 1.5, 1.0 in the MVP. `u = H_v / 1080`, `H_v` 720 to 2160. Keep integer rounding in one helper so story 019 and the hatch construction share it.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-paint-markers-in-world-anchor-leaf.md`: drawing at this size and the DPI rule (UI-V4).
- `story-020-glyph-states-and-edge-variant-evidence.md`: whether the hatch is legible at 24 px.
- `story-011-edge-clamp-f3.md`: the safe rectangle that uses `W` and `H`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerSize.h` and `MarkerSize.cpp`
- `Source/StealthPresentation/Private/Tests/Markers/MarkerSizeTest.cpp` (name `StealthGame.Markers.SizeFormula`, class `FMarkersSizeFormulaTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: heights 720, 1080, 1440, 2160, `s_ui` {0.75, 1.0, 1.5}, `k_large` {1.0, 1.5}. When: F1 runs. Then: every cell equals the table in Implementation Notes, size >= 24 x 28 and keyline >= 3 px in every cell. Edge cases: the 31.5 rounding cell (1080p, 0.75, 1.5) gives 32 x 36.
- **AC2**: Given: the same inputs. When: the band is computed. Then: `b = max(2, round(2*W/28))`, giving 2 at 1080p, 3 at 1440p, 4 at 4K and 3 for large, and `W x H` is the outer size including the keyline. Edge cases: `W` = 24.
- **AC3**: Given: world distances and field-of-view values varied with the viewport and `s_ui` fixed. When: F1 runs. Then: the size is identical for all of them. Edge cases: extreme distance and field of view.

QL-STORY-READY: GAPS, revised in place per the agent's proposal (the 24-cell expected table added, hand-computed from F1); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerSizeTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-001-marker-tuning-keys-and-local-settings.md`
- Unlocks: `production/epics/teammate-markers/story-011-edge-clamp-f3.md`, `story-019-paint-markers-in-world-anchor-leaf.md`, `story-020-glyph-states-and-edge-variant-evidence.md`
