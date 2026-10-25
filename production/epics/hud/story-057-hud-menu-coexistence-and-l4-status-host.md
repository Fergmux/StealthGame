# Story 057: In-session menu coexistence and the display-only L4 status host

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§1 L4 input rule, §7 Common UI menus only)
**ADR Decision Summary**: Every layer below ZOrder 100 is display-only and `HitTestInvisible`; L4 hosts only the display part of Session's status overlays (message, spinner, progress, countdown), and an overlay that needs buttons splits in two: the action part is a Session-owned `UStealthStatusActionModal` (a `UCommonActivatableWidget`) pushed onto the Menu root stack at ZOrder 100 through `UStealthMenuStack::PushMenu`. Menus open only through `PushMenu`, which ends outstanding gameplay Begins synchronously first; the world is never paused and the HUD is not dimmed.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Common UI routing requires `UCommonGameViewportClient` and the unified Enhanced Input / Common UI path of 5.8 (engine facts E6 to E8); `UCommonActivatableWidget` adds its `InputMapping` on activation, so only the persistent menu root carries `IMC_Menu` and pushed menus (the status modal included) carry none. UI-V2 (menu edge safety, one `End` per outstanding `Begin` before the router's next-tick key flush) is settled by `production/epics/input-interaction-framework/story-011-spike-v5-commonui-menu-end.md`; this story does not re-prove it. UMG is composited after the scene (E2), which is why no scene post-process dim can be applied to the HUD behind the menu. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Layers below ZOrder 100 are `HitTestInvisible`, non-focusable, with no input bindings, and never host a button or a `UCommonActivatableWidget`; a Session status overlay that needs buttons shows its text on L4 (display only) and pushes a Menu-root modal (`UStealthStatusActionModal`) through `UStealthMenuStack::PushMenu` — source: ADR-0021 (closeout 2026-10-25)
- Forbidden: Never let Common UI add or remove a gameplay `IMC_*` context; never give a pushed menu an `InputMapping`; never route the HUD input layer through Common UI — source: ADR-0021
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

Story 013 proves the layer order against the camera fade and defers the Session-overlay clause to this story; this story is the one place that verifies it, together with the menu coexistence rule (UXA-HUD-10).

---

## Acceptance Criteria

*From UX spec `design/ux/hud.md` (UXA-HUD-10) and ADR-0021 §1 and §7 (HUD-E20, D15), scoped to this story:*

- [ ] **AC1** (UXA-HUD-10, retained screenshot): **GIVEN** the in-session menu **WHEN** it is opened **THEN** every HUD element is left unchanged and no scrim, dim, blur or desaturation is added; the menu panel's bottom edge (at most 70 % of height) clears the bottom-left cluster by at least 76 px at every text scale.
- [ ] **AC2** (derived, ADR-0021 §1 and §7, HUD-E20 and D15): **GIVEN** a Session status overlay **WHEN** shown **THEN** L4 hosts only its display part (message, spinner, progress, countdown) as `HitTestInvisible`; an overlay that needs buttons pushes the Session-owned modal onto the menu stack at ZOrder 100 and the HUD owns neither its content nor when it is pushed.

---

## Implementation Notes

*Derived from ADR-0021 Decision §1 (L4 input rule, closeout 2026-10-25) and §7:*

