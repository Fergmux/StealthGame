# Story 058: AC 45 RoundQueueOrder

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Every interacting state change in a round resolves in one server-side total order: one resolution queue per round keyed `(ServerTickIndex, Seq)`, drained once per frame in `TG_PostPhysics`, with the key assigned when an item is stamped (never on arrival). ADR-0011 builds the pawn command interface on top of it: stage 2 of command validation runs only at the command's queue slot, and replies are flushed once at the end of the drain's resolution step.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH, command path MEDIUM; ADR-0002 MEDIUM)
**Engine Notes**: ADR-0002 drains from a custom `FTickFunction` in `TG_PostPhysics` on the game thread, registered on the authority only. ADR-0011 E10: server RPC bodies run at the top of the world tick, before the tick groups, so a command received this frame is in its mailbox before the drain. This story needs no post-cutoff API, so none of V1-V6 gates it; it does depend on the queue's `Submit` / key-assignment seam, which the round-match-framework epic delivers and the harness stub (story 095) models.

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — source: ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state; never deliver replies as replicated owner-only properties — source: ADR-0011
- Guardrail: at most one reply RPC per connection per drain — source: ADR-0011

This story proves that the order in which a defender's Prepare and an attacker's Attack are resolved is decided by the queue key and nothing else, and that both orderings produce the outcome the GDD specifies. It is the Integration-level check of the R9/R10 interplay: the IIF owns the command slot, Assassination (a cross-epic predicate and resolver) owns the reversal and the strike, and Round's queue owns the key. If the Prepare has the lower key it executes first and Assassination reports a reversal against the incoming Attack; if the Attack has the lower key the defender is struck before its Prepare gets a slot, so the Prepare never starts and the owner is told `NotNow` exactly once, at the end of that drain's resolution step.

