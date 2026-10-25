# Story 025: AC 79 PossessionEpochGate

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-013`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story implements the server-side possession-epoch gate at ingress (GDD R5.1, ADR-0011 Decision section 2 and section 3.1 step 2). A controller counts its own possessions in a server-owned `ServerPossessionEpoch`; every command carries `FClientCommandSeq = (PossessionEpoch, Seq)`. A reliable `Interact(Begin)` stamped with epoch N that was still in flight when the controller possessed a new pawn (epoch N+1) must be dropped silently and must never reach the new pawn's mailbox, and a command stamped N+1 with a strictly higher `Seq` must be accepted.

This is the logic-level half of TR-iif-013. The delivery half (that `ClientSetPossessionEpoch` always reaches the owner before `ClientRestart`) is verification item V2 and is reported by spike story 009; the hand-over-window integration test is story 091. This story depends on story 020 (the ingress function) and story 094 (the controller shell that owns `ServerPossessionEpoch`, `OwnerPossessionEpoch` and `PossessedControlEpoch`). It is Ready because it tests the server comparison, not the delivery order.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; the possession epoch is server-owned and delivered owner-only by `ClientSetPossessionEpoch` before the engine's `ClientRestart`, and ingress drops any command whose epoch is not the controller's current one.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: E6: `APlayerController::OnPossess` calls `ClientRestart(Pawn)` once per possession, and the server may re-send it through `ClientRetryClientRestart` or a null-pawn resend, so the owner can see more than one `ClientRestart` per possession; V2 (story 009) verifies the epoch RPC always precedes it. Reliable RPCs on one controller channel arrive in order. This story needs no engine behaviour beyond a uint16 and a uint32 comparison.

**Control Manifest Rules (this layer)**:
- Required: `PossessionEpoch` is server-owned and delivered only by `ClientSetPossessionEpoch` before `ClientRestart`; ingress stamps the controller's recorded `PossessedControlEpoch`, never the pawn's current control epoch — source: ADR-0011
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order — source: ADR-0011
- Required: The listen host's input goes through `IngestCommand(HostLocal)` with the same ingress and reply flush — source: ADR-0011
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener — source: ADR-0011
- Forbidden: Never special-case `NM_ListenServer` in input ingress — source: ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case; ingress is cheap and state-free — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 79 (R5.1, Logic, U `PossessionEpochGate`): **GIVEN** a reliable `Interact(Begin)` stamped with `PossessionEpoch` N that arrives after the controller has possessed a new pawn at epoch N+1 **WHEN** ingress runs **THEN** it is dropped without a reply and never reaches the new pawn's mailbox; **AND** a command stamped N+1 with a strictly higher `Seq` is accepted.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 2 (`FClientCommandSeq`, control epoch at ingress) and section 3.1 step 2:*

`FClientCommandSeq = (PossessionEpoch, Seq)`: `Seq` is strictly increasing per controller; `PossessionEpoch` counts this controller's possessions. `AStealthPlayerController::OnPossess` increments the server-side `ServerPossessionEpoch` and, before the engine's `ClientRestart`, sends the owner-only reliable `ClientSetPossessionEpoch(uint16)`. The two fields (`ServerPossessionEpoch` on the server, `OwnerPossessionEpoch` on the owner) are separate, so the listen host, where both run on one object, never double-counts; no dedupe by pawn is needed, and same-pawn re-possession (Round R17.1, Unresponsive hand-over then the same player) still advances the epoch. It is an RPC, not a replicated property.

Ingress step 2: `PossessionEpoch` equals the controller's `ServerPossessionEpoch` and `Seq` is newer than the last accepted; stale or duplicate is dropped, and an old-epoch command never reaches the new pawn. Distinct from this, ingress stamps the payload with the controller's recorded `PossessedControlEpoch` (the factory's hand-over epoch, ADR-0010 section 4.3-4.4), which is what story 091 and stage 2 step 0 (`StaleControl`) use; do not confuse the two epochs.

**Rationale.** Without an epoch, a reliable `Interact(Begin)` sent to the old pawn and delayed in flight would land on the newly possessed pawn (assassinated pawn to informant respawn), executing a command the player never issued for that body. Making it server-owned and delivering it by RPC adds nothing to the pawn, so no per-pawn property differs between kinds.

**Author note (QL-STORY-READY).** The ADR says `Seq` is strictly increasing per controller across epochs; "strictly higher" here means higher than the last accepted Seq. Confirm there is no `Seq` reset per epoch before `/dev-story`.

**GDD references.** R5.1 (the `ClientCommandSeq` bullet); Edge Cases "If the pawn is unpossessed or re-possessed mid-hold". No tuning keys apply.

**Prototype superseded.** New code, no prototype counterpart.

**Edge cases (from QA review):** `{N+1, 10}` (not higher than last accepted) dropped; `{N+2, any}` (ahead of server) dropped; same-pawn re-possession still advances the epoch; uint16 wrap 65535 to 0; host epoch uses `ServerPossessionEpoch` only.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-009-spike-v2-possession-epoch-order.md`: V2, that `ClientSetPossessionEpoch` always precedes `ClientRestart`
- `production/epics/input-interaction-framework/story-091-hand-over-window-test.md`: the command sent between a hand-over's commit and its possession step, voided `StaleControl`
- `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`: the controller fields and `ClientSetPossessionEpoch` RPC shell
- `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md`: the owner-side input context swap on possession

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/CommandIngress.h` (epoch and Seq gate state)
- `Source/StealthGameplay/Private/Core/CommandIngress.cpp`
- `Source/StealthGameplay/Private/Tests/Input/PossessionEpochGateTest.cpp` (class `FInputPossessionEpochGateTest`, name `StealthGame.Input.PossessionEpochGate`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Logic — automated test specs (QL-STORY-READY: ADEQUATE):**

- **AC-1**: AC 79 `PossessionEpochGate`
  - Given: a controller at `ServerPossessionEpoch` N with last accepted Seq 10; a reliable `Interact(Begin){N, 11}` still in flight; the controller then possesses a new pawn (epoch N+1).
  - When: the N command arrives.
  - Then: dropped, no reply, the new pawn's mailbox unchanged (stub queue spy: 0 submits), drop counter +1. And: command `{N+1, 12}` accepted and submitted to the new pawn's mailbox.
  - Edge cases: `{N+1, 10}` (not higher than last accepted) dropped; `{N+2, any}` (ahead of server) dropped; same-pawn re-possession still advances the epoch; uint16 wrap 65535 to 0 handled; host epoch (`ServerPossessionEpoch` only).
  - Author note: ADR says Seq is strictly increasing per controller across epochs; "strictly higher" means higher than the last accepted. Confirm no Seq reset per epoch.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/PossessionEpochGateTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`
  - `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`
  - cross-epic: shared-character-locomotion (`UStealthPawnFactory::GetControlEpoch` for `PossessedControlEpoch`; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-091-hand-over-window-test.md`
