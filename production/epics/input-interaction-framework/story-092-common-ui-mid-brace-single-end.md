# Story 092: Common UI mid-brace single End

> **Epic**: Input & Interaction Framework
> **Status**: Blocked — BLOCKED: spike V5 (story 011) has not reported
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-003`, `TR-iif-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

ADR-derived story (no numbered GDD acceptance criterion): the criterion is the ADR-0011 Validation Criteria "Common UI test".

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: ADR-0011 section 9 makes `UPawnCommandClient` own release safety: it tracks every outstanding Begin, sends End synchronously for each on a Common UI switch to Menu mode (via `UCommonUIActionRouterBase::OnActiveInputModeChanged`) and makes Ends idempotent, so a later engine `Completed` or `Canceled` for that press is swallowed. ADR-0021 section 7 routes every menu through `UStealthMenuStack::PushMenu`, which first calls `EndAllOutstanding(EEndReason::MenuOpen)` synchronously.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Conditioned by verification item **V5** (ADR-0011; mirrored as ADR-0021 UI-V2): switching to Menu input mode while a gameplay hold key is down yields the builder's End synchronously, before CommonUI's next-tick `FlushPressedKeys` (engine fact E11: Common UI flushes pressed keys on the next tick after the mode becomes Menu, and in Menu mode the game viewport may never see the key release, so Enhanced Input is not guaranteed to deliver `Completed`). Router delegate `UCommonUIActionRouterBase::OnActiveInputModeChanged` is [V] (ADR-0021 E7). V5 has not reported.

**Control Manifest Rules (this layer)**:
- Required: Ends are idempotent per Begin; a Common UI switch to `Menu` produces the Ends synchronously; `UPawnCommandClient` sends End for every outstanding Begin before any context swap, menu open, focus loss, possession change or `ClientRestart`; context changes set `bIgnoreAllPressedKeysUntilRelease` — source: ADR-0011
- Forbidden: Never use Hold/HoldAndRelease/Tap/Pulse/Chorded triggers for server-meaningful actions; never make a gameplay command a Common UI action — source: ADR-0011
- Required: Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` to Begin and both `Completed` and `Canceled` to End — source: ADR-0011

---

## Acceptance Criteria

*From ADR-0011 Validation Criteria ("Common UI test"), phrased as one testable criterion:*

- [ ] **GIVEN** an owner holding Prepare (a brace active) with `UPawnCommandClient` subscribed to the router's input-mode change **WHEN** a menu opens (input mode changes to Menu) **THEN** exactly one End is sent, in the same call stack as the mode change and before the next tick; **AND** no second End is sent when the engine later reports a release (`Completed` or `Canceled`) for the held key. *(Source: ADR-0011 Validation Criteria, "Common UI test".)*

---

## Implementation Notes

*Derived from ADR-0011 Decision section 9 ("The builder owns release safety", "Common UI (5.8 unification, E5)") and ADR-0021 Decision section 7 ("Common UI: menus only"):*

**What this story builds.** A PIE test of the synchronous menu-open End path, using the real Common UI router. `UPawnCommandClient` binds `OnActiveInputModeChanged` as the second line (the first line is `UStealthMenuStack::PushMenu` calling `EndAllOutstanding(EEndReason::MenuOpen)`), and sends every outstanding End on a switch to Menu without waiting for Enhanced Input, because Common UI flushes pressed keys on the next tick and the release may never reach the game viewport. Ends are idempotent: once the builder has sent an End for a Begin, any later engine `Completed` or `Canceled` for that press is swallowed. The server-side effect is a clean brace end via the End; a missed End would extend the hold to the 4.0 s cap (GDD R4: a lost End extends a hold).

**Relationship to other stories.** Story 013 (AC 5 `FlushSendsEnd`, case b: a menu opens) proves the general flush behaviour including that `Move` and `Look` are zero while the menu is open; this story is the explicit V5-conditioned Common UI test from the ADR validation list (exactly one End before the next tick, none after). Story 011 is the V5 spike that produces the report; this story depends on its result being PASS.

**Test notes (qa-lead).** Proposed name `StealthGame.Input.PIE.CommonUiMidBraceSingleEnd`. Edge cases: the key is physically released after the menu closes (still no second End); a Begin outstanding twice cannot occur (the builder tracks one outstanding Begin per press). The End count is read from the story 095 server command log or the outgoing-RPC spy; the same-call-stack check needs the RPC-send hook of story 095.

**Why Blocked.** The unblocking condition is the V5 report (story 011). If V5 shows the End is not synchronous, ADR-0011 section 9 and ADR-0021 section 7 need amendment first.

**Prototype.** Supersedes the prototype `StealthPlayerController` input mode handling (input-mode switching with no End bookkeeping); final removal is story 093.

**Tuning keys (defaults).** None. The Common UI settings (`bEnableEnhancedInputSupport`, `CommonUI.AutoFlushPressedKeys` default true) are engine settings, not tuning.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-013-flush-sends-end.md`: the general flush cases (focus loss, menu, context swap, unpossession) and zero Move/Look in menu.
- `production/epics/input-interaction-framework/story-011-spike-v5-commonui-menu-end.md`: the V5 report.
- `production/epics/input-interaction-framework/story-012-edges-not-repeat.md`: edges, not auto-repeat.
- `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md`: context swap on state and possession.
- The menu stack and menu root themselves: cross-epic hud / session-travel (ADR-0021 section 7).

---

## Files

- `Source/StealthGame/` (root module) — `UPawnCommandClient` router subscription and idempotent End bookkeeping (planned; the shell is story 094)
- `Source/StealthTestHarness/Private/CommonUiMidBraceSingleEndTest.cpp` — CQTest `NETWORK_TEST_CLASS(CommonUiMidBraceSingleEnd, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.CommonUiMidBraceSingleEnd.<Method>` (proposed name; planned)

---

## QA Test Cases

**Automated test specs:**

- **AC-1**: Opening a menu mid-brace sends exactly one End before the next tick and no second End later (proposed name `StealthGame.Input.PIE.CommonUiMidBraceSingleEnd`).
  - Given: the owner holding Prepare (brace active), `UPawnCommandClient` subscribed to the router mode change.
  - When: a menu opens (input mode changes to Menu).
  - Then: exactly one End is logged, in the same call stack as the mode change (before the next tick).
  - Given: the engine later reports `Completed` or `Canceled` for the held key after CommonUI's flush.
  - Then: no second End is sent.
  - Edge cases: key physically released after the menu closes; Begin outstanding twice cannot occur.

*QL-STORY-READY: ADEQUATE (qa-lead); also Blocked on V5 per the plan.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CommonUiMidBraceSingleEndTest.cpp` (CQTest `StealthGame.Input.PIE.CommonUiMidBraceSingleEnd`) must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-013-flush-sends-end.md`, `production/epics/input-interaction-framework/story-011-spike-v5-commonui-menu-end.md`; cross-epic: hud (stories not yet written; `UStealthMenuStack`, menu root, `IMC_Menu`)
- Unlocks: None
