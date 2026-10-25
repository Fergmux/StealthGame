# Story 014: PresentedOwnState (HUD-R6a), the viewer state table and the visibility matrix

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§4 view-models and the presenter, §8 input contexts follow the presented own role)
**ADR Decision Summary**: One plain C++ presenter core, `FHudPresenterCore`, owns every presentation state machine the GDD defines, including the viewer state table and `PresentedOwnState` (HUD-R6a), and turns an `FHudInputs` snapshot plus `ServerNowEst` and `LocalNow` into an `FHudViewModel`. Presentation is sampled every frame by value, never driven by RepNotify, so the listen host runs the identical path. The Informant input context follows `PresentedOwnState`, selected only by the pure `UseInformantContext` function (the input-context swap itself is input-interaction-framework story 071).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. The ADR-0021 verification items UI-V1 to UI-V4 are not touched by this story. ADR-0018 §5.2 defines the client-local `OnRoleChangePresented` dispatch this story consumes through a queued field of `FHudInputs`; the `FEncounterPresentation::Refresh` handler that fires it is the assassination epic's, and the HIGH networking domain (MotionWarping Beta in 5.8.4) is not involved here. Use `UE_LOGF` rather than `UE_LOG` for any diagnostics.

**Control Manifest Rules (this layer)**:
- Required: Presentation builds view-models by sampling its carriers every frame by value (the listen host runs the same path) — source: ADR-0021
- Forbidden: Never drive the Informant context from the raw own `State` — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

Every own-role predicate in the HUD (the strike hint, prompts and ring, the brace indicator, the tally freeze, the informant brackets, HUD-R8) must read one client-local value, `PresentedOwnState`, which changes from `Active` to `Informant` only at the own removal presentation. This story builds that value and the eight-state viewer machine around it inside the plain C++ core created by story 002, so that every later logic story (brace, tally, removal card, countdown) reads it instead of the raw C-TeamView own `State`. It also fixes the contract for what is "held" until the flip: outcome-caused owner data that reaches the client early is presented at the flip, never on receipt, because a frozen or cancelled element would otherwise announce the outcome before contact (ASN-R6).

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (HUD-R6a, States and Transitions) and `design/ux/hud.md` (UXA-HUD-08), scoped to this story:*

- [ ] **AC1** (derived, HUD-R6a and the States table, `StealthGame.HUD.PresentedOwnState`): **GIVEN** raw own `State` goes Active to Informant on arrival **THEN** `PresentedOwnState` stays Active until `OnRoleChangePresented` (Assassinated, ReversedAgainst), the exile card time (StrikeLimit) or, for a rejoin after the grace, at once; held outcome data (own State, FHoldStatus cancel, NotNow replies, tally re-push, `bExiled`) is presented at that moment, never on receipt; the raw State is read only inside the `PresentedOwnState` owner.
- [ ] **AC2** (derived, States table, `StealthGame.HUD.ViewerStates`): **GIVEN** the viewer state machine (Lobby, Briefing, ActiveAlive, Eliminating, InformantAwaiting, InformantBodied, Ended, CalledOff) **THEN** it follows the States table, including: own `RoundEnded` from `EndRoundAll` is ignored (also a net frame before `Phase = Ended`); any state hard-clears and rebuilds on `RoundEpoch` change or travel.
- [ ] **AC3** (UXA-HUD-08, `StealthGame.HUD.ViewerStates`): For each viewer state (Lobby, Briefing, Self-Active, Eliminating, Informant Awaiting, Informant Bodied, Ended, Called off) the visible elements equal the visibility matrix; the Briefing shows no HUD element at all.

---

## Implementation Notes

*Derived from ADR-0021 Decision §4 and §8, GDD HUD-R6a, HUD-R9, HUD-R17 and the States and Transitions table:*

**Where it lives.** Plain C++ in `Source/StealthPresentation/Private/Hud/`, as part of `FHudPresenterCore` (story 002 created the shell, `FHudInputs`, `FHudViewModel` and the fake clock). No `UWorld`, no `UObject`. Two small owners: `FHudPresentedOwnState` (the HUD-R6a owner, the only code that reads the raw own `State` field of `FHudInputs`) and `FHudViewerStateMachine` (the table), both stepped from `FHudPresenterCore::Step`. The view-model gains `PresentedOwnState` and `ViewerState` fields plus an element-visibility mask used by the dump.

