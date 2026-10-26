# Story 094: AStealthPlayerController carries the command transport and the client builder shell

> **Epic**: Input & Interaction Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-003`, `TR-iif-004`, `TR-iif-013`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Discrete commands travel in one reliable Server RPC on `AStealthPlayerController`, `ServerPawnCommand(const FNetPawnCommand&)`, with `WithValidation` rejecting only structurally impossible payloads; replies return in one reliable owner-only `ClientCommandReplies`; the server-owned `PossessionEpoch` is delivered by `ClientSetPossessionEpoch` before `ClientRestart`; gameplay input is bound on the controller through `UEnhancedInputComponent`, with a client-only `UPawnCommandClient` tracking outstanding Begins.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Input layer HIGH, command path MEDIUM. Engine facts: E9 `APlayerController::SetupInputComponent` and `AController::OnRep_Pawn` are virtual (`PlayerController.h:2011`, `Controller.h:182`); E6 `OnPossess` calls `ClientRestart(Pawn)` once per possession and the engine may resend it; E10 server RPC bodies run during the NetDriver's receive at the top of the world tick, before the tick groups. Verification items V1 (edges), V2 (epoch order, story 009) and V5 (Common UI) test behaviour on this skeleton; this story only declares and wires it. No V item gates writing the skeleton, but stories that depend on V1/V2/V5 behaviour stay Blocked.

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand`; `PawnRef` travels as a NetGUID value — source: ADR-0011
- Required: `PossessionEpoch` is server-owned and delivered only by `ClientSetPossessionEpoch` before `ClientRestart`; ingress stamps the controller's recorded `PossessedControlEpoch`, never the pawn's current control epoch — source: ADR-0011
- Required: Gameplay input is bound on `AStealthPlayerController`; held commands map `Started` to Begin and both `Completed` and `Canceled` to End; server-meaningful actions are Boolean — source: ADR-0011
- Required: Owner-only data lives on the PlayerController carrier family, never on the pawn — source: ADR-0011
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn; the client never names the pawn it commands — source: ADR-0011
- Forbidden: Never special-case `NM_ListenServer` in input ingress — source: ADR-0011
- Forbidden: Never use `UPlayerMappableInputConfig` or the Raw Input plugin; never make a gameplay command a Common UI action — source: ADR-0011
- Guardrail: Uplink at most 20 reliable RPCs per second per connection (transport bucket); at most one reply RPC per connection per drain — source: ADR-0011

This story was added after the QA-lead pass to close QL-STORY-READY systemic gap 3: no other story owns the controller-side transport and builder shell that the input stories (012, 013, 015, 018) and the ingress stories (020, 023, 025) all assume. It builds the skeleton of `AStealthPlayerController`: input setup binding the `IA_*` actions through `UEnhancedInputComponent`, the reliable validated server RPC for discrete commands, the two reliable client RPCs (replies and the epoch), the epoch fields, and a client-only `UPawnCommandClient` component that tracks outstanding Begins. It is a skeleton: the edge semantics (012), flush on swap/menu/focus (013), ingress behaviour (020-025) and reply flushing (004) fill it in.

It depends on story 001 (the `FNetPawnCommand` and reply types the RPCs carry) and 007 (the `IA_*` assets it binds). It sits ahead of the stories that need a command transport to exist. The plan notes the module layout (ADR-0007) is created by the tuning-data-config bootstrap epic; the root-module controller is cross-epic with the camera epic (ADR-0015 `bUseClientSideCameraUpdates = false`, `bAutoManageActiveCameraTarget = false` on the controller) and the session/lobby epics, which also touch `AStealthPlayerController`. This story adds only the IIF's part.

---

## Acceptance Criteria

*ADR-derived (no numbered GDD AC). Source: ADR-0011 Key Interfaces (the controller transport comment block) and Decision §2 and §9; criterion text from the plan (QL-STORY-READY systemic gap 3):*

- [ ] **GIVEN** `AStealthPlayerController` and a possessed `AStealthPawn` **WHEN** `SetupInputComponent` runs and the class is reflected **THEN** it binds the `IA_*` actions through `UEnhancedInputComponent`; `ServerPawnCommand(FNetPawnCommand)` is a reliable `WithValidation` Server RPC; `ClientCommandReplies` and `ClientSetPossessionEpoch` exist as reliable Client RPCs; `UPawnCommandClient` exists as a client-only component tracking outstanding Begins; and no gameplay RPC exists on `AStealthPawn` or its components.

---

## Implementation Notes

*Derived from ADR-0011 Decision §2, §9, Key Interfaces and Implementation Guidelines:*

