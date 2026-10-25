# Story 042: End banner with round summary, called-off line and lobby line widgets

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering and Common UI
**ADR Decision Summary**: The banner stack, the called-off line and the lobby line are L3 (ZOrder 50) UMG widgets added with `AddToPlayerScreen`, `HitTestInvisible`, created once at HUD init and shown or collapsed from the `Banner` cluster of `FHudViewModel`. Widgets are dumb and apply a value only on change; the round summary is two string-table lines filled from the integer `FRoundResult.TeamSummary`, shown with the banner at its F10 time, only in `Ended`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 engine fact E1: `AddToPlayerScreen` widgets sit in the Player slot (100) below `AddToViewport` widgets, and a camera fade can never cover UMG; L3 therefore stays readable over the neutral-view fade of story 013. Menus (ZOrder 100) sit above this layer. Rects below are measured at 1080p where the DPI factor `H/1080` is 1.0; the DPI rule (UI-V4, story 008) is not exercised here. The 600 ms cluster fade is sampled in C++ from `now − t0` (never a widget animation asset). `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: The round summary (HUD-E21) is two string-table lines filled from the integer `FRoundResult.TeamSummary`, shown with the end banner at its F10 time, only in `Ended` — source: ADR-0021 (user decision 2026-10-25)
- Forbidden: Never create or destroy a widget during a round in response to gameplay — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story draws what story 040 decides: the end banner stack with the round summary directly under it, the called-off line and the non-host lobby line (UX E18, E21). Its retained screenshots are the first visual evidence of the round-end surfaces.

---

## Acceptance Criteria

*From UX spec `design/ux/hud.md` (E18, E21, UXA-HUD-13, UXA-HUD-20) and GDD `design/gdd/hud.md` HUD-R14, scoped to this story:*

- [ ] **AC1** (HUD-R14, UXA-HUD-13, UXA-HUD-20, retained screenshots): **GIVEN** a Victory and a Defeat reached with a summary **WHEN** each banner is captured **THEN** Victory (Bone bar) and Defeat (Cochineal Deep bar) banners show the reason and score lines with the two summary lines directly under them in Bone 18 px on the banner plate, no icons, no colour coding of the numbers; the banner stack covers at most 30 % of the screen and is never full-bleed (UXA-HUD-13, -20); persistent clusters fade out over 600 ms as the banner enters and every ping ring hides.
- [ ] **AC2** (HUD-R14 called-off and lobby lines, retained screenshots): **GIVEN** a called-off round and a non-host lobby **WHEN** each is captured **THEN** the called-off line "The round was called off" clears every transient at once; the non-host lobby line "The host will begin the next round" shows with no player count text; retained screenshots of Victory with summary, Defeat with summary, called off and lobby.

---

## Implementation Notes

*Derived from ADR-0021 Decision §1 and §4 and `design/ux/hud.md` E18, E21, Layout Zones (Z6):*

**What to build.** C++ widget base classes in `Source/StealthPresentation/Private/Hud/Widgets/` (Widget Blueprints carry layout and style only), created once at HUD init by the L3 root (story 006):

- **Banner stack (E18)**: 720 wide, top at y = 340 (the stack is 720 x 238, spanning y 340-578). Header bar 720 x 80 with "Victory" on a Bone bar (Obsidian text) or "Defeat" on a Cochineal Deep bar (`#B01E3C`, Bone text), Display 48 px; the word on the bar, not its colour, carries the outcome. Reason plate 720 x 40 with exactly one approved phrase (Body 20 px); score plate 720 x 36 "Your house {a} · Rival house {b}" (Body 20 px). Banner stack covers at most 30 % of the screen (reference 8.3 %) and is never full-bleed.
- **Round summary (E21)**: one plate 720 x 64 directly under the score plate, entering with the banner at the F10 time: two Minimum 18 px Bone lines, own house first, "Your house — steps {s} of 3 · eliminated {e} · wrong targets {w}" and "Rival house — …". Plain text, no icons, no colour coding of the numbers, no duration, no per-participant list, no opponent colours. It stays until the HUD leaves `Ended` and is never shown in `CalledOff`. The text comes from the `Banner` cluster of story 040; the widget formats nothing and derives no number.
- **Called-off and lobby lines**: one plate 560 x 44 centred at (960, 540), shared by "The round was called off" (clears every transient element at once) and "The host will begin the next round" (non-host, in `Lobby`; no "waiting for players" text and no count beyond the lobby's `n / capacity`, which belongs to the lobby UI, not this line).

**Entry behaviour.** On banner entry the persistent L1 clusters fade over 600 ms (`ClusterFadeStart = t_banner`, from story 040) and every ping ring hides (the L0 ping rings of story 049 hide in the `Step` that sees `Phase = Ended`; this widget triggers only the L1 fade). Entrances follow the shared plate timeline (story 016), including Reduce motion (instant). The banner never shows before the owner's own plate or fall (story 040's `t_banner`); do not re-implement timing here. An exiting removal card exits over `HudPreemptExitSec` when `Ended` arrives mid-card (story 025); this widget just appears at `t_banner`.

