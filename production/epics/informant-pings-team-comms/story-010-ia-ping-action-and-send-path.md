# Story 010: IA_Ping action, binding and send path

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R3 input paragraph, PNG-R4, PNG-R12 binding label; AC-PNG-06)
**Requirement**: `TR-png-004` (also the client half of `TR-png-008`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline (§9 client input pipeline; user decision 2026-10-25 adding `IA_Ping`), with ADR-0021 §7 (Common UI for menus only)
**ADR Decision Summary**: Gameplay input lives on `AStealthPlayerController::SetupInputComponent` (not the pawn, never Common UI); `IA_Ping` is a Boolean press action in `IMC_Common`, present in the Active and Informant contexts, that sends `ServerTeamPing(FNetTeamPing {Point})` and nothing else; a ping is not a command, so it never ends or alters an outstanding `Begin`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 overall; Enhanced Input and Common UI are unified in 5.8)
**Engine Notes**: Enhanced Input edge facts are ADR-0011 E1-E3, E12; ADR-0011 V1 (edge behaviour on this project's contexts) is covered by `production/epics/input-interaction-framework/story-008-spike-v1-enhanced-input-edges.md`. `IA_Ping` is a single Boolean `Started` press with no Hold, Tap, Pulse or chord trigger, so it does not depend on the flush behaviour that V1 verifies for held actions; do not add `Completed`/`Canceled` handling for it.

**Control Manifest Rules (this layer)**:
- Required: `IA_Ping` (`IMC_Common`) sends only `ServerTeamPing` (point only), reliable; the client sends only when its gate passes — source: ADR-0011
- Required: the `IA_Ping` binding label is the `ST_Controls` entry "Ping"; `IMC_Common` has no chat or voice action — source: ADR-0011 (Technical Setup 2026-10-25b)
- Forbidden: Never route a gameplay input through Common UI; never give a ping a hold, chord or repeat; never send `End` for or interrupt any outstanding pawn command because of a ping — source: ADR-0011, ADR-0021

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-06, PNG-R3, PNG-R12), scoped to this story:*

- [ ] **AC1** (TR-png-004): **GIVEN** the input assets **WHEN** inspected **THEN** `IA_Ping` is one Boolean action with only the default trigger (no Hold, Tap, Pulse or chord) in `IMC_Common` (so present in both the Active and Informant contexts), default keyboard G and gamepad D-pad Up, rebindable, labelled by the `ST_Controls` entry "Ping", and bound in `AStealthPlayerController::SetupInputComponent`, never through Common UI.
- [ ] **AC2** (AC-PNG-06, `[Integration]` `StealthGame.Pings.PIE.InputContexts`): **GIVEN** an Active player and an informant with a body **WHEN** each presses `IA_Ping` aimed at the ground 20 m away **THEN** each sends exactly one `ServerTeamPing` and nothing through `ServerPawnCommand`; **WHEN** the informant presses it while awaiting a takeover (neutral view), or either aims at the sky, **THEN** nothing is sent and nothing is shown.
- [ ] **AC3** (PNG-R3, release safety): **GIVEN** a held Interact or Brace **WHEN** `IA_Ping` is pressed **THEN** the hold is not ended or altered (no `End` sent, the outstanding-`Begin` set unchanged), and holding the key sends one ping (no repeat, hold or chord).

---

## Implementation Notes

*Derived from ADR-0011 §9 and Technical Setup 2026-10-25b:*

Create `IA_Ping` (Boolean, no trigger list or the single default trigger, matching the table pinned in `input-interaction-framework/story-007-input-assets-and-trigger-lint.md`) and map it in `IMC_Common` (G, D-pad Up; `IMC_Common` is already present in the Active and Informant contexts per IIF R13). The mapping asset itself is created by IIF story 007; this story adds the Pings-owned binding row to the default-binding table (`story-015-default-bindings.md`) and the `UPawnCommandClient`-independent handler on the controller: on `Started`, the controller collects the plain owner arguments (round phase from `FRoundPublicState`, `PresentedOwnRole` from the const `UPawnCommandClient::GetPresentedOwnRole()`, `bOwnPawnBound`, Concealment's `IsOwnPawnConcealedForPing()`, the point from story 008, the mirror from story 009), calls `FTeamPingClientGate::Evaluate`, and on `Send` calls `ServerTeamPing` and spends the mirror token; `RefusedConcealed` and `RefusedSilent` follow story 011 and 012. Binding is on the controller, so possession changes never rebind it. The binding label comes from `ST_Controls`; story 026 lints its text.

Prototype replaced: the T-key bind in `AStealthCharacter::SetupPlayerInputComponent` (`BindKey(EKeys::T, IE_Pressed, ...)`). Removed in story 027.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 008 and 009: the point rule, gate and mirror.
- Story 011: the concealed-press feedback.
- Story 012: the strike-window behaviour.
- input-interaction-framework epic: the `IA_*` asset set, the trigger lint and the context swap (`story-007`, `story-015`, `story-071`).
- Story 026: the label text lint.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/StealthPlayerController.h` and `Source/StealthGame/Private/StealthPlayerController.cpp` (`IA_Ping` handler, gate call, send)
- `Content/StealthGame/Input/IA_Ping` and `IMC_Common` mapping row (`.uasset`; only if IIF story 007 has not added them)
- `Source/StealthTestHarness/Private/Pings/InputContextsTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(InputContexts, "StealthGame.Pings.PIE")`)
- `Source/StealthGameplay/Private/Tests/Pings/PingInputAssetLintTest.cpp` (class `FPingsInputAssetLintTest`, name `StealthGame.Pings.InputAssetLint`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 asset and binding inspection**
- Given: the input assets and `AStealthPlayerController::SetupInputComponent`.
- When: inspect `IA_Ping`, `IMC_Common`, the key map and the binding site.
- Then: one Boolean action, default trigger only (no Hold/Tap/Pulse/chord); in `IMC_Common`; keyboard G and gamepad D-pad Up; rebindable; label from `ST_Controls` "Ping"; bound in `SetupInputComponent`, not via Common UI.
- Edge cases: reachable in both the Active and Informant contexts.

**Test: AC2 send path (AC-PNG-06)**
- Given: an Active player and an informant with a body.
- When: each presses `IA_Ping` aimed at the ground 20 m away; then the informant presses while awaiting takeover; then each aims at the sky.
- Then: each sends exactly one `ServerTeamPing` and nothing through `ServerPawnCommand`; the takeover and sky presses send nothing and show nothing.
- Edge cases: a double press within one frame.

**Test: AC3 does not disturb holds**
- Given: a held Interact, then a held Brace.
- When: press `IA_Ping`; then hold the Ping key for several seconds.
- Then: no `End` sent, the outstanding `Begin` set unchanged; holding sends one ping total.
- Edge cases: a Ping press on the same frame as the hold's `Begin`.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Pings/InputContextsTest.cpp` and `Source/StealthGameplay/Private/Tests/Pings/PingInputAssetLintTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-006-server-team-ping-handler-and-host-parity.md`, `story-008-client-ping-point-rule.md`, `story-009-client-gate-and-bucket-mirror.md`; cross-epic `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`, `story-015-default-bindings.md`, `story-094-controller-transport-skeleton.md`
- Unlocks: `production/epics/informant-pings-team-comms/story-011-concealed-press-refusal.md`, `story-012-strike-window-drop.md`, `story-014-sender-pawn-parity.md`, `story-022-echo-latency-and-no-local-prediction.md`, `story-026-ping-language-scope-and-no-feed-lints.md`, `story-027-retire-prototype-ping.md`