**`PresentedOwnState` rules (HUD-R6a, HUD-R9, F4).** Values `Active` / `Informant` (the `EPresentedOwnRole` of ADR-0021 Key Interfaces; do not create a second enum). It flips Active to Informant only at: (1) `OnRoleChangePresented` for Assassinated or ReversedAgainst (a queued `FHudInputs` field carrying the reason and the local time, from ADR-0018 §5.2); (2) the exile card time for StrikeLimit (story 024 computes `t_card`; this story exposes the flip input); (3) a rejoin after the grace, at once and with no card. If `OnRoleChangePresented` has not fired within `HudOwnRemovalHoldCapSec` (1.25 s, I-HUD-2) of the raw State arriving, the cap is treated as the fall start (F4): the flip is then `t_state + HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec` as story 024 defines it; this story only owns that the raw arrival alone never flips the value. A rebuild (HUD-R17) rebuilds `PresentedOwnState` from the current own `State` at once with no card. `Ended` arriving before the flip freezes the value: no later flip.

**Held data.** Everything outcome-caused that reaches the client before the flip is buffered inside the presenter core and released at the flip: the own `State`, an `FHoldStatus` cancel of a hold the victim was in, `NotNow` replies to its commands, the tally re-push and `bExiled`. Implement one `FHeldOwnerData` struct with a `Hold(...)` and a `ReleaseAt(flipTime)` and give each consumer story (018 brace, 020 tally, 024 card, the informant-context path of IIF 071) a single read accessor. Until the flip the HUD runs exactly as in `ActiveAlive` (the cooldown sweep and numeral keep moving, a tend ring keeps its prediction). Exception D1: when `OnRoleChangePresented` fires for the owner's own fall, the strike hint, the prompts and the ring hide at once while the brace indicator stays until the card; expose this as a separate `bOwnFallStarted` flag, not as a `PresentedOwnState` change.

**The raw-State spy.** The test wraps `FHudInputs` so that reading the raw `State` field outside `FHudPresentedOwnState` fails the test (a counting accessor, compiled only under `WITH_DEV_AUTOMATION_TESTS`). Do not rely on a lint for this: the invariant is "one reader".

**Viewer states.** Implement the table at GDD lines 133-142 as written, as data in the test and as a transition function in code. Entries: `Lobby` (`Phase = Lobby`, or `Reset` without `bCalledOff`), `Briefing` (`Phase = Preparing`; shows nothing), `ActiveAlive` (`Phase = Active`, `PresentedOwnState = Active`, own pawn bound), `Eliminating` (own fall start or exile plate shown, own State held), `InformantAwaiting` (Informant, no pawn), `InformantBodied` (Informant, pawn bound), `Ended`, `CalledOff`. Transitions that need care: Briefing to ActiveAlive needs the own pawn bound, and a late join after the grace with State Informant and no pawn goes Briefing to InformantAwaiting; an own `RoundEnded` written by `EndRoundAll` is ignored as a removal (it creates no state change and does not move `PresentedOwnState`), also when it lands a net frame before `Phase = Ended`; `InformantKilled` leaves the state unchanged through the fall and then returns to `InformantAwaiting` when Camera's fade completes (the countdown timing is story 024 AC3 and story 033); any state hard-clears and rebuilds on a `RoundEpoch` change or travel (HUD-R17, rebuild details in story 044).

