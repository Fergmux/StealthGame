# Story 040: F10 banner show time, banner text and the HUD-E21 round summary lines (MVP item 9, display half)

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering and Common UI (presenter core, F10, round summary), with ADR-0002: Resolution queue and round-end staging (the `Ended` arrival time and the elimination-win staging)
**ADR Decision Summary**: `FHudPresenterCore` owns the F10 banner state machine and the round summary: both are computed from `FRoundPublicState`, `FRoundResult` and the owner's own pending plate and fall, sampled every frame by value, with all timing on `EstimatedServerNow()`. The summary is two string-table lines filled from the integer `FRoundResult.TeamSummary`, shown with the end banner at its F10 time, only in `Ended`. ADR-0002 stages an elimination win's `Ended` broadcast to the deciding strike's death write, which is what makes a bystander's banner time `t_phase`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. All times are doubles on `ServerNowEst` (ADR-0002); the test drives a fake clock. Strings come from `ST_Hud` and are compared by exact characters (middle dot and em dash), not their ASCII renderings. Log with `UE_LOGF`.

**Control Manifest Rules (this layer)**:
- Required: The round summary (HUD-E21) is two string-table lines filled from the integer `FRoundResult.TeamSummary`, shown with the end banner at its F10 time, only in `Ended` — source: ADR-0021 (user decision 2026-10-25)
- Forbidden: Never carry text on the wire — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story owns the display half of MVP item 9 (the round summary): when the banner appears, what it says, and what the two summary lines read. The data owner of `TeamSummary` is the Round epic; story 042 draws what this story decides.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-17 (banner clauses, F10) and HUD-R14 (round summary), scoped to this story:*

- [ ] **AC1** (AC-HUD-17 banner clauses (F10), `StealthGame.HUD.ObjectivesAndBanner`): **GIVEN** an objective-win `Ended` at 300.00 with an own plate due at 300.38, the same without a pending plate (winners and losers), an own plate due at 301.20 with `HudBannerMinPlateSec` 1.0, a victim whose own fall (300.15) ends the round, and a bystander of a team-1 elimination win **WHEN** the banner time is computed **THEN** an objective-win `Ended` at 300.00 with an own plate due at 300.38 shows the banner at 301.38; without a pending plate, the winners' banner at 300.50 (after the Final fill) and the losers' at 300.00; with an own plate due at 301.20 and `HudBannerMinPlateSec` 1.0, the banner at 301.60 (cap), never before 301.20; a victim whose own fall (300.15) ends the round sees the banner at 300.15 and no removal card; AND a bystander (no own plate or fall) of a team-1 elimination win shows the banner at its own `t_phase`, which on every client, the listen host included, is never earlier than the deciding strike's death write (`DeathServerTime − StrikeNetLeadSec`, 0.15 s before `DeathServerTime` at defaults), and the HUD adds no hold for it (F10, ruling D21).
- [ ] **AC2** (AC-HUD-17 banner text clause, `StealthGame.HUD.ObjectivesAndBanner`): **GIVEN** an objective win, an elimination win and a forfeit **WHEN** the banner is built **THEN** the banner text is exactly one of the approved phrasings ("Victory" or "Defeat"; "completed the final objective" or "every rival eliminated for the round", the latter also for a forfeit) plus the line "Your house {a} · Rival house {b}" from `MatchScore`.
- [ ] **AC3** (HUD-R14 and HUD-E21, UXA-HUD-20, `StealthGame.HUD.RoundSummaryText`): **GIVEN** a `FRoundResult.TeamSummary` that differs from locally observed counts, a pre-`Ended` frame and a called-off round **WHEN** the summary is built **THEN** it is two lines from `FRoundResult.TeamSummary`, own house first, "Your house — steps {s} of 3 · eliminated {e} · wrong targets {w}" and "Rival house — steps {s} of 3 · eliminated {e} · wrong targets {w}", entering with the banner at its F10 time, absent before `Ended` and in a called-off round, never derived locally, and matching no time format.

---

## Implementation Notes

*Derived from ADR-0021 Decision §4 and `design/gdd/hud.md` F10, HUD-R14, HUD-R9:*

**What to build.** The Banner sub-model of `FHudPresenterCore` (private to `Source/StealthPresentation/Private/Hud/`, for example `FHudBannerModel`) and the `Banner` cluster of `FHudViewModel` (story 002 created the shell): `bVisible`, `Outcome` (Victory or Defeat), `Reason` (Objective or Elimination), score values `{a, b}`, the two summary line texts, and `ShowTime`. It reads from `FHudInputs`: `FRoundPublicState` (`Phase`, `bCalledOff`, `MatchScore`), `FRoundResult` (winning team, reason, `TeamSummary`), the owner's pending plate and fall from stories 015 and 024, and the cluster fill-complete time from story 039.

