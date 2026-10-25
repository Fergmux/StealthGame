# Story 006: ServerTeamPing handler, service and host parity

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R4, PNG-R5, PNG-R6, PNG-R9; AC-PNG-09)
**Requirement**: `TR-png-008` (also `TR-png-009` writes and `TR-png-018` handler reads)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline (controller-level RPC outside the pawn-command path); handler placement from ADR-0002 (Technical Setup 2026-10-25) and ADR-0007 (Technical Setup 2026-10-25)
**ADR Decision Summary**: `ServerTeamPing(FNetTeamPing {Point})` is a reliable Server RPC on the sender's own `AStealthPlayerController`, not a pawn command: it never enters `ServerPawnCommand`, the action slot, the resolution queue or `FActionState`, and names no pawn. The Pings service (`UTeamPingService`, a `UWorldSubsystem` in `Public/Server/`, over the headless `FTeamPingCore`) runs the handler at RPC arrival, outside the queue drain, writing only the sender's token bucket and its team's `UTeamPingViewComponent` parts; the composition root forwards the RPC to the service and the listen host uses the same path.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 overall; the RPC and controller path)
**Engine Notes**: A reliable RPC on the PlayerController is bounded by the token bucket so the reliable buffer cannot grow. Use `WithValidation` only for impossible payloads that should disconnect (non-finite points are an ordinary silent drop, not a validation failure, per PNG-R5). The service is subscribed through `UWorldSubsystem`; ordering with the Roster is by `Collection.InitializeDependency<T>()`, never creation order (ADR-0007 §4).

**Control Manifest Rules (this layer)**:
- Required: the `ServerTeamPing` handler runs at RPC arrival, outside the queue drain, and writes only the sender's token bucket and its team's `UTeamPingViewComponent` parts; it reads Round's phase and `IsLatched()` and changes no pawn, Roster, crowd, objective, score or queue state — source: ADR-0002
- Required: Pings live in `StealthGameplay/Features/Pings/` (`UTeamPingService` over `FTeamPingCore`); the composition root declares `ServerTeamPing` on `AStealthPlayerController` and forwards it — source: ADR-0007
- Forbidden: Never route a team ping through the resolution queue, the action slot or a pawn command, and never let the ping handler mutate gameplay state — source: ADR-0002, ADR-0011
- Forbidden: Never answer a dropped `ServerTeamPing` with a reply, client RPC, replicated field or client-visible log row — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-09, PNG-R4, PNG-R5, PNG-R6, PNG-R9), scoped to this story:*

- [ ] **AC1** (AC-PNG-09, `[Integration]` `StealthGame.Pings.PIE.HostParity`): **GIVEN** the listen host and a remote client on the same team **WHEN** the host pings and then the client pings **THEN** both pings pass the same validation and bucket through the same server function, and both machines' ordered ring event lists (timestamps excluded) are identical.
- [ ] **AC2** (PNG-R4): **GIVEN** `ServerTeamPing` is a reliable Server RPC on `AStealthPlayerController` **WHEN** a ping is sent **THEN** it never enters `ServerPawnCommand`, the action slot, the resolution queue or `FActionState` (queue length and slot state unchanged; a static scan finds no Pings type in the queue payloads).
- [ ] **AC3** (PNG-R9, ADR-0002): **GIVEN** the handler **WHEN** it accepts a ping **THEN** it runs at RPC arrival outside the queue drain and writes only the sender's token bucket and the `UTeamPingViewComponent` parts of the sender's team (every member including the sender and informants), reading no pawn other than the sender's own; a spy over Roster, crowd, objectives, score, queue and pawn state sees reads only of Round phase and latch, Roster `State` and team membership, the Concealment predicate and the sender's own pawn, and no write.
- [ ] **AC4** (PNG-R5, PNG-R6): **GIVEN** a 5-member team **WHEN** one member's ping is accepted **THEN** exactly that member's slot changes in every member's part, a disconnected teammate's slot (including its still-aging ring) is untouched by another member's accepted ping, and a dropped ping sends no reply, client RPC, replicated field or client-visible log row.

---

## Implementation Notes

*Derived from ADR-0011 §2, ADR-0002 Technical Setup 2026-10-25 and ADR-0007 Technical Setup 2026-10-25:*

