# Story 052: Client-local settings hooks honoured at fixed MVP defaults (scale, text, Reduce motion, high contrast, read hold)

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§6 DPI and scale)
**ADR Decision Summary**: UI scale `s_ui` (MVP 1.0, a client-local setting, never tuning) is applied once by the HUD layer roots (a scale container per layer) and by the L0 leaf, never inside `UStealthDpiScalingRule`, because the engine caches the rule's result per viewport size outside the editor; `ApplicationScale` stays 1.0. Text size follows F1 `max(floor_px · H/1080, base_px · u · s_text)`, so the 18/22/28 px floors ignore `s_ui`. The settings are fixed at 100 % / 100 % / off in MVP, but the HUD must already honour them so the Vertical Slice settings screen only exposes them.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 UI-V4 (DPI rule `H/1080` and the root scaler reacting to a settings change without a viewport resize; spike story 008, production rule story 009) is the engine basis: engine fact E5 says the custom rule's result is cached per viewport size outside the editor, so a runtime `s_ui` change only works through the root scaler (a `UScaleBox` or equivalent layout-affecting root). This story is Ready because it reads the story 009 root scaler contract, not the spike's verdict; if the spike later fails, only the scaler mechanism changes. Never call `SetApplicationScale` (process-wide, would scale the editor in PIE, E5). `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: DPI comes only from `UStealthDpiScalingRule` (`H/1080`); `s_ui` is applied once by the layer roots and the L0 leaf; `ApplicationScale` stays 1.0 — source: ADR-0021
- Forbidden: Never call `SetApplicationScale`, apply `s_ui` inside the DPI rule, or use the engine DPI curve — source: ADR-0021
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

This story proves HUD-R16 ("the HUD must already honour them"): every accessibility setting has its defined effect when set, at fixed MVP defaults, and no setting adds directional or player-only information (IVP R15).

---

## Acceptance Criteria

*From UX spec `design/ux/hud.md` (UXA-HUD-24 UI half, UXA-HUD-26) and GDD `design/gdd/hud.md` HUD-R16, scoped to this story:*

- [ ] **AC1** (TR-hud-006, retained dumps and 1080p captures per setting): **GIVEN** UI scale, text scale, Reduce motion, Reduce screen effects, high contrast and `HudReadHoldMult` read from a client-local settings object fixed at 100 % / 100 % / off in MVP **WHEN** each is set **THEN** it already has its defined effect: `s_ui` 0.75 / 1.5 scales the resolver rects; text scale 100/125/150/200 follows the resolver; Reduce motion uses the story 016 timelines; Reduce screen effects makes the ping exit stepped; high contrast follows UXA-HUD-25; `HudReadHoldMult` 1.5 gives a 4.5 s plate hold; in every combination the dump differs from the default dump only in the fields `alpha`, `stroke_px`, element rect size and event timestamps and gains no directional or player-only field. The streamer alias mask is Vertical Slice and is out of MVP scope (no MVP behaviour).
- [ ] **AC2** (UXA-HUD-24, UI half): With Reduce motion on, no entrance or exit lasts longer than one frame, the result-plate hold is still 3.0 s, the card hold still 4.0 s and the sweep advances in 1 s steps.
- [ ] **AC3** (UXA-HUD-26): At text scale 200 % no message is truncated (names may ellipsise), no cluster overlaps another, the Result and Notice slots do not overlap and the feed keeps three entries of at most two lines each; retained screenshots at 200 %.

---

## Implementation Notes

*Derived from ADR-0021 Decision §6 and GDD HUD-R16, F1, F3:*

**What to build.** A client-local settings object, `FHudLocalSettings` (plain C++ struct in `Source/StealthPresentation/Private/Hud/`, defaults `UiScale = 1.0`, `TextScale = 1.0`, `ReduceMotion = false`, `ReduceScreenEffects = false`, `HighContrast = false`, `StreamerAliasMask` not present in MVP), sampled into `FHudInputs` every frame by `UStealthHudPresenter` (story 003 path, "local settings"). It is a settings object, never a tuning key: the registry has no key for any of them (story 001 AC3, TDC CR-14). `HudReadHoldMult` is the one tuning key (client audience, 1.0 default, hard range 1.0-2.0) that the VS settings screen will tie to text scale (ruling D11); here it is read through `GetClientTuning()` and the test sets it by a test preset.

