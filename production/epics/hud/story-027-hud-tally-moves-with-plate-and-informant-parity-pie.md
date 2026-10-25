# Story 027: PIE proof that the tally never moves before its plate and informant kills equal NPC kills

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-002`, `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (with ADR-0018 for the owner-only result timing)
**ADR Decision Summary**: ADR-0008 makes leak and parity checks first-class automated tests: headless pure-C++ cores plus multi-client PIE runs compared by deterministic dumps, with net-emulation profiles. ADR-0018 keeps results owner-only and contact-timed, so an outcome-caused owner datum (the tally re-push, `State`, `bExiled`) that reaches the client before the plate is held and presented at the plate's `t_show`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: PIE comparisons run at a fixed 60 Hz step with TAA off and identical seeds. The tally re-push is a C-TeamView owner-part field pair (`WrongTargetTally`, `LastResultSeq`) written by the server; the test needs a carrier-injection hook (non-Shipping) that delivers a re-push, an own `State = Informant` and `bExiled` 0, 100 and 200 ms before the `FStrikeResult` without a real server timing dependency. The listen host and a remote client use the same sampling path (no RepNotify dependency, ADR-0021). `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Presentation builds view-models by sampling its carriers every frame by value (the listen host runs the same path) — source: ADR-0021
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

Two of the leak rules meet the tally here: an early server datum must not move the displayed tally, squares or triangle before the plate that explains them (HUD-R7), and killing an enemy informant must be indistinguishable on the attacker's HUD from killing an NPC (Roster R10).

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-03 and AC-HUD-02 (verbatim):*

- [ ] **AC1** (AC-HUD-03, `StealthGame.HUD.TallyNotBeforePlate`): **GIVEN** an injected owner tally re-push, own `State = Informant` and `bExiled` (third wrong kill) arriving 0, 100 and 200 ms before the `FStrikeResult`, and a control run where they arrive with it, **WHEN** the HUD runs, **THEN** every tick before the plate's `t_show` equals the control run (tally, squares, triangle, brace sweep, strike hint, prompts), the tally changes on the `t_show` tick, and the exile card enters at `t_show + ExileBeatSec` (± 1 tick).
- [ ] **AC2** (AC-HUD-02, `StealthGame.HUD.PIE.InformantNpcParity`): **GIVEN** an attacker with tally 0, **WHEN** it kills an enemy informant (run 1) and an NPC (run 2), **THEN** its view-model dumps and cue events are equal on every frame from the press to 5 s after `t_show`, including "Wrong target", tally 1, the square fill and the tally tick (Roster R10; extends AC-NKP-03).

---

## Implementation Notes

*Derived from ADR-0008, ADR-0021 §4 and GDD HUD-R6a, HUD-R7, HUD-R3 and F6:*

**What to build.** Two CQTest scenarios in `StealthTestHarness` and a carrier-injection hook:

1. `TallyNotBeforePlate`: the production presenter (story 020's tally logic, story 014's `PresentedOwnState`) is driven with the real carriers plus an injection hook that writes the owner re-push (`WrongTargetTally` 3, `LastResultSeq` of the causing strike), own `State = Informant` and `bExiled` at offsets 0, 100 and 200 ms before the `FStrikeResult` arrives, plus a control run where all arrive with it. The test name is the GDD's (`StealthGame.HUD.TallyNotBeforePlate`, not under the `PIE` group in the GDD): keep that registered name so the AC text and the test agree; if the harness registers CQTest classes under `StealthGame.HUD.PIE`, record the final name in the story file when done.
2. `InformantNpcParity`: 2v2 PIE with identical seeds. Run 1: attacker (tally 0) strikes an enemy informant; run 2: the same attacker strikes an NPC at the same distance and bearing. Compare the attacker's `FHudViewModel::Dump()` and cue events on every tick from the press to `t_show + 5 s`, TAA off. Informant and NPC victims must be byte-identical (Roster R10), including "Wrong target", tally 1, the square fill (300 ms stepped) and the tally tick.

