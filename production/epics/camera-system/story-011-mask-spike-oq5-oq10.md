# Story 011: Camera mask spike (OQ5 / OQ10)

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md` (Open Questions OQ5 partial masking, OQ10 mask trace cost; F3)
**Requirement**: `TR-camera-003`, `TR-camera-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (conditioned by ADR-0022: Rendering configuration and performance budgets, RC-3)
**ADR Decision Summary**: The eye-origin mask hides a pawn at α = 0 with `SetVisibility(false, Propagate)` (the proxy leaves the scene, so no mesh, shadow, Lumen or RT presence), dithers in between through the single `CPD_EyeMaskAlpha` custom-primitive-data slot, and tests visibility with async `ViewBlock` line traces at rate bands; ADR-0015 accepted these conditionally on this spike reporting.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: This is the spike behind ADR-0015's "Verification Required" items (1)-(3): hide/show cost, CPD dither on the shared character material with TSR, Lumen and VSM at 60 characters (skinned path verified in source; Nanite skinned and Substrate unverified), and the worst-case 295 async `ViewBlock` traces per frame at ≤ 0.5 ms game-thread. Per ADR-0022 the project has no RTX 3060-class hardware yet: a run on the RTX 5090 development machine is **development evidence, not acceptance evidence**, and must be labelled so; the 3060-class pass lines belong to story 022. Throwaway spike code goes behind `#if !UE_BUILD_SHIPPING` and is not promoted (ADR-0007 §8). Do not resave any sample asset; build the 60-character scene in the existing graybox map and character pool.

**Control Manifest Rules (this layer)**:
- Required: α = 0 hides via `SetVisibility(false, Propagate)`; pawn primitives keep `bCastHiddenShadow` and `bAffectIndirectLightingWhileHidden` false; the dither uses the single `CPD_EyeMaskAlpha` slot — source: ADR-0015
- Forbidden: Never toggle `CastShadow` per frame; never per-pawn material instances (ADR-0012 lint) — source: ADR-0015, ADR-0012
- Guardrail: Mask game-thread mean ≤ 0.5 ms, p99 ≤ 1.0 ms at 59 near pawns; masked-master GPU delta ≤ 0.3 ms at 50 characters (ADR-0022 RC-3)

The spike decides whether story 015 (apply path) can be built as written or must take a fallback (ADR-0015 R-2, R-3, R-4). The report is the deliverable; the spike does not ship code.

---

## Acceptance Criteria

