# Story 052: AC 39 NoBraceOrTendInBush

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`, `TR-iif-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story covers the bush rule of the action slot: a concealed pawn's `Prepare(Begin)` and `Interact(Begin)` are rejected `NotNow`, while an Attack from the bush executes and breaks concealment. A concealed brace would stack two advantages behind one tell and counter blind attacks; concealed tending would defeat "the objective as a lens" (R9 rationale). In the architecture the rule is not in the command component: the component never learns concealment, so rejection comes from Concealment's registered `ICommandPredicate` for Prepare and Interact, and the Attack's `OnActionStarted(Attack)` is the stimulus Concealment listens to.

It depends on story 033 (the legality matrix and the stage-2 order that calls the predicates). In ADR-0011 it is section 4 (Prepare and Interact inside a bush are rejected `NotNow`) with section 6 (predicates registered weakly per `EActionType`, hub event fired inside the slot). GDD rule: R9 (Prepare and Interact inside a bush).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: One action slot per pawn with fixed priority; Prepare and Interact inside a bush are rejected `NotNow` by the owning feature's predicate (Concealment), registered weakly per `EActionType`, and an Attack from a bush stays legal and breaks concealment through the action hub.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility; input-layer HIGH does not apply. No V1-V6 gate. `ISharedPawnStateView` has no concealed flag (it exposes only `IsRunning`, `IsInTraversal`, `CommandedDisplacementCm`), which confirms that the verdict has to come from Concealment's predicate.

**Control Manifest Rules (this layer)**:
- Required: Feature predicates are registered weakly from the composition root per `EActionType`; a missing predicate resolves `Reject(NotNow)` — ADR-0011
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — ADR-0011
- Required: Action-hub events fire only inside queue slots — ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state (a reply code may use only facts the owner already knows; here the rejection is the predicate's own `NotNow`) — ADR-0011
- Forbidden: Never let `AllowNoEffect` vs `Reject` depend on anything but the site and legality class — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame, inside the 0.5 ms queue line — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 39 (R9, Logic, U `NoBraceOrTendInBush`): **GIVEN** a concealed pawn **WHEN** it issues Prepare(Begin) or Interact(Begin) **THEN** both are rejected `NotNow`; **AND** an Attack from the bush executes and breaks concealment.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.2, 4 and 6:*

- **Who owns the rule.** Concealment owns "no Prepare/Interact in a bush" (confirmed by Concealment change request 1). Stage 2 step 4 calls `ICommandPredicate::CanBegin` and the Concealment predicate returns `Reject(NotNow)`; this surfaces as exactly one `NotNow` reply per Begin, no event, no token (refused before step 5), no replicated change. The IIF verifies that it routes the verdict and voids correctly; it does not implement the concealment test.
- **Attack from the bush.** The Attack predicate (Assassination) allows it; `OnActionStarted(Attack)` fires inside the slot, no reply, and Concealment's hub subscriber observes it in the same slot (the break itself is story 054).
- **Known weakness of the GDD criterion (QL-STORY-READY GAPS).** The component may not know concealment, so a stub predicate makes the test tautological for the IIF. The qa-lead proposes testing the routing: "GIVEN a pawn whose Prepare and Interact predicates (Concealment's) return Reject(NotNow) WHEN Begin is issued THEN one NotNow reply each, no event; AND an Attack from that pawn executes (OnActionStarted(Attack) fires, no reply) and the Concealment stub subscriber observes it." The rule itself is verified in the Concealment epic. The GDD AC text stays the authority.
- **Edge cases.** Hide Exit while concealed remains legal (R7 table); an AI pawn gets the same result with a null sink (0 replies).
- **Test seam.** Predicate stubs (Prepare, Interact = `Reject(NotNow)`; Attack = Allow) and a Concealment stub subscribed to `OnActionStarted`. Named doubles come from story 095.
- **Prototype.** New code, no prototype counterpart (the prototype had no bush rule).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 054 (`story-054-concealment-hears-attack.md`): Concealment breaking concealment on `OnActionStarted(Attack)`.
- Story 033 (`story-033-legality-matrix.md`): the class and locomotion legality matrix.
- Story 035 (`story-035-slot-priority.md`): the slot priority matrix (Hide transitions are atomic).
- Concealment epic (cross-epic): the real `IsInBushOrTransition` test and Hide transition.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (predicate verdict routing for Prepare, Interact, Attack)
- Source/StealthGameplay/Private/Tests/Input/NoBraceOrTendInBushTest.cpp (`FInputNoBraceOrTendInBushTest`, `StealthGame.Input.NoBraceOrTendInBush`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- The owner of the "no Prepare/Interact in a bush" rule is unclear. `ISharedPawnStateView` has no concealed flag and the component may not know concealment, so rejection must come from Concealment's registered predicate; a stub predicate makes the test tautological for the IIF.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "GIVEN a pawn whose Prepare and Interact predicates (Concealment's) return Reject(NotNow) WHEN Begin is issued THEN one NotNow reply each, no event; AND an Attack from that pawn executes (OnActionStarted(Attack) fires, no reply) and the Concealment stub subscriber observes it." Rule itself verified in the Concealment epic.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/NoBraceOrTendInBushTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-033-legality-matrix.md`; cross-epic: concealment (stories not yet written) for the Prepare/Interact predicate and concealed state; cross-epic: round-match-framework (stories not yet written) for the resolution queue
- Unlocks: None
