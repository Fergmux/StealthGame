# Spike V5: Common UI Menu-mode switch yields a synchronous End (ADR-0011 V5, ADR-0021 UI-V2)

> **Story:** `production/epics/input-interaction-framework/story-011-spike-v5-commonui-menu-end.md` · **Date:** 2026-10-26 · **Verdict:** **PASS** (80/80 runs: 40 held-then-menu runs give the End in the callback frame, strictly before the router's next-tick `FlushPressedKeys`, one End in total; 40 same-frame runs start no hold under the menu) after one design change found by the first run (the builder's Menu-mode gate)

## Setup

| Item | Value |
|---|---|
| Engine build | 5.8.4-58902052+++UE5+Release-5.8 |
| Common UI | plugin enabled (`StealthGame.uproject`), `bEnableEnhancedInputSupport=True` (`DefaultGame.ini`), `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient` (`DefaultEngine.ini`) |
| Callback | `UCommonUIActionRouterBase::OnActiveInputModeChanged()`: `DECLARE_EVENT_OneParam(UCommonUIActionRouterBase, FOnActiveInputModeChanged, ECommonInputMode)`, `CommonUIActionRouterBase.h:92-93`, broadcast from `ApplyUIInputConfig` (`CommonUIActionRouterBase.cpp:2012`) in the same call that schedules `FlushPressedKeys` with `SetTimerForNextTick` (`:1901-1905`, `CommonUI.AutoFlushPressedKeys` default on) |
| Builder | the production `UPawnCommandClient` on `AStealthPlayerController` (story iif-094): subscribes to the router at BeginPlay and calls `EndAllOutstanding(MenuOpen)` synchronously on a change to `Menu` |
| Menu | a minimal `UCommonActivatableWidget` whose `GetDesiredInputConfig()` is `Menu` (`Source/StealthTestHarness/Private/Spikes/Input/StealthSpikeMenuWidget.h`), added to the viewport and activated; in the headless PIE the activation alone did not switch the router (`widgetSwitchedRouter: false` in every run), so the spike then applies the same `Menu` config through `UCommonUIActionRouterBase::SetActiveUIInputConfig`, the router's own path that a widget activation ends in |
| Keys | Prepare: keyboard `Q` and `Gamepad_LeftTrigger`; 20 runs per key and per variant (80 runs) |
| Topology | standalone PIE on `L_Prototype` with `AStealthGameMode`, `-nullrhi` |
| Harness | `Source/StealthTestHarness/Private/Tests/Spikes/SpikeV5CommonUiMenuEndTest.cpp`, `StealthSpike.Input.V5CommonUiMenuEnd` (throwaway) |
| Raw data | `production/qa/evidence/story-011-spike-v5-commonui-menu-end/v5-results.json`, `run-log-excerpt.txt` |

Frames come from `GFrameCounter`; `FlushPressedKeys` frames from `AStealthPlayerController::FlushPressedKeys` (a
development-only trace that then calls the engine's).

## Results

| Variant | Key | Runs | Begin | Callback frame | End frame | FlushPressedKeys | Ends | Later release | Verdict |
|---|---|---|---|---|---|---|---|---|---|
| Held, then menu (4 frames later) | Q | 20 | press + 1 | M | **M** | M + 1 | 1 | swallowed (1 per run) | PASS 20/20 |
| Held, then menu | LT | 20 | press + 1 | M | **M** | M + 1 | 1 | swallowed | PASS 20/20 |
| Press and menu in the same frame | Q | 20 | **none** | M | none | M + 1 | 0 | swallowed | PASS 20/20 |
| Press and menu in the same frame | LT | 20 | none | M | none | M + 1 | 0 | swallowed | PASS 20/20 |

(M = the frame the menu was pushed.) The key release never produced a second End: the builder swallowed the
engine's later terminal edge every time.

## Findings

1. **The End is synchronous** (same frame as the router callback) and strictly before the router's next-tick
   `FlushPressedKeys`, for keyboard and gamepad alike: the second line of ADR-0021 section 7 works as designed.
2. **First-run FAIL, same-frame variant (fixed):** before the gate, a press simulated in the push frame was processed
   by Enhanced Input in the next frame, after the callback; the builder then sent a Begin **under the menu** (frame
   M + 1), and that hold ended only through the router's flush (End at M + 2, after `FlushPressedKeys` at M + 1). One
   End in total, but not synchronous and a hold started while a menu was up. Fix: `UPawnCommandClient` tracks the
   router mode and starts no Begin and no instant command while it is `Menu` (`bMenuMode`); the press then needs a
   fresh press after the menu closes, which is ADR-0011's rule after any cancellation.
3. **Activating a bare activatable widget did not switch the router in a headless PIE.** The menu stack of ADR-0021
   (`UStealthMenuStack::PushMenu`) calls `EndAllOutstanding(MenuOpen)` itself before the push (the first line), so this
   does not weaken the design; the real menu widget's activation path is to be re-checked with the first real menu
   (Presentation epic) in a windowed run.

## Consequences

- Stories iif-013 (flush on menu) and iif-092 (Common UI mid-brace single End) can rely on: callback-frame End,
  before the flush, idempotent; and no Begin while the router is in Menu mode.
- ADR-0011 section 9 note for the next sync: "the builder starts no command while the router's input mode is Menu".
