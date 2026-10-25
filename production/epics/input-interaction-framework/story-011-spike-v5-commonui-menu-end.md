# Story 011: Spike V5 reports Common UI Menu-mode switch yields a synchronous End

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-003`, `TR-iif-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: `UPawnCommandClient` subscribes to the Common UI action router's input-mode change and sends every outstanding End synchronously on a change to `Menu`, never waiting for Enhanced Input, because Common UI flushes pressed keys on the next tick and the key release may never reach the game viewport. ADR-0021 owns the menu side: every menu is pushed through one menu stack that first ends outstanding gameplay holds synchronously.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Highest of ADR-0011 (HIGH) and ADR-0021 (HIGH). This story IS verification item V5 of ADR-0011 (and UI-V2 of ADR-0021): "Common UI under the 5.8 unification: switching to `Menu` input mode while a gameplay hold key is down produces the builder's End synchronously, before CommonUI's next-tick `FlushPressedKeys` (E11; ADR-0021 owns the menu side)." Engine facts: E11 Common UI's action router calls `FlushPressedKeys` on the next tick after the input mode becomes `Menu` (`CommonUI.AutoFlushPressedKeys`, default true), and in `Menu` mode the game viewport may never see the key release, so `Completed` is not guaranteed; ADR-0021 E7 confirms `UCommonUIActionRouterBase::OnActiveInputModeChanged()` (event with `ECommonInputMode`) and `GetActiveInputMode()` exist (`CommonUIActionRouterBase.h:92-94`, `.cpp:39-42`); E8 Common UI routing requires `UCommonGameViewportClient`.

**Control Manifest Rules (this layer)**:
- Required: Ends are idempotent per Begin; a Common UI switch to `Menu` produces the Ends synchronously; `UPawnCommandClient` sends End for every outstanding Begin before any context swap, menu open, focus loss, possession change or `ClientRestart` — source: ADR-0011
- Forbidden: Never use `UPlayerMappableInputConfig` or the Raw Input plugin; never make a gameplay command a Common UI action — source: ADR-0011
- Guardrail: Engine API constraint (5.8.4): Common UI flushes pressed keys on the next tick after `Menu` — source: ADR-0011, ADR-0021
- Guardrail: Gameplay `IMC_*` contexts are added and removed only by `UPawnCommandClient`; Common UI never adds or removes them — source: ADR-0021

This is a verification spike. The menu path is the most likely place for a hold to outlive the player's intent: the player holds Prepare, opens a menu, the viewport stops seeing input, the key is released while the menu has focus, and no `Completed` ever arrives. Without a synchronous End the server would keep the brace until the cap expires, extending a hold the owner meant to end (and the reliable End is the only way the server learns). V5 measures whether the router's mode-change callback can drive the builder's End strictly before the next tick's `FlushPressedKeys`. Stories 013 (FlushSendsEnd) and 092 (Common UI mid-brace single End) are Blocked ("spike V5 (story 011) has not reported") until this reports.

It depends on story 007 (the contexts under test). The production `UPawnCommandClient` and menu stack are delivered by stories 094, 013 and the HUD epic; this spike uses a minimal throwaway probe mirroring the ADR §9 subscription so it can run before them and states that in the report.

---

## Acceptance Criteria

*ADR-derived spike. Source: ADR-0011 Engine Compatibility, Verification Required item V5 (and ADR-0021 UI-V2); criterion text from the plan:*

- [ ] **GIVEN** a held gameplay key (Prepare) and Common UI enabled with Enhanced Input support **WHEN** a menu is pushed so the input mode switches to `Menu`, by keyboard and by gamepad, over 20 runs **THEN** a report at `production/qa/spikes/` records the frame numbers of the mode-change callback, the builder's End and Common UI's `FlushPressedKeys`, whether the key release ever reaches the game viewport and the End count over the sequence, showing the End issued in the same frame as the callback and strictly before the next tick's `FlushPressedKeys` with exactly one End in total (no second End after the engine's later `Completed` or `Canceled`), and states PASS or FAIL naming any divergent frame ordering.

---

## Implementation Notes

*Derived from ADR-0011 Decision §9 (Common UI bullet), Engine Compatibility V5; ADR-0021 Decision §7:*