Key Interfaces (root module): `UFUNCTION(Server, Reliable, WithValidation) void ServerPawnCommand(const FNetPawnCommand& Cmd);` `UFUNCTION(Client, Reliable) void ClientCommandReplies(const TArray<FCommandReply>& Replies);` `virtual void SetupInputComponent() override;` (Enhanced Input; `UPawnCommandClient` builder) `UFUNCTION(Client, Reliable) void ClientSetPossessionEpoch(uint16 Epoch);` (sent in `OnPossess` before `ClientRestart`) and the fields `uint16 ServerPossessionEpoch` (server), `OwnerPossessionEpoch` (owner) and `uint32 PossessedControlEpoch` (server, from the factory). "Server RPC validation (`WithValidation`) rejects only structurally impossible payloads (enum out of range, non-finite vector); everything else is ingress's silent drop." "The client never names the pawn it commands; the server resolves the controller's own possessed pawn." "Gameplay input lives on `AStealthPlayerController`, not the pawn: `SetupInputComponent` (E9) binds an `UEnhancedInputComponent` to the IIF actions (`IA_Move`, `IA_Look`, `IA_Run`, `IA_Traverse`, `IA_Interact`, `IA_Prepare`, `IA_Attack`, `IA_Hide`, `IA_CameraToggle`, `IA_ShoulderSwap`, `IA_WalkSlower`, `IA_Ping`). The pawn's `SetupPlayerInputComponent` binds nothing gameplay-relevant." Possession changes therefore never rebind gameplay input.

Scope split with other stories: this story declares the RPCs, fields and the `UPawnCommandClient` shell (outstanding-Begin table, no behaviour). The `ServerPawnCommand` body calls `IngestCommand` (stage 1, story 020); the epoch fields' update logic and the gate are story 025 and the V2 spike (009); the reply buffering is story 004; `IA_Ping` binds to `ServerTeamPing`, whose handler belongs to the informant-pings epic (cross-epic, stories not yet written); this story only binds the action and declares nothing that routes a ping through `ServerPawnCommand` (forbidden). The controller must also host the listen host's `IngestCommand(HostLocal)` path without an `NM_ListenServer` branch (story 024).

Static-scan note: the criterion that no gameplay `UFUNCTION(Server|Client|NetMulticast)` exists on `AStealthPawn` or its components is verified here by a reflection check and covered more broadly by story 093.

Prototype replaced: `StealthPlayerController.*` (input mode, controller RPCs) and `StealthCharacter.cpp` `SetupPlayerInputComponent` and the pawn-side Server RPCs `ServerAssassinate`, `ServerSetDefence`, `ServerSetInteracting`, `ServerPing` are superseded. The production controller is authored fresh (the prototype body is not ported) and may reuse the file path `Source/StealthGame/Public/StealthPlayerController.h`. Tuning keys: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-edges-not-repeat.md`: Begin/End edge handling on top of this shell.
- `story-013-flush-sends-end.md`: End on context swap, menu, focus loss, possession change.
- `story-020-ingress-clamp-and-drop.md`: the `IngestCommand` stage-1 body that `ServerPawnCommand` calls.
- `story-023-impossible-payload-disconnects.md`: `WithValidation` behaviour on impossible payloads.
- `story-025-possession-epoch-gate.md` and `story-009-spike-v2-possession-epoch-order.md`: the epoch gate and its ordering spike.
- `story-004-reply-sink-and-drain-end-flush.md`: reply buffering and flush that call `ClientCommandReplies`.
- `story-095-command-test-seams.md`: the harness seams (command log, RPC spy) that observe this transport.
- Informant-pings epic (cross-epic): the `ServerTeamPing` handler.

---

## Files

*The code and test files this story changed (written by /dev-story; /story-done checks scope against it).*

- `Source/StealthGame/Public/StealthPlayerController.h`
- `Source/StealthGame/Private/StealthPlayerController.cpp`
- `Source/StealthGameplay/Public/View/Input/PawnCommandClient.h`
- `Source/StealthGameplay/Private/View/Input/PawnCommandClient.cpp`
- `Source/StealthGameplay/Public/Server/Core/CommandIngress.h`
- `Source/StealthGameplay/Private/Core/CommandIngress.cpp`
- `Source/StealthGame/Public/StealthCharacter.h`
- `Source/StealthGame/Private/StealthCharacter.cpp`
- `Source/StealthGame/Public/StealthSettings.h`
- `Source/StealthGame/Private/StealthHUD.cpp`
- `Source/StealthGame/Private/Composition/StealthPrototypeCommandQueue.h`
- `Source/StealthGame/Private/Composition/StealthPrototypeCommandQueue.cpp`
- `Source/StealthGame/Public/Server/StealthCompositionRoot.h`
- `Source/StealthGame/Private/Composition/StealthCompositionRoot.cpp`
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp`
- `Source/StealthGameplay/Public/View/Core/StealthPawn.h`
- `Source/StealthGameplay/Private/Tests/Locomotion/PawnClassDefaultsTest.cpp`
- `Source/StealthGameEditor/Private/Input/InputAssetSpec.h`
- `Source/StealthGameEditor/Private/Input/InputAssetAuthoring.cpp`
- `Source/StealthGameEditor/Private/Input/InputAssetLint.cpp`
- `Content/StealthGame/Input/IA_Run.uasset`
- `Content/StealthGame/Input/IA_Traverse.uasset`
- `Content/StealthGame/Input/IMC_Common.uasset`
- `Source/StealthGameplay/StealthGameplay.Build.cs`
- `Source/StealthTestHarness/StealthTestHarness.Build.cs`
- `Source/StealthTestHarness/Private/Tests/Input/ControllerTransportSkeletonTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: NOT ASSESSED — story added after the review

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/ControllerTransportSkeletonTest.cpp` — must exist and pass (reflection check on the RPC flags and component, plus a PIE check that `SetupInputComponent` binds the actions), or a documented playtest

