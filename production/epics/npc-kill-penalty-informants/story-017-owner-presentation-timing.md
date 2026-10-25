# Story 017: Owner-side exile, informant-killed and tally timing

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-002`, `TR-pen-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§2.2a, §2.3 step 4, §4.4 the owner's view across unpossess), with ADR-0002 (server clock and the client estimator) and ADR-0001 (owner-only carriers)
**ADR Decision Summary**: The exile has one anchor on the owner's client: HUD's card-entrance event at the `WrongTarget` plate's show time `s` plus `ExileBeatSec`, gated on `bExiled` and the causing `ClientCommandSeq`; Camera's `ViewRelease(StrikeLimit)` fires from that event, so the screen is never black before the card; fallback fade at `ExileCardTimeoutSec` after the owner's `StrikeLimit` state arrives. A killed informant's view stays on its falling body and fades over `NoPawnFadeSec` from `DeathServerTime + DeathFallSec`, with no card. The tally is delivered inside `FStrikeResult` so a re-push can never beat a plate (HUD-R7 gate).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Risk is the highest of the governing ADRs: ADR-0001 is HIGH. The timing depends on the client server-time estimator (ADR-0002 §9; the engine's own `AGameStateBase` delta has no RTT compensation and stores a float, so the project's estimator is used), on engine paths that call `SetViewTarget` on unpossess (ADR-0019 §4.4: `bAutoManageActiveCameraTarget = false` and an `OnUnPossess` override, `AStealthGameMode::PlayerCanRestart_Implementation` returning false), and on an `Inactive` PlayerController state that must never spectate. Net emulation: `LeakNetProfile` (PktLag 100, PktLoss 1-2 %) plus the injected RPC lateness the AC names (+0, +300, +900 ms).

**Control Manifest Rules (this layer)**:
- Required: Camera alone sets view targets; the exile card, the camera release and the exile cue all follow one client-local card-entrance event — source: ADR-0019, ADR-0015
- Forbidden: never replicate the outcome at the slot and delay client presentation; never fire the exile cue from `bExiled` at contact — source: ADR-0018, GDD VA-NKP-5
- Guardrail: presentation is checked to ± 1 frame at 60 Hz — source: GDD Game Feel

These are the three timing contracts of the owner's own view: the exile beat (plate, held breath, card, camera release), the killed-informant fall-and-fade (no card, no cue), and the tally gate (never before the plate, never lower, equal to `TallyAfter` from the plate's show time). They are PIE timing tests that log presentation timestamps; the visuals themselves are stories 018 and 019.

**Prototype replaced:** the prototype's immediate `MessagePlayer(Offender, "Exiled for killing ...", true, 8.f)` on the server with an instant possession change in `ExilePlayer`, and `MessagePlayer` for an informant kill. These have no beat, no held view and no gating. Not promoted.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-38, AC-NKP-44, AC-NKP-46), scoped to this story:*

- [ ] **AC1** (AC-NKP-38, [I][PIE-3][Dep:Camera CR-10] `StealthGame.Penalty.PIE.ExileBeatTiming`): **GIVEN** an exiling kill with injected `FStrikeResult` RPC lateness of +0, +300 and +900 ms (three runs), and a fourth run where the RPC is dropped **WHEN** the owner client's presentation timestamps are logged **THEN** in the first three runs the plate appears at its show time `s` (HUD F2), the card enters at `s + ExileBeatSec` (± 1 frame), the view stays on the released body without control until the card-entrance event, `ViewRelease` fires from that event, and no frame is black before the card has entered; in the fourth run the camera fades at `ExileCardTimeoutSec` after the owner's `StrikeLimit` state arrives (± 1 frame) (PEN-R16).
- [ ] **AC2** (AC-NKP-44, [I][PIE-3][Dep:Camera CR-18][Dep:HUD CR-12] `StealthGame.Penalty.PIE.InformantKilledView`): **GIVEN** an embodied informant that is killed **WHEN** its own client's presentation timestamps are logged **THEN** the view stays on its falling body without control, fades over `NoPawnFadeSec` starting at `DeathServerTime + DeathFallSec` (± 1 frame), no frame is black before that, no removal card and no exile cue are shown, and the countdown appears only after the fade completes (PEN-R16).
- [ ] **AC3** (AC-NKP-46, [I][PIE-3][the never-before-plate and never-decreasing checks also apply to the client-local display cache] `StealthGame.Penalty.PIE.TallyRepushGate`): **GIVEN** an attacker whose `FStrikeResult` RPC is delayed by 500 ms while a reclaim re-push with an older `LastResultSeq` arrives first **WHEN** the HUD tally is sampled every frame **THEN** it never changes before the plate's show time, never decreases within the round, and equals `TallyAfter` from the plate's show time on (PEN-R3, PEN-R15, CR-12).

---

## Implementation Notes

*Derived from ADR-0019 Decision §2.3, §4.4 and PEN-R16:*

