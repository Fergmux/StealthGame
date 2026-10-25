# Story 016: F3 plate and card timelines sampled from now - t0, including Reduce motion and read hold

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-004`, `TR-hud-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§4: plate and card timelines sampled from `now − t0` in C++, never widget animation assets)
**ADR Decision Summary**: The presenter core owns the plate states (`Pending → Entering → Holding → Exiting`) and the F3 timelines, sampled from `now − t0` each frame with `ServerNowEst` (deterministic and time-dilation-safe), never from UMG widget animation assets. Read holds are multiplied by `HudReadHoldMult` and the client-local Reduce motion setting sets entrance and exit to 0 s while holds stay unchanged.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. The ADR-0021 verification items UI-V1 to UI-V4 are not touched here; UMG widget animation assets, whose timing would depend on widget ticks and global time dilation, are the thing this story keeps out of the HUD. Use `UE_LOGF` for any diagnostics.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never sample plate or card timelines from widget animation assets — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

Every transient element in the HUD (result plates, the defence plate, the rejection and informant plates, the removal card) is a short timeline with an entrance, a hold and an exit. This story builds the one shared evaluator for them, so the result-plate timing is identical for every outcome by construction (HUD-R3: nothing in the timeline may depend on the outcome), the Reduce motion and read-hold settings are honoured in one place, and a preemption (a card or a round clear taking the slot) falls out of the same curve. The plate widgets (story 022) and the card widget (story 025) only apply what this evaluator returns.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC-HUD-09, F3) and `design/ux/hud.md` (UXA-HUD-24 logic half), scoped to this story:*

- [ ] **AC1** (AC-HUD-09, `StealthGame.HUD.PlateTimeline`): **GIVEN** each of the four `FStrikeResult` outcomes and `FDefenceResult`, **WHEN** the timeline is sampled, **THEN** α steps 0.25/0.5/0.75/1.0 at 62.5 ms intervals, holds 3.0 s, fades over 0.6 s (± 1 tick at 60 Hz), with identical size and slot for every outcome; with Reduce motion, entrance and exit are 0 s and the hold is 3.0 s; with `HudReadHoldMult` 1.5 the hold is 4.5 s.
- [ ] **AC2** (UXA-HUD-24, logic half, `StealthGame.HUD.PlateTimeline`): **GIVEN** Reduce motion on **THEN** the cooldown sweep advances in 1 s steps and the card hold is still 4.0 s with entrance and exit lasting at most one frame.
- [ ] **AC3** (derived, F3 Preempt and fixed timelines, `StealthGame.HUD.PlateTimeline`): **GIVEN** a plate preempted at `t_preempt` with current alpha a0 **THEN** `t_out := t_preempt`, `T_out := HudPreemptExitSec`, and alpha falls from a0 to 0 in 4 equal steps over 0.25 s: alpha = a0 x (1 - floor(4x/T)/4), reaching 0 at x = T; the rejection (0.4/1.5/0.6) and informant (0.4/2.0/0.6) timelines assert only their phase boundaries unless the eased curve is named by the art-director in story 017.

---

## Implementation Notes

*Derived from ADR-0021 Decision §4 and GDD F3, HUD-R6, HUD-R16:*

**What to build.** `FHudTimeline` in `Source/StealthPresentation/Private/Hud/`, plain C++, one small value type plus a pure sampling function. Parameters (`T_in`, `T_hold`, `T_out`, entrance kind) come from a `FHudTimelineSpec`; the evaluator is `float Sample(const FHudTimelineSpec&, double Now, double T0, TOptional<FPreempt> Preempt, const FHudComfort& Comfort)`, with `FHudComfort {bReduceMotion, ReadHoldMult}` read from the client-local settings snapshot in `FHudInputs` (HUD-R16: in MVP fixed at off and 1.0 but already honoured, the settings system is VS, story 052 adds the hooks). The presenter keeps only `t0` per element and calls `Sample` each frame with `now = ServerNowEst` (or `LocalNow` for purely local elements); nothing accumulates delta time, which is what makes it independent of time dilation and frame rate.

**Formula (F3).** `α(t) = in(t − t0)` for `t − t0 < T_in`; 1 until `t0 + T_in + T_hold`; then `1 − clamp((t − t_out) / T_out, 0, 1)`. Stepped entrance `in(x) = floor(k · clamp(x / T_in, 0, 1)) / k` with `k = 4`; eased entrance (non-urgent plates) `in(x) = easeOut(clamp(x / T_in))` plus an 8 px settle in whole 4 px steps (the settle is a view-model integer, not a widget animation). Read hold: every `T_hold` is multiplied by `HudReadHoldMult`. Reduce motion: `T_in = T_out = 0`, no settle, `T_hold` unchanged (so with Reduce motion the alpha is 1 at `t0` and drops to 0 at `t0 + T_hold`: define the `T = 0` branch explicitly, no division by zero). Alpha at `t0` is 0 for the non-reduced stepped entrance (the first step is at `t0 + 62.5 ms`).

**Specs and defaults (tuning keys, read from the `FHudTuning` snapshot of story 001, never constants):**