**F10, exactly as the GDD defines it.**
- `t_floor = max(t_phase, t_plate_show if an own result or defence plate is Pending at t_phase, t_fall if the viewer's own removal is pending at t_phase, t_card + HudRemovalCardInSec if a card is entering)`.
- `t_want = max(t_floor, t_plate_show + HudBannerMinPlateSec if that plate exists, t_phase + HudObjectiveStepSec if the viewer's team won by the final objective)`.
- `t_banner = max(t_floor, min(t_want, t_phase + HudBannerMaxDelaySec))`. The floor is never cut: no banner before the viewer's own plate or own fall (ASN-R6, P3, ruling D12); the cap only trims the readability and fill extras (ruling D8).
- Worked cases, which are the test table: `Ended` 300.00, own plate 300.38, objective win by another team member: `t_want = max(301.38, 300.50) = 301.38`, cap 301.60, banner 301.38. Winners without a plate: 300.50 (Final fill). Losers without a plate: 300.00. Plate due 301.20 with `HudBannerMinPlateSec` 1.0: `t_want = 302.20`, cap 301.60, floor 301.20 gives 301.60. Elimination win, victim falls at 300.15: floor is `t_fall`, no plate, banner 300.15, no card (HUD-R9: if the own removal ends the round no card is shown, or a showing card exits over `HudPreemptExitSec`). Bystander of an elimination win: `t_phase`, nothing held. For an elimination win `t_phase` is the arrival of the broadcast that Round staged to the death write (`DeathServerTime − StrikeNetLeadSec`), so the HUD must not add any hold or look for the contact time of a strike it was not part of (ruling D21; bound AC-ASN-35). The test asserts the HUD adds zero delay for a bystander and that on the host (arrival delay zero) the banner is exactly at that bound.
- Keys `HudBannerMinPlateSec` (1.0) and `HudBannerMaxDelaySec` (1.6) and `HudObjectiveStepSec` come from `GetClientTuning()` (story 001; I-HUD-7 keeps min plate at or below max delay). Read holds are multiplied by `HudReadHoldMult` where the plate hold applies, not to these two.
- The cluster fade starts at `t_banner`: expose `ClusterFadeStart = t_banner` to the view-model; story 042 draws the 600 ms fade and the ping-ring hide.

**Banner text.** Outcome is in the viewer's own terms (own team won or lost) from `FRoundResult`. Reason is exactly one of `Hud.Banner.Reason.Objective` "completed the final objective" or `Hud.Banner.Reason.Elimination` "every rival eliminated for the round"; a forfeit uses the elimination phrase and is never named. Score line `Hud.Banner.Score` "Your house {a} · Rival house {b}" from `MatchScore` (a team total, art bible 7.1), `a` own house first. Use the approved characters of the UX string table (middle dot). The capitalisation and the defeated viewer's reading of the reason are an open UX question (HUD-UX-5); implement the string table as approved and do not add variants.

**Round summary.** Two `FText` lines from `Hud.Summary.Own` and `Hud.Summary.Rival`: "Your house — steps {s} of 3 · eliminated {e} · wrong targets {w}" and "Rival house — steps {s} of 3 · eliminated {e} · wrong targets {w}" (em dash and middle dots as approved). Steps are stages completed (the final counted only for the objective winner), eliminated is members of that house eliminated for the round when the round ended (every cause alike), wrong targets is the house's total `WrongTarget` results, which Round counts from the Event Log's `KillResolved` rows. The values are the integers of `FRoundResult.TeamSummary`; the HUD never derives them locally, so the test feeds a `TeamSummary` that differs from any locally observed count and asserts the TeamSummary values win. Own house first: the model picks own and rival by the viewer's team index. The lines enter with the banner at `t_banner`, are absent from the dump before `Ended` (not alpha 0), and are never created in a called-off round (`bCalledOff` in `Reset`). They stay until the HUD leaves `Ended`. No duration, no per-participant list, no opponent colours. A lint-style assertion checks the strings match no time format (no `m:ss`, no seconds suffix, no "min"), and the language lint of story 005 already covers the banned word list.

**Called off and lobby lines.** The banner model must not show in `CalledOff`; the "The round was called off" and "The host will begin the next round" lines are widgets driven by the viewer state of story 014 and drawn in story 042.

