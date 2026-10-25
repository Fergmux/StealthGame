# Story 026: PIE proof of removal card timing and the informant-killed and hold-cap cases

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (with ADR-0021 for the presenter and ADR-0008 for the multi-process harness)
**ADR Decision Summary**: ADR-0018 keeps the outcome blind until contact: the `FActionState` write and the owner-only result carry the contact time, so presentation shows the verdict no earlier than the owner's own contact frame. ADR-0021 puts every presentation timeline (F2 show time, F3 plate and card, F4 card time) in the plain C++ `FHudPresenterCore`, sampled from `ServerNowEst`, so these PIE runs can compare deterministic view-model dumps tick by tick.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0018 notes MotionWarping is Beta in 5.8.4 and the HIGH networking domain: the contact oracle is the `Death_Fall` / strike / `Counter` anim-notify tick on the client under test, never the HUD's own F2 mapping. The listen-server host and a remote client run the identical sampling path (no RepNotify dependency, ADR-0021). Tests are CQTest PIE tests in `StealthTestHarness` at a fixed 60 Hz step; multi-process runs depend on the harness from `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md` (ADR-0008). `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story proves, end to end in PIE, what stories 024 (logic) and 025 (widget) specify: the removal card enters exactly one beat after the visible fall or the exile plate, never before, never twice, and an informant killed while embodied gets no card at all.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-13 and AC-HUD-29 (verbatim, scoped to this story):*

- [ ] **AC1** (AC-HUD-13, `StealthGame.HUD.PIE.RemovalCardTiming`): **GIVEN** (a) an exile on the third NPC kill, (b) an assassinated victim, (c) a reversed attacker, **WHEN** the owner's dumps are inspected under the determinism rule, **THEN** (a) the card enters at the WrongTarget plate's `t_show + 0.8 s` and the plate exits over 0.25 s from that tick; (b) every tick before the victim's `Death_Fall` notify equals a control run, and the card enters 0.8 s after it; (c) "Reversed" shows at the attacker's contact tick and the card preempts it 0.8 s after the attacker's fall start; all marks ± 1 tick at 60 Hz; in all cases the card holds 4.0 s, collapses into the brackets, fires `OnRemovalCardEntered` once and is never shown again (including after a reconnect); (d) with the `FStrikeResult` of (a) delayed 1.5 s, the plate and card are still shown, the card 0.8 s after the late plate.
- [ ] **AC2** (AC-HUD-29, `StealthGame.HUD.PIE.InformantKilledAndHoldCap`): **GIVEN** (a) an embodied informant killed by an enemy strike, **WHEN** the owner's dumps are inspected under the determinism rule, **THEN** no removal card, exile cue or `OnRemovalCardEntered` event occurs, the frozen tally, brackets and objectives cluster are unchanged through the fall, the countdown first appears when Camera's fade completes (`DeathServerTime + DeathFallSec + NoPawnFadeSec`, ± 1 tick) with the value `ceil(T_due − ServerNowEst)`, and the view state returns to `InformantAwaiting`; **GIVEN** (b) an Active victim whose `FDeathState` is dropped (fault injection), **THEN** the strike hint, prompts and ring hide and the card enters at `t_state + HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec` (± 1 tick), and every tick before `t_state + HudOwnRemovalHoldCapSec` equals a control run without the strike; **GIVEN** (c) an Active victim for whom `Phase = Ended` arrives between `t_state` and `t_card`, **THEN** no card is shown and the banner is at the own fall start (F10).

---

## Implementation Notes

*Derived from ADR-0018, ADR-0021 §4 and ADR-0008, GDD determinism rule and HUD-R9 / F4 / F7:*

**What to build.** Two CQTest PIE scenario files in `StealthTestHarness` (`NETWORK_TEST_CLASS(RemovalCardTiming, "StealthGame.HUD.PIE")` and `NETWORK_TEST_CLASS(InformantKilledAndHoldCap, "StealthGame.HUD.PIE")`) plus whatever fixture glue they need. No production presenter code is written here: if a dump field, fixture hook or time source is missing, add the smallest test-only accessor (non-Shipping) and note it.

**Determinism rule (every comparison).** Fixed step 60 Hz (`-benchmark -fps=60` or the automation fixed-delta equivalent), identical seeds, dumps keyed by tick index with wall-clock fields excluded. "Contact" oracles come from the strike, `Death_Fall` or `Counter` anim-notify tick on that client, never from the HUD's own F2 mapping. "Unchanged" means equal to a control run with the same seed and no strike (a running cooldown sweep or tend ring is compared, not expected to freeze). Disable TAA where captures are compared.

**Scenarios.**
- AC1 (a): 2v2 PIE, the owner reaches the third wrong-target kill (NPC); read `t_show` of the WrongTarget plate from the owner's dump and assert the card's first `Entering` tick is `t_show + ExileBeatSec` (0.8 s) and the plate's preempt exit starts on that tick and lasts 0.25 s.
- (b): enemy strikes the owner; the owner's dump before the `Death_Fall` notify tick must equal the control run; the card enters 0.8 s (`HudRemovalCardDelaySec`) after that tick.
- (c): the owner strikes into a brace; "Reversed" shows at the contact tick (`Counter` notify) and the card preempts it 0.8 s after the attacker's fall start.
- (d): delay the (a) `FStrikeResult` 1.5 s (net-emulation or a harness delay on the reply sink); plate and card still show, the card 0.8 s after the late plate. The plate is never dropped for lateness (F2).
- In every case: hold 4.0 s (x `HudReadHoldMult`), collapse over 0.8 s, `OnRemovalCardEntered` fires once, and a reconnect (same-controller rebuild, then a rejoin) never shows the card again. All marks are ± 1 tick.
- AC2 (a): embodied informant killed. Assert no card, no exile cue id in the cue stream, no `OnRemovalCardEntered`; tally, brackets and objectives cluster unchanged through the fall; the countdown first appears at `DeathServerTime + DeathFallSec + NoPawnFadeSec` with value `ceil(T_due - ServerNowEst)` and the view state returns to `InformantAwaiting`.
- (b): fault-inject a dropped `FDeathState` for an Active victim. Strike hint, prompts and ring hide at `t_state + HudOwnRemovalHoldCapSec` (the cap is treated as the fall start); the card enters at `t_state + HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec`; every earlier tick equals the control.
- (c): the victim's strike ends the round (`Phase = Ended` arrives between `t_state` and `t_card`); no card, and the banner at the own fall start (F10, ruling D12).

**Traps (GDD Edge Cases).** A late `FStrikeResult` delays the card with its plate while Camera's `ExileCardTimeoutSec` fallback fades the view meanwhile; the card shows when the data lands. An embodied informant has no `State` change, so nothing may key a card off the death. The raw own `State` arrives up to `StrikeContactSec` before the visible blow: nothing visible may change before the fall (HUD-R6a).

**Hooks needed.** AC2 (b) needs a fault-injection hook for `FDeathState`; AC2 needs the multi-process harness. Spec status: AC2 is DEFERRED until IVP story 001 lands (see QA Test Cases).

**Prototype.** Nothing from `Source/StealthGame/Private/StealthHUD.cpp` is used or promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`: the F4 logic and its unit test (`StealthGame.HUD.RemovalCardTiming`).
- `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`: the card widget, wording and stepped preempt.
- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`: the plates the card preempts.
- `production/epics/hud/story-034-hud-informant-presentation.md`: informant view contract after the card (AC-HUD-21).
- `production/epics/hud/story-055-hud-photosensitivity-removal-sequence.md`: the budgeted removal sequence.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/RemovalCardTimingTest.cpp` (CQTest, `StealthGame.HUD.PIE.RemovalCardTiming.*`)
- `Source/StealthTestHarness/Private/HUD/InformantKilledAndHoldCapTest.cpp` (CQTest, `StealthGame.HUD.PIE.InformantKilledAndHoldCap.*`)
- `Source/StealthTestHarness/Private/HUD/HudPieFixtures.h` and `.cpp` (shared dump-by-tick and fault-injection helpers; created by an earlier HUD PIE story if present, otherwise here)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-13 (restated)
  - Given: (a) an exile on the third NPC kill, (b) an assassinated victim, (c) a reversed attacker, (d) the (a) `FStrikeResult` delayed 1.5 s, in PIE at 60 Hz with fixed seeds.
  - When: the owner's dumps are read.
  - Then: (a) the card enters at plate `t_show` + 0.8 s, and the plate exits over 0.25 s from that tick. (b) Ticks before `Death_Fall` equal a control run, and the card enters 0.8 s after. (c) "Reversed" shows at the contact tick, and the card preempts it 0.8 s after the fall start. (d) The card enters 0.8 s after the late plate. All marks +/-1 tick. In every case the card holds 4.0 s, collapses, fires once and never reappears, including after a reconnect.
  - Edge cases: none.
