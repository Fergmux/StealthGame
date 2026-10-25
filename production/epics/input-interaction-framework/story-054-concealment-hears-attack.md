# Story 054: AC 41 ConcealmentHearsAttack

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story proves the action hub is the seam through which Concealment learns that its pawn attacked: when a concealed pawn issues an Attack, `OnActionStarted(Attack)` fires once, Concealment's hub subscriber runs synchronously inside the Attack's own queue slot (same key, before the next queue item), and concealment is broken by the end of that resolution. Concealment is a listener, not a caller: the IIF knows nothing about concealment, and the subscription is the single world-level hub every feature uses (R11).

It depends only on story 003 (the `UActionHub` and the predicate registry), because the event is fired by the hub and the Attack's execution is stubbed. In ADR-0011 it is section 6 (hub events fire only inside queue slots, in key order, native delegates only, "runs the owning feature's follow-ons inline"). GDD rules: R9 (Attack from a bush stays legal and breaks concealment), R11 (generic action events). Real Concealment conformance is verified in the Concealment epic.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: `UActionHub` is a server-only world subsystem with native multicast delegates `OnActionStarted/Cancelled/Completed`, fired only inside queue slots in key order; features subscribe once and run their follow-ons inline in the slot.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility; the input-layer HIGH does not apply. No V1-V6 gate. The hub uses native `DECLARE_MULTICAST_DELEGATE_*` delegates (no dynamic or Blueprint-assignable delegate, no client copy). U-form automation test with stubs.

**Control Manifest Rules (this layer)**:
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — ADR-0011
- Required: Feature predicates are registered weakly from the composition root per `EActionType` — ADR-0011
- Forbidden: Never relay action-hub events to a client except through the Visibility Policy — ADR-0011
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener — ADR-0011
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn — ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case, inside the 0.5 ms queue line — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 41 (Integration, U `ConcealmentHearsAttack`): **GIVEN** a concealed pawn **WHEN** it Attacks **THEN** `OnActionStarted(Attack)` fires; **AND** Concealment breaks concealment in the same resolution.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 6 and Implementation Guidelines:*

- **Mechanism.** The Attack executes inside its slot and emits `OnActionStarted(Attack)` through the one hub. Concealment's handler (a stub here) flips `concealed = false` synchronously inside that call stack, so it completes in the Attack's own slot, before the next queue item, under the Attack's key. The test asserts the handler ran inside the slot (same key) and that concealment is broken by the end of the resolution.
- **No relay.** The handler sends no client RPC (story 067 tests this for every subscriber); the Concealment stub must not either.
- **No reply.** The Attack was legal, so no reply is produced; "After attacker-side legality passes, an Attack is never rejected" (ADR-0011 section 8).
- **Subscriber order.** Subscribers are fired in subscription order from the single hub; the order across subscribers is story 066's concern, not asserted here.
- **Test seam.** A Concealment stub subscribed to the hub that flips `concealed = false` on `OnActionStarted(Attack)`; an Attack predicate stub = Allow. Named doubles come from story 095.
- **Prototype.** New code, no prototype counterpart (the prototype had no hub and no concealment).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 052 (`story-052-no-brace-or-tend-in-bush.md`): rejecting Prepare and Interact while concealed.
- Story 066 (`story-066-event-hub-order.md`): hub event order across pawns and the reflection check that `CancelReason` is on no RPC.
- Story 067 (`story-067-no-event-relay.md`): no client RPC from any hub handler.
- Concealment epic (cross-epic): the real concealment break and the bush transitions.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Public/Server/Core/ActionHub.h (event fired from the Attack slot; hub itself is story 003)
- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (Attack execution emits `OnActionStarted(Attack)`)
- Source/StealthGameplay/Private/Tests/Input/ConcealmentHearsAttackTest.cpp (`FInputConcealmentHearsAttackTest`, `StealthGame.Input.ConcealmentHearsAttack`; U-form automation test)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Logic / Integration story — automated test spec (`StealthGame.Input.ConcealmentHearsAttack`):**

- **AC-1**: Concealment hears the attack
  - Given: concealed pawn (Concealment stub subscribed to the hub, flips `concealed = false` on Attack); Attack predicate Allow.
  - When: the pawn issues Attack.
  - Then: `OnActionStarted(Attack)` fires once; the stub handler runs synchronously inside the Attack's own slot (same key, before the next queue item) and concealment is broken by the end of that resolution.
  - Edge cases: subscriber order via the single hub; no reply; real Concealment conformance is cross-epic.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Input/ConcealmentHearsAttackTest.cpp` (U-form automation test `StealthGame.Input.ConcealmentHearsAttack`) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`; cross-epic: concealment (stories not yet written) for the real hub subscriber; cross-epic: round-match-framework (stories not yet written) for the resolution queue
- Unlocks: None
