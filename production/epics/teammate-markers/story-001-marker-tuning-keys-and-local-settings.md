# Story 001: Marker tuning keys, constants and local-setting hooks

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

Every number the marker system uses (anchor height, fade, retire hold, feed staleness, label focus radius, edge gap) is a data-driven key under owner `TeammateMarkers`, with one value for every client and no player, team or role variant. This story registers those six keys and the one discrete user setting (`TeammateMarkerLargeMult`), makes the marker module read them only through `GetClientTuning()`, and fixes the few true constants in code. It also creates the client-local setting hooks that the Vertical Slice Settings system will later expose (accent colour, large markers, Reduce motion, UI scale), shipping only their defaults in the MVP. Nothing here is replicated: an opponent can learn nothing from a marker setting, and the identity-tell review (CR-15.8) found no key that times a crowd-versus-player event.

**GDD**: `design/gdd/teammate-markers.md` (Tuning Knobs, TM-R12)
**Requirement**: `TR-tmk-???` (no registry entry covers the Tuning Knobs section; nearest `TR-tmk-004`; the registry should gain one)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**Also governed by**: ADR-0004 (tuning data pipeline and validator, Accepted) for the registry mechanism
**ADR Decision Summary**: Client-local presentation values, including the UI scale `s_ui` and the markers' keys, are read through the client tuning face and are never gameplay tuning; `s_ui` is applied once by the layer roots.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No post-cutoff API is used by this story; the HIGH rating of ADR-0021 comes from the Common UI and Enhanced Input unification, which markers do not touch.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never apply a fallback or setting through gameplay tuning or a per-client heuristic; never write anything replicated from Presentation — source: ADR-0021, ADR-0022
- Guardrail: No hardcoded gameplay values (coding standards); the Tuning Knobs table is the only source of defaults

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (Tuning Knobs, TM-R12, AC-TMK-03 for the replication half), scoped to this story:*

- [ ] **AC1**: **GIVEN** the six `TeammateMarkers` keys **THEN** each exists with owner `TeammateMarkers`, audience `Client` and latch `Use`, with these defaults and hard ranges, and is read only through `GetClientTuning()`; and the TDC registry-mirror and CR-15 column doc-lints (tuning-data-config stories 031 and 033) pass:

  | Key | Default | Hard range |
  |---|---|---|
  | `TeammateMarkerAnchorCm` | 195 | 150-260 |
  | `TeammateMarkerEdgeGapSize` | 4 | 0-16 |
  | `TeammateMarkerFadeSec` | 0.30 | 0-0.6 |
  | `TeammateMarkerRetireHoldSec` | 0.25 | 0-1.0 |
  | `TeammateMarkerFeedStaleSec` | 2.0 | 1.0-5.0 |
  | `TeammateLabelFocusSize` | 64 | 0-200 |

- [ ] **AC2**: **GIVEN** `TeammateMarkerLargeMult` **THEN** only the discrete set {1.0, 1.5} is accepted (1.0 in the MVP) and any other value (for example 1.25, 0, 2.0, -1) is rejected, never clamped.
- [ ] **AC3**: **GIVEN** accent colour, large markers, Reduce motion and UI scale **THEN** each is a client-local setting hook with its MVP default and no menu, appears in no replicated layout, and the glyph constants (28 x 32 px, absolute floors 24 x 28 px, 3 px keyline, hatch band minimum 2 px, `MarkerCapacityCount` 4) are code constants that appear in no tuning registry.

*(Revised after QL-STORY-READY: AC1 now lists the ranges and the lint checks; AC2 carries its own unit test.)*

---

## Implementation Notes

*Derived from ADR-0021 §6 and the Tuning Knobs section of the GDD:*

Register the six keys in the TDC schema under owner `TeammateMarkers` with the columns the CR-15 doc-lint reads (type, owner, default, hard range, safe range, audience `Client`, latch `Use`, unit, effect). `TeammateMarkerLargeMult` is a two-step user setting, so TDC F-1's continuous clamp does not apply: model it as an enumerated setting that rejects a value outside {1.0, 1.5} (the hatch must land on whole pixels). Safe ranges (not hard) are in the GDD table: anchor 185-210, edge gap 2-8, fade 0.25-0.4, retire hold 0.15-0.35, feed stale 1.5-3.0, label focus 48-96; out-of-safe-range values only warn.

`TeammateMarkerFeedStaleSec` must be at least `3 / TeamPositionFeedHz` when the feed rate is 0.6 Hz or more (TDC invariant CR-5); that invariant is owned and tested by TDC, and story 005 asserts the module applies the validated value unchanged.

The local settings hooks are plain client-local values (a `ULocalPlayerSubsystem` or the HUD's local store, `UStealthHudLocalStore`, as ADR-0021 §4 puts client-local stores). The MVP ships defaults only: accent Turquoise, `k_large` 1.0, Reduce motion off, `s_ui` 1.0. Ship no settings menu. Do not add any key for a distance fade, a line-of-sight fade, an opacity state or a marker on non-allies (Deliberately absent, GDD Tuning Knobs).

Prototype replaced: the prototype hardcodes marker sizes and colours inside `AStealthHUD::DrawTeammateMarkers` (`StealthHUD.cpp:164`); no value is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-010-marker-size-f1.md`: the F1 size formula that consumes `s_ui` and `k_large`.
- `story-009-fade-draw-order-no-motion.md`: how Reduce motion zeroes the fade.
- `story-017-include-grep-ban-no-replicated-fields.md`: the replicated-layout audit over the whole module (AC3 here asserts only the setting hooks).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerTuning.h` and `MarkerTuning.cpp` (typed read of the six keys, the constants, the local-setting hooks)
- the TDC schema rows for owner `TeammateMarkers` (generated per tuning-data-config stories 040 and 041; no `.uasset` is edited by hand)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerTuningTest.cpp` (name `StealthGame.Markers.TuningKeys`, class `FMarkersTuningKeysTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Setup: load the schema rows and run the TDC registry-mirror and CR-15 doc-lint. Verify: the six keys with the default, hard range, owner, audience and latch in the table above, and that the module calls only `GetClientTuning()`. Pass condition: every value matches, both lints pass, a grep of the module finds no other read path.
- **AC2** (unit test): Given: `TeammateMarkerLargeMult` values {1.0, 1.5, 1.25, 0, 2.0, -1}. When: each is validated. Then: only 1.0 and 1.5 are accepted; every other value is rejected, never clamped. Edge cases: NaN.
- **AC3**: Setup: enumerate the settings for accent colour, large markers, Reduce motion and UI scale, and the replicated layouts. Verify: each is a client-local hook with its MVP default, no menu, and in no replicated layout. Pass condition: 28 x 32, 24 x 28 floors, the 3 px keyline and `MarkerCapacityCount` 4 are code constants and appear in no tuning registry.

QL-STORY-READY: GAPS, revised in place per the agent's proposal (AC1 ranges and lint names added; AC2 given its own unit test); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass (`production/qa/smoke-*.md`) plus the unit test `MarkerTuningTest.cpp` for AC2

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`, `story-066-tuning-service-and-parity.md`, `story-031-doc-lint-cr15-columns.md`, `story-033-doc-lint-registry-match.md`, and `story-002-gameplay-presentation-module-skeletons.md` (the `StealthPresentation` module)
- Unlocks: `production/epics/teammate-markers/story-005-fallback-feed-staleness.md`, `story-007-retire-contact-alignment.md`, `story-009-fade-draw-order-no-motion.md`, `story-010-marker-size-f1.md`, `story-013-label-focus-and-reconnecting-text.md`
