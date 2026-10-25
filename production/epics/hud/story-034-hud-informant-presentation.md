# Story 034: Informant presentation: brackets, frozen tally, countdown slot and the informant view contract

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (with ADR-0018 for presenting the own role change at the fall)
**ADR Decision Summary**: The HUD gates all own-elimination presentation through `PresentedOwnState`, and the Informant input context is selected only by the §8 `UseInformantContext` function, never by the raw own `State`, so no informant element or input swap appears before the removal card. The informant's view is one layout for every viewer: elements that do not apply are collapsed, persistent slots are never reused, and the informant brackets live on L1 with the frozen tally and objectives cluster.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 §8: `UseInformantContext = (RawOwnState == Informant) AND (NoPresenterBound OR PresentedRole == Informant OR LocalNow >= t_state + HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec)`, so this PIE test must run with the presenter bound. The informant grade is Camera's post-process (art bible 2.9), not a HUD element; the view state `InformantAwaiting` / `InformantBodied` comes from Camera's published `GetViewState()` plus the own pawn binding (`production/epics/camera-system/story-014-concealed-and-informant-view.md`). Common UI is not involved. ADR-0018 notes MotionWarping is Beta in 5.8.4. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: The Informant input context follows `PresentedOwnState` and is selected only by the §8 `UseInformantContext` function — source: ADR-0021
- Forbidden: Never drive the Informant context from the raw own `State` — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story proves, and where needed completes, the informant's screen: the four corner brackets, the frozen tally without its triangle, the objectives cluster, the team feed, the countdown in the Notice slot while awaiting a body, and the informant plate on a refused Interact. It is the integration of stories 014, 021, 031, 032, 033 and 041 into one verified contract, plus the E09 bracket widget.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-21 (verbatim, with the fixture note from the QA gate) and UX spec UXA-HUD-07:*

- [ ] **AC1** (AC-HUD-21, `StealthGame.HUD.PIE.InformantView`): **GIVEN** a player turned informant, **WHEN** its dumps are inspected in IA and IB, **THEN** the brace indicator, prompts, interact ring and strike hint are absent; the tally is frozen without the triangle; brackets, objectives cluster and feed are present (the feed present when at least one line exists; the fixture guarantees one); the countdown sits in the Notice slot in Informant Awaiting and is absent in Informant Bodied; an Interact press shows "Informants can only observe" in the Result slot at most once per 3 s and never before the card has collapsed.
- [ ] **AC2** (UXA-HUD-07; retained screenshots `hud-informant-awaiting-1080p.png`, `hud-informant-bodied-1080p.png`): **GIVEN** an informant in Informant Awaiting (with countdown) and in Informant Bodied **WHEN** captured at 1080p **THEN** the informant's four corner brackets are Bone 2 px stepped, the brace slot stays empty and the tally does not move.

---

## Implementation Notes

*Derived from ADR-0021 §1, §4 and §8, GDD HUD-R15, HUD-R6a, HUD-R13 and UX elements E09, E11, E16:*

**What to build.**
1. The informant brackets widget (E09) in `UStealthHudPersistentLayer` (L1, ZOrder 30): four stepped corner brackets, one per safe-rectangle corner, arm length 32 px, Bone 2 px stroke with a 2 px Obsidian keyline on both sides (reads over any ground), no fill, no label. Presented at `PresentedOwnState = Informant` (the removal-card entrance time, F4) with a 400 ms ease-out fade in, static afterwards. On a rebuild or a rejoin after the grace they are present at once with no animation. They fade with the other L1 clusters at the end banner (600 ms). Never visible to any other viewer. Dumb widget: applies a view-model bool plus alpha on change only.
2. The PIE scenario `InformantView` (`NETWORK_TEST_CLASS(InformantView, "StealthGame.HUD.PIE")`) in `StealthTestHarness` that turns a player into an informant, steps to Informant Awaiting (IA) and Informant Bodied (IB) and inspects the view-model dump in each. The fixture must guarantee at least one team-feed line (an ally removal staged earlier) so "feed present" is testable.
3. Glue only if a gap is found: this story must not duplicate the logic of stories 021 (tally frozen state), 031 (informant plate limiter), 033 (countdown) or 041 (objectives cluster); it asserts their joint behaviour. Fix a gap in the owning story's code, noting it.