**AC1: menu coexistence.** Opening an in-session menu uses `UStealthMenuStack::PushMenu` (menus epic and IIF story 092 own it), which ends outstanding gameplay Begins first and does not pause the world. The HUD takes no action when the menu opens: no scrim, dim, blur or desaturation is applied by the HUD, the menu root or a post-process volume; the HUD layers keep their ZOrders (10/20/30/40/50/60) beneath the menu root at 100, and the menu panel is laid out so its bottom edge sits at most at 70 % of the viewport height and at least 76 px above the top of the bottom-left cluster (cluster rect from story 007's resolver, which grows upward at larger text scales). The 76 px figure is this story's own number (the UX spec states "clears the bottom-left cluster"); if the menu epic wants a different value, record it as a deliberate change here. Test at text scales 100, 125, 150 and 200 %: at 200 % the cluster rises and the clearance must be measured, not assumed. Compare the HUD view-model dump before and after the push (identical) and pixel-check the HUD region of the capture for absence of dim/blur/desaturation.

**AC2: L4 display-only host.** `UStealthStatusOverlayLayer` (L4, ZOrder 60) hosts only the display part of Session's connection and loading overlays: message text, spinner, progress and countdown, all `HitTestInvisible`, not focusable, no input bindings, never a `UCommonActivatableWidget`. For a state that needs buttons (Cancel / OK on a connection-lost, travel-failed or loading-cancel state, PAT-CRD-3) Session pushes `UStealthStatusActionModal` through `PushMenu` at ZOrder 100; the modal shows the buttons and may repeat the headline text; Session owns its content and when it is pushed and popped, and the HUD never decides either (HUD-E20, D15). Gameplay input contexts are never touched by either part. The modal never opens during a live strike (the states are `Traveling`, `Disconnected`/`Reconnecting`, a failed travel and `Ended`), and an overlay that can appear with a live pawn (a latency warning) is display-only with no action part.

**Fixtures.** Use the Session status overlay fixtures of `production/epics/session-travel/story-083-status-overlays-display-only.md` (message, spinner, progress, countdown, and one needing buttons) rather than inventing overlays. The lint of the L4 rule (a widget under ZOrder 100 that binds a button/click handler, sets a non-`HitTestInvisible` visibility or derives from `UCommonActivatableWidget` fails the build) is added or confirmed by story 006; this story exercises it with the button fixture.

**Traps.** Do not let the HUD push the modal itself "for convenience" (Session owns when). Do not give the modal an `InputMapping` (the persistent menu root carries `IMC_Menu`; a push or pop must never remove the context another menu needs). Do not pause the world for the menu (no pause in multiplayer). The 70 % height bound applies to the panel, not the whole menu root.

**Decisions that apply.** D15 (L4 display-only, Session's modal at 100). Out of this story: the menu stack and root, Session's overlay content, the fade/layer-order proof (013), the mid-brace single End (IIF story 092).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`: the layer stack, ZOrders and the input-isolation lint.
- `production/epics/hud/story-013-hud-fade-layer-and-layer-order-test.md`: layer order against the fade (defers the Session-overlay clause to this story).
- `production/epics/session-travel/story-083-status-overlays-display-only.md`: Session's overlay content and fixtures.
- `production/epics/input-interaction-framework/story-092-common-ui-mid-brace-single-end.md`: the single `End` when a menu opens mid-brace.
- `production/epics/input-interaction-framework/story-011-spike-v5-commonui-menu-end.md`: UI-V2.
- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: the general layout matrix.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Widgets/StealthStatusOverlayLayer.cpp` (L4 display-only host, `HitTestInvisible`)
- `Source/StealthTestHarness/Private/HUD/MenuCoexistenceTest.cpp` (CQTest, `NETWORK_TEST_CLASS(MenuCoexistence, "StealthGame.HUD.PIE")`; derived name, the plan names none)
- Evidence: `production/qa/evidence/hud-menu-open-1080p.png`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: UXA-HUD-10 menu coexistence
  - Given: the in-session menu opened at text scales 100, 125, 150 and 200 %.
  - When: the HUD dump and a capture are compared before and after.
  - Then: every HUD element is unchanged, and there is no scrim, dim, blur or desaturation. The menu panel's bottom edge (at most 70 % of height) clears the bottom-left cluster by at least 76 px.
  - Edge cases: at 200 % text the cluster grows upward, so measure the clearance. The 76 px figure is the story's own number. `hud-menu-open-1080p.png` retained.
- **AC-2**: L4 display-only host
  - Given: a Session status overlay with message, spinner, progress and countdown, and one that needs buttons.
  - When: each is shown.
  - Then: the display part is `HitTestInvisible` on L4. The button overlay pushes the Session-owned modal at ZOrder 100, and the HUD owns neither its content nor its push time.
  - Edge cases: uses fixtures from session-travel story 083. This is the one place the Session-overlay clause is verified (013 defers to it).

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest PIE test `Source/StealthTestHarness/Private/HUD/MenuCoexistenceTest.cpp` plus the retained screenshot `production/qa/evidence/hud-menu-open-1080p.png`
- Gate: BLOCKING

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`, `production/epics/hud/story-013-hud-fade-layer-and-layer-order-test.md`, `production/epics/session-travel/story-083-status-overlays-display-only.md`, `production/epics/input-interaction-framework/story-092-common-ui-mid-brace-single-end.md`
- Unlocks: None within this epic (see the EPIC Definition of Done)
