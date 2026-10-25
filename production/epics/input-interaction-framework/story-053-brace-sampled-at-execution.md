# Story 053: AC 40 BraceSampledAtExecution

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

This story verifies the anti-"reflex game" rule that the defender's brace state is sampled at Attack execution (queue resolution), never during the strike animation. An Attack executes against an unbraced defender; the defender's `Prepare(Begin)` then resolves in a later drain, during the strike animation. The outcome stays the one decided at execution (struck), the late Prepare starts no brace (`OnActionStarted(Prepare)` never fires), and the defender's owner gets exactly one `NotNow`, enqueued at that drain's end. No parry window can exist, because the outcome is decided before any swing is visible.

It depends on story 045 (the brace machinery whose Begin would otherwise start a brace). The decision that the late Prepare is voided belongs to the Assassination predicate (stage 2 step 4, which marks a struck defender non-braceable until the strike resolves); this story names that stage as the voiding stage and verifies it is surfaced through the one reply step. In ADR-0011 it is section 3.2 (steps 3-4) and section 8 (replies computed from facts the owner already knows; no reply code depends on a target pawn's brace state). GDD rules: R10 (brace state sampled at execution) and R6.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Stage 2 runs at the command's queue slot against live state in a fixed order (legality, locomotion, slot, the owning feature's predicate, per-pawn bucket); a voided command receives exactly one reply flushed at the end of that drain's resolution step.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility; the input-layer HIGH does not apply. No V1-V6 gate. This is a U-form test (automation unit test with stubs, no PIE), classified Integration because two systems meet; full fidelity needs the real Assassination epic.

**Control Manifest Rules (this layer)**:
- Required: Feature predicates are registered weakly from the composition root per `EActionType` — ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — ADR-0011
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order — ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — ADR-0011
- Forbidden: Never put owner-only data on the pawn (the late-Prepare reply goes to the owner's sink, no replicated change) — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: at most one reply RPC per connection per drain — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 40 (R10, Integration, U `BraceSampledAtExecution`): **GIVEN** an Attack executed against an unbraced defender **WHEN** the defender's Prepare resolves in a later drain, during the strike animation **THEN** the outcome is the one decided at execution (struck); **AND** the late Prepare is voided with one `NotNow` to the defender's sink.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.2 and 8 and GDD R10:*

- **Voiding stage.** The qa-lead asks that the story name the stage: the late Prepare is voided at stage 2 step 4 (2d), by the Assassination predicate returning `Reject(NotNow)` for a defender who has been struck at execution and is marked non-braceable. The IIF routes the verdict to exactly one `NotNow` reply in the defender's sink, enqueued at that drain's end. The brace state sampled during the Attack slot is Assassination's data, never the IIF's.
- **No distinct timing or code.** The reply code and its timing must not reveal brace state of any pawn to anyone else (`NotNow` goes only to the defender's owner, computed from facts that owner already knows).
- **No replicated change.** The late Prepare writes no `FActionState`, no `FHoldStatus`, no cooldown change, and fires no hub event.
- **Edge cases from the qa-lead.** A Prepare in the same drain after the Attack (AC 45, story 058); an AI defender gets a null sink (0 replies). Full fidelity requires the real Assassination epic (cross-epic, deferred).
- **Known weakness of the GDD criterion (QL-STORY-READY GAPS).** The stage that voids the late Prepare is not named, and the "struck" oracle is Assassination's; with a stub the sampling is tautological for the IIF. The qa-lead's proposed rewrite is in QA Test Cases. The GDD AC text stays the authority.
- **Test seam.** An Assassination stub that records the brace state sampled during the Attack slot, decides `Struck` at execution and marks the defender non-braceable. Named doubles come from story 095.
- **Prototype.** Supersedes the prototype's server-side victim choice in `ServerAssassinate(Target, StrikePoint)` and `ServerSetDefence` (the prototype let the client name a victim and set the hold state); replaced, not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 058 (`story-058-round-queue-order.md`): Prepare and Attack in the same drain, both key orders.
- Story 049 (`story-049-reversal-consumes-brace.md`): the reversal case where the brace was earlier in key order.
- Story 045 (`story-045-brace-window-bounds.md`): normal brace window.
- Assassination epic (cross-epic): the sampling, the `Struck` decision and the non-braceable marking.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (predicate-void routing for a late Prepare)
- Source/StealthGameplay/Private/Tests/Input/BraceSampledAtExecutionTest.cpp (`FInputBraceSampledAtExecutionTest`, `StealthGame.Input.BraceSampledAtExecution`; U-form automation test, not a PIE test)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- The stage that voids the late Prepare is not named (the defender is struck at execution but not yet removed; which check yields `NotNow`?), and the "struck" oracle is Assassination's. With a stub the sampling is tautological.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "GIVEN an Attack executed against an unbraced defender (Assassination stub decides Struck at execution and marks the defender non-braceable) WHEN the defender's Prepare Begin resolves in a later drain before ContactServerTime THEN the outcome stays Struck, no OnActionStarted(Prepare), and exactly one NotNow reply to the defender's sink, enqueued at that drain's end." Name 2d (Assassination predicate) as the voiding stage.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Input/BraceSampledAtExecutionTest.cpp` (U-form automation test `StealthGame.Input.BraceSampledAtExecution`; the AC is U-form so it is not a harness `NETWORK_TEST_CLASS`) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-045-brace-window-bounds.md`; cross-epic: assassination (stories not yet written) for the real Attack resolution and brace sampling; cross-epic: round-match-framework (stories not yet written) for the resolution queue
- Unlocks: None