It sits late in the epic because it needs the rejection-code story (029: one reply per voided command, enqueued in the end-of-resolution step) and the brace window story (045: the Prepare execution and hold timers it races against). It relates to GDD R9 (one action slot, fixed priority) and R10 (Attack resolution against a braced defender), and to ADR-0011 sections 3.2 (stage 2 order, step 2d is the owning feature's predicate), 4 (slot arbitration) and 8 (one reply step).

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 45 (Integration, U `RoundQueueOrder`): **GIVEN** a defender Prepare and an attacker Attack in one drain **WHEN** the Prepare has the lower key **THEN** a reversal is reported; **WHEN** the Attack has the lower key **THEN** the defender is struck, no `OnActionStarted(Prepare)` fires, and the defender gets exactly one `NotNow`, enqueued at the end of that drain's resolution.

---

## Implementation Notes

*Derived from ADR-0002 Decision sections 1, 5 and 8, and ADR-0011 Implementation Guidelines and Decision sections 3.2, 4 and 8:*

- Keys come from the queue, never from the IIF. `FQueueKey { ServerTickIndex, Seq }` is assigned when an item is stamped in the drain, in ascending order; the test must set up "either command has the lower key" by letting the stub queue assign keys (or by submission order into permuted mailboxes through the stub's controllable permutation), not by giving the IIF a clock. Systems order events only with `Compare` / `GetKey` / `CurrentKey` (ADR-0002 Implementation Guidelines).
- Stage 2 runs only at the command's slot, against live state, in the order 0 pawn valid and control epoch current, 1 legality class, 2 locomotion, 3 slot arbitration, 4 the owning feature's `ICommandPredicate::CanBegin`, 5 token bucket (ADR-0011 3.2). In case 2 (Attack lower key) the defender's `Prepare(Begin)` reaches step 4 after the strike has been executed; the Assassination predicate, which reads live state at that slot, returns `Reject(NotNow)`. The IIF does not read the defender's brace or strike state itself to decide this.
- Reversal path (case 1): `PrepareBegin` executes and emits `OnActionStarted(Prepare)`; the Attack then resolves, Assassination reports a reversal at the contact, and the IIF emits `OnActionCancelled(Prepare, Reversed)` and takes the slot to `Countering` (ADR-0011 section 4; this is the same behaviour story 049 pins in detail, so here only assert that the reversal is reported and the defender is not struck).
- Reply rule: the voided Prepare produces exactly one reply, appended to the sink's per-pass buffer and flushed once at the end of the drain's resolution step (ADR-0011 section 8). An AI defender has a null sink, so it records zero replies; that is the only difference and is decided by the sink, never by a caller check.
- A pawn removed or struck earlier in the same drain is treated as gone by later slots (ADR-0002: resolvers treat actors pending teardown as gone; resolvers never destroy actors inline).
- Tuning keys touched (read from tuning data at test time, never hard-coded): `DefenceDurationSec` 4.0 and `DefenceCooldownSec` 60 (owned by Assassination), `BucketPrepareSize` 2 and `BucketPreparePerSec` 1.0, `BucketAttackSize` 2 and `BucketAttackPerSec` 1.0 (IIF). The test must not depend on the bucket being exhausted.
- Prototype disposition: the prototype resolves `ServerAssassinate(Target, StrikePoint)` and `ServerSetDefence` inline in RPC arrival order in `StealthCharacter.cpp`, with no queue key, no slot and no reply step. They are replaced, not ported; this test has no prototype counterpart.
- Test seams: the stub Round queue (models `Submit` results including `WrongPhase` and `Rejected_Latched`, the due-time heap and Round R12's 1 ms epsilon), an Assassination stub that can report `Reversal` or `Struck`, and a reply-sink recorder are the shared doubles of story 095 (declared as interfaces in story 002). The qa-lead notes that the criterion "a reversal is reported" names no observable; the QA Test Cases section below carries the proposed observable (`OnActionCancelled(Prepare, Reversed)`) pending author confirmation.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-029-rejection-code-set.md`: the rejection code set and the end-of-resolution reply timing in general.
- `production/epics/input-interaction-framework/story-045-brace-window-bounds.md`: brace window bounds and the Prepare hold timers.
- `production/epics/input-interaction-framework/story-049-reversal-consumes-brace.md`: the full reversal contract (`Countering` slot, `FHoldStatus.ServerEndTime = ContactServerTime`, no `OnActionCompleted`).
- `production/epics/input-interaction-framework/story-053-brace-sampled-at-execution.md`: the late Prepare during the strike animation.
- `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`: reply timing across target kinds.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (stage 2 ordering and slot arbitration, shared with earlier stories; changed only if this test exposes a defect)
- `Source/StealthGameplay/Private/Tests/Input/RoundQueueOrderTest.cpp` (class `FInputRoundQueueOrderTest`, name `StealthGame.Input.RoundQueueOrder`)
- `Source/StealthTestHarness/Private/` stub doubles for the Round queue and the Assassination predicate (owned by story 095; consumed here)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Gap 1: "a reversal is reported" names no observable.
- Gap 2: the second case does not say why the defender's Prepare is voided with `NotNow`.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Prepare lower key: `OnActionStarted(Prepare)` fires, then Assassination stub reports Reversal and `OnActionCancelled(Prepare, Reversed)` fires. Attack lower key: defender Struck, no `OnActionStarted(Prepare)`, Prepare voided by 2d (Assassination predicate) with exactly one `NotNow` enqueued at that drain's end."

QA-lead test sketch (unconfirmed): test `StealthGame.Input.RoundQueueOrder`; Given one drain holding defender Prepare and attacker Attack, stub queue assigns keys so either is lower; When Prepare lower, then (new drain) Attack lower; Then case 1 Started(Prepare) and the Reversed cancel, defender not struck, case 2 defender Struck, no Started(Prepare), one `NotNow` to the defender's sink enqueued in that drain's end-of-resolution step. Edge cases: reply counted once; AI defender null sink; keys from the queue, never from IIF.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Input/RoundQueueOrderTest.cpp` (automation test `StealthGame.Input.RoundQueueOrder`, U form, driven by the stub queue) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-029-rejection-code-set.md`, `production/epics/input-interaction-framework/story-045-brace-window-bounds.md`; cross-epic: round-match-framework (stories not yet written; the real queue and its key assignment), cross-epic: assassination-defence-reversal (stories not yet written; the Prepare and Attack predicates and the reversal result)
- Unlocks: None