"`UPawnCommandClient` subscribes to the Common UI action router's input-mode change (`UCommonUIActionRouterBase::OnActiveInputModeChanged` [V], confirmed by ADR-0021 E7) and sends every outstanding End **synchronously** on a change to `Menu`, never waiting for Enhanced Input, because Common UI flushes pressed keys on the next tick and the release may never reach the game viewport (E11, V5); gameplay `IMC_*` contexts are added and removed only by `UPawnCommandClient`." ADR-0021 §7 adds the first line of defence: "Every menu is pushed through `UStealthMenuStack::PushMenu`, which first calls `UPawnCommandClient::EndAllOutstanding(EEndReason::MenuOpen)` synchronously, then pushes the activatable widget (whose `GetDesiredInputConfig()` is `Menu`). The router's `OnActiveInputModeChanged` (E7) is also bound by `UPawnCommandClient` as the second line, so a menu opened by any other path still ends holds before the router's next-tick key flush." V5 tests the second line (the router callback), since the first line is deterministic code. "No pause in multiplayer": opening a menu never pauses the world.

**Spike procedure (from the qa-lead spec):** record the Common UI plugin state (enabled in `StealthGame.uproject` since 2026-10-25, `bEnableEnhancedInputSupport = true`, viewport client `UCommonGameViewportClient`), the 5.8.4 header and exact name of `UCommonUIActionRouterBase::OnActiveInputModeChanged` (confirm it exists and its signature). For a held gameplay key (Prepare) when a menu is pushed, record: the frame number of the mode-change callback, the frame number of the builder's End, the frame number of Common UI's `FlushPressedKeys`, whether the key release ever reaches the game viewport, and the End count over the whole sequence. Pass rule: End issued in the same frame as the callback and strictly before the next tick's `FlushPressedKeys`; exactly one End in total (no second after the engine's later `Completed`/`Canceled`), across 20 runs. A fail names the divergent frame ordering. Edge cases: menu pushed by keyboard versus gamepad; menu opened the same frame as the press (Begin and End in one frame, FIFO order). Read `production/never-resave-assets.md` before any Content step; the spike creates no Content that modifies the sample's assets.

Prototype: new code, no prototype counterpart. Tuning keys: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-input-assets-and-trigger-lint.md`: authoring the contexts (prerequisite).
- `story-008-spike-v1-enhanced-input-edges.md`: V1, edge behaviour on context swap and focus loss.
- `story-013-flush-sends-end.md`: production flush on focus loss, menu, swap and unpossess (AC 5), Blocked on this spike.
- `story-092-common-ui-mid-brace-single-end.md`: the production Common UI mid-brace test (ADR Validation Criteria), Blocked on this spike.
- HUD epic (cross-epic, stories not yet written): the menu stack `UStealthMenuStack` and `IMC_Menu` (ADR-0021 §7).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `production/qa/spikes/V5-commonui-menu-end-report.md` (the report; final dated name chosen at write time)
- `Source/StealthTestHarness/Private/SpikeV5CommonUiMenuEndTest.cpp` (throwaway probe harness; not a gate test)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Integration — spike specification** (QL-STORY-READY: ADEQUATE):

- **AC-1**: Spike V5 reports Common UI Menu-mode switch yields a synchronous End
  - Given: a held gameplay key (Prepare); the report records the Common UI plugin state and the 5.8.4 header and exact name of `UCommonUIActionRouterBase::OnActiveInputModeChanged` (confirmed to exist, with signature).
  - When: a menu is pushed over 20 runs.
  - Then: records the frame number of the mode-change callback, the frame number of the builder's End, the frame number of Common UI's `FlushPressedKeys`, whether the key release ever reaches the game viewport, and the End count over the whole sequence. Pass: End issued in the same frame as the callback and strictly before the next tick's `FlushPressedKeys`; exactly 1 End total (no second after the engine's later Completed/Canceled), across 20 runs. A fail names the divergent frame ordering; stories 013 and 092 stay Blocked.
  - Edge cases: menu pushed by keyboard vs gamepad; menu opened the same frame as the press (Begin and End in one frame, FIFO).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the spike report `production/qa/spikes/V5-commonui-menu-end-report.md` (a documented result, retained) OR the probe harness test `Source/StealthTestHarness/Private/SpikeV5CommonUiMenuEndTest.cpp`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`; cross-epic: tuning-data-config (stories not yet written; StealthTestHarness module); cross-epic: HUD / UI epic (stories not yet written; menu root and `UCommonGameViewportClient` configuration, ADR-0021 §7)
- Unlocks: `production/epics/input-interaction-framework/story-013-flush-sends-end.md`, `production/epics/input-interaction-framework/story-092-common-ui-mid-brace-single-end.md`
