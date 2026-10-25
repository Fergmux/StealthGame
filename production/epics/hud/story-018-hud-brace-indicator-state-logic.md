# Story 018: Brace indicator states and the F5 cooldown numeral and sweep

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

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§5 client presentation: the brace End is presented at `ContactServerTime`, never on receipt), with ADR-0021 (§4 presenter core, F5)
**ADR Decision Summary**: Anything a client can observe about a strike aimed at it must appear at the moment of contact, so the defender's HUD changes nothing before contact. The brace indicator's End on a reversal is presented when `ServerNowEst` reaches the consumed brace's `FHoldStatus.ServerEndTime` (= `ContactServerTime`), never on receipt. The presenter core owns the F5 cooldown numeral and sweep and reads `PresentedOwnState`, sampled each frame by value.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. ADR-0018's HIGH networking domain applies to the `FHoldStatus` reply and to the Counter write the End time derives from, delivered as owner data; MotionWarping (Beta in 5.8.4) plays the counter clip but is not read here. `ServerNowEst` is `EstimatedServerNow()` (ADR-0002) supplied as a number so tests use a fake clock. Use `UE_LOGF` for diagnostics.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; plate and card timelines are sampled from `now − t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never depend on a RepNotify having fired; never drive widgets event-style from RepNotify/RPC handlers — the host gets no RepNotify for its own writes — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

The brace indicator is the HUD's one element that reflects a state the player's own button press set up, and that a server decision can end at any time, including by a strike aimed at the viewer. Its logic therefore has to be exact about three things: it shows Protected only after the server confirmed (`FHoldStatus`), it shows End at the server's end time rather than on the packet that announced it (otherwise a cancelled hold would reveal an incoming strike before contact, ASN-R6), and it counts the cooldown from the server activation time with a rule that never shows Ready before the server would accept the press (ruling D5). This story builds that state machine and the F5 arithmetic in the plain C++ core; the widget is story 019.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC-HUD-26, AC-HUD-14 first half, HUD-R12, HUD-R6a), scoped to this story:*

- [ ] **AC1** (AC-HUD-26, `StealthGame.HUD.BraceStates`): **GIVEN** the brace states **THEN** Ready shows the empty outline, "Brace" and the bound glyph (the glyph swaps on device change without re-animating); Protected appears only after `FHoldStatus` confirmation, with segments 1-4 at `DefenceDurationSec`/4 steps and no sweep drawn; End is identical for release, cap, cancel and reversal and is presented when `ServerNowEst` reaches `ServerEndTime`, not on receipt.
- [ ] **AC2** (AC-HUD-14 first half, F5, `StealthGame.HUD.CooldownAndCountdown` cooldown cases): **GIVEN** activation 100.0, now 117.2 **THEN** the numeral is "Brace in 0:43" and the sweep 0.287 (± 0.001); `DefenceCooldownSec` 120 at now 100.0 gives "Brace in 2:00"; cooldown 60 with own RTT 0.1: at now 160.02 "Brace in 0:01", from 160.05 Ready.
- [ ] **AC3** (derived, HUD-R12 and HUD-R6a, `StealthGame.HUD.BraceStates` and `StealthGame.HUD.CooldownAndCountdown`): **GIVEN** `PresentedOwnState = Informant` **THEN** the indicator is hidden; **GIVEN** a strike aimed at the viewer **THEN** the cooldown sweep and numeral keep running until the presented removal (HUD-R6a); the numeral text updates only when `ceil(r)` changes.

---

## Implementation Notes

*Derived from GDD HUD-R12, F5, HUD-R6a, ASN-R3/R6/R23 and ADR-0021 §4:*

**What to build.** `FHudBraceModel` in `Source/StealthPresentation/Private/Hud/`, plain C++, stepped from `FHudPresenterCore::Step` and writing a `FHudBraceView` sub-struct of `FHudViewModel` (state enum `Ready | Pending | Protected | Ending | Cooling | Hidden`, `ProtectedSegments` 0-4, `SweepFraction`, `Numeral` as `FText` from `ST_Hud` keys `Hud.Brace.Label` and `Hud.Brace.Cooling`, `GlyphId`, `bUnbound`). Inputs by value in `FHudInputs`: the `FHoldStatus` reply from the owner reply sink (story 003 samples; IIF story 004 owns the sink), `LastPrepareActivation` and the cooldown end from the defence owner component (assassination story 016), the press event from the own command history (`ClientCommandSeq`), `DefenceDurationSec` and `DefenceCooldownSec` from the public tuning snapshot, own RTT from the net connection (host RTT = 0), the bound glyph id and device from the local input-binding query, and `PresentedOwnState` from story 014.

**States (HUD-R12, UX E01).** Ready: empty outline, label "Brace", bound glyph. On press: a local Pending emphasis only (the outline thickens one step), within one frame, no protection shown, nothing fills. Protected: only on `FHoldStatus` confirmation; inner stepped square at once; the outer ring fills in 4 stepped segments of `DefenceDurationSec`/4: `seg = min(4, floor(4 · (ServerNowEst − t_confirm) / DefenceDurationSec) + 1)`; never shrinks, narrows, closes or flashes; no sweep drawn. Cases `DefenceDurationSec` 3 and 5 are in the test. End: identical for release, cap, cancel and reversal; the square is removed with a 250 ms stepped exit (the widget's timeline, story 016's spec); End is presented when `ServerNowEst` reaches `FHoldStatus.ServerEndTime` (on a reversal, `ServerEndTime` = the consumed contact time, ASN-R23), never on receipt. If the End message arrives after `ServerEndTime`, End is presented on receipt. Because the four causes produce one view, the End dump is byte-identical across them: the cause must not be an input of the view model. A `FHoldStatus` cancel of a hold the victim was in is held by the story 014 `FHeldOwnerData` and released at the `PresentedOwnState` flip, so it never announces an outcome early. Cooling: from `LastPrepareActivation + DefenceCooldownSec`, linear sweep and "Brace in m:ss".

**Formula (F5).** `T_end = LastPrepareActivation + DefenceCooldownSec`; `r = max(0, T_end − ServerNowEst)`; numeral `"Brace in " + floor(ceil(r) / 60) + ":" + pad2(ceil(r) mod 60)` while `r > 0`; held at "Brace in 0:01" while `r = 0` and `ServerNowEst < T_end + 0.5 · RTT_own`, then Ready (ruling D5: never show Ready before the server would accept the press; no plate for an early press beyond IIF's normal rejection). `sweep = clamp((ServerNowEst − LastPrepareActivation) / DefenceCooldownSec, 0, 1)`, drawn only in Cooling, linear 0-1. Worked values: activation 100.0 and now 117.2 give r = 42.8, "Brace in 0:43", sweep 0.287; a brace released at 101.5 still counts from 100.0 (stamped at activation, ASN-R3). The 160.05 boundary in the RTT case is inclusive (Ready from 160.05): with T_end = 160.0 and RTT 0.1, 160.05 is exactly `T_end + 0.5 · RTT`; use the exact doubles the test builds and compare `<`, not a tolerance. The numeral text is rewritten only when `ceil(r)` changes (the test counts text writes in the view; `r` exactly on an integer must not flicker). Reduce motion makes the sweep advance in 1 s steps: the quantiser is story 016's, applied by the widget layer in story 019.

**Hidden and the pre-removal rule.** While `PresentedOwnState = Informant` the indicator is `Hidden` (collapsed, slot stays empty). Until that flip the model runs exactly as in `ActiveAlive`: for a strike aimed at the viewer the sweep and numeral keep moving and Protected keeps its segment count, so there is no HUD change of any kind before the contact frame (AC-HUD-05, ASN-R6). Hold-to-toggle changes nothing: the indicator is identical either way.

**Decisions that apply.** D3 (the "Brace" label is persistent in every state; Onboarding in Alpha may hide it later), D5 (the Ready-again rule above), D1 (the brace indicator stays until the card on an own fall, so do not hide it at `OnRoleChangePresented`).

**The test oracles.** `BraceStates`: scripted timeline with Ready, a press (Pending), `FHoldStatus` at `t_confirm`, then End by release, cap, cancel and reversal; asserts the dump at each tick; edge: End received after `ServerEndTime`; reversal ends at the contact time. `CooldownAndCountdown` is a test shared with story 033, which adds the F7 returning-countdown cases; this story creates the test class with the cooldown cases only and leaves a clearly named empty section for 033.

**Out of this story:** the widget, glyph swap rendering, the "Unbound" plate text and the ready-again tone (story 019, cue stream story 051), the returning countdown (story 033), `FHoldStatus` production (IIF and assassination epics).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-019-hud-brace-indicator-widget.md`: the widget, glyph rendering, "Unbound" and screenshots.
- `production/epics/hud/story-033-hud-returning-countdown-f7.md`: the F7 half of `StealthGame.HUD.CooldownAndCountdown`.
- `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`: the owner-only ready-again cue.
- `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`: `PresentedOwnState` and the held-data mechanism this story reads.
- `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`: the 1 s step quantiser and the 250 ms exit spec.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudBraceModel.h`
- `Source/StealthPresentation/Private/Hud/HudBraceModel.cpp`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (step the model; add `FHudBraceView` to the view model and `Dump()`)
- `Source/StealthPresentation/Private/Tests/HUD/BraceStatesTest.cpp` (class `FHudBraceStatesTest`, name `StealthGame.HUD.BraceStates`)
- `Source/StealthPresentation/Private/Tests/HUD/CooldownAndCountdownTest.cpp` (class `FHudCooldownAndCountdownTest`, name `StealthGame.HUD.CooldownAndCountdown`; cooldown cases here, countdown cases in story 033)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `BraceStates`
  - Given: Ready, a press (Pending), `FHoldStatus` at `t_confirm`, then End by release, cap, cancel and reversal.
  - When: `Step` runs.
  - Then: Ready is an empty outline plus "Brace" plus the bound glyph. Protected shows only after `FHoldStatus`, segment = min(4, floor(4(now - `t_confirm`)/`DefenceDurationSec`)+1), with no sweep. End dumps are identical for the four causes, and End is presented when `ServerNowEst` >= `ServerEndTime`, not on receipt.
  - Edge cases: `DefenceDurationSec` 3 and 5. End received after `ServerEndTime` presents on receipt. Reversal uses the contact time as end.
- **AC-2**: `CooldownAndCountdown` (cooldown cases)
  - Given: activation 100.0, cooldown 60, now 117.2. Cooldown 120 at now 100.0. Cooldown 60 with RTT 0.1 at 160.02 and 160.05.
  - When: formatted.
  - Then: "Brace in 0:43" with sweep 0.287 (+/-0.001). "Brace in 2:00". "Brace in 0:01" at 160.02, Ready from 160.05.
  - Edge cases: the 160.05 boundary is inclusive. Use exact doubles. Host RTT = 0.
- **AC-3**: Informant hide and numeral updates
  - Given: `PresentedOwnState` Active with a strike aimed at the viewer, then Informant.
  - When: 10 s of frames run.
  - Then: sweep and numeral keep running until the presented removal, then the indicator is hidden. The numeral text changes only when `ceil(r)` changes (count text writes).
  - Edge cases: `r` exactly on an integer.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/BraceStatesTest.cpp` (`StealthGame.HUD.BraceStates`) and `Source/StealthPresentation/Private/Tests/HUD/CooldownAndCountdownTest.cpp` (`StealthGame.HUD.CooldownAndCountdown`, cooldown cases) — must exist and pass. Gate level BLOCKING.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`; cross-epic: input-interaction-framework `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md` (`FHoldStatus`), assassination-defence-reversal `production/epics/assassination-defence-reversal/story-016-defence-owner-cooldown-component.md`
- Unlocks: `production/epics/hud/story-019-hud-brace-indicator-widget.md`, `production/epics/hud/story-029-hud-leak-suite-no-warning-before-contact.md`
