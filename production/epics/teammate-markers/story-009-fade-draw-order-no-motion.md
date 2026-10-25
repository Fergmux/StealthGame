# Story 009: Fade, draw order and no motion of its own

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The only motions a marker has are its entrance and exit fades and its 1:1 anchor tracking. This story implements the fade curve (a smoothstep over `TeammateMarkerFadeSec`, 0.30 s by default, sampled on real undilated, unpaused time so it is frame-rate and time-dilation independent), the far-to-near draw order of overlapping on-screen markers (the nearest ally is on top), and the rule that nothing else moves or sounds: no bob, pulse, scale pop, spin or idle animation, and no audio. Reduce motion sets the fade to 0 ms while the retire hold stays, because the hold is a timing, not a motion.

**GDD**: `design/gdd/teammate-markers.md` (F7, TM-R9 draw order, TM-R13, Visual/Audio VA-3 and VA-6)
**Requirement**: `TR-tmk-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (sections 2 and 4)
**ADR Decision Summary**: Timelines are sampled from `now - t0` in C++, never from widget animation assets; the leaf draws far-to-near on-screen markers; widgets are dumb.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The fade clock is `FPlatformTime`-style real time (or the engine's unpaused real-time clock), not `World->GetTimeSeconds()`, so global time dilation and pause do not change it; the test injects the real-time clock.

**Control Manifest Rules (this layer)**:
- Required: Plate and card timelines are sampled from `now - t0` in C++ — source: ADR-0021
- Forbidden: Never sample timelines from widget animation assets — source: ADR-0021
- Guardrail: No sound asset reference and no sound call from the marker module (VA-6)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-33, F7, TM-R13), scoped to this story:*

- [ ] **AC1** (AC-TMK-33, fade, [U] `DrawOrderAndFade`): **GIVEN** a 0.30 s entrance **THEN** alpha = 0.156 (+/-0.001) at 0.075 s, 0.5 at 0.15 s and 1.0 at 0.30 s or later, measured on undilated real time with global time dilation 0.5 and with the game paused.
- [ ] **AC2** (AC-TMK-33, order): **GIVEN** three overlapping on-screen markers at 5, 12 and 30 m **WHEN** drawn **THEN** the 5 m marker is on top and the 30 m marker at the bottom (far-to-near).
- [ ] **AC3** (TM-R13, VA-3, derived): **GIVEN** Reduce motion **THEN** fades are 0 ms but the retire hold is unchanged; and **GIVEN** static inputs **THEN** the glyph transform, size and alpha are constant across frames (no bob, pulse, scale pop, spin or idle motion), and the module references no sound asset and makes no sound call.

---

## Implementation Notes

*Derived from F7 and TM-R9, TM-R13:*

`alpha(tau) = S(clamp(tau / T_fade, 0, 1))` for an entrance and `1 - S(...)` for an exit, with `S(x) = 3x^2 - 2x^3`. `T_fade` is `TeammateMarkerFadeSec` (0 to 0.6, default 0.30), 0 when Reduce motion is on. Examples: 0.15 s into a 0.30 s entrance gives `x = 0.5`, `alpha = 0.5`; at 0.075 s `alpha = 0.156`. Alpha is a transition only, never a state (art bible 3.4: opacity encodes no state). Draw order is by world distance from the viewer, far first; the list is produced by the model and consumed by the leaf (story 019).

Visual rules to honour (VA-3, art bible 7.6): appear and disappear only on entry, removal and the view gate; no motion faster than 0.5 Hz; nothing moves more than 8 px except the 1:1 anchor tracking. A lint over the module (shared with story 017) fails on any `UGameplayStatics::PlaySound*`, `UAudioComponent` or `USoundBase` reference.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-paint-markers-in-world-anchor-leaf.md`: actually drawing in the order produced here.
- `story-015-view-gate-model.md`: the gate that triggers the group fade-in.
- `story-022-name-label-plate-rendering.md`: label fades (400 ms and 600 ms).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerFade.h` and `MarkerFade.cpp` (F7, draw order)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerFadeTest.cpp` (name `StealthGame.Markers.DrawOrderAndFade`, class `FMarkersDrawOrderAndFadeTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: a 0.30 s entrance driven by an injected real-time clock. When: sampled at 0.075 s, 0.15 s and 0.30 s, with global time dilation 0.5 and with the game paused. Then: alpha = 0.156 (+/-0.001), 0.5 and 1.0 at >= 0.30 s in every mode. Edge cases: a sample after 0.30 s.
- **AC2**: Given: three overlapping on-screen markers at 5, 12 and 30 m. When: the draw list is built. Then: far-to-near order puts the 30 m marker first (bottom) and the 5 m marker last (top). Edge cases: insertion order shuffled.
- **AC3**: Given: Reduce motion on, then off, plus static inputs sampled over N frames. When: entrance, exit and retire hold run. Then: fades are 0 ms and the 0.25 s hold is intact; glyph transform, size and alpha are constant across frames; lint finds no sound asset reference or `Play*` call. Edge cases: toggling Reduce motion mid-fade.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerFadeTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-001-marker-tuning-keys-and-local-settings.md`, `story-006-lifecycle-retire-hold-takeover-direct-switch.md`
- Unlocks: `production/epics/teammate-markers/story-019-paint-markers-in-world-anchor-leaf.md`, `story-022-name-label-plate-rendering.md`