| Timeline | T_in | T_hold | T_out | Entrance | Keys |
|---|---|---|---|---|---|
| Result and defence plate | 0.25 | 3.0 | 0.6 | stepped, k = 4 | `HudResultPlateInSec`, `HudResultPlateHoldSec`, `HudResultPlateOutSec` |
| Removal card | 0.6 | 4.0 | 0.8 | eased | `HudRemovalCardInSec`, `HudRemovalCardHoldSec`, `HudRemovalCardOutSec` |
| Rejection plate | 0.4 | 1.5 | 0.6 | eased | constants (IIF UI-5) |
| Informant plate | 0.4 | 2.0 | 0.6 | eased | constants (IIF UI-6) |

Total result-plate time on screen is 0.25 + 3.0 + 0.6 = 3.85 s; the card is 5.4 s. The result-plate spec is one shared instance for all five outcomes: do not give `FHudTimelineSpec` an outcome field.

**Preempt.** A higher-priority element taking the slot (the card taking the Result slot, story 024; a round clear, story 040; the Notice slot priorities, story 031) sets `t_out := t_preempt`, `T_out := HudPreemptExitSec` (0.25, range 0.1-0.4) and the alpha falls from its current value a0 in 4 equal steps: `alpha = a0 × (1 − floor(4x/T)/4)` with `x = now − t_preempt`, reaching exactly 0 at `x = T` (clamp x to T). For Reduce motion the preempt exit is also 0 s. The eased curve for the rejection, informant and card timelines is named by the art-director in story 017 (HUD-UX-8 scope); until then the tests assert only the phase boundaries (alpha 0 at `t0`, 1 at `t0 + T_in`, 1 until the hold ends, 0 at `t0 + T_in + T_hold + T_out`), and the eased-curve assertion is added by story 017 when the curve is named.

**Cooldown sweep and card.** The cooldown sweep is a linear 0-1 value computed by story 018 (F5); this story supplies the Reduce-motion quantiser (`floor(progress_seconds)` steps of 1 s) used by the sweep and asserts it with its own case. The card hold stays 4.0 s under Reduce motion with entrance and exit at most one frame (a single `Sample` call returns the end values on the frame after `t0` and after the hold).

**Test method.** Sample at 60 Hz (`dt = 1/60`) from `t0`, fake clock; tolerances are one tick (±1/60 s) on boundaries, exact on the step values. Run the five outcome inputs through the same spec to prove identity (equal sample arrays). Variants: Reduce motion on, `HudReadHoldMult` 1.5 (hold 4.5 s).

**Out of this story:** F2 show time (story 015), the plate widgets and per-outcome text (022, 025), the card timing F4 (024), the photosensitivity sequence budget (story 055, VA-HUD-4), the settings UI (VS).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-015-hud-result-show-time-f2.md`: when `t0` is.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: the art-director's named eased curve and the plate style.
- `production/epics/hud/story-018-hud-brace-indicator-state-logic.md`: the F5 sweep value itself.
- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md` and `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`: the widgets that apply the returned alpha.
- `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`: the Reduce motion and read-hold settings plumbing (this story consumes them as plain inputs).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudTimeline.h`
- `Source/StealthPresentation/Private/Hud/HudTimeline.cpp`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (hold `t0` per transient element and call `Sample` in `Step`)
- `Source/StealthPresentation/Private/Tests/HUD/PlateTimelineTest.cpp` (class `FHudPlateTimelineTest`, name `StealthGame.HUD.PlateTimeline`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `PlateTimeline`
  - Given: each of the four `FStrikeResult` outcomes and `FDefenceResult`, sampled at 60 Hz. Variants: Reduce motion, `HudReadHoldMult` 1.5.
  - When: the timeline is sampled from `now - t0`.
  - Then: alpha steps 0.25/0.5/0.75/1.0 at 62.5 ms intervals, holds 3.0 s, fades over 0.6 s (+/- 1 tick), with identical size and slot for every outcome. With Reduce motion entrance and exit are 0 s and the hold is 3.0 s. With 1.5 the hold is 4.5 s.
  - Edge cases: alpha at `t0` is 0.
- **AC-2**: Reduce motion for sweep and card
  - Given: Reduce motion on.
  - When: the cooldown sweep and card are sampled.
  - Then: the sweep advances in 1 s steps. The card hold is 4.0 s with entrance and exit at most one frame.
  - Edge cases: none.
- **AC-3**: Preempt and fixed timelines
  - Given: a plate preempted at `t_preempt` with current alpha a0. The rejection and informant plates.
  - When: sampled.
  - Then: alpha = a0 x (1 - floor(4x/T)/4) over `HudPreemptExitSec` (0.25 s), reaching 0 at x = T. Rejection (0.4/1.5/0.6) and informant (0.4/2.0/0.6) assert only their phase boundaries.
  - Edge cases: the eased curve is asserted only if story 017's art-director names it.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/PlateTimelineTest.cpp` (`StealthGame.HUD.PlateTimeline`) — must exist and pass. Gate level BLOCKING.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`
- Unlocks: `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`, `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`, `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`, `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`
