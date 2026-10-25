# Story 001: Command and reply types match the ADR key interfaces

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-001`, `TR-iif-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Every Shared Pawn is driven through one set of server-validated commands: a non-replicated `UPawnCommandComponent` with seven entry points, one owner-to-server transport (CMC saved moves for Move/Look, one reliable PlayerController RPC for discrete commands), two-stage validation, one drain-end reply step. The Key Interfaces section fixes the command and reply value types this story creates.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0011 rates the input layer HIGH and the command path MEDIUM. This story touches only plain `UENUM`/`USTRUCT` value types (pre-cutoff, no post-cutoff API). The relevant engine fact is E7: `PawnRefGuid` is carried as an `FNetworkGUID` value, never an object-pointer field, because the server resolves it through the NetDriver's GUID cache (verification item V3, story 010). No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand`; `PawnRef` travels as a NetGUID value — source: ADR-0011
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family, never on the pawn — source: ADR-0011
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener — source: ADR-0011
- Forbidden: `PawnRef` is never an object-pointer field — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011
- Guardrail: Command interface uplink at most 20 reliable RPCs/s per connection; at most one reply RPC per connection per drain (the value types here must stay small and fixed-width) — source: ADR-0011

This story is the first in the epic and the type foundation for almost every other one. It creates the value types of ADR-0011 Key Interfaces so that later stories (the core skeleton in 002, the hub in 003, the reply flush in 004, the controller transport in 094) all compile against one agreed definition instead of each declaring its own. Nothing here has behaviour; the story proves only that the types have exactly the shape the ADR fixes, so that a later accidental addition (a new reject code, an identity-named field, an object-pointer `PawnRef`) fails a test instead of leaking into the wire format.

It sits first because stories 002, 003, 004 and 094 depend on it. It maps to GDD rules R1 (the entry-point set), R2 (no caller identity), R5.1 (`ClientCommandSeq` and `PossessionEpoch`) and R6 (reply codes `NotNow`, `Cooldown`, `Busy`, `OutOfReach`), and to ADR-0011 sections 1, 2, 6 and 8. Registry entries TR-iif-001 (one component, no caller identity) and TR-iif-007 (exactly one reply per voided command) are the requirements the shapes serve.

---

## Acceptance Criteria

*ADR-derived (no numbered GDD AC). Source: ADR-0011 Key Interfaces (`ECommandRejectCode`, `EHoldOutcome`, `EActionType`, `FClientCommandSeq`, `FHoldStatus`, `FCommandReply`, `FNetPawnCommand`) and §6 (`ECancelReason`), with the fixed enum value sets:*

- [ ] **GIVEN** the reflected `UENUM`s and `USTRUCT`s of the command interface **WHEN** their names, values and fields are enumerated **THEN** `ECommandRejectCode` = {NotNow, Cooldown, Busy, OutOfReach}, `EHoldOutcome` = {Active, Completed, Cancelled}, `EActionType` = {Interact, Prepare, Attack, HideEnter, HideExit} and `ECancelReason` = {Released, Moved, Superseded, Illegal, OutOfRange, TargetInvalid, PawnRemoved, Reversed}, each `uint8` with contiguous values from 0 and an asserted exact count; **AND** `FClientCommandSeq` = {uint16 PossessionEpoch, uint32 Seq}, `FHoldStatus` = {Seq, ActionType, Outcome, double ServerStartTime, double ServerEndTime} with no cancel-reason field, `FCommandReply` = {Kind, Seq, Code, Hold} and `FNetPawnCommand` = {Kind, Seq, Args}, where `Args` holds only ids and quantised values (`FCourtFixtureId`, `BushId`, `SideIndex`, `FVector_NetQuantize10` WorldPoint, `FNetworkGUID` value `PawnRefGuid`, the `TraversalPointHint` id, the CMC client timestamp); **AND** no UObject-pointer field and no `ECancelReason` field exists in any reply or command struct.

---

## Implementation Notes

*Derived from ADR-0011 Key Interfaces, Decision §2, §6, §7, §8 and Implementation Guidelines:*

The reply structs live in `StealthFoundation/Public/View` "so Presentation reads them without including the root PlayerController" (ADR-0011 §7). `FNetPawnCommand { ECommandKind Kind; FClientCommandSeq Seq; FNetCommandArgs Args; }` carries "only ids and quantised values"; the client never names the pawn it commands (§2). `FClientCommandSeq = (PossessionEpoch, Seq)`: `Seq` is strictly increasing per controller; `PossessionEpoch` counts the controller's possessions and is server-owned (§2; behaviour belongs to stories 025 and 094, not here). `FHoldStatus` has "no cancel reason" by design (§7): hold cancellation is told to observers only by the pose ending, and the owner never learns a cause, which keeps the reply channel identity-blind. `FCommandReply` is a tagged entry (`Rejected | HoldStatus | StrikeResult | DefenceResult`); `FStrikeResult` and `FDefenceResult` are ADR-0018 types owned by the assassination epic, so this story reserves the tag values and leaves their payload fields to that epic.

