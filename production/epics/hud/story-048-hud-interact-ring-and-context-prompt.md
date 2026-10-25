# Story 048: Interact ring and context prompt on the L0 leaf (HUD-E12, HUD-E13)

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: spike UI-V1 (teammate-markers story 018) has not reported
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-003`, `TR-hud-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021 §2: L0 is one volatile Slate leaf with in-paint projection (UI/HUD layering and Common UI)
**ADR Decision Summary**: `SStealthWorldAnchorLeaf` is a custom `SLeafWidget`, `ForceVolatile(true)`, never inside an invalidation or retainer box; in `OnPaint` it reads the rendered frame's view through `ULocalPlayer::GetProjectionData`, projects each anchor, and emits the draw elements, so projection never happens in a UMG binding, a widget tick or a game-thread tick. The leaf's inputs are a per-frame `FWorldAnchorViewModel` produced by the presenter, which for the ring is `{target object, fill fraction, colour state}` and for the prompt `{object, verb, glyph}`; it reads world positions only for public interact objects (court fixtures, bushes) from the locally built layout components.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 verification item UI-V1 (zero-lag world projection) is the conditioning spike for this story: engine fact E3 (player cameras update after all actors tick and Slate paints later in the same engine-loop iteration, so `OnPaint` sees the camera of the frame being rendered, `ULocalPlayer::GetProjectionData` returning the aspect-constrained, sub-rect view) is verified in the 5.8.4 source, and UI-V1 observes it on the graybox; this story must not start until story 010 reports. Engine facts E4 and E12 apply: `UWidget::ForceVolatile` is pushed to the Slate widget in `SynchronizeProperties`, and a `UWidget` wrapping a custom Slate widget must release it in `ReleaseSlateResources` and implement `ComputeDesiredSize` (story 011 owns the shell). The ring and prompt layer is L0 at ZOrder 10 via `AddToPlayerScreen` (E1: below `AddToViewport` widgets), collapsed while `GetViewState() = Neutral`. Size is constant in physical pixels from F1 and divided by the leaf geometry scale so the DPI scale is applied once. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: World-anchored drawing (markers, interact ring, context prompt) happens in `SStealthWorldAnchorLeaf::OnPaint` from the rendered frame's camera view; the leaf is volatile and never inside an invalidation or retainer box — source: ADR-0021
- Forbidden: Never compute world-anchored positions in a UMG binding, widget tick or game-thread tick; never use one widget per marker positioned by tick — one frame of lag — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story puts the interact ring and the context prompt on the L0 leaf: a 48 px ring centred on the object being tended, a verb prompt anchored above it, and the leak rule that the prompt (and the first second of the ring) is identical at every court fixture in reach, whoever's court it is.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-25 (the interact clauses), UX spec `design/ux/hud.md` UXA-HUD-27, and HUD-R13 / HUD-R6a, scoped to this story:*

- [ ] **AC1** (AC-HUD-25 interact clauses, `StealthGame.HUD.PIE.PromptAndRing`; HUD-R13, Objectives OC-6): **GIVEN** an Active owner in reach of, in turn, its own live court, a locked court, the hidden final court and an opponent's live court **WHEN** it looks at each and then tends each for `TendCueDelaySec` + 0.5 s, with the camera moved so the object is off-centre **THEN** the prompt dumps are identical at all four ("Tend", "Slip in", "Slip out"; computed from public fixture geometry and `PresentedOwnState` only, never from team live-site data) and only the own live court's ring turns Marigold with four cardinal ticks after `TendCueDelaySec` + 0.5 s; the ring is 48 px and its centre follows the object's projected position: with the camera moved so the object is off-centre the ring centre stays on the object within 1 px and not at a fixed screen position.
- [ ] **AC2** (UXA-HUD-27, retained screenshots): **GIVEN** the live ring state (Marigold with four cardinal ticks) and the plain state **WHEN** both are captured in colour and in greyscale **THEN** the live ring state (Marigold with four cardinal ticks) and the plain state differ by at least 3:1 in luminance and are distinguishable in greyscale by the ticks alone.
- [ ] **AC3** (HUD-R6a ruling D1, HUD-R13, `StealthGame.HUD.PIE.PromptAndRing` cancel case): **GIVEN** a tend cancelled by the owner's own removal **WHEN** `OnRoleChangePresented` fires **THEN** a cancel caused by the owner's own removal is held to `OnRoleChangePresented` and the ring then hides at once with the prompts (HUD-R6a, ruling D1); the ring empties in one step on cancel and clamps at 0.98.

