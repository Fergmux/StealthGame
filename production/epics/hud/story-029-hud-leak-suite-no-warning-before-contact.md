# Story 029: Leak suite: no HUD warning before the viewer's own contact frame

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-002`, `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (with ADR-0018 for the contact-time presentation rule)
**ADR Decision Summary**: ADR-0008 makes the leak suite an automated multi-client PIE comparison against control runs. ADR-0018 keeps the outcome blind until contact: the victim's own `FActionState` write carries the contact time, the defender result is held until the Counter write is observed, and nothing outcome-caused (own `State`, `FHoldStatus` cancel, `NotNow` replies, tally re-push) is presented before the victim's own fall or counter is visible. ADR-0021 §8 gates the Informant input context with `PresentedOwnState` for the same reason.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0018 notes MotionWarping is Beta in 5.8.4 and the HIGH networking domain. The contact oracle is the `Death_Fall` anim-notify tick on the victim's client in cases a-c and the `Counter` notify tick in case d, never the HUD's F2 mapping. This test must run with the HUD presenter bound (ADR-0021 §8: without a bound presenter the Informant-context gate reduces to raw state and the leak would be hidden). Runs at a fixed 60 Hz step with TAA off; the multi-process harness comes from `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

The server resolves a strike at the slot, up to `StrikeContactSec` (+ `BushStrikeEmergeSec`) before the visible blow. Everything the victim's client learns in that window must stay invisible until the victim's own contact frame, or the HUD becomes a pre-contact warning (ASN-R6, HUD-R6, HUD-R6a). This story proves it for the four victim situations in AC-HUD-05.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-05 (verbatim, cases a-d) and UX spec UXA-HUD-32 (last clause):*

- [ ] **AC1** (AC-HUD-05, `StealthGame.HUD.PIE.NoWarningBeforeContact`): **GIVEN** a target that is, in separate runs, (a) unbraced and idle with a running cooldown sweep, (b) unbraced and mid-tend at a court, (c) unbraced with the strike hint showing on an NPC, (d) braced (reversal), each with a control run without the attacker, **WHEN** an enemy strikes it, **THEN** every tick of the target's dump before its contact tick (the `Death_Fall` notify in a–c, the `Counter` notify in d) equals the control run; in a–c the strike hint, prompt and ring hide on the contact tick and the card enters 0.8 s (± 1 tick) later; in d the "Strike reversed" plate, the brace End and a `Busy` reply to a command pressed 0.1 s after the attacker's execution all appear at or after the contact tick, never before.
- [ ] **AC2** (UXA-HUD-32 last clause, restated oracle): **GIVEN** a strike aimed at the viewer **WHEN** the dumps are compared to the control run **THEN** no HUD element changes before the viewer's own contact frame (the oracle is the `Counter` or `Death_Fall` notify tick on the viewer's client).

---

## Implementation Notes

*Derived from ADR-0018, ADR-0021 §4 and §8, GDD HUD-R6, HUD-R6a, HUD-R9 and the determinism rule:*

**What to build.** One CQTest PIE scenario file in `StealthTestHarness`, `NETWORK_TEST_CLASS(NoWarningBeforeContact, "StealthGame.HUD.PIE")`, with one method per case a-d and one for the restated UXA-HUD-32 clause. No production presenter code is written here; fix any leak in the story that owns the leaking element and re-run.

**Determinism rule.** Fixed 60 Hz, identical seeds, dumps keyed by tick index, wall-clock fields excluded, TAA off. Each case has a control run with the same seed and no attacker; "unchanged" means equal to the control, so a running cooldown sweep or tend ring is compared, not expected to freeze. Contact oracles come from the anim-notify tick on the target's client.

**Cases.**
- (a) unbraced idle target with a running brace cooldown: the sweep and numeral must keep moving exactly as in the control until contact.
- (b) unbraced target mid-tend at a court: the tend ring keeps its prediction (an `FHoldStatus` cancel caused by the own removal is held to `OnRoleChangePresented`, HUD-R6a), prompts and ring unchanged until contact.
- (c) unbraced target with the strike hint showing on an NPC: the hint stays up until contact. In a-c the hint, prompt and ring hide on the contact tick (ruling D1) and the card enters 0.8 s (`HudRemovalCardDelaySec`) later, ± 1 tick.
- (d) braced target (reversal): the "Strike reversed" plate, the brace End and a `Busy` reply to a command pressed 0.1 s after the attacker's execution all appear at or after the contact tick (`Counter` notify), never before. The brace End is presented when `ServerNowEst` reaches the consumed brace's `FHoldStatus.ServerEndTime` (= `ContactServerTime`), never on receipt; the defender plate is held Pending until the defender's own Counter `FActionState` write is observed (story 022).

**What must not leak.** The raw own `State`, `bExiled`, the tally re-push, `NotNow` replies and the `IMC_Active` to `IMC_Informant` swap all arrive up to `StrikeContactSec` before contact. `UseInformantContext` (ADR-0021 §8) must keep the client on `IMC_Active` until `PresentedRole == Informant` or the cap, and the test must run with the presenter bound so this gate is in force. A cue event or latency icon change that precedes the contact tick is also a leak.

**Traps.** The `Busy` reply case must press the command 0.1 s after the attacker's execution so the server's reply lands inside the resolving drain; the HUD must present it no earlier than the target's contact time. Dumps of the target must be compared whole, including cue events and the notice slots, not only the named elements.

**Prototype.** The prototype `StealthHUD.cpp` and the message plumbing in `StealthPlayerController.*` show outcomes on receipt; none of it is promoted.

**Out of this story.** Strike-target and look parity (story 028), the pixel-diff strip (story 023), the removal card timing PIE (story 026).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md`: AC-HUD-01 and AC-HUD-04 parity runs.
- `production/epics/hud/story-023-hud-strike-strip-test-and-plate-evidence.md`: AC-HUD-07 strike strip capture diff.
- `production/epics/hud/story-018-hud-brace-indicator-state-logic.md`: the brace states and End timing under test here.
- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`: the defender plate held until the Counter write.
- `production/epics/hud/story-026-hud-removal-card-pie-timing.md`: card timing PIE proof.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/NoWarningBeforeContactTest.cpp` (CQTest, `StealthGame.HUD.PIE.NoWarningBeforeContact.*`)
- `Source/StealthTestHarness/Private/HUD/HudPieFixtures.h` and `.cpp` (shared 2v2 fixture, control-run and dump-by-tick comparer; created by an earlier HUD PIE story if present, otherwise here)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-HUD-05 `NoWarningBeforeContact`
  - Given: a target (a) unbraced idle with a cooldown sweep, (b) unbraced mid-tend at a court, (c) unbraced with the strike hint showing on an NPC, (d) braced, each with a control run without the attacker.
  - When: an enemy strikes it.
  - Then: every tick before the target's contact tick (`Death_Fall` notify in a-c, `Counter` in d) equals the control. In a-c the hint, prompt and ring hide on the contact tick and the card enters 0.8 s +/-1 tick later. In d the "Strike reversed" plate, brace End and the `Busy` reply (command pressed 0.1 s after execution) appear at or after the contact tick.
  - Edge cases: the contact oracle is the anim notify, not HUD mapping.
- **AC-2**: UXA-HUD-32 last clause
  - Given: a strike aimed at the viewer.
  - When: dumps are compared to the control.
  - Then: no HUD element changes before the viewer's own contact frame.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/NoWarningBeforeContactTest.cpp` (`StealthGame.HUD.PIE.NoWarningBeforeContact`) — must exist and pass (gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-018-hud-brace-indicator-state-logic.md`, `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md`; `production/epics/assassination-defence-reversal/story-017-no-warning-and-no-observer-cue.md`
- Unlocks: `production/epics/hud/story-061-hud-mvp-exit-5v5-presentation-verification.md`
