# ADR-0021: UI/HUD layering, CommonUI and Enhanced Input

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. It cannot be Accepted before ADR-0001 (carriers), ADR-0002 (`EstimatedServerNow`), ADR-0007 (Presentation module, bootstrap), ADR-0011 (input pipeline; with the context-gate amendment requested in §8), ADR-0015 (camera view state; with the screen-fade amendment requested in §3), ADR-0017 (`IsAllyBodyHidden`) and ADR-0020 (`FObjectiveTeamView`) are Accepted, the HUD and Teammate Markers GDDs have passed their pending re-reviews, and verification items **UI-V1** to **UI-V4** have reported.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED (§8 context gate stated as one pure function with the `HudOwnRemovalHoldCapSec` bound; both client interfaces made `UINTERFACE`s and `IOwnerCommandClientView` added to `FStealthClientViewHandles` as an ADR-0007 amendment; §3 fade fallback rewritten because `FadeAmount` only animates while the scene fade is on; `AddToViewport` banned; widgets apply values only on change, GPU line added; one persistent menu root holds `IMC_Menu`; menu ZOrder vs HUD-R4 wording raised as GDD sync). Engine validation (unreal-specialist lens): MINOR NOTES → applied (E3 now verified; projection from `ULocalPlayer::GetProjectionData`; leaf wrapper resets in `ReleaseSlateResources` and implements `ComputeDesiredSize`; DPI rule result is cached per viewport size outside the editor, so `s_ui` moves to a root scaler; `UCommonGameViewportClient` is required [V]; activatable stack push order stays [U] under UI-V2).

## Date
2026-10-24

## Last Verified
2026-10-24 (design against `design/gdd/hud.md` HUD-R1..R17 and F1-F10, `design/gdd/teammate-markers.md` TM-R1..R14 and VA-2, `design/gdd/camera-system.md` CS-R15, `design/gdd/input-interaction-framework.md` R13, as revised and reconciled 2026-10-24; engine claims re-read against the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full); technical-director (TD-ADR) and unreal-specialist validation; acceptance reserved for the user.

## Summary
The HUD and the teammate markers must show a player only what is theirs to know, in a fixed layer order (markers and world rings under the camera's black, plates and cards above it), with presentation timed by the server clock and never by packet arrival, and without letting UI input routing corrupt the gameplay command edges the IIF depends on. This ADR decides: UMG overlay layers added per local player at the HUD-R4 ZOrders, all hit-test-invisible and never activatable; one volatile Slate leaf in layer L0 that projects markers, the interact ring and the context prompt inside its paint pass from the rendered frame's view; a HUD-hosted screen-fade layer that Camera drives; plain-`USTRUCT` view-models built by a headless C++ presenter that samples its carriers every frame (identical on the listen host); CommonUI with Enhanced Input support for menus only, opened through one menu stack that ends outstanding gameplay holds synchronously first; and one custom DPI rule so 1080p reference pixels and the client UI scale are applied exactly once.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | UI (UMG, Slate, CommonUI) + Input (Enhanced Input / Common UI unification) |
| **Layer** | Presentation |
| **Knowledge Risk** | HIGH — architecture.md lists HUD and Teammate Markers under "UI and input", HIGH for 5.8 because Enhanced Input and Common UI were unified in 5.8 and the engine reference has no material on their composition with gameplay contexts |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/ui.md` (5.8 block: EI/Common UI unified, Slate renames), `modules/input.md` (5.8 flush CVars), `breaking-changes.md`, `deprecated-apis.md`, `PLUGINS.md` (CommonUI); `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Presentation layer, Data Flow §2-§3, Presentation contract, Performance budget, AQ-11, AQ-15); ADR-0001, ADR-0002, ADR-0007, ADR-0011 (engine facts E2, E3, E5, E11, E12; §9), ADR-0015 (§6, Key Interfaces), ADR-0017 (registry staging), ADR-0020; prototype `AStealthHUD::DrawTeammateMarkers` (evidence only) |
| **Post-Cutoff APIs Used** | Common UI / Enhanced Input unification (`UCommonInputSettings` `bEnableEnhancedInputSupport`; `UCommonActivatableWidget::InputMapping` applied on activation) [V]; `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` (5.8.4 name, per ADR-0011 E2) [V]; `UCommonUIActionRouterBase::OnActiveInputModeChanged` [V]. Everything else (UMG `AddToPlayerScreen`, Slate leaf widgets, `UDPICustomScalingRule`) predates the cutoff and is re-verified below |
| **Verification Required** | **UI-V1** (zero-lag projection): a marker painted by the L0 leaf stays on its ally's anchor with no frame of lag during fast camera turns and boom collisions at 60 and 144 Hz (TM ACs; spike on the graybox). **UI-V2** (menu edge safety, with ADR-0011 V5): opening a CommonUI menu while a gameplay hold key is down yields exactly one `End` per outstanding `Begin` before the router's next-tick key flush, and closing it requires a fresh press. **UI-V3** (layer order): AC-HUD-24 — the screen fade covers markers, ring and prompt and stays under plates, cards and menus. **UI-V4** (DPI): the custom scaling rule gives Slate DPI scale = `H/1080` at 720p-4320p and in PIE without scaling editor UI, the root scaler applies `s_ui` once and reacts to a settings change without a resize; text floors hold (AC-HUD-15) |

**Engine facts this ADR relies on** ([V] read in the 5.8.4 source this session or verified by a cited ADR; [D] engine-reference; [U] unverified, owned by the named check):