**Effects to wire (each already specified).**
- UI scale `s_ui` 0.75-1.5: applied by each layer root's scale container (story 009) and the L0 leaf; resolver rects scale by `u = s_ui · H/1080`; floors `W_min`/`H_min` and the 18/22/28 px text floors do not scale with `s_ui`.
- Text scale 100/125/150/200 %: follows F1 text sizing; plates grow with text and never truncate a message; names ellipsise at the plate width; a cluster grows upward at 200 % (the top-centre cluster ends at or above y = 432 at 1080p, UXA-HUD-02, measured in story 056).
- Reduce motion: uses the story 016 timelines (every entrance and exit lasts one frame; plate hold stays 3.0 s, card hold 4.0 s; the cooldown sweep advances in 1 s steps; F3 and F5 reduce-motion branches).
- Reduce screen effects: the ping ring exit is stepped, not faded (UX spec Reduce-screen-effects table), consumed by the L0 leaf (story 049).
- High contrast (UXA-HUD-25): strokes and keylines go one step thicker (2 to 3 px inward) without changing any footprint rect, Bone Dim is promoted to Bone, and the dump shows the same facts; the style asset from story 017 carries the high-contrast variant, this story only wires the setting to it.
- `HudReadHoldMult` 1.5: multiplies plate holds (result 3.0 s becomes 4.5 s), card hold, feed life and dwell (F3, F8).

**Dump allowlist.** Compare each setting (singly and in combination) against the default dump with the same seed: the only fields allowed to differ are `alpha`, `stroke_px`, element rect size and event timestamps; no new directional or player-only field may appear (IVP R15). Implement the comparison as a small helper in the PIE/capture script so the same check can be re-used by story 056.

**Traps.** `s_ui` inside the DPI rule would be cached per viewport size and would not respond until a resize (E5); do it only in the roots. Reduce motion must not shorten holds (UXA-HUD-24), only entrances and exits. Do not add a settings screen (Vertical Slice). Streamer alias mask has no MVP behaviour: do not stub a visible control.

**Decisions that apply.** D11 (read-hold multiplier tied to text scale in VS), D16 (display face and text widths come from story 017 and are re-measured there). Out of this story: the settings UI, persistence, the alias mask, the full layout matrix (story 056).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`: the DPI rule and the root scaler this story drives.
- `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`: the Reduce-motion timelines.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: the style asset and the high-contrast variant (UXA-HUD-25 measurement).
- `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`: slot priority the 200 % check relies on.
- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: the full resolution x scale matrix and the safe-rectangle proof.
- Settings & Accessibility screen, the streamer alias mask, saved settings: Vertical Slice.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudLocalSettings.h` and `HudLocalSettings.cpp` (`FHudLocalSettings`, defaults, sampling)
- `Source/StealthPresentation/Private/Hud/HudInputs.h` (settings field)
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` and `Private/Hud/Widgets/` layer roots (apply `ReduceMotion`, `ReduceScreenEffects`, `HighContrast`, `HudReadHoldMult`; roots from story 009)
- `Source/StealthPresentation/Private/WorldAnchor/SStealthWorldAnchorLeaf.cpp` (stepped ping exit under Reduce screen effects)
- Evidence: `production/qa/evidence/hud-text-200-1080p.png`, `production/qa/evidence/hud-reduce-motion-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: client-local settings hooks
  - Setup: with defaults fixed at 100 % / 100 % / off, set each setting singly and in combination: UI scale 0.75 and 1.5, text scale 100/125/150/200, Reduce motion, Reduce screen effects, high contrast, `HudReadHoldMult` 1.5. Take a dump and a 1080p capture of each.
  - Verify: `s_ui` 0.75 and 1.5 scale the resolver rects. Text scale follows the resolver. Reduce motion uses the story 016 timelines. Reduce screen effects makes the ping exit stepped. High contrast follows UXA-HUD-25. `HudReadHoldMult` 1.5 gives a 4.5 s plate hold. In every combination the dump differs from the default dump only in the fields alpha, stroke_px, element rect size and event timestamps, and gains no directional or player-only field.
  - Pass condition: all effects and the dump allowlist hold. The streamer alias mask is Vertical Slice and out of MVP scope.
- **AC-2**: UXA-HUD-24 Reduce motion (UI half)
  - Setup: Reduce motion on, frame-stepped capture of a plate, a card and the cooldown sweep.
  - Verify: no entrance or exit lasts longer than one frame, the result-plate hold is 3.0 s, the card hold is 4.0 s, and the sweep advances in 1 s steps.
  - Pass condition: all four measurements hold.
- **AC-3**: UXA-HUD-26 text scale 200 %
  - Setup: text scale 200 % at 1080p with a card, a plate, a hint and a three-line feed. Capture.
  - Verify: no message is truncated (names may ellipsise), no cluster overlaps another, the Result and Notice slots do not overlap, and the feed keeps three entries of at most two lines each.
  - Pass condition: `hud-text-200-1080p.png` retained and all four checks hold.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots `production/qa/evidence/hud-text-200-1080p.png` (and the per-setting 1080p captures listed in the evidence note) plus the sign-off and measurements `production/qa/evidence/hud-reduce-motion-evidence.md`
- Gate: BLOCKING (UI default; not waived at `qa.level: minimal`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`, `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`, `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`
- Unlocks: `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`