- **AC-2**: AC-HUD-29 (restated)
  - Given: (a) an embodied informant killed, (b) an Active victim with `FDeathState` dropped (fault injection), (c) `Ended` between `t_state` and `t_card`.
  - When: dumps are read.
  - Then: (a) no card, cue or event, with tally, brackets and objectives unchanged. The countdown appears at fade completion with `ceil(T_due - ServerNowEst)`. (b) The card enters at `t_state + HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec`, and ticks before `t_state + HudOwnRemovalHoldCapSec` equal control. (c) No card, and the banner at own fall start.
  - Edge cases: needs a fault-injection hook and the multi-process harness. DEFERRED until IVP story 001 lands.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/RemovalCardTimingTest.cpp` (`StealthGame.HUD.PIE.RemovalCardTiming`) and `Source/StealthTestHarness/Private/HUD/InformantKilledAndHoldCapTest.cpp` (`StealthGame.HUD.PIE.InformantKilledAndHoldCap`) — must exist and pass (gate BLOCKING; AC2 deferred until IVP story 001 lands)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`, `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`; `production/epics/npc-kill-penalty-informants/story-005-exile-sequence-in-attack-slot.md`; `production/epics/camera-system/story-018-dying-hold-and-view-release.md`
- Unlocks: `production/epics/hud/story-055-hud-photosensitivity-removal-sequence.md`
