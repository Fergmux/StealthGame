# Story 039: F9 objectives cluster update: own steps, rival steps and the monotonic rival count

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-001`, `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (the rival count is released data, never a timing leak), with ADR-0021: UI/HUD layering and Common UI (presenter core, F9)
**ADR Decision Summary**: The presenter core `FHudPresenterCore` owns every presentation state machine, F9 (objectives cluster, `rival_shown` monotonic within a `RoundEpoch`) among them, and turns an `FHudInputs` snapshot into an `FHudViewModel`; headless specs drive it with a fake clock and fake carriers. Timelines are sampled from `now − t0` in C++, never from widget animation assets. The HUD adds no delay or timing of its own to the rival steps and plays the tick at arrival, so it never sharpens the correlation with an opponent's visible tending (GDD edge case).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. The Objectives team view is read through a fake `FObjectiveTeamView` in the test because the objectives-chain epic has no stories yet; the real carrier (ADR-0020) is wired by story 003's sampling once it exists. Log with `UE_LOGF`.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never sample plate or card timelines from widget animation assets — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story is the logic behind the top-centre objectives cluster (HUD-E03, E04, E05). It decides what is filled and when; story 041 draws it. It also provides the "Final step fills over `HudObjectiveStepSec` from `Ended`" signal that story 040's banner time waits on.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-17 (first clause, F9) and UX spec `design/ux/hud.md` UXA-HUD-17, scoped to the cluster cases:*

- [ ] **AC1** (AC-HUD-17 first clause (F9), UXA-HUD-17, `StealthGame.HUD.ObjectivesCluster` rival steps; the cluster cases of `StealthGame.HUD.ObjectivesAndBanner`): **GIVEN** `RivalPrelimCount` going 0 → 2 in one update, then a later 1, then a new `RoundEpoch` **WHEN** `Step` runs **THEN** `RivalPrelimCount` 0 → 2 in one update animates both steps in one 0.5 s fill with exactly one tick event; a later 1 does not lower the display; and the shown value resets to 0 on a new `RoundEpoch`.
- [ ] **AC2** (F9 and HUD-R11, `StealthGame.HUD.ObjectivesCluster` own steps and card): **GIVEN** `StageIndex` Stage1, Stage2 and Final, then `Ended` with a win, and a `Preparing` frame **WHEN** `Step` runs **THEN** own steps map `StageIndex` Stage1/Stage2/Final to 0/1/2 filled; the Final step fills only at `Ended` by the win, over `HudObjectiveStepSec`; the stage card turns over in 500 ms on a counted completion; the cluster is empty during `Preparing`; and there are exactly 3 own and 2 rival steps, never a final rival step, a sub-stage fill or a time.

---

## Implementation Notes

*Derived from ADR-0021 Decision §4 and `design/gdd/hud.md` HUD-R11, F9:*

**What to build.** The Objectives sub-model of `FHudPresenterCore` (a private state struct in `Source/StealthPresentation/Private/Hud/`, for example `FHudObjectivesModel`) and the `Objectives` cluster of `FHudViewModel` (story 002 created the cluster shell; fill in the fields). `FHudInputs` carries the own team's `FObjectiveTeamView` values (ADR-0020; Team audience, including the team's informants): `StageIndex` (enum `Stage1`, `Stage2`, `Final`), `RivalPrelimCount` (`uint8` 0-2), the stage-card strings (title, one line, `ClueTag`), and the `RoundEpoch`. Until the objectives-chain epic has stories, define the input struct field names to match the ADR-0020 `FObjectiveTeamView` and test through a fake.