**Visibility matrix.** The UX spec section "Element visibility by viewer state" (`design/ux/hud.md` lines 517-539) is embedded in the test as expected data: for each of the 8 states, elements marked "shown" are present in the dump, "-" elements are absent, and contextual elements appear only when their own trigger holds (each contextual row needs a trigger fixture: a feed line needs a pending line, the strike hint needs the predicate of story 030, and so on; where the contextual element's own story is not built yet, the fixture asserts the matrix's allowance and the story's own test asserts the trigger). The Briefing shows zero elements, including no persistent cluster. Elements are identified by the UX ids E01-E22 in the dump, not by widget class.

**Decisions that apply.** D1 (own-fall exception above), D3 (brace label persistent, E01 is story 018/019), D4 (informant tally has no triangle, story 020), D12 (banner never shows before the own fall start, story 040).

**Out of this story:** the F4 card time arithmetic and `OnRemovalCardEntered` (story 024), the brace and tally models (018, 020), the input-context swap (IIF story 071 consumes `PresentedOwnState` through `SetPresentedOwnRole`), the rebuild logic itself (story 044), the widgets.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`: F4 own removal time, card gating and `OnRemovalCardEntered` (this story only holds and releases the data).
- `production/epics/hud/story-018-hud-brace-indicator-state-logic.md` and `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`: the brace and tally models that read `PresentedOwnState`.
- `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`: the full HUD-R17 reset and rebuild procedure (this story only requires that the state machine hard-clears).
- `production/epics/hud/story-030-hud-strike-hint-predicate-and-lockout.md`: the strike hint predicate.
- `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`: Ended presentation, the banner and F10.
- `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md`: `UseInformantContext` and the Informant input context swap.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudPresentedOwnState.h`
- `Source/StealthPresentation/Private/Hud/HudPresentedOwnState.cpp`
- `Source/StealthPresentation/Private/Hud/HudViewerStateMachine.h`
- `Source/StealthPresentation/Private/Hud/HudViewerStateMachine.cpp`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (step the two owners; add the fields to `FHudViewModel` and `Dump()`)
- `Source/StealthPresentation/Private/Tests/HUD/PresentedOwnStateTest.cpp` (class `FHudPresentedOwnStateTest`, name `StealthGame.HUD.PresentedOwnState`)
- `Source/StealthPresentation/Private/Tests/HUD/ViewerStatesTest.cpp` (class `FHudViewerStatesTest`, name `StealthGame.HUD.ViewerStates`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `PresentedOwnState` hold
  - Given: own State goes Active to Informant at `t_state` for each cause (Assassinated, ReversedAgainst, StrikeLimit), plus a rejoin after the grace.
  - When: `Step` runs past `OnRoleChangePresented` or the exile `t_card`.
  - Then: `PresentedOwnState` stays Active until `OnRoleChangePresented` or `t_card`, and flips exactly then. A rejoin after the grace is Informant at once. The held items (own State, `FHoldStatus` cancel, NotNow replies, tally re-push, `bExiled`) are presented at the flip, not on receipt. A spy shows raw State is read only inside the owner.
  - Edge cases: a missing `OnRoleChangePresented` gives `t_state + HudOwnRemovalHoldCapSec`. `Ended` arrives before the flip.
- **AC-2**: Viewer state machine
  - Given: each row of the States table (GDD lines 133-142).
  - When: the triggering inputs are injected.
  - Then: state and transition match the row. Own `RoundEnded` from `EndRoundAll` is ignored, including one frame before `Phase = Ended`. A `RoundEpoch` change or travel hard-clears and rebuilds.
  - Edge cases: embed the table as expected data.
- **AC-3**: Visibility matrix
  - Given: each of the 8 viewer states.
  - When: the dump is taken.
  - Then: elements marked "shown" are present, "-" elements are absent, and contextual elements appear only when their own trigger holds. Briefing has zero elements.
  - Edge cases: embed the UX matrix (lines 517-539). Each contextual row needs a trigger fixture.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/PresentedOwnStateTest.cpp` (`StealthGame.HUD.PresentedOwnState`) and `Source/StealthPresentation/Private/Tests/HUD/ViewerStatesTest.cpp` (`StealthGame.HUD.ViewerStates`) — must exist and pass. Gate level BLOCKING.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`; cross-epic: input-interaction-framework `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md` (`UseInformantContext`, the consumer of `PresentedOwnState`)
- Unlocks: `production/epics/hud/story-018-hud-brace-indicator-state-logic.md`, `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`, `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`, `production/epics/hud/story-030-hud-strike-hint-predicate-and-lockout.md`, `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`, `production/epics/hud/story-033-hud-returning-countdown-f7.md`, `production/epics/hud/story-034-hud-informant-presentation.md`, `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`, `production/epics/hud/story-039-hud-objectives-cluster-f9-logic.md`, `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`, `production/epics/hud/story-044-hud-reset-and-rebuild-logic.md`