**What the comparison covers (AC1).** Every tick before `t_show`: tally numeral, squares, danger triangle, brace sweep and numeral, strike hint, prompts. All must equal the control tick for tick. On the `t_show` tick the tally changes (stepped fill, one-step numeral). The exile card enters at `t_show + ExileBeatSec` (0.8 s), ± 1 tick. While the early datum is held, the HUD keeps running as in `ActiveAlive` (HUD-R6a): the brace cooldown sweep keeps moving and nothing freezes early.

**Rules under test.** HUD-R7: a C-TeamView owner re-push of the tally that arrives before its plate is held (the HUD keeps the last shown value) and is used directly only when no result plate is pending and only if `LastResultSeq` is at least the last plate's seq; it never lowers the displayed tally within a round (this story proves the timing; the gate logic is story 020). HUD-R6a: the own `State`, `bExiled`, the tally re-push and the `FHoldStatus` cancel are held and presented at `PresentedOwnState`'s change, never on receipt.

**Traps.** The re-push arrives on the owner part up to `StrikeContactSec` before anything is visible; any HUD element keyed to the raw datum would announce the outcome early. The test must also hold with the presenter bound (it always is in PIE). A 0 ms offset is still "with the result" for ordering inside one drain, so the plate path must win.

**Prototype.** The prototype HUD reads a team field replicated on every pawn and shows counts directly; these leaks are never promoted (EPIC prototype disposition).

**Out of this story.** The tally logic and widget (stories 020 and 021), the plate flow (story 022), and the strike-target parity run (story 028).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`: F6 tally display, the re-push gate and the client-local cache.
- `production/epics/hud/story-021-hud-wrong-target-tally-widget.md`: the squares, numeral and triangle widget.
- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`: plates, text, stamp and cue ids.
- `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md`: strike-target parity (AC-HUD-01) and look parity (AC-HUD-04).
- `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`: the card time the exile case also depends on.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/TallyNotBeforePlateTest.cpp` (CQTest, `StealthGame.HUD.TallyNotBeforePlate`; name per AC-HUD-03)
- `Source/StealthTestHarness/Private/HUD/InformantNpcParityTest.cpp` (CQTest, `StealthGame.HUD.PIE.InformantNpcParity.*`)
- `Source/StealthTestHarness/Private/HUD/HudCarrierInjection.h` and `.cpp` (non-Shipping owner-carrier injection hook; may already exist from an earlier HUD PIE story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-03 `TallyNotBeforePlate`
  - Given: an injected tally re-push, own State = Informant and `bExiled`, arriving 0, 100 and 200 ms before the `FStrikeResult`, plus a control where they arrive with it.
  - When: the HUD runs.
  - Then: every tick before `t_show` equals the control (tally, squares, triangle, sweep, strike hint, prompts). The tally changes on the `t_show` tick. The card enters at `t_show + ExileBeatSec` +/-1 tick.
  - Edge cases: needs a carrier-injection hook.
- **AC-2**: AC-HUD-02 `InformantNpcParity`
  - Given: an attacker with tally 0 kills an enemy informant (run 1) and an NPC (run 2).
  - When: dumps and cue events are compared from the press to `t_show` + 5 s.
  - Then: they are equal on every frame, including "Wrong target", tally 1, square fill and tally tick.
  - Edge cases: same seeds, TAA off.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/TallyNotBeforePlateTest.cpp` (`StealthGame.HUD.TallyNotBeforePlate`) and `Source/StealthTestHarness/Private/HUD/InformantNpcParityTest.cpp` (`StealthGame.HUD.PIE.InformantNpcParity`) — must exist and pass (gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`, `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`; `production/epics/npc-kill-penalty-informants/story-003-tally-rule-and-strike-result-supply.md`
- Unlocks: None within this epic (see the EPIC Definition of Done)