**Strings.** All text comes from `ST_Hud` (`Hud.Banner.Victory`, `.Defeat`, `.Reason.Objective`, `.Reason.Elimination`, `.Score`, `Hud.Summary.Own`, `.Rival`, `Hud.CalledOff`, `Hud.Lobby.Wait`) with the approved characters (middle dot, em dash). No string names a clock, a count of opponents, a killer, a kind or a team. The capitalisation and the defeated viewer's reading of the reason remain the open question HUD-UX-5; do not add variants.

**Not built here.** The post-round reveal (Vertical Slice): the banner yields to it at hand-off, which is a future story. The post-banner lobby UI.

**Measurement.** The pass criteria are measured on the captures: banner stack area as a share of the 1920 x 1080 frame (at most 30 %, reference 8.3 %), and the 600 ms fade by frame-stepping the cluster alpha. Record both numbers in the sign-off.

**Does not touch.** The F10 timing and text logic (story 040), the viewer state machine that selects the called-off and lobby lines (story 014), the objectives cluster widget (story 041). Prototype: the prototype's end-of-round text (`MessageAll` in `StealthGameMode.cpp`, drawn by `AStealthHUD` canvas code in `Source/StealthGame/Private/StealthHUD.cpp`) is not promoted; story 059 removes it.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`: banner time, banner text and summary values.
- `production/epics/hud/story-043-hud-round-summary-pie.md`: the 2v2 PIE proof of the summary strings and absence rules.
- `production/epics/hud/story-041-hud-objectives-cluster-widgets.md`: the cluster that fades out.
- `production/epics/hud/story-049-hud-team-ping-rings-l0.md`: the ping rings that hide at `Ended`.
- `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`: the removal card exit when `Ended` arrives.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudBannerWidget.h`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudBannerWidget.cpp`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudStatusLineWidget.h` (called-off and lobby line)
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudStatusLineWidget.cpp`
- `Source/StealthPresentation/Private/Hud/HudLayerRoots.cpp` (register the L3 banner and status-line widgets, created once at HUD init)

The Widget Blueprints `WBP_HudEndBanner` and `WBP_HudStatusLine` (layout and style only) are authored in the editor and are not listed as files here.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Manual check: banners
  - Setup: reach Victory and Defeat with a summary.
  - Verify: the Victory banner has a Bone bar and the Defeat banner a Cochineal Deep bar. Reason and score lines are present, with the two summary lines directly under them in Bone 18 px on the banner plate. No icons and no colour coding of numbers. The banner stack is at most 30 % of the screen and not full-bleed (reference 8.3 %). Persistent clusters fade over 600 ms as it enters, and every ping ring hides.
  - Pass condition: measured area <= 30 % and the fade is 600 ms by frame-step.
- **AC-2**: Manual check: called-off and lobby lines
  - Setup: call off a round, then show a non-host lobby.
  - Verify: "The round was called off" clears every transient at once. "The host will begin the next round" shows with no player count text.
  - Pass condition: four screenshots retained (Victory, Defeat, called-off, lobby). The plan's Evidence list omits the lobby capture, so add `hud-lobby-line-1080p.png`.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots `production/qa/evidence/hud-banner-victory-1080p.png`, `production/qa/evidence/hud-banner-defeat-1080p.png`, `production/qa/evidence/hud-called-off-1080p.png` and `production/qa/evidence/hud-lobby-line-1080p.png` (the lobby capture added by the qa-lead gate), with the measured banner area and the 600 ms fade frame-step recorded in the sign-off `production/qa/evidence/hud-end-banner-summary-called-off-lobby-widgets-evidence.md`. Gate level: BLOCKING (the UI default; not waived at `qa.level: minimal`).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`
- Unlocks: `production/epics/hud/story-043-hud-round-summary-pie.md`, `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`, `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`, `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`
