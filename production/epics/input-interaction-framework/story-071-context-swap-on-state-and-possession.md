# Story 071: AC 55 ContextSwapOnStateAndPossession

> **Epic**: Input & Interaction Framework
> **Status**: Blocked — BLOCKED: spike V2 (story 009) has not reported (V1, story 008, reported PASS 2026-10-25: production/qa/spikes/2026-10-25_V1-enhanced-input-edges-report.md)
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation, Enhanced Input edges and batched owner replies. Mapping contexts are UX only: `IMC_Common` always, `IMC_Active`, and `IMC_Informant`, swapped on the owner's own participant `State` and on possession, with the Informant context selected only by the pure `UseInformantContext` gate (the presented own role, ADR-0021 section 8). Every swap first ends outstanding Begins and sets `bIgnoreAllPressedKeysUntilRelease`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH; ADR-0021 HIGH)
**Engine Notes**: Verification items that gate this story: **V1** (Enhanced Input edge behaviour on this project's contexts: exactly one `Completed` or `Canceled` per release on context add/remove, and a builder flush that yields exactly one End and no re-fired `Started`; E1, E2, E3, E12) and **V2** (`ClientSetPossessionEpoch` always precedes the `ClientRestart` of the same possession on the owner, including the engine's `ClientRetryClientRestart` and null-pawn resend paths, so the owner can see more than one `ClientRestart` per possession; E6). Also relevant: the flush CVar is `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` (default true, marked for removal) and `FModifyContextOptions::bIgnoreAllPressedKeysUntilRelease` covers Boolean actions only; `UEnhancedInputUserSettings` replaces `UPlayerMappableInputConfig`; no Raw Input. ADR-0021 E10: a context added with default options ignores keys already pressed until release, for Boolean actions only.