**F9 formulas.**
- `rival_shown = clamp(max(RivalPrelimCount, rival_shown_prev), 0, 2)` within one `RoundEpoch`; reset to 0 on a new epoch. A `RivalPrelimCount` that arrives lower than the shown value (out-of-order rebuild) never decreases the display.
- Any change `Δ >= 1` animates all new steps in one `HudObjectiveStepSec` fill (default 0.5 s, read through `GetClientTuning()`, hard range 0.25-1.0) with exactly one tick event in the cue stream. A rise by 2 in one update is one fill and one tick, not two. The tick is a cue event in `FHudViewModel::Dump()`; the cue audio is story 051's.
- `filled_own = StageIndex` mapped Stage1 to 0, Stage2 to 1, Final to 2. The Final step never fills before `Ended`: while `Active`, `StageIndex = Final` shows the first two steps filled and the third (Final) step empty. The Final step fills only in `Ended` by the win, over `HudObjectiveStepSec` starting at the F10-gated `Ended` arrival (ruling D2: the completed bar is the race's payoff). Expose the fill-complete time (`Ended` arrival + `HudObjectiveStepSec`) to the banner logic of story 040, which waits for it (the winners' banner at 300.50 for an `Ended` at 300.00) and caps it by `HudBannerMaxDelaySec`.
- The stage card turns over in `HudObjectiveStepSec` when `StageIndex` increments (a "counted completion"): old text fades over the first half, new text over the second, the plate unchanged. The model holds both strings and a turn start time `t0`; the view-model carries `turn_phase` sampled from `now − t0`.

**Never.** No final rival step, no sub-stage fill, no time, no waypoint, compass, arrow, distance read-out, map or world plate (OC-15), and no opponent count or elapsed time anywhere (Round R20). No "n/5" label. The HUD adds no timing of its own to the rival steps: the rise is delayed 30-195 s by Objectives' global grid and the tick plays at arrival.

**Empty states.** Team views arrive only at `Active` entry (OC-17); the cluster is empty during `Preparing` (the Dump lists the cluster elements as absent, not alpha 0). A 1v0 development round keeps rival steps at 0 throughout (Objectives forces the signal off); nothing else changes.

**HUD-R17 interaction.** The rival display is per-`RoundEpoch`, so a rebuild within an epoch keeps the shown value and a rebuild across an epoch resets it; the reset wiring is story 044's trigger, and this model exposes the epoch it was last primed for. A rebuild draws persistent elements with no animation and no tick.

**Does not touch.** The widgets (story 041), the banner time and summary (story 040), the clue phrase content (objectives-chain epic). Prototype: the prototype HUD has no objectives cluster; nothing is promoted from `Source/StealthGame/Private/StealthHUD.cpp`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`: the banner show time and text that consume this story's Final-fill time.
- `production/epics/hud/story-041-hud-objectives-cluster-widgets.md`: drawing the stage card and steps.
- `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`: the `RoundEpoch`/travel/reconnect rebuild triggers.
- `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`: the audio cue for the step tick.
- Objectives-chain epic: `FObjectiveTeamView` itself, `RivalPrelimCount` release timing and the clue text (OC-12, OC-15).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudObjectivesModel.h`
- `Source/StealthPresentation/Private/Hud/HudObjectivesModel.cpp`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (call the model from `Step`, fill the `Objectives` cluster)
- `Source/StealthPresentation/Private/Tests/HUD/ObjectivesClusterTest.cpp` (class `FHudObjectivesClusterTest`, name `StealthGame.HUD.ObjectivesCluster`; uses a fake `FObjectiveTeamView`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `ObjectivesCluster` rival steps
  - Given: `RivalPrelimCount` 0 to 2 in one update, then 1, then a new `RoundEpoch`.
  - When: `Step` runs.
  - Then: both steps animate in one 0.5 s fill with exactly one tick event. The later 1 does not lower the display. The shown value resets to 0 on the new epoch.
  - Edge cases: none.
- **AC-2**: Own steps and card
  - Given: `StageIndex` Stage1/Stage2/Final, then `Ended` with a win, and `Preparing`.
  - When: `Step` runs.
  - Then: 0/1/2 own steps filled. The Final step fills only at `Ended` over `HudObjectiveStepSec`. The stage card turns over in 500 ms on a counted completion. The cluster is empty in `Preparing`. Exactly 3 own and 2 rival steps, never a final rival step, sub-stage fill or time.
  - Edge cases: "counted completion" means `StageIndex` increments. Use a fake `FObjectiveTeamView` (the objectives epic has no stories).

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/ObjectivesClusterTest.cpp`, test `StealthGame.HUD.ObjectivesCluster` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Gate level: BLOCKING (the Logic default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`; objectives-chain epic (`FObjectiveTeamView`, OC-12; no stories yet)
- Unlocks: `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`, `production/epics/hud/story-041-hud-objectives-cluster-widgets.md`
