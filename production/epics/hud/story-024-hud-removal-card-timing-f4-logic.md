# Story 024: F4 own removal time, card gating and the once-only OnRemovalCardEntered event

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

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§5.2 own-role gating through `OnRoleChangePresented`; §5.4 the exile's presentation clock is HUD's card-entrance event), with ADR-0021 (§4 presenter core owns F4)
**ADR Decision Summary**: A client-local `OnRoleChangePresented` fires for the locally possessed pawn once per `FDeathState` at `DeathServerTime` (or on arrival when late), and HUD, Camera and the input-context swap subscribe to it; none may subscribe to the arrival of the Roster state. The exile (`StrikeLimit`) has no own death, so its presentation clock is HUD's card-entrance event (PEN-R16, HUD-R9). The presenter core owns the F4 card time, sampled from `ServerNowEst`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. The `OnRemovalCardEntered` event is a plain C++ delegate on the presenter's output (a queue the host `UStealthHudPresenter` drains into a native multicast that Camera's exile `ViewRelease` and the removal cue subscribe to; that native binding is a presenter-shell detail in story 003). ADR-0018's HIGH networking domain applies to the data feeding F4 (`FDeathState`, the owner `State`, `bExiled`); MotionWarping is Beta in 5.8.4 but only plays the fall. Use `UE_LOGF` for diagnostics.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never sample plate or card timelines from widget animation assets — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

Own elimination is the most delicate presentation in the HUD: the owner's `State` arrives up to a contact delay before anything is visible, so everything about their removal (the card, the informant brackets, hiding the brace indicator, the frozen tally) is gated to one computed moment, the F4 card time, and nothing may happen on receipt (HUD-R9). This story builds that computation, the gating of the card, the once-only `OnRemovalCardEntered` event (Camera's exile `ViewRelease` and the removal cue hang off it), and the two special cases that must not produce a card: a round that ends first, and an embodied informant being killed. It is pure logic; the card widget is story 025 and the PIE proof is story 026.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (HUD-R9, F4, States and Transitions), scoped to this story:*

- [ ] **AC1** (derived F4, HUD-R9, `StealthGame.HUD.RemovalCardTiming`): **GIVEN** an exile **THEN** `t_card = t_show(WrongTarget plate) + ExileBeatSec`; **GIVEN** assassinated or reversed **THEN** `t_fall` is `OnRoleChangePresented` or `t_state + HudOwnRemovalHoldCapSec` when it has not fired, and `t_card = t_fall + HudRemovalCardDelaySec`; a late `FStrikeResult` delays the card with its plate; the card shows once, holds `HudRemovalCardHoldSec`, collapses, fires `OnRemovalCardEntered` exactly once per entrance and is never shown again including after a reconnect.
- [ ] **AC2** (derived, HUD-R9 and F10 interaction, `StealthGame.HUD.RemovalCardTiming`): **GIVEN** `Phase = Ended` before `t_card` **THEN** no card is shown; **GIVEN** an entering card **THEN** the card finishes its entrance then exits over `HudPreemptExitSec` as the banner enters.
- [ ] **AC3** (derived, HUD-R9 Informant killed, `StealthGame.HUD.RemovalCardTiming`): **GIVEN** an informant killed (embodied informant, no State change) **THEN** there is no card, no exile cue and no `OnRemovalCardEntered`; the countdown appears when Camera's fade completes (`DeathServerTime + DeathFallSec + NoPawnFadeSec`) and the view state returns to `InformantAwaiting`.

---

## Implementation Notes

*Derived from GDD HUD-R9, F4, HUD-R6a and ADR-0018 §5.2 and §5.4:*