**What an informant sees (HUD-R15).** Present: informant brackets (E09), the frozen tally (numeral and squares at the last shown value, danger triangle removed, ruling D4), the objectives cluster, the team feed (when a line exists), the latency icon, the informant plate on a refused press, and, while awaiting a body, the returning countdown in the Notice slot. Hidden: brace indicator (the slot stays empty, no reflow), prompts, interact ring, strike hint, result plates. The informant grade is Camera's post-process and is not asserted here.

**Informant plate rules.** "Informants can only observe" shows in the Result slot only from presses caught under `IMC_Informant` (ADR-0021 §8), at most once per `InformantPlateCooldownSec` (3 s default in the acceptance wording), and never before the removal card has collapsed (a press before the card has collapsed shows nothing: dropped, not deferred, story 031). The test presses Interact before the card collapses (expect nothing), after (expect the plate), and twice within 3 s (expect one).

**Dump assertions.** Use `FHudViewModel::Dump()` field presence, not pixels, for AC1. Pixels are AC2's screenshots.

**Traps.** `PresentedOwnState` flips at the card time, not on receipt of the raw `State`; the brackets must not appear earlier (that would be a pre-contact warning, story 029). A rejoin after the grace presents everything at once with no animation and no card. The tally stays visible as a record and must not move in IA or IB even if the server re-pushes a value.

**Prototype.** The prototype HUD has no informant presentation; nothing is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`: `PresentedOwnState` and the viewer state machine.
- `production/epics/hud/story-021-hud-wrong-target-tally-widget.md`: the tally widget, including its frozen state.
- `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`: the informant-plate limiter and slot arbitration.
- `production/epics/hud/story-032-hud-rejection-informant-and-hint-widgets.md`: the informant plate widget.
- `production/epics/hud/story-033-hud-returning-countdown-f7.md`: F7 countdown logic.
- `production/epics/hud/story-041-hud-objectives-cluster-widgets.md`: the objectives cluster widgets.
- `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`: the removal card that collapses into the brackets.
- `production/epics/hud/story-026-hud-removal-card-pie-timing.md`: the informant-killed PIE case (AC-HUD-29).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudPersistentLayer.h` and `.cpp` (informant brackets child)
- `Source/StealthPresentation/Private/Hud/Widgets/StealthInformantBrackets.h` and `.cpp` (dumb bracket widget)
- `Source/StealthPresentation/Private/Hud/HudViewModel.h` (brackets sub-struct, if not already present)
- `Source/StealthTestHarness/Private/HUD/InformantViewTest.cpp` (CQTest, `StealthGame.HUD.PIE.InformantView.*`)
- `production/qa/evidence/hud-informant-awaiting-1080p.png`, `production/qa/evidence/hud-informant-bodied-1080p.png`, `production/qa/evidence/hud-informant-presentation-evidence.md` (retained evidence, not code)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-21 `InformantView`
  - Given: a player turned informant, in Informant Awaiting (IA) and Informant Bodied (IB). The fixture guarantees at least one feed line.
  - When: its dumps are inspected.
  - Then: brace indicator, prompts, interact ring and strike hint are absent. The tally is frozen without the triangle. Brackets and objectives cluster are present. The feed is present (a line exists). The countdown is in the Notice slot in IA and absent in IB. An Interact press shows "Informants can only observe" at most once per 3 s and never before the card has collapsed.
  - Edge cases: a press before the card collapses shows nothing.
- **AC-2**: UXA-HUD-07 informant frame (Manual check)
  - Setup: reach Informant Awaiting (with countdown) and Informant Bodied. Capture both at 1080p.
  - Verify: four Bone 2 px stepped corner brackets, an empty brace slot, and a tally that does not move.
  - Pass condition: `hud-informant-awaiting-1080p.png` and `hud-informant-bodied-1080p.png` retained and matching.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/InformantViewTest.cpp` (`StealthGame.HUD.PIE.InformantView`) — must exist and pass; plus retained screenshots `production/qa/evidence/hud-informant-awaiting-1080p.png` and `production/qa/evidence/hud-informant-bodied-1080p.png` with sign-off in `production/qa/evidence/hud-informant-presentation-evidence.md` (gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`, `production/epics/hud/story-021-hud-wrong-target-tally-widget.md`, `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`, `production/epics/hud/story-032-hud-rejection-informant-and-hint-widgets.md`, `production/epics/hud/story-033-hud-returning-countdown-f7.md`, `production/epics/hud/story-041-hud-objectives-cluster-widgets.md`; `production/epics/camera-system/story-014-concealed-and-informant-view.md`
- Unlocks: `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`, `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`
