# Story 063: AC 72 PawnRefGate

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation, Enhanced Input edges and batched owner replies. A `PawnRef` is honoured only if `CanViewerKnow(Attacker, Pawn, Position)` is true and the pawn lies within `PawnRefAgreementCm` of `WorldPoint`; a resolved `PawnRef` is never evidence of visibility.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH, command path MEDIUM)
**Engine Notes**: ADR-0011 E7 (V3, story 010): the server's NetGUID resolution succeeds even for a pawn culled for that connection, which is why the two gates alone must decide. No post-cutoff API is used by this Logic test.

**Control Manifest Rules (this layer)**:
- Required: The Attack payload comes only from Camera's `GetAimRay()`; `PawnRef` is honoured only through `CanViewerKnow` and `PawnRefAgreementCm` — source: ADR-0011, ADR-0015
- Forbidden: Never treat a resolved `PawnRef` as evidence of visibility — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011
- Forbidden: Never add lock-on, snapping, magnetism or aim assist — source: ADR-0011, ADR-0015

This story tests the two admission gates of a `PawnRef` in detail. The first gate is `CanViewerKnow`: the attacker's client may only legitimately be rendering a pawn if the Visibility Policy says the viewer can know about it. The second gate is spatial agreement: the pawn must lie within `PawnRefAgreementCm` (75 cm default) of the Attack's `WorldPoint`, so a forged reference cannot reach a pawn the ray did not hit. If either gate fails, the Attack resolves exactly as a `TargetSpot` at the `WorldPoint`, with the same victim resolution, executed tick offset, reply set (none) and hub events as the reference run. The GDD's AC states the second gate with a 100 cm offset; because the default knob is 75 cm, 100 cm is out of range by design.

It follows story 061, which establishes that a `PawnRef` is equivalent to a spot when the viewer cannot know the pawn, and it unlocks story 064, the two-client check that the client never sends an ally's reference and never applies aim assist. It relates to GDD R10 and ADR-0011 sections 3.1 step 4 and 9.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 72 (R10, Logic, U `PawnRefGate`): **GIVEN** an Attack whose `PawnRef` (a) fails `CanViewerKnow` or (b) lies 100 cm from the `WorldPoint` **WHEN** it resolves **THEN** it resolves exactly as a `TargetSpot` at the `WorldPoint`.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.1 (step 4), 9 (Attack payload) and the Implementation Guidelines:*

- At the Attack slot: honour `PawnRef` only if `CanViewerKnow(Attacker, Pawn, Position)` and `Distance(Pawn, WorldPoint) <= PawnRefAgreementCm`; otherwise treat as `TargetSpot` at `WorldPoint`, with no distinct reply or timing. `CanViewerKnow` is a consumer-owned interface in Core that Information Visibility Policy implements; stub it.
- Do not hard-code 100 cm (qa-lead): the knob's upper safe bound is 100 cm. Derive the offset as `PawnRefAgreementCm + margin`; the qa-lead uses `PawnRefAgreementCm + 25` for case (b). Read `PawnRefAgreementCm` (default 75, hard 25-150, safe 50-100) from tuning at test time.
- Boundary edge cases (qa-lead): distance exactly `PawnRefAgreementCm` is honoured, `+0.1 cm` is ignored. A control case with both gates passing still honours the reference (proves the gate is the rule, not a blanket strip). A stale or unresolvable `PawnRef` is story 062 and is not repeated here.
- The comparison against the reference run (the same Attack sent as `TargetSpot` at the `WorldPoint`) covers four observables: victim resolution, executed tick offset, reply set (none) and hub events.
- Prototype disposition: the prototype `FindAssassinationTarget` lock-on-style targeting and `ServerAssassinate(Target, StrikePoint)` are replaced, not ported. This test has no prototype counterpart.
- Test seams: a `CanViewerKnow` stub with a settable answer, a victim pawn fixture with a settable position, and an Assassination recorder that logs the delivered command and victim (story 095 doubles).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-061-pawnref-same-as-spot.md`: a concealed pawn's `PawnRef` equals a spot.
- `production/epics/input-interaction-framework/story-062-stale-pawnref-falls-back.md`: a stale or unresolvable `PawnRef`.
- `production/epics/input-interaction-framework/story-064-no-ally-attack-no-aim-assist.md`: the client-side rule that an ally's `PawnRef` is never sent.
- `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`: timing identity across target kinds.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (the two `PawnRef` gates at the Attack slot)
- `Source/StealthGameplay/Private/Tests/Input/PawnRefGateTest.cpp` (class `FInputPawnRefGateTest`, name `StealthGame.Input.PawnRefGate`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC 72 PawnRefGate (`StealthGame.Input.PawnRefGate`)
  - Given: an attacker with a stub `CanViewerKnow`; victim pawn V at X; a reference run = the same Attack sent as `TargetSpot` at the WorldPoint.
  - When: (a) the Attack carries V's PawnRef and `CanViewerKnow` returns false; (b) `CanViewerKnow` true but the WorldPoint is `PawnRefAgreementCm + 25` cm from V.
  - Then: in each case the victim resolution, executed tick offset, reply set (none) and hub events are identical to the reference TargetSpot run.
  - Edge cases: distance exactly `PawnRefAgreementCm` (honoured) vs +0.1 cm (ignored); stale or unresolvable PawnRef (covered by 062, not repeated); no distinct reply or timing in either case; control case (gate passes) still honours the ref.
  - Note (qa-lead): 100 cm is the upper safe bound of the knob, so derive the offset as `PawnRefAgreementCm + margin` rather than hard-coding 100.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/PawnRefGateTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-061-pawnref-same-as-spot.md`; cross-epic: information-visibility-policy (stories not yet written; `CanViewerKnow`)
- Unlocks: `production/epics/input-interaction-framework/story-064-no-ally-attack-no-aim-assist.md`
