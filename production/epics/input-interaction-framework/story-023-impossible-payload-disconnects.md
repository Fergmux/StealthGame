# Story 023: AC 60 ImpossiblePayloadDisconnects

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story covers the one ingress path that is not silent: Unreal's `WithValidation` on `AStealthPlayerController::ServerPawnCommand`. ADR-0011 section 2 says validation "rejects only structurally impossible payloads (enum out of range, non-finite vector); everything else is ingress's silent drop". A `_Validate` failure disconnects the sender, so it must never fire for a merely clamped, stale or duplicate command, or an honest high-latency client would be kicked.

It is an Integration story because the disconnect is a real server `NetConnection` behaviour: it needs the two-client PIE harness (listen server + 2 clients). It depends on the ingress function (story 020) and on the controller transport skeleton (story 094), which owns the `ServerPawnCommand` declaration this story gives a validator to.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; discrete commands travel in one reliable `ServerPawnCommand` RPC whose `WithValidation` rejects only structurally impossible payloads; all other ingress failures are silent drops.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: A `_Validate` returning false closes the sender's connection in the engine; the resulting disconnect log is expected network failure, so the harness must be told to ignore it. This is pre-cutoff RPC machinery re-verified for the command path (MEDIUM). No V1-V6 item gates the story. Host-local ingest of a malformed payload cannot disconnect the host itself: it drops silently.

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand`; `PawnRef` travels as a NetGUID value — source: ADR-0011
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — source: ADR-0011
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn; the client never names the pawn it commands; `PawnRef` is never an object-pointer field — source: ADR-0011
- Forbidden: Never special-case `NM_ListenServer` in input ingress — source: ADR-0011
- Guardrail: Command interface: uplink at most 20 reliable RPCs/s per connection (transport bucket) — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 60 (R5.1, Integration, P `ImpossiblePayloadDisconnects`): **GIVEN** a payload with an out-of-range enum value **WHEN** it arrives **THEN** `WithValidation` fails and the connection is closed; **AND GIVEN** a merely clamped or stale payload **THEN** it is not disconnected.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 2 (Server RPC validation) and Implementation Guidelines:*

Declare `ServerPawnCommand` as `UFUNCTION(Server, Reliable, WithValidation)` (story 094 owns the declaration; this story owns `_Validate`). `_Validate` returns false only for a structurally impossible payload: `ECommandKind` outside its value set, and a non-finite `WorldPoint` (NaN or infinity). Everything else, including a `WorldPoint` 900 cm away (clamped), a stale `Seq` and a stale epoch, passes `_Validate` and is handled by the silent ingress rules of stories 020 and 025.

**Rationale.** A single `_Validate` is the only place a client can be disconnected, which makes the boundary between "dishonest or buggy enough to kick" and "silently ignore" explicit and testable. Disconnecting for clamped or stale payloads would punish honest lag.

**GDD references.** R5.1 (the `WithValidation` sentence in the stage 1 list: "An RPC `WithValidation` failure, which disconnects, is reserved for structurally impossible payloads only"). The enum value set for `ECommandKind` is not fixed by the ADR (QL-STORY-READY note on story 001); use whatever set story 001 confirms, and assert the value just past the last valid one (the Count) is rejected.

**Test form.** Two-client PIE (listen server + 2 clients) with CQTest `NETWORK_TEST_CLASS(ImpossiblePayloadDisconnects, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.ImpossiblePayloadDisconnects.<Method>` (ADR-0008, CQTest exemption in `.claude/rules/test-standards.md`). Client 2 sends the bad payload; client 1 must be unaffected. The harness must ignore the resulting disconnect log (network failure expected). Closed means server connection state closed by the end of the next server tick.

**Prototype superseded.** New code, no prototype counterpart (the prototype RPCs had no validation functions).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`: the silent clamp, strip and drop rules (the "merely clamped" half of this AC's control case is exercised there at logic level)
- `production/epics/input-interaction-framework/story-025-possession-epoch-gate.md`: the stale-epoch silent drop
- `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`: the `ServerPawnCommand` declaration and controller transport shell
- `production/epics/input-interaction-framework/story-024-host-ingress-parity.md`: host-local ingest parity

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGame/StealthPlayerController.cpp` (root module; planned: `ServerPawnCommand_Validate`, extends story 094's skeleton)
- `Source/StealthGameplay/Public/Server/Core/CommandIngress.h` (shared structural-validity helper, planned)
- `Source/StealthTestHarness/Private/ImpossiblePayloadDisconnectsTest.cpp` (CQTest `NETWORK_TEST_CLASS(ImpossiblePayloadDisconnects, "StealthGame.Input.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Integration — automated test specs (QL-STORY-READY: ADEQUATE):**

- **AC-1**: AC 60 `ImpossiblePayloadDisconnects` (P)
  - Given: 2 PIE clients (network failure expected, so the harness ignores the resulting disconnect log).
  - When: client 2 sends `ServerPawnCommand` with `Kind` cast to an out-of-range uint8 (e.g. 255).
  - Then: `_Validate` returns false; the server closes that connection within 1 tick; client 1 is unaffected.
  - Given: a command with a non-finite `WorldPoint` (NaN). Then: same.
  - Given: a clamped (900 cm) command, a stale `Seq` and a stale epoch. Then: the connection stays open; ingress handles them silently.
  - Edge cases: enum value just past the last valid (Count) is rejected; host-local ingest of a malformed payload cannot disconnect itself (drops silently, no crash); no reply sent on disconnect.
  - Note: "closed" = server connection state closed by the end of the next server tick.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/ImpossiblePayloadDisconnectsTest.cpp` (CQTest `NETWORK_TEST_CLASS`, full name `StealthGame.Input.PIE.ImpossiblePayloadDisconnects.<Method>`) OR a playtest doc in `production/qa/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`
  - `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`
  - cross-epic: tuning-data-config (StealthTestHarness module skeleton and ADR-0008 multi-client harness; stories not yet written)
- Unlocks:
  - None