**What to build.** `FHudRemovalCard` in `Source/StealthPresentation/Private/Hud/`, plain C++, stepped from `FHudPresenterCore::Step` after the result timer and `PresentedOwnState` owner, writing `FHudCardView` (kind `None | Removal`, cause `Exile | Removed`, alpha from the F3 timeline of story 016, `bVisible`) and an output queue `TArray<FHudEvent>` containing `OnRemovalCardEntered` events. It consumes: the held own `State` and `bExiled` from `FHeldOwnerData` (story 014), the WrongTarget plate's `t_show` and the causing `ClientCommandSeq` from the result timer (story 015), `OnRoleChangePresented` (reason, local time) from `FHudInputs`, `DeathServerTime` and `FDeathState` arrival, `Phase`, the offset, and the tuning values `ExileBeatSec` (Penalty key, 0.8, safe range 0.75-0.95), `HudRemovalCardDelaySec` (0.8, hard 0.5-1.2, invariant I-HUD-3 rejects a preset where it differs from `ExileBeatSec`), `HudOwnRemovalHoldCapSec` (1.25, hard minimum 1.25, I-HUD-2) and the card times `HudRemovalCardInSec/HoldSec/OutSec` (0.6/4.0/0.8).

**F4.**
- Exile: `t_card = t_show(WrongTarget plate of seq_exile) + ExileBeatSec`, gated on `bExiled` and the causing `ClientCommandSeq` (PEN-R15). The plate is never dropped for lateness (F2), so a late `FStrikeResult` delays the card with its plate (Camera's `ExileCardTimeoutSec` fallback fades the view meanwhile, and the card shows when the data lands).
- Assassinated / ReversedAgainst: `t_fall` = the local time `OnRoleChangePresented` fires (`DeathServerTime − offset`, or the late `FDeathState`'s arrival, ASN-R23) if it fires by `t_state + HudOwnRemovalHoldCapSec`, else `t_fall = t_state + HudOwnRemovalHoldCapSec`; `t_card = t_fall + HudRemovalCardDelaySec`. The strike hint, prompts and ring hide at `t_fall` (HUD-R6a, D1; the hiding itself is the strike-hint and prompt stories, this story exposes `t_fall` as the flag `bOwnFallStarted`, already declared by story 014).
- `t_removal` (the moment `PresentedOwnState := Informant`: brace indicator hidden, informant brackets and frozen tally presented) equals `t_card`; hand `t_card` to story 014's flip owner.
- `Phase = Ended` before `t_card`: no card (HUD-R9, F10).

**Worked values (the test data).** Exile: plate `t_show` 10.00 gives the card at 10.80, fully in at 11.40, plate alpha falling 1 to 0 over 10.80-11.05 (preempt, story 016). Assassination: `t_state` 20.00, death state observed at 20.25 with the fall start mapped to 20.40 gives the card at 21.20 (20.40 + 0.8), fully in 21.80, collapse at 25.80-26.60 (21.20 + 0.6 + 4.0 = 25.80, plus 0.8). Assassination with no death state: `t_fall = 20.00 + 1.25 = 21.25`, card at 22.05 (20.00 + 1.25 + 0.8). A late `FStrikeResult` for an exile: the card follows the plate by 0.8 s whenever the plate is eventually shown.

**Once-only and never replayed.** The card shows once, holds `HudRemovalCardHoldSec` times `HudReadHoldMult`, collapses over `HudRemovalCardOutSec` (the informant brackets are already present, HUD-UX-16), fires `OnRemovalCardEntered` exactly once per entrance, and is never shown again, including after a reconnect: a rebuild rebuilds `PresentedOwnState` from the current own `State` at once with no card (HUD-R17), and a rejoin after the grace enters `InformantAwaiting` or `InformantBodied` with no card (the card belongs to a moment the player did not see). Keep a per-`RoundEpoch` `bCardConsumed` flag; do not store it in a replicated value.

**`Ended` and preempt (AC2).** If `Phase = Ended` arrives while the own removal is pending: no card is shown. If it arrives while the card is entering: the card finishes its entrance, then exits over `HudPreemptExitSec` as the banner enters (F10 never shows the banner before the own fall start, ruling D12; the banner is story 040). A card already holding exits over `HudPreemptExitSec` too (preempt rule of story 016). Also, when the card enters, a result plate still in the Result slot leaves over `HudPreemptExitSec` so that after 0.25 s exactly one text block is on screen (HUD-R9, photosensitivity sequence VA-HUD-4 in story 055).

**Informant killed (AC3).** An embodied informant has no `State` change, so there is no card, no exile cue and no `OnRemovalCardEntered`; the view stays on the own falling body and Camera fades it from `DeathServerTime + DeathFallSec`; the returning countdown appears only when that fade completes: `t_countdown = DeathServerTime + DeathFallSec + NoPawnFadeSec` (mapped through the offset), and the viewer state goes `InformantBodied` to `InformantAwaiting` then (the state machine is story 014's; this story supplies the instant). If `Ended` arrives during the fall, no countdown. The reason value (`InformantKilled`) is client-local, from `OnRoleChangePresented`'s reason, and is never inferred from the raw `State`.

**The event.** `OnRemovalCardEntered` is a client-local event raised at every card entrance (never on receipt); Camera's exile `ViewRelease` (Penalty CR-10, Camera CS-R12.1) and the removal cue (story 051) subscribe. It carries no killer, kind or team: no element names the killer.

**Decisions that apply.** D9 (late results show on processing; the card follows), D11 (card wording, story 025), D12 (banner not before own fall start, story 040), D1 (hint hidden at the fall), D8 (hard minimum 1.25 for the cap values).

**Out of this story:** the card widget and its preempt rendering (025), the PIE timing proof with a real server (026), the countdown F7 (033), the end banner (040), Camera's `ViewRelease` handling (camera-system epic).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`: the card widget, its text and preempt rendering.
- `production/epics/hud/story-026-hud-removal-card-pie-timing.md`: the PIE proof (`StealthGame.HUD.PIE.RemovalCardTiming`, a different name from this story's logic test).
- `production/epics/hud/story-033-hud-returning-countdown-f7.md`: the returning countdown and its F7 formula.
- `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`: F10 and the banner that preempts the card.
- `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`: `PresentedOwnState` and the viewer state machine this story feeds.
- `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`: the removal cue subscriber.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudRemovalCard.h`
- `Source/StealthPresentation/Private/Hud/HudRemovalCard.cpp`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (step the card logic; add `FHudCardView` and the event queue to the view model and `Dump()`)
- `Source/StealthPresentation/Private/Tests/HUD/RemovalCardTimingTest.cpp` (class `FHudRemovalCardTimingTest`, name `StealthGame.HUD.RemovalCardTiming`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `RemovalCardTiming` (logic)
  - Given: an exile plate `t_show` 10.00. An assassination with `t_state` 20.00 and fall 20.40. An assassination with no death state. A late `FStrikeResult`.
  - When: `Step` runs.
  - Then: the exile card enters at 10.80. The assassination card enters at 21.20, or 22.05 with no death state (20.00 + 1.25 + 0.8). A late result delays the card with its plate. The card holds `HudRemovalCardHoldSec`, collapses, fires `OnRemovalCardEntered` once and never shows again, including after a reconnect.
  - Edge cases: the PIE test in story 026 has a different name (`...PIE.RemovalCardTiming`).
- **AC-2**: `Ended` preempt
  - Given: `Phase = Ended` before `t_card`, and during an entering card.
  - When: the banner is due.
  - Then: no card in the first case. In the second the card finishes entering, then exits over `HudPreemptExitSec`.
  - Edge cases: none.
- **AC-3**: Informant killed
  - Given: an embodied informant is killed.
  - When: the fall completes.
  - Then: no card, no exile cue, no event. The countdown appears at `DeathServerTime + DeathFallSec + NoPawnFadeSec`, and the state returns to `InformantAwaiting`.
  - Edge cases: `Ended` during the fall.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/RemovalCardTimingTest.cpp` (`StealthGame.HUD.RemovalCardTiming`) — must exist and pass. Gate level BLOCKING.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`, `production/epics/hud/story-015-hud-result-show-time-f2.md`, `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`
- Unlocks: `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`, `production/epics/hud/story-026-hud-removal-card-pie-timing.md`, `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`, `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`