*Derived from ADR-0015 "Verification Required" (1)-(3), Risks R-1 to R-4 and ADR-0022 RC-3 (no GDD criterion; the GDD's AC-CS-09 and AC-CS-13 are the acceptance versions, in stories 015 and 022):*

- [ ] **AC1** (derived, ADR-0015 V1, R-1, R-3): **GIVEN** 60 characters in the graybox street **WHEN** the spike hides and shows pawns **THEN** the report records proxy re-add cost, toggles per second in a crowded street, and the first-frame pose after re-add (no one-frame pop), and confirms with RT and Lumen on that a hidden pawn has no shadow, Lumen or ray-traced presence.
- [ ] **AC2** (derived, ADR-0015 V2, R-4; ADR-0022 RC-3): **GIVEN** the shared masked-dither character material **WHEN** `CPD_EyeMaskAlpha` drives the dither with TSR, Lumen and VSM at 60 characters **THEN** the report shows the dither works on the classic skinned path, the GPU delta against an opaque master at 50 characters (≤ 0.3 ms), TSR ghosting and dithered-shadow noise visually checked (screenshots retained), and records Nanite-skinned and Substrate state.
- [ ] **AC3** (derived, ADR-0015 V3, R-2; GDD OQ10): **GIVEN** 295 async `ViewBlock` traces per frame (59 other characters × 5 probes, all in the eye cone and blocked) **WHEN** the mask runs for 60 s **THEN** the report records game-thread mean and p99 cost on the named machine (target ≤ 0.5 ms mean, ≤ 1.0 ms p99, labelled development evidence if not 3060-class), sets `EyeMaskTracesPerFrameMax`, and states whether the synchronous near-band fallback (ADR-0015 §4.3.6) is needed.
- [ ] **AC4** (derived): **GIVEN** AC1-AC3 **WHEN** the report is written to `production/qa/spikes/<date>_camera-mask-OQ5-OQ10.md` **THEN** it states PASS or FAIL per item, the fallback path taken for any FAIL (ADR-0015 R-2, R-3, R-4), and which stories (015, 022) it releases.

---

## Implementation Notes

*Derived from ADR-0015 Decision §4 and Risks:*

Scene: the graybox map with the full 60-character pool deal (character-pool-appearance), the production mask material parameter `CPD_EyeMaskAlpha` reserved in the shared character master (ADR-0022 RC-3 builds the master; if it is not ready, spike on a throwaway copy made by duplicating, never editing, the sample material). Measure with Unreal Insights (`stat unit`, `stat game`, trace channels) and the existing measurement habit from the S1/S2/S14 spike report (`production/qa/spikes/2026-10-25_S1-S2-S14.md`). Record the machine (GPU, CPU, RAM), resolution 1080p, settings from ADR-0022 (Substrate off, Lumen software GI, HWRT off, VSM on, TSR fixed screen percentage). Hide-state test: toggle `SetVisibility` on pawns at the rates the F3 hysteresis would produce, measure render-thread proxy creation and the first-frame pose.

Outcomes feed story 015's fallback ladder: if toggling churns render state, CPD-only fade to α = 0 with the proxy kept (R-3); if CPD dither fails on the chosen path, hard cut at α thresholds (R-4, never per-pawn MIDs); if async cost over budget, near-band synchronous traces with early exit (§4.3.6), and failure goes to technical-director (OQ10), never by dropping the line-of-sight test.

Prototype replaced: none. The prototype has no mask; `bFirstPerson` hiding (HideBoneByName) is unrelated.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-eye-mask-core.md`, `story-013-mask-live-world-is-eye-mask-visible.md`, `story-015-mask-apply-path.md`: the production mask.
- `story-022-mask-performance-acceptance.md`: the 3060-class acceptance capture.
- ADR-0022 R1 to R3 captures (kit Lumen/Nanite, skin cache, animation): their owners.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/spikes/<date>_camera-mask-OQ5-OQ10.md` (the report)
- `production/qa/evidence/<date>_camera-mask-spike-*.png` (retained screenshots)
- `Source/StealthGame/Private/Spikes/StealthMaskSpike.cpp` (throwaway, `#if !UE_BUILD_SHIPPING`, console commands; deleted or kept out of Shipping by the orchestrator)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC1: no budget exists for proxy re-add cost or toggles per second, and "crowded street" is undefined, so AC4's PASS/FAIL per item cannot be computed. QA-lead proposed rewrite (unconfirmed): crowded street = 60 characters under the AC-CS-13 yaw sweep; PASS = added frame cost per re-add ≤ [author to set, for example 0.5 ms]; "no pop" and "no shadow, Lumen or RT presence" are binary.
- AC2: ADR-0022 RC-3 sets ≤ 0.3 ms at 50 characters but the AC measures 60. Proposed rewrite: measure at 50 for PASS, report 60 informationally, and mark Nanite-skinned and Substrate as record-only (N/A in AC4).
- AC3 sketch: 59 targets in the cone, blocked (295 async traces per frame) for 60 s with an Insights scope over submission, result handling and apply; mean ≤ 0.5 ms and p99 ≤ 1.0 ms on the named machine (an RTX 5090 run is labelled development evidence); the report sets `EyeMaskTracesPerFrameMax` (initial 160) and says whether the synchronous near-band fallback is needed (needed if async submission or result handling exceeds 0.5 ms game-thread, ADR-0015 §4.3.6). AC4 sketch: the report file has a PASS/FAIL line per item and names the fallback path taken.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the spike report in `production/qa/spikes/` (documented measurements, not an automated test) with retained screenshots

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic: character-pool-appearance (60-character deal and the shared masked-dither character master, ADR-0022 RC-3), level-socket-contract (graybox map); `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`
- Unlocks: `production/epics/camera-system/story-015-mask-apply-path.md`, `story-022-mask-performance-acceptance.md`