`AStealthPlayerController` (root module `StealthGame`) declares `UFUNCTION(Server, Reliable) void ServerTeamPing(const FNetTeamPing& Ping)` and forwards to `UTeamPingService::HandleTeamPing(Controller, Ping)` (server header in `Public/Server/Features/Pings/StealthTeamPingService.h`, guard include first, `NotBlueprintType`/`NotBlueprintable`). The service resolves `ParticipantId` from the controller through the Roster server face, gathers the plain inputs (phase and latch from Round, sender `State` and bound pawn from the Roster, the sender's eye pivot from the pawn, concealment from the injected predicate), calls `FTeamPingCore` (story 005), and on acceptance writes the entry into slot `i` of every member's `UTeamPingViewComponent` (Roster team slot `i`) in one pass so the per-frame coalescing in story 002 applies. The handler only reads the sender's own pawn (for the eye pivot) and never touches another pawn. Registration with the Roster's reset event and with Round's `ITeamPingReset` is story 007. Host parity (Session rule 3): the host's own controller calls `ServerTeamPing` locally and passes the same code path; no `IsLocalController` branch.

Prototype replaced: the prototype `AStealthCharacter::ServerPing(FVector_NetQuantize)` -> `AStealthGameMode::HandlePing(...)` -> per-controller `ClientReceivePing` fan-out with a cooldown on `AStealthPlayerState`. Do not port any of it; story 027 removes it.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005: validation order, drop reasons, range and bucket spend decisions (this story wires them).
- Story 007: clears at reset, travel and round end; the `ITeamPingReset` registration.
- Story 010: the client's `IA_Ping` binding and sending.
- Story 013 and 014: Pillar 1 leak and pawn-parity proofs.
- Story 024: forwarding statistics to telemetry.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/Server/Features/Pings/StealthTeamPingService.h`
- `Source/StealthGameplay/Private/Features/Pings/StealthTeamPingService.cpp`
- `Source/StealthGame/Public/StealthPlayerController.h` and `Source/StealthGame/Private/StealthPlayerController.cpp` (`ServerTeamPing` declaration and forward)
- `Source/StealthGame/Private/` composition root registration of the service (the server composition root named by tuning-data-config story 003)
- `Source/StealthTestHarness/Private/Pings/HostParityTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(HostParity, "StealthGame.Pings.PIE")`)
- `Source/StealthGameplay/Private/Tests/Pings/PingHandlerWritesOnlyPartsTest.cpp` (class `FPingsHandlerWritesOnlyPartsTest`, name `StealthGame.Pings.HandlerWritesOnlyParts`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 host parity (AC-PNG-09)**
- Given: the listen host and a remote client on one team.
- When: the host pings, then the client pings.
- Then: both pass the same validation and bucket via the same server function; both machines' ordered ring-event lists (timestamps excluded) are identical.
- Edge cases: client then host order; both in one server frame.

**Test: AC2 the RPC is not a pawn command**
- Given: `ServerTeamPing`, a reliable Server RPC on `AStealthPlayerController`.
- When: a ping is accepted and one is dropped.
- Then: `ServerPawnCommand` count, action slot, queue length and `FActionState` unchanged; a static scan finds no Pings type in the queue payloads.
- Edge cases: a ping during a pending pawn command leaves that command untouched.

**Test: AC3 handler scope**
- Given: spies over Roster, crowd, objectives, score, queue and pawn.
- When: a ping is accepted at RPC arrival.
- Then: it runs outside the queue drain; writes only the sender's bucket and the sender's team parts (all members including the sender and informants); no other write; reads match the AC3 list.
- Edge cases: a team of 5 with an informant member.

**Test: AC4 slot isolation and silent drops**
- Given: a 5-member team, one member disconnected.
- When: another member's ping is accepted; then a dropped ping.
- Then: exactly the sender's slot changes in every member's part; the disconnected member's slot is untouched; the drop produces no reply, client RPC, replicated field or client-visible log row.
- Edge cases: the sender is slot 0 and slot 4.

QL-STORY-READY: GAPS (revised) — AC3's read list and AC4's disconnected-teammate wording were corrected as above on the qa-lead's proposals. TR-png-002's "live connection" has no AC of its own (the RPC cannot arrive without a connection).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Pings/HostParityTest.cpp` and `Source/StealthGameplay/Private/Tests/Pings/PingHandlerWritesOnlyPartsTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-002-team-ping-wire-types-and-slot-component.md`, `story-004-ping-token-bucket-core.md`, `story-005-server-validation-core.md`; cross-epic `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`, participant-state-roster epic (Roster server face)
- Unlocks: `production/epics/informant-pings-team-comms/story-007-ping-lifecycle-clears-and-reset.md`, `story-010-ia-ping-action-and-send-path.md`, `story-013-opponent-sees-nothing.md`, `story-014-sender-pawn-parity.md`, `story-015-one-ring-per-sender.md`, `story-022-echo-latency-and-no-local-prediction.md`, `story-024-ping-telemetry-statistics.md`, `story-027-retire-prototype-ping.md`
