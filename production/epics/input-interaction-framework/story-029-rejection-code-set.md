# Story 029: AC 10 RejectionCodeSet

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story verifies the resolution-tier reply behaviour of GDD R6 and ADR-0011 section 8: a command voided at stage 2 for any reason produces exactly one owner reply with a code from {`NotNow`, `Cooldown`, `Busy`, `OutOfReach`}, enqueued only in the end-of-resolution step of the drain that resolved it, and a rejection changes no replicated property (no `FActionState` write, no forced net update). The code mapping is fixed: stages 2a, 2b and a 2e bucket refusal give `NotNow`; 2c gives `Busy`; 2d gives the predicate's code (`Cooldown` for the defence cooldown); an Interact distance or facing failure at Begin gives `OutOfReach`.

It follows stories 004 (the reply sink and batched flush) and 027 (the stage 2 resolver whose failures this test provokes one by one). The reply timing property (never inside a slot, so arrival time never encodes the failure point) is the Pillar 1 timing-side-channel guard; story 083 extends it to 1,000 samples and identical timing across nearby-pawn identities.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; a resolution-voided command gets exactly one reply with a code computed only from facts the owner already knows, appended to the sink's per-pass buffer and flushed once at drain end in one owner-only RPC.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story. Replies leave as `ClientCommandReplies(const TArray<FCommandReply>&)`, a reliable owner-only client RPC, at the end of the pass that produced them; the batched RPC replaces the GDD's per-reply `ClientCommandRejected` (a GDD change request noted in ADR-0011 Related). Replies are never replicated owner-only properties (Alternative 5 rejected).

**Control Manifest Rules (this layer)**:
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection; `Rejected_Latched` replies `NotNow` through the held-reply buffer — source: ADR-0011
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — source: ADR-0011
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state; never deliver replies as replicated owner-only properties — source: ADR-0011
- Forbidden: Never put owner-only data on the pawn — source: ADR-0011
- Guardrail: Command interface: at most one reply RPC per connection per drain — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 10 (R6, Logic, U `RejectionCodeSet`): **GIVEN** a failure at each stage 2a, 2b, 2c, 2d (cooldown), 2e, Interact distance and Interact facing **WHEN** the command resolves **THEN** exactly one reply is enqueued per failure, with codes `NotNow`, `NotNow`, `Busy`, `Cooldown`, `NotNow`, `OutOfReach` and `OutOfReach` respectively; **AND** every reply is enqueued in the end-of-resolution step of that drain; **AND** no replicated property changes.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.2 and 8 and Implementation Guidelines:*

Resolution tier: exactly one reply per command voided at stage 2 for any reason (including "pawn removed earlier in this drain"), with `Code` per the R6 mapping; codes are computed only from facts the owner already knows (own state, own cooldown, own slot, own distance and facing to a site). Every reply is appended to its sink's per-pass buffer and flushed once at the end of the pass that produced it; entries inside a batch are ordered by the queue key of the slot that produced them. Zero appends may happen inside a slot body. A failure at 2d consumes no token (the bucket is stage 2e and runs only after 0-4 pass).

**Rationale.** Computing codes only from owner-known facts and flushing at drain end means rejection timing and content cannot reveal anything about another pawn (GDD R6: "No code ever depends on a target pawn's identity, team, concealment or brace state").

**Code mapping (GDD R6).** 2a, 2b and a 2e bucket refusal: `NotNow`. 2c: `Busy`. 2d: the predicate's code (`Cooldown` for the defence cooldown and for Concealment's re-entry cooldown; `NotNow` for Objectives' full-fixture refusal). A `Rejected_Latched` void: `NotNow` through the held-reply buffer (story 051). Interact distance or facing failure at Begin: `OutOfReach`.

**Author confirmation needed (QL-STORY-READY).** Confirm the stage 2b versus 2c scenario assignment before dev: AC 14 has Traverse while Bracing as `NotNow`, which is a locomotion rule, not slot `Busy`. The qa-lead's scenario assignment: 2a Informant issues Interact; 2b Interact while `IsInTraversal`; 2c Prepare during a Hide transition; 2d Prepare during cooldown; 2e bucket empty (stub Allow); Interact Begin at 190.1 cm; Interact Begin at 61 degrees off facing.

**GDD references.** Core Rules R5 and R6 (code mapping table); F6 for the reach and facing numbers; Edge Cases "If an earlier event in the drain invalidates a later command". Tuning Knobs (defaults): `InteractRangeCm` 150, `CommandRangeSlackCm` 40 (accept 190 cm), `InteractFacingHalfAngleDeg` 60.

**Prototype superseded.** New code, no prototype counterpart (the prototype returned no typed rejection codes).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-051-wrong-phase-commands.md`: latch rejection (`Rejected_Latched` to `NotNow`) and `WrongPhase`
- `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`: 1,000-sample timing and identity-blindness
- `production/epics/input-interaction-framework/story-076-rejection-plate.md`: the owner's "Not now" plate UI
- `production/epics/input-interaction-framework/story-040-interact-hysteresis.md`: the 190.0 versus 190.1 cm and 60 versus 61 degree boundary tests (AC 37)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (reject-code mapping and reply enqueue)
- `Source/StealthFoundation/Public/View/CommandReplyTypes.h` (uses `ECommandRejectCode`, `FCommandReply` from story 001)
- `Source/StealthGameplay/Private/Tests/Input/RejectionCodeSetTest.cpp` (class `FInputRejectionCodeSetTest`, name `StealthGame.Input.RejectionCodeSet`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Logic — automated test specs (QL-STORY-READY: ADEQUATE):**

- **AC-1**: AC 10 `RejectionCodeSet`
  - Given: one failure per stage: 2a Informant issues Interact; 2b Interact while `IsInTraversal`; 2c Prepare during a Hide transition; 2d Prepare during cooldown; 2e bucket empty (stub Allow); Interact Begin at 190.1 cm; Interact Begin at 61 degrees off facing.
  - When: each resolves.
  - Then: exactly 1 reply each with codes `NotNow`, `NotNow`, `Busy`, `Cooldown`, `NotNow`, `OutOfReach`, `OutOfReach`.
  - And: every reply is appended in the drain's end-of-resolution step (0 appends inside a slot).
  - And: no replicated property changes (`FActionState` and Seq unchanged; 0 ForceNetUpdate calls).
  - Edge cases: all seven in one drain yield one batch ordered by key; a failure at 2d consumes no token; latch rejection is story 051.
  - Author note: confirm the stage 2b versus 2c scenario assignment (AC 14 has Traverse while Bracing as `NotNow`, a locomotion rule, not slot `Busy`).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/RejectionCodeSetTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`
  - `production/epics/input-interaction-framework/story-027-stage-two-at-resolution.md`
  - cross-epic: assassination-defence-reversal (Prepare cooldown predicate; stories not yet written)
  - cross-epic: shared-character-locomotion (`IsInTraversal` state; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-051-wrong-phase-commands.md`
  - `production/epics/input-interaction-framework/story-058-round-queue-order.md`
  - `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`