GDD rules: R1 (entry-point list, Core Rules lines 40-55), R2 (no caller identity, line 56), R5.1 (ingress and epoch, line ~70), R6 (rejection codes, line ~90). Read them by line range, not whole.

**Known gaps from QL-STORY-READY (do not paper over them):**
- The ADR fixes no value set for `ECommandKind` (`FNetPawnCommand.Kind`) or for the `FCommandReply.Kind` tag (the ADR gives a comment only). The qa-lead proposal, to be confirmed by the author at implementation: `ECommandKind = {Traverse, InteractBegin, InteractEnd, PrepareBegin, PrepareEnd, Attack, HideEnter, HideExit}` (the R1 list minus Move and Look, which ride `ServerMove`) and `FCommandReply.Kind = {Rejected, HoldStatus, StrikeResult, DefenceResult}`. Record the confirmed sets in the header doc comment.
- `ECancelReason` is declared with the action hub in ADR-0011 §6 (StealthGameplay), not in Foundation. The plan lists it here, so this story declares the enum in a StealthGameplay header (`Source/StealthGameplay/Public/Server/Core/`) and story 003 includes it. The reply structs deliberately contain no `ECancelReason`.
- ADR-0011 does not say where `FNetPawnCommand` lives. `IngestCommand` (StealthGameplay) and the controller RPC (root module) both need it, so the planned home is Foundation `Public/Server`; confirm at implementation.

Prototype: new code, no prototype counterpart (the prototype names a victim object in `ServerAssassinate(Target, StrikePoint)`; this type set replaces that shape). Tuning keys: none (value types only).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-002-pawn-command-core-skeleton.md`: the `FPawnCommandCore` class that uses these types.
- `story-003-action-hub-and-predicate-registry.md`: `UActionHub` and its delegates (this story only declares the enums it uses).
- `story-004-reply-sink-and-drain-end-flush.md`: reply buffering and the batched flush.
- `story-006-no-caller-identity-parameter.md`: the identity-name checker applied to the same structs (AC 2); this story only asserts the shape.
- `story-094-controller-transport-skeleton.md`: the `ServerPawnCommand` / `ClientCommandReplies` RPC declarations that carry these structs.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Public/View/CommandReplyTypes.h` (reply enums and structs)
- `Source/StealthFoundation/Public/Server/NetPawnCommand.h` (`ECommandKind`, `FNetCommandArgs`, `FNetPawnCommand`; location to confirm)
- `Source/StealthGameplay/Public/Server/Core/CancelReason.h` (`ECancelReason`)
- `Source/StealthGameplay/Private/Tests/Input/CommandReplyTypesTest.cpp` (class `FInputCommandReplyTypesTest`, name `StealthGame.Input.CommandReplyTypes`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- ADR Key Interfaces fixes no value set for `ECommandKind` (`FNetPawnCommand.Kind`) or `FCommandReply.Kind` (comment only), so "fixed enum value sets" cannot be asserted for them. `ECancelReason` lives in `UActionHub` (StealthGameplay, ADR 6), not Foundation.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): `ECommandKind = {Traverse, InteractBegin, InteractEnd, PrepareBegin, PrepareEnd, Attack, HideEnter, HideExit}`; `FCommandReply.Kind` = enum `{Rejected, HoldStatus, StrikeResult, DefenceResult}`.
- Scenario sketch from the qa file (`CommandReplyTypes`, derived, not an AC scenario): enumerate reflected enums and assert exact names, contiguous values and counts; list struct fields and assert the shapes above; assert no UObject-pointer or `ECancelReason` field and no identity-named field (shares its checker with story 006); NetSerialize round trip of each struct is value-equal; Seq 0 and uint16/uint32 maxima survive; an invalid `PawnRefGuid` round-trips as invalid.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/CommandReplyTypesTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (cross-epic: tuning-data-config module skeleton, stories not yet written; ADR-0007 modules StealthFoundation and StealthGameplay must exist)
- Unlocks: `production/epics/input-interaction-framework/story-002-pawn-command-core-skeleton.md`, `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`, `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`, `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`