| # | Fact | Tag |
|---|---|---|
| E1 | `UUserWidget::AddToPlayerScreen(int32 ZOrder)` puts the widget in the local player's overlay ordered by ZOrder; that player canvas sits in the game layer's `Player` slot (100), **below** the `Viewport` slot (200) where every `AddToViewport` widget goes whatever its ZOrder | [V] `UMG/Public/Blueprint/UserWidget.h:342, 351`; `SGameLayerManager.cpp:133-143, 294-299`; `SGameLayerManager.h:107-116`; `GameViewportSubsystem.cpp:186, 192` (engine gate) |
| E2 | The camera fade (`APlayerCameraManager::FadeAmount`/`FadeColor`, set by `StartCameraFade`) is applied as the scene view's `OverlayColor`, i.e. inside the scene render; all UMG/Slate is composited after the scene, so a camera fade can never cover any UMG layer | [V] `Runtime/Engine/Private/LocalPlayer.cpp:813-814`; `SceneView.cpp:895` |
| E3 | Player cameras are updated after all actors tick (`PlayerController->UpdateCameraManager` at the end of the level tick); inside `GEngine->Tick` the world tick and `RedrawViewports` both run, and Slate's `Tick(TimeAndWidgets)` paints later in the same engine-loop iteration, so a paint pass sees the camera of the frame being rendered; `ULocalPlayer::GetProjectionData` returns that view with the aspect constraint and the player's sub-rect applied | [V] `Runtime/Engine/Private/LevelTick.cpp:1835-1847`; `Launch/Private/LaunchEngineLoop.cpp:5859, 5991`; `Engine/Private/GameEngine.cpp:1901, 2034`; `Engine/Classes/Engine/LocalPlayer.h:623` (engine gate) |
| E4 | `UWidget::ForceVolatile(bool)` / `SWidget::ForceVolatile` exist; a volatile widget is laid out and painted every frame and never cached by an invalidation panel | [V] `UMG/Public/Components/Widget.h:620`; `SlateCore/Public/Widgets/SWidget.h:1148` |
| E5 | `UUserInterfaceSettings` has `UIScaleRule` (`EUIScalingRule`: `ShortestSide`, `LongestSide`, `Horizontal`, `Vertical`, `ScaleToFit`, `Custom`), `UIScaleCurve`, `ApplicationScale`, `CustomScalingRuleClass`; `UDPICustomScalingRule::GetDPIScaleBasedOnSize(FIntPoint)` is the custom hook and runs on the class default object; outside the editor its result is **cached per viewport size** (reset only by editor property edits), so a runtime-changing factor in the rule would not take effect until a resize; `ApplicationScale` is multiplied in and platform DPI is divided out by the game layer manager; `FSlateApplication::SetApplicationScale` is process-wide (would also scale the editor in PIE) | [V] `Engine/Classes/Engine/UserInterfaceSettings.h:32-44, 168-189, 224`; `UserInterfaceSettings.cpp:91-109, 133, 243`; `Engine/Classes/Engine/DPICustomScalingRule.h:15-23`; `SGameLayerManager.cpp:491-495`; `Slate/Public/Framework/Application/SlateApplication.h:908` (engine gate) |
| E6 | `UCommonActivatableWidget` adds its optional `InputMapping` context on activation and removes it on deactivation when Enhanced Input support is enabled; it cancels its active hold inputs on deactivation; `GetDesiredInputConfig()` returns an `FUIInputConfig` (`ECommonInputMode` Game/Menu/All) | [V] `Plugins/Runtime/CommonUI/Source/CommonUI/Private/CommonActivatableWidget.cpp:310-345`; `Public/CommonActivatableWidget.h:108, 225-235`; `Public/Input/UIActionBindingHandle.h:94-105` |
| E7 | `UCommonUIActionRouterBase::OnActiveInputModeChanged()` (event with `ECommonInputMode`) and `GetActiveInputMode()` exist; the router flushes pressed keys on the next tick after a switch to `Menu` (`CommonUI.AutoFlushPressedKeys`, default true) | [V] `CommonUI/Public/Input/CommonUIActionRouterBase.h:92-94`; `Private/Input/CommonUIActionRouterBase.cpp:39-42` (resolves ADR-0011 E11's [U] delegate name) |
| E8 | `UCommonInputSettings::IsEnhancedInputSupportEnabled()` / `bEnableEnhancedInputSupport` exist; Common UI routing **requires** `UCommonGameViewportClient` (without it the router logs that routing "will not function correctly"); `Menu` input mode blocks game input in that viewport client; `UCommonActivatableWidgetStack` exists | [V] `CommonInput/Public/CommonInputSettings.h:60-75`; `CommonUIActionRouterBase.cpp:333-336, 667`; `CommonGameViewportClient.cpp:133, 163`; `CommonActivatableWidgetContainer.h:202` (engine gate) |
| E11 | `FadeAmount` animates only while `bEnableFading` is set, and the scene `OverlayColor` is applied under the same flag; the camera manager's audio fade is tied to the same fade | [V] `PlayerCameraManager.cpp:503, 977-984`; `LocalPlayer.cpp:810-815` (engine gate) |
| E12 | `UWidget::ForceVolatile` is pushed to the Slate widget in `SynchronizeProperties`; a `UWidget` wrapping a custom Slate widget must release it in `ReleaseSlateResources` | [V] `UMG/Private/Components/Widget.cpp:510-516, 1499` (engine gate) |
| E9 | The CommonUI plugin is **not** enabled in `StealthGame.uproject` today; Enhanced Input is (`DefaultInput.ini` sets the Enhanced player input and component classes) | [V] `StealthGame.uproject:13`; `Config/DefaultInput.ini:89-90` |
| E10 | A context added with default `FModifyContextOptions` ignores keys already pressed until release (`bIgnoreAllPressedKeysUntilRelease` default true), for Boolean actions only | [V] per ADR-0011 E3, E12 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (Presentation reads only its own controller's carriers; no world iteration; C-TeamView parts). ADR-0002 (`EstimatedServerNow`, `IsServerClockSynced`; presentation never on arrival). ADR-0007 (`StealthPresentation` module, compile guard, `AStealthHUD` + `UStealthPresentationBootstrap::Bind(FStealthClientViewHandles)`; this ADR **requests** that the handles carry `IOwnerCommandClientView`). ADR-0011 (reply batch, `FHoldStatus`, `UPawnCommandClient`, contexts, Common UI rules of §9; this ADR **requests** the context-gate amendment of §8). ADR-0015 (`IStealthCameraView::GetViewState`, view release; this ADR **requests** the screen-fade amendment of §3). ADR-0017 (`IsAllyBodyHidden`). ADR-0018 (`FStrikeResult`/`FDefenceResult`, `OnRoleChangePresented`, `FActionState` contact time). ADR-0020 (`FObjectiveTeamView`, view helpers). ADR-0003 (fallback position feed part). ADR-0004 (client tuning face for HUD/Markers keys) |
| **Enables** | HUD and Teammate Markers epics; Settings & Accessibility (VS) hooks; Event Feedback & Cues (VS) cue timing |
| **Blocks** | HUD epic, Teammate Markers epic, any menu story (lobby, host panel, pause) |
| **Ordering Note** | Presentation is built after the carriers it reads exist (ADR-0001 C-TeamView, ADR-0011 replies, ADR-0020 objective part). The two amendment requests (ADR-0011 §9 context gate; ADR-0015 §6 screen fade) must be folded into those ADRs before either is Accepted, or §3/§8 use the fallbacks stated there. Enabling the CommonUI plugin and the DPI/viewport config is a project-file change done by the implementing story (outside this run's write scope) |

## Context

### Problem Statement

The HUD and Teammate Markers GDDs fix behaviour and defer the mechanism to "the UI ADR" (HUD OQ8, TM OQ6, architecture AQ-15): UMG layers or an `AHUD` canvas; how markers project with zero lag and sit under the camera's black but over the world; the view-model shape that the leak suite dumps; where `PresentedOwnState` lives; how timed plates use the server clock; the DPI rule; and how far CommonUI reaches under 5.8's Enhanced Input / Common UI unification without breaking the IIF's edge contract (ADR-0011 §9). The prototype `AStealthHUD::DrawTeammateMarkers` iterates every character and reads a team field replicated on every pawn (`StealthHUD.cpp:171-174`, ADR-0001 migration table), the exact pattern TM-R2 bans, and it draws on the `AHUD` canvas, which cannot be ordered against UMG plates.

### Constraints

- ADR-0001 / IVP R4, R10: presentation reads only its own controller's carriers, `FRoundPublicState`, the client tuning face and client-local queries; never a pawn's `PlayerState`/`Controller`/`Owner`, never a world iteration; the listen host reads the same way (host parity).
- ADR-0002: presentation is scheduled only from `EstimatedServerNow()`; never on replication arrival, `GetServerWorldTimeSeconds` or wall clock.
- ADR-0007: Presentation includes only `View/` headers (compile guard); client wiring only through the bootstrap; no service discovery by world search.
- ADR-0011 §9: no gameplay command is a Common UI action; gameplay `IMC_*` contexts are added and removed only by `UPawnCommandClient`; Ends are idempotent and sent synchronously before a switch to `Menu`.
- HUD-R4 ZOrders are the contract; every HUD layer is a plain overlay, `HitTestInvisible`, never a `UCommonActivatableWidget`.
- TM-R1/TM-R9/VA-2: no component on any pawn (no `UWidgetComponent`), screen space after post-process, no depth test, constant pixel size, edge-clamped, projection inside the paint pass from the rendered frame's view.
- Budget: UMG HUD + markers ≤ 0.5 ms game thread on the listen host (architecture CPU table); AC-HUD-22 asks ≤ 0.3 ms for the HUD.

### Requirements

- HUD-R1..R17 (carriers, leak parity, layers, anchors, timing, `PresentedOwnState`, clusters, prompts, reset/rebuild) and F1-F10.
- TM-R3..R14 (marker set, anchor, fills, retire hold, takeover, view gate, always-on-top/edge clamp, `IsMarkedAlly`, labels, settings, `OnAllyMarkerRetired`).
- CS-R15 (`GetViewState`, fade layer order) and IIF R13 (contexts follow the owner's own state; flush on swap).
- Deterministic, dumpable view-models for AC-HUD-01..07, -20 (host parity).

## Decision

### 1. Layers (HUD-R4)

Each layer is one `UUserWidget` subclass created once per local player by the bootstrap and added with `AddToPlayerScreen(ZOrder)` (E1); all are created at HUD init, **collapsed** until needed, and never re-created during a round (the widget tree is identical for every viewer and outcome, which AC-HUD-07's capture diff relies on).

| Layer | ZOrder | Widget | Content | Notes |
|---|---|---|---|---|
| L0 world-anchored | 10 | `UStealthWorldAnchorLayer` hosting one `SStealthWorldAnchorLeaf` (§2) | teammate markers + name labels, interact ring (HUD-E12), context prompt (HUD-E13), **Informant Pings: ground rings, edge rings and sender labels (HUD-E22; PNG-R7)** beneath the markers | volatile; collapsed while `GetViewState() == Neutral` |
| Fade | 20 | `UStealthScreenFadeLayer` (one full-screen `SColorBlock`-style image) | camera-driven black | §3 |
| L1 persistent plates | 30 | `UStealthHudPersistentLayer` | brace cluster, tally, rejection plate, objectives cluster, team feed, informant brackets, latency icon | invalidation panels per cluster |
| L2 lower-centre | 40 | `UStealthHudTransientLayer` | Result slot, Notice slot | |
| L3 cards and banners | 50 | `UStealthHudCardLayer` | removal card, end banner, called-off, lobby line, **round summary (HUD-E21: two string-table lines from `FRoundResult.TeamSummary`, shown with the end banner)** | |
| L4 status overlays | 60 | `UStealthStatusOverlayLayer` | Session's connection and loading overlays: **display only** (text, spinner, progress; never a button) | status buttons live on the Menu root, see the L4 input rule below |
| Menus | 100 | `UStealthMenuRoot` (Common UI `UCommonActivatableWidgetStack`) | lobby, host panel, pause/settings | §7 |

Every layer below 100 is `HitTestInvisible`, not focusable, has no input bindings and is **never** a `UCommonActivatableWidget` (an activatable's input config or mapping context could change contexts mid-strike, E6). UMG is composited after the scene and its post-process, so no grade, vignette or fade-in-scene darkens any layer (E2). **`AddToViewport` is banned for game UI** (lint): viewport content sits above every player-screen layer whatever its ZOrder (E1), so one stray widget would break the HUD-R4 order.

**L4 input rule (closeout 2026-10-25; resolves interaction-patterns P7).** "No input below layer 100" is **absolute**: L4 (and every layer below 100) is display-only, `HitTestInvisible`, never an activatable, and never hosts a button. A Session status overlay that needs buttons (PAT-CRD-3: Cancel / OK on a connection-lost, travel-failed or loading-cancel state) is split in two: (1) the **display part** on L4 (message, spinner, progress, countdown), and (2) the **action part** as a modal `UCommonActivatableWidget` (`UStealthStatusActionModal`, Session-owned content) pushed onto the Menu root stack at ZOrder 100 through `UStealthMenuStack::PushMenu` (§7), so it gets Common UI focus, `IMC_Menu` routing and the synchronous `EndAllOutstanding(MenuOpen)` exactly like any menu. The modal sits above L4 by ZOrder, shows only the buttons (and may repeat the headline text), and Session owns when it is pushed and popped (it never opens during a live strike: these states are `Traveling`, `Disconnected`/`Reconnecting`, a failed travel and `Ended`, where the pawn has no meaningful input anyway; overlays that can appear while the pawn is live, such as a latency warning, are display-only with no action part). Gameplay input contexts are never touched by either part. A lint fails the build if a widget under ZOrder 100 binds a button/click handler, sets a non-`HitTestInvisible` visibility or derives from `UCommonActivatableWidget`.

**Apply on change only.** Each layer widget keeps the last applied value of every field and calls `SetText`, `SetVisibility`, `SetRenderOpacity` or a brush setter only when the new view-model value differs, so per-frame sampling (§4) never invalidates the L1-L3 invalidation panels when nothing changed.

**`AHUD` canvas is not used** for drawing (Alternative 1): the canvas is drawn with the scene viewport, under every UMG layer, so markers could not sit above the world yet below a UMG fade and in one order with the ring and prompt; `AStealthHUD` exists only as the owner of the bootstrap (ADR-0007).

### 2. L0: one volatile Slate leaf with in-paint projection (TM-R4, TM-R9, VA-2; HUD-E12/E13)

- `SStealthWorldAnchorLeaf` is a custom `SLeafWidget`, `ForceVolatile(true)` (E4), never inside an invalidation or retainer box. Its `UWidget` wrapper (`UStealthWorldAnchorLayer`'s child) builds it in `RebuildWidget`, releases it in `ReleaseSlateResources`, and the leaf implements `ComputeDesiredSize` (E12). In `OnPaint` it (1) reads the frame's view through `ULocalPlayer::GetProjectionData` (location, rotation, the view's actual FOV, aspect constraint and the player's sub-rect, after the boom and camera manager update, E3), (2) projects each anchor, (3) applies F2-F4 of the Markers GDD (anchor offset, edge clamp with hysteresis, edge de-overlap, far-to-near draw order) and the ring/prompt anchors, and (4) emits draw elements (glyph brushes or a hard-step UI material, nearest filtering; text for labels and the prompt). Projection never happens in a UMG binding, a widget tick before the camera update, or a game-thread tick (VA-2 projection contract).
- **Inputs** are a per-frame `FWorldAnchorViewModel` (markers: handle, anchor source, fill, phase of fade/hold; ring: target object, fill fraction, colour state; prompt: object, verb, glyph) produced by the presenter (§4) and the markers model (§5). The leaf reads world positions only for (a) the resolved `PawnRef`s of its own C-TeamView teammate entries (the pawn's skeletal-mesh component location, TM-R4), (b) fallback-feed positions, and (c) public interact-object positions (court fixtures, bushes) from the locally built layout components. It never reaches any other actor.
- **Constant pixel size:** sizes are computed in physical pixels from F1 (`u · s_ui · k_large`, absolute floors `W_min`/`H_min` not scaled), then divided by the leaf geometry's scale, so the DPI scale (§6) is applied once.
- **Zero lag** follows from the verified frame order (E3); UI-V1 remains as an observation on the graybox. Fallback if it ever fails: paint from a `FSceneViewExtension` hook's view data captured on the game thread for the same frame (the projection stays in the paint pass; only the view source changes).

### 3. The screen fade is a HUD layer that Camera drives (CS-R15, CR-HUD-10)

`StartCameraFade` darkens only the scene (E2), so it cannot cover L0 (markers, ring, prompt) while leaving L1-L4 readable. Decision: Camera keeps **all fade timing** (when, how fast, colour; CS-R12, ADR-0015 §6) and publishes it; the HUD's fade layer renders it. **Amendment requested to ADR-0015** (§6 and `IStealthCameraView`): add `FStealthScreenFade GetScreenFade() const` (`{float Alpha; FLinearColor Color}`, sampled per frame) and stop using `StartCameraFade` for the neutral-view, cut and elimination fades (the transient and informant grades stay post-process blends). The fade layer samples it in its own paint, so the black and the world/L0 content are consistent on every frame. Camera computes the alpha itself (it can no longer rely on `FadeAmount`, which animates only while the scene fade is on, E11); any audio fade ADR-0015 needs is re-homed with the same alpha. Fallback if the amendment is refused: Camera keeps calling `StartCameraFade`, and the fade layer **also** draws `FadeAmount`/`FadeColor` each frame, accepting that the world is darkened twice mid-fade (it reaches the same full black at the end; L0 is covered only by the layer).

### 4. View-models and the presenter (HUD-R1..R17, F1-F10)

- **One plain C++ presenter core**, `FHudPresenterCore` (no `UWorld`, no `UObject` access), turns an `FHudInputs` snapshot plus `ServerNowEst` and `LocalNow` into an `FHudViewModel`. It owns every presentation state machine the GDD defines: the viewer state table, `PresentedOwnState` (HUD-R6a), result-plate `Pending → Entering → Holding → Exiting` with F2 matching and drop rules, F3 timelines (sampled from `now − t0`, never UMG animation assets), F4 card time, F5 cooldown, F6 tally, F7 countdown, F8 feed queue, F9 objectives cluster (`rival_shown` monotonic within a `RoundEpoch`), F10 banner, HUD-R13 slot priorities, HUD-R17 rebuild. Headless specs drive it with a fake clock and fake carriers (`StealthGame.HUD.*` Logic ACs).
- **`FHudViewModel`** is a tree of plain `USTRUCT`s (one per cluster: brace, tally, objectives, feed, result slot, notice slot, card, banner, brackets, latency, world-anchor), holding only values to draw (strings as `FText` from string tables, enum ids, alphas, counts). It has a deterministic `Dump()` (stable field order, fixed float precision) used by the leak suite (AC-HUD-01..07) and host parity (AC-HUD-20).
- **Sampling, not events.** `UStealthHudPresenter` (Presentation, owned by the bootstrap) builds `FHudInputs` **every frame** from its handles: its own controller's C-TeamView parts (`URosterTeamViewComponent`, `UObjectiveTeamViewComponent`, Penalty's owner part), the owner reply sink (ADR-0011 `ClientCommandReplies` entries, consumed in order), `FRoundPublicState` and the client tuning face (through `FStealthClientViewHandles`), Camera's `GetViewState()`, Assassination's client-local `OnRoleChangePresented` (queued), the own pawn's `FActionState`/`FDeathState` for the pawn handle captured at the press (F2), own RTT from the local net connection, and local settings. Change detection is by value; **no presentation depends on a RepNotify having fired**, so the listen host (which gets no RepNotify for its own writes) and a remote client run the identical path (HUD-R1, AC-HUD-20).
- **Widgets are dumb.** Each layer widget receives the view-model sub-struct for its cluster and applies it (visibility, text, brush, render opacity). No UMG property bindings, no Blueprint graph logic, no widget animation assets; widget Blueprints carry layout and style only (ADR-0007 widget audit).
- **Time.** Every timed element uses `ServerNowEst = EstimatedServerNow(World)` (ADR-0002) and its one slewed offset; until `IsServerClockSynced()` the HUD shows persistent clusters but defers timed plates per F2's cap. Read holds are multiplied by `HudReadHoldMult` (HUD-R16).
- **Strings.** All player-facing text comes from a string table (`ST_Hud`), never from data on the wire; dynamic values are formatted locally (names from C-Public `DisplayName` by `PublicHandle`, numbers from owner data). The language rule word list is linted over the string table (AC-HUD-12 support).
- **Client-local stores that outlive the controller** (HUD-R7 tally display cache keyed by `RoundEpoch`, bridging until the C-TeamView owner re-push lands, the third-person-hint profile flag) live in a `ULocalPlayerSubsystem` (`UStealthHudLocalStore`), never in a replicated value.

### 5. Teammate markers model (TM-R3..R8, R10, R11, R14)

- `FTeammateMarkerModel` (plain C++) holds the marker set keyed by `PublicHandle`: validation (TM-R3), anchor source (`Tracking` pawn / `Fed` position / `Held` cached anchor, TM-R4/R4a), fill (TM-R5: informant → outline; concealed → hatch from `IsAllyBodyHidden(Pawn)` for tracked entries (ADR-0017) or the entry's `bIsConcealed` for fed entries; else filled; `RoundEnded` frozen by phase), retire hold and fade (TM-R6, F6-F7), takeover (TM-R7), label focus (TM-R11, F8). It is updated every frame by `UTeammateMarkerPresenter` from the local controller's `URosterTeamViewComponent` and `UTeamPositionFeedComponent` (ADR-0003) only — found with `FindComponentByClass` on its own controller (ADR-0001), never by world iteration (TM-R2; include/grep lint per ADR-0007/ADR-0008).
- **View gate** (TM-R8): drawing only while `GetViewState() == Live`; the model (and `IsMarkedAlly`) keeps running while gated.
- **`IsMarkedAlly`** (TM-R10) is needed by the IIF client (Core, ADR-0011 §9 Attack payload and the strike hint), which cannot include Presentation. Decision: a consumer-owned View interface in Core, `IAllyMarkerQuery { bool IsMarkedAlly(const APawn*) const; }` (`StealthGameplay/Public/View/Core/`), implemented by the markers presenter and injected into `UPawnCommandClient` by the bootstrap (weak). With no presentation bound (headless runs) it answers false; the server's teammate rule (ASN-R9) is unaffected.
- **`OnAllyMarkerRetired(PublicHandle)`** (TM-R14) is a native client-local delegate on the markers presenter, consumed by the HUD presenter in the same module (HUD-R10). Never replicated, never called by server code.
- **Reset/travel** (TM-R6.5): the bootstrap is re-bound when a new local controller arrives (ADR-0005 replaces controllers at travel); the model is cleared with no hold or fade on `Phase = Reset` and on travel start.
- **`ShowTeamNames`** (TM-R11 hold; **deferred to the Vertical Slice by user decision 2026-10-25**, not bound in MVP) is a local UI input action in `IMC_Common`; `UPawnCommandClient` raises a client-local `OnLocalUiAction(ShowTeamNames, bPressed)` that the markers presenter subscribes to; it is never sent to the server (TM CR-4 to IIF; GDD sync, Related).

### 6. DPI and scale (F1, HUD-R16, TM F1)

One custom rule, `UStealthDpiScalingRule : UDPICustomScalingRule` (E5), returns `ViewportHeight / 1080` only, so one Slate unit is one 1080p reference pixel. The client-local UI scale `s_ui` (MVP 1.0, VS setting; TDC CR-14, never tuning) is applied **by the HUD root**: every layer's root is a scale container (`UScaleBox` with user-specified scale, or an equivalent layout-affecting root scaler) set to `s_ui`, and the L0 leaf multiplies its glyph sizes by `s_ui` directly. `s_ui` must not live inside the DPI rule, because the engine caches the rule's result per viewport size outside the editor (E5), so a runtime change would not apply until a resize. `u · s_ui` is therefore applied exactly once; `ApplicationScale` stays 1.0 and `FSlateApplication::SetApplicationScale` is never used (process-wide; it would scale the editor in PIE, E5). Text size follows F1 (`max(floor_px · H/1080, base_px · u · s_text)` in physical pixels, converted by the geometry scale), so the 18/22/28 px floors ignore `s_ui` as the GDD requires. Persistent-area lint (F1) runs at build time over the UX spec's footprints. Config (implementing story): `[/Script/Engine.UserInterfaceSettings] UIScaleRule=Custom`, `CustomScalingRuleClass=/Script/StealthPresentation.StealthDpiScalingRule`.

### 7. Common UI: menus only (AQ-15)

- **Scope.** Common UI is used **only** for the menu root at ZOrder 100 (lobby, host panel, pause/settings, and the Session status action modal of the L4 input rule, §1). The in-round HUD has no input layer at all; it is not a Common UI activatable and owns no action domain. This answers AQ-15: menus only.
- **Unified input (5.8).** The CommonUI plugin is enabled with `bEnableEnhancedInputSupport = true` (E8), so menus use Enhanced Input actions (one data-asset set, the 5.8 unification) through one `IMC_Menu` context at a priority above the gameplay contexts. `IMC_Menu` is the `InputMapping` of **one persistent menu root activatable** (`UStealthMenuRoot`), activated while any menu is open; the individual menus pushed onto its `UCommonActivatableWidgetStack` carry **no** `InputMapping`, so a push or pop can never remove the context another menu still needs (the stack's activate/deactivate order on push is [U], UI-V2). `IMC_Menu` contains **only** UI navigation actions (navigate, accept, back, tab); a lint fails the build if it maps any `IA_*` gameplay action. Gameplay `IMC_Common`/`IMC_Active`/`IMC_Informant` remain owned by `UPawnCommandClient` (ADR-0011 §9); Common UI never adds or removes them. The game viewport client class becomes `UCommonGameViewportClient` (E8; config in the implementing story).
- **One way to open a menu.** Every menu is pushed through `UStealthMenuStack::PushMenu`, which first calls `UPawnCommandClient::EndAllOutstanding(EEndReason::MenuOpen)` **synchronously** (ADR-0011 builder; idempotent Ends), then pushes the activatable widget (whose `GetDesiredInputConfig()` is `Menu`). The router's `OnActiveInputModeChanged` (E7) is also bound by `UPawnCommandClient` as the second line (ADR-0011 §9), so a menu opened by any other path still ends holds before the router's next-tick key flush. On close, gameplay keys need a fresh press (E10).
- **No pause in multiplayer.** Opening a menu during a round never pauses the world; the pawn simply receives no input while in `Menu` mode (zero move, holds ended), identical for every player.

### 8. Input contexts follow the presented own role (IIF R13, HUD-R6a) — amendment request to ADR-0011

ADR-0011 §9 swaps `IMC_Active → IMC_Informant` when the owner's own `State` changes in its C-TeamView owner part. That state arrives at the resolving slot, up to `StrikeContactSec` (+ `BushStrikeEmergeSec`) **before** the visible blow; swapping then would stop the victim's local prediction and silence its presses before contact, a pre-contact warning that ASN-R6 and HUD-R6a forbid. Decision: the Informant context is selected by **one pure Core function** (`StealthGameplay/Public/View/Core/OwnerContextGate.h`, with a headless spec):

`UseInformantContext = (RawOwnState == Informant) ∧ (NoPresenterBound ∨ PresentedRole == Informant ∨ LocalNow ≥ t_state + HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec)`

where `RawOwnState` is the C-TeamView owner-part `State`, `PresentedRole` is the HUD's `PresentedOwnState` (`Active → Informant` at the card time after `OnRoleChangePresented`, or the exile card time, HUD-R6a/F4), and `t_state` is the local arrival of the raw `Informant` state. The cap term (the latest F4 card time) guarantees the gate cannot stick if the presenter never flips. A return to `Active` (rejoin inside the grace, a new round) is immediate because the raw term goes false, and HUD-R17's rebuild of `PresentedOwnState` from raw state at a rebuild agrees with it. The HUD presenter pushes `PresentedRole` through a Core View setter, `IOwnerCommandClientView::SetPresentedOwnRole(EPresentedOwnRole)`, implemented by `UPawnCommandClient`. Until the gate flips, the client keeps `IMC_Active` and keeps sending commands, which the server rejects (`NotNow`) and whose replies ASN-R23 already holds to `OnRoleChangePresented`. Every swap still ends outstanding Begins first and uses `bIgnoreAllPressedKeysUntilRelease` (E10). Without a bound presenter (headless) the gate reduces to raw state; AC-HUD-05 `NoWarningBeforeContact` must therefore run **with** the presenter bound. The informant plate (HUD-E16) is shown only from presses caught under `IMC_Informant`, so it never appears before the card has collapsed (HUD-R13).

### 9. What Presentation may and may not touch

Presentation reads exactly (architecture Presentation contract): its own controller's C-TeamView parts and owner data, the owner reply sink, `FRoundPublicState` and `FLayoutManifest`-derived local layout components (court and bush positions for prompts/rings), `GetClientTuning()`, Camera's view facade, Concealment's `IsAllyBodyHidden`, Assassination's client-local dispatches, the own pawn's `FActionState`/`FDeathState`, the teammate pawns resolved from its own `PawnRef`s, local settings and own RTT. It never iterates actors, never reads a pawn's `PlayerState`/`Controller`/`Owner`, never calls `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, never includes a `Public/Server/` header (compile guard), and writes nothing replicated.

### Architecture Diagram

```
 CARRIERS (own PlayerController, owner-only)          PUBLIC / LOCAL
  URosterTeamViewComponent  UObjectiveTeamViewComponent   FRoundPublicState (GameState)   client tuning face
  UTeamPositionFeedComponent  owner reply sink (ClientCommandReplies)   Camera view facade   IsAllyBodyHidden
  Penalty owner part (tally re-push, T_due)            own pawn FActionState/FDeathState   OnRoleChangePresented
            │  (sampled every frame by value; no RepNotify dependency — host == client)
            ▼
 UStealthPresentationBootstrap (owned by AStealthHUD; Bind(FStealthClientViewHandles))
   ├─ UStealthHudPresenter ── FHudPresenterCore (plain C++) ──► FHudViewModel (USTRUCTs, Dump())
   │       ├─ PresentedOwnState ──► IOwnerCommandClientView::SetPresentedOwnRole (IIF contexts, §8)
   │       └─ ServerNowEst = EstimatedServerNow() (ADR-0002)
   ├─ UTeammateMarkerPresenter ── FTeammateMarkerModel ──► IAllyMarkerQuery (→ UPawnCommandClient), OnAllyMarkerRetired (→ HUD)
   └─ UStealthMenuStack (Common UI) ── PushMenu: EndAllOutstanding() then push activatable (IMC_Menu)
            ▼  AddToPlayerScreen(ZOrder), all pre-created, collapsed when unused
  Z10  L0  UStealthWorldAnchorLayer ─ SStealthWorldAnchorLeaf (volatile; projects in OnPaint): markers, ring, prompt
  Z20  Fade UStealthScreenFadeLayer ◄── IStealthCameraView::GetScreenFade() (Camera owns timing)
  Z30  L1  persistent plates    Z40 L2 Result/Notice    Z50 L3 cards/banners    Z60 L4 status overlays
  Z100 Menus (UCommonActivatableWidgetStack)
  scene + post-process + StartCameraFade overlay are below all of the above (E2)
```

### Key Interfaces

```cpp
// --- StealthGameplay/Public/View/Core/ (consumer-owned by Core, implemented in Presentation) ---------
// Both are UINTERFACEs so TWeakInterfacePtr / TScriptInterface work (ADR-0007 weak registration).
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UAllyMarkerQuery : public UInterface { GENERATED_BODY() };
class IAllyMarkerQuery { GENERATED_BODY() public: virtual bool IsMarkedAlly(const APawn* Pawn) const = 0; }; // TM-R10
UENUM() enum class EPresentedOwnRole : uint8 { Active, Informant };
bool UseInformantContext(EParticipantState RawOwnState, bool bPresenterBound, EPresentedOwnRole PresentedRole,
                         double LocalNow, double TStateArrival, double CapSec);   // pure, §8
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UOwnerCommandClientView : public UInterface { GENERATED_BODY() };
class IOwnerCommandClientView { GENERATED_BODY()   // implemented by UPawnCommandClient (ADR-0011 amendment);
public:                                           // handed to Presentation in FStealthClientViewHandles (ADR-0007 amendment)
    virtual void SetAllyMarkerQuery(TWeakInterfacePtr<IAllyMarkerQuery> Query) = 0;
    virtual void SetPresentedOwnRole(EPresentedOwnRole Role) = 0;                    // §8 context gate
    virtual void EndAllOutstanding(EEndReason Reason) = 0;                           // MenuOpen, FocusLost, ...
    virtual FOnLocalUiAction& OnLocalUiAction() = 0;                                  // ShowTeamNames (local only)
};
// IStealthCameraView (ADR-0015) amendment:
struct FStealthScreenFade { float Alpha = 0.f; FLinearColor Color = FLinearColor::Black; };
//   virtual FStealthScreenFade GetScreenFade() const = 0;

// --- StealthPresentation/Private (client only; STEALTH_PRESENTATION_MODULE guard) --------------------
USTRUCT() struct FHudViewModel { /* per-cluster USTRUCTs: Brace, Tally, Objectives, Feed, ResultSlot,
                                     NoticeSlot, Card, Banner, Brackets, Latency, WorldAnchor */ 
                                 FString Dump() const; };   // deterministic, for the leak suite
struct FHudInputs { /* snapshot of carriers, replies, phase, tuning, view state, own pawn writes, settings */ };
class FHudPresenterCore {                                    // plain C++, headless-testable
public: void Step(const FHudInputs& In, double ServerNowEst, double LocalNow, FHudViewModel& Out); };
class FTeammateMarkerModel {                                 // plain C++
public: void Step(const FMarkerInputs& In, double LocalNow, FWorldAnchorMarkers& Out);
        bool IsMarkedAlly(const APawn* Pawn) const; };
UCLASS() class UStealthDpiScalingRule : public UDPICustomScalingRule {
    virtual float GetDPIScaleBasedOnSize(FIntPoint Size) const override; };   // Size.Y / 1080 only; s_ui by root scaler
class SStealthWorldAnchorLeaf : public SLeafWidget {         // ForceVolatile; projection inside OnPaint only
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&,
                          int32 LayerId, const FWidgetStyle&, bool bParentEnabled) const override; };
UCLASS() class UStealthMenuStack : public UObject { public: void PushMenu(TSubclassOf<UCommonActivatableWidget>); };
// ZOrder contract (HUD-R4): L0 10, Fade 20, L1 30, L2 40, L3 50, L4 60, Menus 100.
```

### Implementation Guidelines

- Every HUD and marker layer must be a UMG widget added with `AddToPlayerScreen` at its HUD-R4 ZOrder; `AddToViewport` and the `AHUD` canvas must not be used for game UI (lint).
- Widgets must apply a view-model value only when it differs from the last applied value.
- Layers below ZOrder 100 must be `HitTestInvisible`, non-focusable, have no input bindings and must never be `UCommonActivatableWidget`s.
- All HUD widgets must be created once at HUD init and toggled by visibility; no widget may be created or destroyed during a round in response to gameplay.
- World-anchored drawing (markers, interact ring, context prompt) must happen in `SStealthWorldAnchorLeaf::OnPaint` from the rendered frame's camera view; it must never be computed in a UMG binding, a widget tick or a game-thread tick; the leaf must be volatile and never inside an invalidation or retainer box.
- No `UWidgetComponent` or any other component may be added to a pawn for presentation.
- The screen fade must be drawn by the HUD fade layer at ZOrder 20 from Camera's published fade; Camera owns its timing.
- Presentation must build view-models by sampling its carriers every frame by value; it must never depend on a RepNotify having fired, so the listen host runs the same path.
- Presentation timing must use only `EstimatedServerNow()`; plate and card timelines must be sampled from `now − t0` in C++, never from widget animation assets.
- Widgets must be dumb: no UMG property bindings, no Blueprint logic beyond layout and style; `FHudViewModel::Dump()` must be deterministic.
- Player-facing text must come from string tables; no text may be carried on the wire.
- Common UI must be used only for the menu root; menus must open through `UStealthMenuStack::PushMenu`, which must end outstanding gameplay Begins synchronously first; `IMC_Menu` must contain no gameplay action; Common UI must never add or remove a gameplay `IMC_*` context.
- The Informant input context must follow `PresentedOwnState`, never the raw own `State`.
- `IsMarkedAlly` must be served only from the local marker model built from the own controller's C-TeamView; it must never be sent to the server.
- DPI must come only from `UStealthDpiScalingRule` (`H/1080`); the UI scale `s_ui` must be applied once by the layer roots and the L0 leaf, never inside the DPI rule; `ApplicationScale` must stay 1.0 and `SetApplicationScale` must not be called.
- `IMC_Menu` must be held only by the persistent menu root; pushed menus must carry no `InputMapping`.
- The Informant context must be selected only by the §8 `UseInformantContext` function.
- Presentation must never iterate actors, read a pawn's `PlayerState`/`Controller`/`Owner`, call `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, include a `Public/Server/` header, or write anything replicated.
- The prototype `AStealthHUD` drawing code must be replaced, not ported.

## Alternatives Considered

### Alternative 1: `AHUD` canvas pass for markers (and HUD)
- **Description**: Draw markers (and possibly plates) in `AHUD::DrawHUD` on the canvas, as the prototype does.
- **Pros**: Exact projection with the canvas's own `Project`; least code; proven in the prototype.
- **Cons**: The canvas is drawn with the viewport, below every UMG layer, so it cannot be ordered between the world and a UMG fade that must cover markers but not plates; no DPI scaling or localisation-friendly text layout; mixing canvas and UMG splits the layer contract.
- **Rejection Reason**: Fails HUD-R4's single ordered layer stack (markers under the fade, ring and prompt in the same layer).

### Alternative 2: One `UUserWidget` per marker positioned by a widget tick or binding
- **Description**: Marker widgets moved with `SetPositionInViewport` each tick.
- **Pros**: Pure UMG; designer-friendly.
- **Cons**: The widget tick runs at a different point from the camera update, giving a frame of lag in fast turns (VA-2); per-widget layout cost; marker widgets created and destroyed with entries.
- **Rejection Reason**: Violates the in-paint projection contract and the zero-lag requirement.

### Alternative 3: `UWidgetComponent` on teammate pawns
- **Description**: World-space or screen-space widget components attached to ally pawns.
- **Pros**: Engine handles projection and occlusion.
- **Cons**: A component on a pawn (TM-R1, IVP L17), present or absent per viewer; depth/occlusion and post-process interplay; per-pawn cost.
- **Rejection Reason**: Breaks "no component on any pawn" and the one-pawn-class rule (ADR-0001).

### Alternative 4: Common UI for the HUD input layer too
- **Description**: Make the in-round HUD an activatable root with a `Game` input config and route gameplay actions through Common UI action domains.
- **Pros**: One input router for menus and game; Common UI hold-action features.
- **Cons**: Activation adds/removes mapping contexts and changes input mode at moments the IIF does not control (E6), risking lost or duplicated Ends mid-strike; ADR-0011 forbids gameplay commands as Common UI actions.
- **Rejection Reason**: Edge safety of server-meaningful commands outweighs a single router.

### Alternative 5: No Common UI at all (plain UMG menus)
- **Description**: Menus as plain `UUserWidget`s with manual focus handling.
- **Pros**: No plugin, no viewport-client change, fewer moving parts.
- **Cons**: Gamepad navigation, back handling and input-mode switching written by hand; the 5.8 unification's single action-asset set is lost.
- **Rejection Reason**: Common UI confined to menus gives gamepad-correct menus at low risk once the menu-open flush rule exists.

### Alternative 6: Event-driven presentation (RepNotify handlers drive widgets)
- **Description**: Update widgets from `OnRep_*` and RPC handlers.
- **Pros**: No per-frame sampling.
- **Cons**: The listen host gets no RepNotify for its own writes (host parity breaks); arrival order between actors is unspecified, so presentation would encode arrival order; harder to dump deterministically.
- **Rejection Reason**: State-derived sampling is the architecture's rule (Principle 2) and is cheap at this data size.

### Alternative 7: Engine DPI curve or `ApplicationScale` for UI scale
- **Description**: Use the default DPI curve and multiply the user UI scale through `ApplicationScale` or `SetApplicationScale`.
- **Pros**: No custom class.
- **Cons**: The default curve is not `H/1080`; `SetApplicationScale` is process-wide (scales the editor in PIE); risk of applying the scale twice (VA-2); putting `s_ui` inside a custom rule fails at runtime because the rule's result is cached per viewport size (E5).
- **Rejection Reason**: A custom rule for `u` plus a root scaler for `s_ui` applies `u · s_ui` exactly once and reacts to a settings change at once.

## Consequences

### Positive
- One ordered layer stack makes AC-HUD-24 (fade covers markers, not plates) true by construction.
- The presenter core is headless and deterministic: most HUD ACs run without PIE; the leak suite compares dumps.
- Host and client run the same presentation path (no RepNotify reliance).
- Menus cannot strand a held gameplay key: the menu stack ends holds before Common UI's flush.
- The victim's input context no longer changes before the blow lands.

### Negative
- Two amendments to other ADRs (ADR-0011 context gate and View setter; ADR-0015 screen fade) are required.
- A custom Slate leaf and a custom DPI rule are engine-level code to maintain across engine upgrades.
- Per-frame sampling costs a little game-thread time even when nothing changes (bounded by the small carrier set).
- Enabling CommonUI changes the game viewport client class (a project config change).

## Risks
- **R1 Slate paint not after the camera update in some path** (UI-V1): fallback view source via a scene view extension (§2).
- **R2 Common UI activation order** (activatable adds `IMC_Menu` before the router mode change, E6/E7): mitigated by `PushMenu` ending holds first; UI-V2 tests both orders.
- **R3 `UCommonGameViewportClient` interplay with the neutral view or seamless travel** [U]: covered by the S3 travel spike (ADR-0005) with menus open.
- **R4 Perf over budget** (≤ 0.3 ms HUD, ≤ 0.5 ms with markers): L1-L3 in invalidation panels; only L0 volatile; profiled by AC-HUD-22.
- **R5 PIE DPI**: the custom rule is per-viewport; PIE windows keep editor UI unscaled (UI-V4).
- **R6 Amendments refused**: §3 and §8 state fallbacks; §8's fallback leaves a pre-contact context swap (an ASN-R6 concern recorded, not silently accepted).

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| hud.md | HUD-R1 three carriers, host parity, no server headers, no world iteration | §4 sampling inputs, §9; compile guard (ADR-0007) |
| hud.md | HUD-R2/R3 nothing identifies a non-ally; view-model parity | §4 deterministic view-model and `Dump()`; widgets pre-created (tree constant) |
| hud.md | HUD-R4 layers and ZOrders, after post-process, never activatable, CommonUI only for menus | §1, §3, §7 |
| hud.md | HUD-R5 anchors, safe rect, one layout | §1 layers, §6 DPI, F1 lint |
| hud.md | HUD-R6, R6a, R7, R9, R12, R13 timing and `PresentedOwnState` | §4 presenter core with `EstimatedServerNow`; §8 contexts follow `PresentedOwnState` |
| hud.md | HUD-R10, R11, R14, R15 feed, objectives cluster, round surfaces, informant view | §4 (inputs incl. `FObjectiveTeamView`, `OnAllyMarkerRetired`), §5 |
| hud.md | HUD-R16 scaling and comfort; HUD-R17 reset/rebuild | §6; §4 rebuild in the presenter core; `UStealthHudLocalStore` |
| hud.md | F1-F10 | §4 (all formulas in `FHudPresenterCore`), §6 (F1) |
| teammate-markers.md | TM-R1/R2 one source, no world iteration | §5 own controller components only; §9 |
| teammate-markers.md | TM-R3..R7 set, anchor, fills, retire hold, takeover | §5 `FTeammateMarkerModel` |
| teammate-markers.md | TM-R8 view gate; TM-R9 always on top, constant size, edge clamp, no depth test; VA-2 projection contract | §1 L0 below fade; §2 leaf; §6 size rule |
| teammate-markers.md | TM-R10 `IsMarkedAlly`; TM-R11 labels; TM-R12 settings; TM-R13 no motion/audio; TM-R14 `OnAllyMarkerRetired` | §5 (`IAllyMarkerQuery`, delegate, `ShowTeamNames`) |
| camera-system.md | CS-R15 `GetViewState`, fade layer above markers and below plates | §1 Fade layer Z20, §3 |
| input-interaction-framework.md | R13 contexts follow own state; flush on swap (CVar `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush`) | §8 (follows the presented own state; Ends first) |
| architecture.md | AQ-15 CommonUI scope | §7: menus only |

## Performance Implications
- **CPU**: Presenter sampling and view-model build ≈ 0.05-0.1 ms; L0 leaf paint for ≤ 4 markers + ring + prompt ≈ 0.05 ms; L1-L3 mostly cached by invalidation panels. Target ≤ 0.3 ms HUD (AC-HUD-22) and ≤ 0.5 ms with markers (architecture CPU table "UMG HUD + markers").
- **GPU**: UI ≤ 0.5 ms at 1080p on the reference GPU, including the full-screen fade layer's overdraw when visible and the L0 leaf; owned by ADR-0022's frame-budget table (a UI line inside the 14.4 ms allocation).
- **Memory**: All HUD widgets pre-created: a few hundred KB of widget objects and brushes.
- **Load Time**: HUD init at the local controller's arrival; no runtime widget creation.
- **Network**: None. Presentation adds no replicated property or RPC.

## Migration Plan
The prototype `AStealthHUD` (canvas drawing, `DrawTeammateMarkers` world iteration with `IsNpc()`/team reads) is replaced, not migrated (ADR-0007 promotion rule). The implementing stories also: enable the CommonUI plugin; set `bEnableEnhancedInputSupport`; set `GameViewportClientClassName` to `UCommonGameViewportClient`; set the custom DPI rule in `DefaultEngine.ini`; author `IMC_Menu` and the string tables. These are project-file changes outside this design run's write scope.

## Validation Criteria
- Headless Logic (`StealthGame.HUD.*`): ResultShowTime (AC-HUD-08), PlateTimeline (09), TallyDisplay (10), CooldownAndCountdown (14), LayoutResolver (15 Logic part, incl. DPI rule), FeedQueue (16), ObjectivesAndBanner (17), BraceStates (26); markers model specs (TM ACs for TM-R3..R7, R10, R14).
- PIE/net: StrikeTargetParity, InformantNpcParity, TallyNotBeforePlate, LookParity, NoWarningBeforeContact (with §8), OpponentNoTeamLines, RemovalCardTiming, RebuildNoReplay, HostParity, InformantView, LayerOrder (UI-V3), PromptAndRing (AC-HUD-01..06, 13, 19-21, 24, 25); UI-V2 menu edge test with ADR-0011 V1/V5.
- Captures: AC-HUD-07 strike strip, AC-HUD-23 run-and-observe screenshots in `production/qa/evidence/`.
- Lints: include/grep (AC-HUD-11, TM-R2), `IMC_Menu` has no gameplay action, no `UWidgetComponent` on pawns, no `SetApplicationScale`.
- Perf: AC-HUD-22 on reference hardware.

## Related
- Depends on ADR-0001, ADR-0002, ADR-0003, ADR-0004, ADR-0005, ADR-0007, ADR-0011, ADR-0015, ADR-0017, ADR-0018, ADR-0020.
- Design: `design/gdd/hud.md`, `design/gdd/teammate-markers.md`, `design/gdd/camera-system.md`, `design/gdd/input-interaction-framework.md`, `design/art/art-bible.md` §7.
- **Amendment requests (not applied here):** ADR-0011 §9 — Informant context follows `PresentedOwnState` via `IOwnerCommandClientView::SetPresentedOwnRole`; `IAllyMarkerQuery` injection; `EndAllOutstanding`; `OnLocalUiAction(ShowTeamNames)`; the router delegate name is `UCommonUIActionRouterBase::OnActiveInputModeChanged` [V] (resolves ADR-0011 E11's [U]). ADR-0015 §6 — publish `GetScreenFade()` (Camera computes the alpha itself; audio fade re-homed) and stop using `StartCameraFade` for neutral/cut/elimination fades. ADR-0007 — add `IOwnerCommandClientView` (and the markers' `IAllyMarkerQuery` hand-back) to `FStealthClientViewHandles`. ADR-0022 — a UI GPU line (≤ 0.5 ms).
- **GDD sync requests (not applied here):** input-interaction-framework.md R13 — "the context swaps on the owner's own Participant state change" becomes "on the owner's presented own role (HUD-R6a)"; R13's "verify `EnhancedInput.IgnoreHeldKeysOnFlush`" names `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush`; add the local `ShowTeamNames` action (TM CR-4). hud.md OQ8 and teammate-markers.md OQ6 are answered by §1-§6; hud.md HUD-R4 "CommonUI is used only for L4 menus" becomes "only for the menu root (ZOrder 100); L4 holds Session's status overlays". camera-system.md CS-R15 — the fade is rendered by the HUD fade layer from Camera's published fade.

## Amendments
Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- Requests answered (2026-10-24): the ADR-0011 §9 context-gate and `IOwnerCommandClientView` amendments, the ADR-0015 §6 `GetScreenFade()` amendment and the ADR-0007 `IOwnerCommandClientView` handle (Related) are applied by their owners. The Status line's "with the context-gate amendment requested in §8" and "with the screen-fade amendment requested in §3" conditions are met. The GDD re-reviews and UI-V1..V4 it names are unchanged — architecture-review fix 2026-10-25 (Conflict 9).
- Project config applied 2026-10-25 (phase 2, user-authorised): `CommonUI` plugin enabled in `StealthGame.uproject`; `Config/DefaultGame.ini [/Script/CommonInput.CommonInputSettings] bEnableEnhancedInputSupport=True`; `Config/DefaultEngine.ini [/Script/Engine.Engine] GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`. **`UIScaleRule` left at the engine default (`ShortestSide`)**: `UStealthDpiScalingRule` does not exist in code yet, and pointing `CustomScalingRuleClass` at a missing class would fail to resolve; the implementing story sets `UIScaleRule=Custom` and `CustomScalingRuleClass=/Script/<module>.StealthDpiScalingRule` when it adds the class. The viewport-client swap is not exercised by the headless (-nullrhi) run; a PIE check is an editor follow-up.

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §2 data inputs — user decision 2026-10-25: the presenter reads `T_due` and `{WrongTargetTally, LastResultSeq}` from the C-TeamView owner part (ADR-0001 §5-§6); HUD-E11 shows a live "Returning in {s}" countdown, static "Returning" only at `s ≤ 0`.
- §2 — user decision 2026-10-25: `FStrikeResult.ContactServerTime` is the attacker plate's show-time input (HUD F2); the positional matcher remains for the defender and as fallback.
- §2 — user decision 2026-10-25: the latency icon threshold key is `HudLatencyWarnRttSec` only; `InformantKilled` removal-card suppression confirmed; the wrong-target tally confirmed.

User scope decisions (2026-10-25: "accept all recommendations"); decision unchanged in substance, Status unchanged (Accepted).
- §1, L0 leaf — user decision 2026-10-25: the L0 world-anchor leaf also paints Informant Pings' ground rings, edge rings and sender labels (HUD-E22; PNG-R7), beneath the teammate markers, from the own controller's `UTeamPingViewComponent`, with the same projection, clamp and collapse rules as markers; pre-created, no per-ping widget.
- §2 view-models — user decision 2026-10-25: `FHudViewModel` gains the round summary (HUD-E21: two string-table lines filled from `FRoundResult.TeamSummary`, shown with the end banner at F10) and the team feed's reconnecting/back line types (HUD-R10a, from the teammate `bReconnecting` bit). No text on the wire: the numbers arrive as integers and the strings are local.
- §5 — user decision 2026-10-25: `ShowTeamNames` is deferred to VS (TM OQ11); the markers presenter keeps focus labels and adds the "· reconnecting" label suffix (TM-R11).

Closeout 2026-10-25 (re-review follow-ups, `production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- §1 layer table: Informant Pings rings and labels (HUD-E22) are listed in **L0**, and the round summary (HUD-E21) in **L3** (HUD re-review xdoc).
- §1 new "L4 input rule": no input below layer 100 stays absolute; Session status overlays with buttons are split into a display-only L4 part and a Menu-root modal action part pushed through `PushMenu` (resolves interaction-patterns P7 / PAT-CRD-3). Session owns the modal's content and when it is pushed.
- §8 (closeout D8): `HudOwnRemovalHoldCapSec` and `HudResultPlateMaxHoldBackSec` have a TDC hard minimum of 1.25 s; the context-gate function is unchanged. HUD OQ5 exile wording (D11) is a HUD GDD decision with no layer or interface impact.