---

## Implementation Notes

*Derived from ADR-0021 Decision §2 and §4 and `design/gdd/hud.md` HUD-R13, HUD-R6a, `design/ux/hud.md` E12, E13:*

**What to build.** Ring and prompt state in `FHudPresenterCore` (private to `Source/StealthPresentation/Private/Hud/`, for example `FHudInteractModel`), filling the ring and prompt entries of the `WorldAnchor` cluster of `FHudViewModel` (stories 002 and 011), and the paint code for both in `SStealthWorldAnchorLeaf::OnPaint` (`Private/WorldAnchor/`), reusing the leaf shell of story 011. Nothing is a UMG widget per object; nothing is positioned by tick.

**Interact ring (E12).**
- Form: a 48 px circle centred on the target object's anchor (a court fixture or a bush; never a person, never the screen centre, never the owner's own body). Track 2 px Bone Dim; progress 4 px stroke filling linearly clockwise from 12 o'clock; 2 px Obsidian keyline both sides. Constant pixel size.
- Colour state: Bone for the first `TendCueDelaySec` (1.0 s, owned by the objectives-chain epic, OC-6, read through the client tuning face) after Begin at every court. Only after that delay, at an own live site, the stroke turns Marigold (`#E3A72F`) and gains four short outward ticks (8 px) at the cardinal points; every other court keeps the plain Bone ring. Whether a court is the own live one comes from the own team's `FObjectiveTeamView` (Team audience), never from anything about the opponents. The ticks are the non-colour backup (art bible 4.7, ruling D6).
- Prediction: starts at the local press, re-based on `FHoldStatus` replies drained from the owner reply sink (`production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`), per IIF F4; clamps at 0.98 and never shows completion before the server's `OnActionCompleted`. It empties in one step on any cancel, rejection or release.
- Edge: clamped to the safe rectangle at constant size when its target leaves the view. Reduce motion: the sweep stays (real-time progress).
- Own removal (HUD-R6a, ruling D1): a cancel caused by the owner's own removal arrives early (the server's `FHoldStatus` cancel at the death write, 0.15 s before contact). It is held, the ring keeps its prediction, and at `OnRoleChangePresented` (the owner's fall start) the ring hides at once along with the prompts and the strike hint. Until then the HUD runs exactly as in `ActiveAlive` so a cancelled ring cannot announce the outcome early. A cancel not caused by the owner's own removal empties in one step immediately.

**Context prompt (E13).**
- A plate 32 high holding the glyph plate and the verb "Tend", "Slip in" or "Slip out" (Body 20 px), anchored 8 px above the object's anchor, constant pixel size. A hint, never a gate: the command is valid from the moment the object is in reach.
- Leak rule: the prompt is computed from public fixture geometry (locally built layout components) and `PresentedOwnState` only, never from the team's live-site data (Objectives OC-6, OC-11), so it is identical at every court fixture in reach (own live, rival, locked, or the hidden final). The verb "Slip in" or "Slip out" follows the owner's own `IsConcealed` (owner-local). It is hidden while the owner is running or traversing, never shown to an informant (reads `PresentedOwnState`, never the raw own `State`), and hidden at once at the owner's own fall start (ruling D1). The predicate must read no team, controller, kind, concealment-of-others or live-site field; story 004's include and forbidden-API lint covers the includes.
- While a hold is active the prompt collapses and the ring takes its place, returning when the hold ends (UX decision logged; leak-neutral because it depends only on the owner's own action). Off-screen: the prompt is hidden (only the ring edge-clamps). Entrance 400 ms ease-out with 8 px stepped settle, 600 ms out.

**Projection.** Both are projected in `OnPaint` from `ULocalPlayer::GetProjectionData` of the frame being rendered, using the public object position; the ring centre must stay within 1 px of the object's projected position with the object off-centre (the test moves the camera and compares). Provide a non-Shipping accessor on the leaf that reports the last painted ring and prompt anchor centres in physical pixels so the PIE test asserts the 1 px bound without screen scraping. This depends on UI-V1 (teammate-markers story 018): if the spike reports a lag, the ADR's fallback is a `FSceneViewExtension` view capture, which changes only the view source.

**Luminance check (AC2).** Capture the live and plain ring in colour and greyscale (desaturated by the standard luminance weights); compute the relative luminance ratio of the live-state stroke against the plain-state stroke (and of each against its plate) and record it in the sign-off; the pass bar is at least 3:1, and the ticks alone must make the two states distinguishable in the greyscale capture. The art-director confirms the measurement (CR-HUD-8 item 11, open question HUD-UX-12).

**Does not touch.** The leaf shell and projection plumbing (story 011), slot priority and the rejection plates (story 031/032), ping rings and edge rings (story 049), markers (teammate-markers epic). Prototype: the prototype has no interact ring or prompt; nothing is promoted from `Source/StealthGame/Private/StealthHUD.cpp`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`: the leaf, its `ForceVolatile` handling and projection plumbing.
- `production/epics/teammate-markers/story-018-spike-ui-v1-zero-lag-projection.md`: the conditioning spike.
- `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md` and `story-032-hud-rejection-informant-and-hint-widgets.md`: rejection, informant and hint plates (not the ring or prompt).
- `production/epics/hud/story-049-hud-team-ping-rings-l0.md`: ping rings and edge rings on the same leaf.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: palette, type and glyph plate style.
- Objectives-chain epic: `TendCueDelaySec` and the own-live-site data (OC-6).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudInteractModel.h`
- `Source/StealthPresentation/Private/Hud/HudInteractModel.cpp`
- `Source/StealthPresentation/Private/WorldAnchor/StealthWorldAnchorLeaf.cpp` (ring and prompt painting; non-Shipping painted-anchor accessor)
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (call the model from `Step`, fill the ring and prompt entries)
- `Source/StealthTestHarness/Private/HUD/PromptAndRingTest.cpp` (CQTest, `NETWORK_TEST_CLASS(PromptAndRing, "StealthGame.HUD.PIE")`, `StealthGame.HUD.PIE.PromptAndRing.*`; the interact and cancel cases)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-25 interact clauses
  - Given: an Active owner in reach of, in turn, its own live court, a locked court, the hidden final court and an opponent's live court.
  - When: it looks at each and then tends each for `TendCueDelaySec` + 0.5 s, with the camera moved so the object is off-centre.
  - Then: prompt dumps are identical at all four ("Tend", "Slip in", "Slip out", computed from public fixture geometry and `PresentedOwnState` only). Only the own live court's ring turns Marigold with four cardinal ticks. The ring is 48 px, and its centre stays on the object's projected position within 1 px and not at a fixed screen position.
  - Edge cases: blocked until spike 010 reports.
- **AC-2**: Manual check: UXA-HUD-27 ring states
  - Setup: show the live ring (Marigold with four cardinal ticks) and the plain ring. Capture both in colour and greyscale.
  - Verify: the two states differ by at least 3:1 in luminance and are distinguishable in greyscale by the ticks alone.
  - Pass condition: measured ratio >= 3:1, and `hud-interact-ring-live-1080p.png` and `hud-interact-ring-plain-1080p.png` are retained.
- **AC-3**: Cancel from own removal
  - Given: a tend cancelled by the owner's own removal.
  - When: `OnRoleChangePresented` fires.
  - Then: the cancel is held to `OnRoleChangePresented`, then the ring hides at once with the prompts. The ring empties in one step on cancel and clamps at 0.98.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/PromptAndRingTest.cpp`, test `StealthGame.HUD.PIE.PromptAndRing` OR a documented playtest; plus the retained screenshots `production/qa/evidence/hud-interact-ring-live-1080p.png` and `production/qa/evidence/hud-interact-ring-plain-1080p.png` with the measured luminance ratio recorded in the sign-off `production/qa/evidence/hud-interact-ring-and-context-prompt-evidence.md`. Gate level: BLOCKING (the Integration default; the screenshots are not waived at `qa.level: minimal`).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`, `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`, `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`; `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`; objectives-chain epic (`TendCueDelaySec`, OC-6; no stories yet)
- Unlocks: `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`, `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`