**Control Manifest Rules (this layer)**:
- Required: Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` to Begin and both `Completed` and `Canceled` to End; server-meaningful actions are Boolean — source: ADR-0011
- Required: Ends are idempotent per Begin; `UPawnCommandClient` sends End for every outstanding Begin before any context swap, menu open, focus loss, possession change or `ClientRestart`; context changes set `bIgnoreAllPressedKeysUntilRelease` — source: ADR-0011
- Required: `PossessionEpoch` is server-owned and delivered only by `ClientSetPossessionEpoch` before `ClientRestart` — source: ADR-0011
- Forbidden: Never use Hold/HoldAndRelease/Tap/Pulse/Chorded triggers for server-meaningful actions; never make a gameplay command a Common UI action — source: ADR-0011
- Forbidden: The Informant input context must follow `PresentedOwnState`, never the raw own `State`; it is selected only by `UseInformantContext` — source: ADR-0021 (Implementation Guidelines; the Core manifest rules do not restate it)

This story verifies the client's input-context contract in a two-client PIE: when the owner's own role changes to Informant, and separately when its pawn is re-possessed, the correct mapping context is active within one frame, every held input has been ended with exactly one End, and the player must press fresh to act. The contexts are a UX convenience (the server validates every command regardless), but a swap that left a hold running, or swapped too early, would either leave a hold extended by a lost End or give the victim of a strike a pre-contact warning by silencing its presses before the blow is presented.

It sits near the end of the chain because it depends on story 013 (the flush sends End on focus loss, menu open, context swap or unpossession) and on the two spike reports it needs (008: V1, 009: V2). Per the plan it is the one story in this slice that is Blocked. It relates to GDD R13 (input contexts), R4 (edges only) and ADR-0011 sections 9 and 2; ADR-0021 section 8 owns the presented-role gate.

**Known GDD defect (recorded, not fixed here): AC 55 vs ADR-0011 section 9.** The AC says the context swaps "within one frame", but ADR-0011 section 9 (amendment 2026-10-24, ADR-0021 section 8) selects `IMC_Informant` only when `UseInformantContext(...)` is true. With a bound HUD presenter the swap may wait up to `HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec` after the raw state change (so the victim gets no pre-contact warning, HUD-R6a). The ADR governs. The one-frame clause is read as "within one frame of `UseInformantContext` becoming true"; the test may alternatively run with no presenter bound.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 55 (R13, Integration, P `ContextSwapOnStateAndPossession`): **GIVEN** an Active player **WHEN** Roster's Owner-audience state changes to Informant, and separately **WHEN** the pawn is re-possessed **THEN** `IMC_Informant` (resp. the class-appropriate context) is active within one frame; **AND** all held inputs are flushed.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 9 (Contexts, The builder owns release safety) and ADR-0021 section 8:*

- Contexts (UX only): `IMC_Common` (Move, Look, Camera toggle, Shoulder swap, Walk slower, Ping, menu) always; `IMC_Active` (Run, Traverse, Interact, Prepare, Attack, Hide); `IMC_Informant` (no gameplay actions; former Active keys show the owner-only plate, story 072). The swap follows the owner's own participant `State` from its C-TeamView owner part (ADR-0001 section 5) and possession.
- The gate: `UseInformantContext(RawOwnState, bPresenterBound, PresentedRole, LocalNow, TStateArrival, CapSec) = (RawOwnState == Informant) AND (NoPresenterBound OR PresentedRole == Informant OR LocalNow >= t_state + HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec)`. It is a pure Core function (`StealthGameplay/Public/View/Core/OwnerContextGate.h`, with a headless spec) and the only selector of `IMC_Informant`. The HUD pushes `PresentedRole` through `IOwnerCommandClientView::SetPresentedOwnRole(EPresentedOwnRole)`, implemented by `UPawnCommandClient`. Until the gate flips the client keeps `IMC_Active` and keeps sending, and the server rejects `NotNow` (replies held to `OnRoleChangePresented`, ASN-R23). A return to `Active` is immediate. Without a bound presenter (headless) the gate reduces to raw state.
- Release safety: `UPawnCommandClient` tracks every outstanding Begin; on a context swap, menu open, focus loss, possession change or `ClientRestart` it sends End for every outstanding Begin before the change is applied, then requires a fresh press. Ends are idempotent: once the builder sent an End for a Begin, any later engine `Completed` or `Canceled` for that press is swallowed. Context changes use `FModifyContextOptions{bIgnoreAllPressedKeysUntilRelease = true}`. The engine's held-key flush (E2) is a second line, not the contract.
- Re-possession: `ClientSetPossessionEpoch(uint16)` reaches the owner before `ClientRestart` (V2); the owner can see more than one `ClientRestart` per possession (E6), so a second `ClientRestart` must not re-fire an End for a Begin already ended or re-add a context twice. Gameplay `IMC_*` contexts are added and removed only by `UPawnCommandClient`; possession never rebinds gameplay input because input lives on the controller.
- Reconciliation the qa-lead proposes for "class-appropriate context" (gap 2, author to confirm): re-possessed into an Active-class pawn gives `IMC_Common + IMC_Active`; into an Informant-state owner gives `IMC_Common + IMC_Informant`.
- Test per the qa-lead: Given an Active player holding Prepare with `IMC_Active` active, When Roster's owner-audience state becomes Informant (presenter unbound, gate true), Then `IMC_Informant` is active within 1 frame of the gate flipping, End was sent for the outstanding Begin before the swap, and no Begin is re-fired. Given the same player holding Prepare, When the pawn is re-possessed (epoch advances), Then the expected context for the pawn class is active within 1 frame and held inputs are flushed (End sent, a fresh press is required). Edge cases: presenter bound and role card pending (context stays Active until the gate, server rejects `NotNow`); return to Active is immediate.
- Prototype disposition: `StealthPlayerController.*` input mode and controller RPCs are superseded; the prototype has no context swap, so the swap logic is new code with no prototype counterpart. Content step: the `IMC_*` and `IA_*` assets are authored in story 007 under `Content/StealthGame/Input`. Before any Content step read `production/never-resave-assets.md` (the Game Animation Sample's input assets and Blueprints carry never-resave hazards; do not resave or edit the sample's own input assets).
- Test seams: a controllable Roster owner-audience state source, an `IOwnerCommandClientView` presenter stub, the server command log and outgoing-RPC spy (story 095), and the controller transport skeleton (story 094).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-013-flush-sends-end.md`: End on focus loss, menu open, context swap and unpossession (AC 5) in general.
- `production/epics/input-interaction-framework/story-072-informant-plate.md`: the owner-only plate shown for caught presses under `IMC_Informant`.
- `production/epics/input-interaction-framework/story-091-hand-over-window-test.md`: commands sent between a hand-over's commit and its possession step.
- `production/epics/input-interaction-framework/story-092-common-ui-mid-brace-single-end.md`: Common UI menu opened mid-brace sends exactly one End.
- `production/epics/input-interaction-framework/story-025-possession-epoch-gate.md`: the ingress epoch gate.
- `production/epics/input-interaction-framework/story-015-default-bindings.md`: the bindings inside each context.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/OwnerContextGate.h` (pure `UseInformantContext`, may be authored with the HUD epic; consumed here)
- `Source/StealthGame/Public/PawnCommandClient.h` and `Source/StealthGame/Private/PawnCommandClient.cpp` (context swap, outstanding-Begin flush, `IOwnerCommandClientView`)
- `Source/StealthGame/Public/StealthPlayerController.h` and `Source/StealthGame/Private/StealthPlayerController.cpp` (possession-change hooks)
- `Source/StealthTestHarness/Private/ContextSwapOnStateAndPossessionPieTest.cpp` (CQTest `NETWORK_TEST_CLASS(ContextSwapOnStateAndPossession, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.ContextSwapOnStateAndPossession.<Method>`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS (also Blocked: V1, V2 per plan)
- Gap 1: conflict with ADR-0011 section 9 (amendment 2026-10-24): `IMC_Informant` is selected only when `UseInformantContext(...)` is true; with a bound HUD presenter the swap may wait up to `HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec` after the raw state change. AC 55 says "within one frame". The author must reconcile: either "within one frame of `UseInformantContext` becoming true" (proposed) or run the test with no presenter bound.
- Gap 2: "class-appropriate context" on re-possession is undefined.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): re-possessed into an Active-class pawn gives `IMC_Common + IMC_Active`; into an Informant-state owner gives `IMC_Common + IMC_Informant`; the swap is "within one frame of `UseInformantContext` becoming true".

QA-lead test sketch (unconfirmed): `StealthGame.Input.PIE.ContextSwapOnStateAndPossession`; Given an Active player holding Prepare with `IMC_Active` active, When the Roster owner-audience state becomes Informant (presenter unbound, gate true), Then `IMC_Informant` is active within 1 frame of the gate flipping, End is sent for the outstanding Begin before the swap, and no Begin is re-fired. Given the same player holding Prepare, When the pawn is re-possessed (epoch advances), Then the expected context for the pawn class is active within 1 frame and held inputs are flushed (End sent, a fresh press is required). Edge cases: presenter bound and role card pending (context stays Active until the gate, server rejects NotNow); return to Active is immediate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/ContextSwapOnStateAndPossessionPieTest.cpp` (CQTest `StealthGame.Input.PIE.ContextSwapOnStateAndPossession`) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-013-flush-sends-end.md`, `production/epics/input-interaction-framework/story-008-spike-v1-enhanced-input-edges.md`, `production/epics/input-interaction-framework/story-009-spike-v2-possession-epoch-order.md`; cross-epic: participant-state-roster (stories not yet written; Owner-audience `State`), cross-epic: hud (stories not yet written; `SetPresentedOwnRole` and the presenter), cross-epic: assassination-defence-reversal (stories not yet written; `OnRoleChangePresented`)
- Unlocks: None