**Status**: [x] Created and passing (`StealthGame.Input.ControllerTransportSkeleton`, `StealthGame.Input.PIE.ControllerTransportSkeleton.ClientBindsTheGameplayActionsOnTheController`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-001-command-reply-types.md`, `production/epics/input-interaction-framework/story-007-input-assets-and-trigger-lint.md`; cross-epic: tuning-data-config (stories not yet written; module skeleton and StealthTestHarness); cross-epic: camera-system (stories not yet written; controller camera flags, ADR-0015)
- Unlocks: `production/epics/input-interaction-framework/story-012-edges-not-repeat.md`, `production/epics/input-interaction-framework/story-013-flush-sends-end.md`, `production/epics/input-interaction-framework/story-015-default-bindings.md`, `production/epics/input-interaction-framework/story-018-walk-slower-two-client.md`, `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`, `production/epics/input-interaction-framework/story-023-impossible-payload-disconnects.md`, `production/epics/input-interaction-framework/story-025-possession-epoch-gate.md`, `production/epics/input-interaction-framework/story-095-command-test-seams.md`

---

## Completion Notes
**Completed**: 2026-10-26 (IIF batch 13: command/reply types, core, hub, reply step, transport, seams, spikes V2/V3/V5, input ACs; D53 integration of the production input path)
**Criteria**: 1/1 passing (SetupInputComponent binds the twelve IA_* actions through UEnhancedInputComponent; ServerPawnCommand is reliable + WithValidation; ClientCommandReplies and ClientSetPossessionEpoch are reliable Client RPCs; UPawnCommandClient is a non-replicated controller subobject with the outstanding-Begin table; no RPC declared on AStealthPawn or its project components; in PIE the epoch is delivered and every connection has a reply sink)
**Deviations**: UPawnCommandClient lives in StealthGameplay Public/View/Input (not StealthGame/Public as planned): ADR-0007 rule (a) forbids loose Public headers in the root, the Gameplay unit tests need it, and View/Core is the class-parity scan root. IngestCommand is a stage-1 skeleton (own-pawn resolution, PawnRef GUID resolution with the V3 guards, command-log seam, FScopedCommandOrigin); its context also carries the recorded control epoch and the host's weak PawnRef. PossessedControlEpoch records 0 until ADR-0010 4.4. D53 integration: the prototype pawn's legacy key binds and its four Server RPCs are retired; the composition root wires an immediate prototype queue adapter (until ADR-0002's facade); IA_Run/IA_Traverse are pass-through (bConsumeInput false) so the reparented sample keeps sprint and traversal; the contexts sit above IMC_Sandbox; AStealthPawn::Restart now gives a never-moved body its default movement mode (D53 regression: factory pawns were MOVE_None); ServerTeamPing replaces the pawn ping RPC; a [cmd] dev HUD line and the StealthInputKey dev exec. Known limit: keyboard vs stick is told by WASD key state until rebinding lands.
**Test Evidence**: `Source/StealthTestHarness/Private/Tests/Input/ControllerTransportSkeletonTest.cpp`; `production/qa/evidence/story-094-controller-transport-skeleton/` (evidence.md, 4 screenshots); full suite `production/qa/evidence/iif-command-path-2026-10-26/full-suite.txt`
Run result: OBSERVED — 3-player PIE: client 1 walks with W + Ctrl at move/pace 0.80, Q sends one ServerPawnCommand and the server's defence rule runs, the host walks with W + Alt at 0.90 (production/qa/evidence/story-094-controller-transport-skeleton/01..04)
**Reviewed by**: lead-programmer focused review (sonnet; 3 medium + 6 low findings, all fixed except the documented limits) + self-review, IIF batch 13