**Does not touch.** Widgets (story 042), the objectives cluster and its Final fill (story 039, whose fill-complete time this reads), the removal card timings (story 024), the plate timings (story 015). Prototype: the prototype HUD has no banner or summary; nothing is promoted from `Source/StealthGame/Private/StealthHUD.cpp` or the `MessageAll` end-of-round message plumbing (story 059 removes it).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-042-hud-end-banner-summary-called-off-lobby-widgets.md`: drawing the banner, summary, called-off and lobby lines and the cluster fade.
- `production/epics/hud/story-043-hud-round-summary-pie.md`: the 2v2 PIE proof with the injected summary fixture.
- `production/epics/hud/story-039-hud-objectives-cluster-f9-logic.md`: the Final step fill whose end time the banner waits for.
- `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md` and `story-015-hud-result-show-time-f2.md`: `t_fall`, `t_card` and `t_plate_show`.
- Round epic: the data (`FRoundResult.TeamSummary`, the Round test seam) is `production/epics/round-match-framework/story-054-team-summary-at-ended.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudBannerModel.h`
- `Source/StealthPresentation/Private/Hud/HudBannerModel.cpp`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (call the model from `Step`, fill the `Banner` cluster)
- `Source/StealthPresentation/Private/Tests/HUD/ObjectivesAndBannerTest.cpp` (class `FHudObjectivesAndBannerTest`, name `StealthGame.HUD.ObjectivesAndBanner`; the banner clauses of AC-HUD-17)
- `Source/StealthPresentation/Private/Tests/HUD/RoundSummaryTextTest.cpp` (class `FHudRoundSummaryTextTest`, name `StealthGame.HUD.RoundSummaryText`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `ObjectivesAndBanner` (F10)
  - Given: `Ended` at 300.00 with a plate due 300.38. Without a plate (winners, losers). A plate due 301.20 with `HudBannerMinPlateSec` 1.0. A victim fall at 300.15. A bystander of a team-1 elimination win.
  - When: the banner time is computed.
  - Then: 301.38. 300.50 for winners, 300.00 for losers. 301.60 (cap), never before 301.20. 300.15 with no card. The bystander shows at its own `t_phase` with no HUD hold, never before `DeathServerTime − StrikeNetLeadSec`.
  - Edge cases: none.
- **AC-2**: Banner text
  - Given: an objective win, an elimination win and a forfeit.
  - When: the banner is built.
  - Then: "Victory" or "Defeat", exactly one of "completed the final objective" or "every rival eliminated for the round" (also for a forfeit), plus "Your house {a} · Rival house {b}" from `MatchScore`.
  - Edge cases: compare against the UX string table's exact characters (middle dots, em dashes), not the ASCII rendering.
- **AC-3**: `RoundSummaryText`
  - Given: a `FRoundResult.TeamSummary` that differs from locally observed counts. Also a pre-`Ended` frame and a called-off round.
  - When: the summary is built.
  - Then: two lines, own house first: "Your house — steps {s} of 3 · eliminated {e} · wrong targets {w}" and the "Rival house" mirror. Values come from `TeamSummary`, not local counts. The lines enter with the banner at its F10 time, are absent before `Ended` and in a called-off round, and match no time format.
  - Edge cases: compare against the UX string table's exact characters.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/ObjectivesAndBannerTest.cpp` (`StealthGame.HUD.ObjectivesAndBanner`) and `Source/StealthPresentation/Private/Tests/HUD/RoundSummaryTextTest.cpp` (`StealthGame.HUD.RoundSummaryText`) — must exist and pass (`/story-done` checks that they EXIST; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Gate level: BLOCKING (the Logic default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `story-005-hud-string-table-and-language-lint.md`, `story-014-hud-presented-own-state-and-viewer-state-machine.md`, `story-015-hud-result-show-time-f2.md`, `story-024-hud-removal-card-timing-f4-logic.md`, `story-039-hud-objectives-cluster-f9-logic.md`; `production/epics/round-match-framework/story-054-team-summary-at-ended.md`; `production/epics/round-match-framework/story-053-strike-win-staging.md`
- Unlocks: `production/epics/hud/story-042-hud-end-banner-summary-called-off-lobby-widgets.md`, `production/epics/hud/story-043-hud-round-summary-pie.md`, `production/epics/hud/story-046-hud-host-parity-pie.md`, `production/epics/hud/story-053-hud-no-clock-no-counts-pie.md`, `production/epics/hud/story-059-hud-retire-prototype-canvas-and-message-plumbing.md`