Penalty owns little code here; the behaviour is HUD's and Camera's, driven by data Penalty supplies at the right moment. Penalty's obligations: the exile must write `bExiled` into the same `FStrikeResult` as the causing `ClientCommandSeq` (story 003, 005); the owner's own `State -> Informant (StrikeLimit)` arrives through the Roster; the released body must stay the owner's view target through the unpossess (story 005 step 4: the factory's `RequestHandOver(Ai)` must not cut the view, `AStealthPlayerController::OnUnPossess` keeps the target for factory-initiated unpossess, ADR-0019 §4.4); and the countdown value (`T_due`, story 011) must not be visible before the fade completes (HUD's gate).

The tests are CQTest PIE tests that record client presentation timestamps through the harness' presentation-event log (HUD raises events: plate shown, card entered; Camera raises: view released, fade started/ended; audio raises: exile cue). Use the server clock estimator for the comparison (ADR-0002 §9) and ± 1 frame at the PIE frame rate. Inject RPC lateness with the harness' net emulation hook on the `ClientCommandReplies` batch (the GDD's +0, +300, +900 ms and the dropped-RPC case). For AC3 delay the owner's `FStrikeResult` by 500 ms and force a reclaim re-push (story 003) with an older `LastResultSeq`; sample the HUD display cache every frame (the cache is client-local and must be inspectable by a test accessor in non-Shipping builds).

Cross-epic dependencies are real: the card-entrance event and ViewRelease reasons (`StrikeLimit`, `InformantKilled`) are defined by HUD (CR-12) and Camera (CR-10, CR-18; camera-system `story-018-dying-hold-and-view-release.md`, `story-017-view-state-neutral-view-fade.md`). If a dependency is unbuilt when this story starts, stub the event at the harness seam and mark the dependent assertion pending; do not weaken the AC.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-018-tally-countdown-and-exile-card-ui.md`: how the tally line, countdown and card look (screenshots).
- `story-019-takeover-and-exile-visual-continuity.md`: body continuity at the release and takeover.
- hud and camera-system epics: the card-entrance event, `ViewRelease`, the fade and the neutral view themselves.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Penalty/ExileBeatTimingTest.cpp` (CQTest, `StealthGame.Penalty.PIE.ExileBeatTiming.*`)
- `Source/StealthTestHarness/Private/Penalty/InformantKilledViewTest.cpp` (CQTest, `StealthGame.Penalty.PIE.InformantKilledView.*`)
- `Source/StealthTestHarness/Private/Penalty/TallyRepushGateTest.cpp` (CQTest, `StealthGame.Penalty.PIE.TallyRepushGate.*`)
- `Source/StealthGameplay/Private/Features/Penalty/StealthPenaltyService.cpp` (only for defects the tests expose in the data Penalty supplies)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-38, `StealthGame.Penalty.PIE.ExileBeatTiming`)
  - Given: a fixed 60 Hz tick (+/- 1 frame = +/- 16.7 ms), RPC lateness of + 0, + 300 and + 900 ms (three runs) and a fourth run with the RPC dropped.
  - When: the owner client's presentation timestamps are logged.
  - Then: in runs 1-3 the plate shows at `s` (HUD F2), the card enters at `s + ExileBeatSec` (+/- 1 frame), the view stays on the released body without control, `ViewRelease` fires from the card-entrance event, no frame is black before the card; in run 4 the fade starts `ExileCardTimeoutSec` after the owner's `StrikeLimit` state arrives (+/- 1 frame) and the card shows when the data lands.
  - Edge cases: with + 900 ms `s` moves with the RPC arrival; the fixture needs a per-frame fade-alpha log to detect black frames.
- **AC2** (AC-NKP-44, `StealthGame.Penalty.PIE.InformantKilledView`)
  - Given: an embodied informant that is killed.
  - When: its own client's presentation timestamps are logged.
  - Then: the view stays on the falling body without control; the fade over `NoPawnFadeSec` starts at `DeathServerTime + DeathFallSec` (+/- 1 frame) with no black frame before; no removal card or exile cue; the countdown appears only after the fade completes.
  - Edge cases: the informant is killed during Picking and the neutral view is held.
- **AC3** (AC-NKP-46, `StealthGame.Penalty.PIE.TallyRepushGate`)
  - Given: an attacker's `FStrikeResult` RPC delayed 500 ms with a reclaim re-push carrying an older `LastResultSeq` arriving first.
  - When: the HUD tally is sampled every frame.
  - Then: it never changes before the plate's show time, never decreases within the round, and equals `TallyAfter` from the plate on; the same holds for the client display cache.
  - Edge cases: a re-push with an equal seq is ignored.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three CQTest PIE tests under `Source/StealthTestHarness/Private/Penalty/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-003-tally-rule-and-strike-result-supply.md`, `story-005-exile-sequence-in-attack-slot.md`, `story-008-informant-kill-inline-retire.md`, `story-011-t-due-owner-carrier-adapter.md`; cross-epic: camera-system `story-017-view-state-neutral-view-fade.md`, `story-018-dying-hold-and-view-release.md`; hud (card-entrance event, plate show time F2, tally display cache); round-match-framework (`ViewRelease` reasons, round end)
- Unlocks: `production/epics/npc-kill-penalty-informants/story-018-tally-countdown-and-exile-card-ui.md`
