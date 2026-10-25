# Story 061: AC 21 PawnRefSameAsSpot

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
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation, Enhanced Input edges and batched owner replies. The Attack payload comes only from Camera's `GetAimRay()`; the server honours a `PawnRef` only if `CanViewerKnow` and `PawnRefAgreementCm` both admit it, otherwise the Attack resolves as a `TargetSpot` with no distinct reply or timing.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH, command path MEDIUM)
**Engine Notes**: ADR-0011 E7 and verification item V3: the server resolves an Attack `PawnRefGuid` through the NetDriver's GUID cache, which resolves any GUID the server assigned including a pawn culled for that connection, so resolution grants nothing and visibility is decided only by the `CanViewerKnow` and `PawnRefAgreementCm` gates. V3 (story 010) reports that behaviour; this Logic test does not depend on it because it uses a weak pawn handle at the stub boundary (the host path already carries a weak handle, V6).

**Control Manifest Rules (this layer)**:
- Required: The Attack payload comes only from Camera's `GetAimRay()`; `PawnRef` is honoured only through `CanViewerKnow` and `PawnRefAgreementCm` — source: ADR-0011, ADR-0015
- Forbidden: Never treat a resolved `PawnRef` as evidence of visibility — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011
- Forbidden: Never add lock-on, snapping, magnetism or aim assist — source: ADR-0011, ADR-0015

This story pins the first half of GDD R10's parity rule: a `PawnRef` is only a hint, so an Attack carrying the reference of a concealed pawn that the attacker may not know about resolves exactly like an Attack at the same spot. If a `PawnRef` could change the outcome, a modified client could probe for concealed pawns by naming them (an x-ray), and a legitimate client's reference would reveal a different resolution for a concealed pawn than for an empty spot. The criterion compares two Attacks, one with the concealed pawn's `PawnRef` and one as a plain `TargetSpot` at the same point, and requires that Assassination receives an identical victim resolution.

It follows story 030 (an Attack is never rejected by the target) because both rely on the attacker-side legality having passed and on the Attack executing as a strike whatever the target. It unlocks story 062 (stale reference falls back to the WorldPoint) and story 063 (the two honouring gates). It relates to GDD R10 and to ADR-0011 sections 3.1 step 4 and 9 (Attack payload).

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 21 (R10, Logic, U `PawnRefSameAsSpot`): **GIVEN** a concealed pawn at X **WHEN** Attack is sent (a) with its PawnRef and (b) as TargetSpot at X **THEN** Assassination receives an identical victim resolution.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.1 (step 4), 9 (Attack payload) and the Implementation Guidelines:*

- Ingress step 4 resolves the `PawnRefGuid` (or, for the host, takes the weak handle directly) to a weak pawn handle; unresolvable means absent. This grants nothing. At stage 2 the server honours `PawnRef` only if `CanViewerKnow(Attacker, Pawn, Position)` is true and the pawn lies within `PawnRefAgreementCm` of `WorldPoint`; otherwise the Attack resolves as `TargetSpot` with no distinct reply or timing.
- The comparison point is the payload delivered to the Assassination stub, not the animation. qa-lead reading (author to confirm): "identical victim resolution" means the command delivered to Assassination has no `PawnRef` and `WorldPoint` X in both runs, the resolved victim is identical, no reply is produced and the event tick offset is the same. Use `CanViewerKnow` stubbed to false for the concealed pawn.
- Edge case from the qa-lead: with `CanViewerKnow` true and 0 cm agreement the resolution must still be identical, through the shared visibility check, with no distinct reply or timing. The concealed pawn is never itself the reason a reply differs.
- Tuning keys read at test time: `PawnRefAgreementCm` 75 (hard 25-150, safe 50-100), `CommandRangeSlackCm` 40; `AssassinationRangeCm` 200 is owned by Assassination.
- Prototype disposition: the prototype `ServerAssassinate(Target, StrikePoint)` names a victim object chosen by the client and `FindAssassinationTarget` uses lock-on-style targeting; both are replaced, not ported (ADR-0011 Migration Plan). This test is new code with no prototype counterpart.
- Test seams: a `CanViewerKnow` stub (cross-epic, Information Visibility Policy's consumer-owned interface), a stub for the shared visibility resolver used by Assassination, and an Assassination recorder that logs the delivered command. These are the named doubles of story 095.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-062-stale-pawnref-falls-back.md`: a `PawnRef` killed earlier in the drain falls back to the `WorldPoint`.
- `production/epics/input-interaction-framework/story-063-pawnref-gate.md`: the `CanViewerKnow` failure and the `PawnRefAgreementCm` distance gate, in detail.
- `production/epics/input-interaction-framework/story-064-no-ally-attack-no-aim-assist.md`: the client-side ally rule and no aim assist.
- `production/epics/input-interaction-framework/story-030-attack-never-rejected-by-target.md`: an Attack is never rejected by the target.
- `production/epics/input-interaction-framework/story-010-spike-v3-netguid-server-resolution.md`: the V3 report on GUID resolution of a culled pawn.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (Attack `PawnRef` honouring gate at the slot)
- `Source/StealthGameplay/Private/Tests/Input/PawnRefSameAsSpotTest.cpp` (class `FInputPawnRefSameAsSpotTest`, name `StealthGame.Input.PawnRefSameAsSpot`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC 21 PawnRefSameAsSpot — PawnRef is equivalent to a spot (`StealthGame.Input.PawnRefSameAsSpot`)
  - Given: a concealed pawn at X; `CanViewerKnow` stub = false for it; a shared visibility resolver stub for Assassination.
  - When: Attack is sent (a) with its PawnRef and WorldPoint = X, (b) as TargetSpot at X.
  - Then: the command delivered to Assassination is identical in both (no PawnRef, WorldPoint X) and the resolved victim is identical; no reply; the same event tick offset.
  - Edge cases: repeat with `CanViewerKnow` = true and 0 cm agreement, still identical resolution via the shared visibility check; no distinct reply or timing.
  - Reading to confirm (qa-lead): "identical victim resolution" is interpreted as the Attack payload delivered to the Assassination stub, with `PawnRef` honoured only through `CanViewerKnow` and `PawnRefAgreementCm`, otherwise stripped to a `TargetSpot` at `WorldPoint`.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/PawnRefSameAsSpotTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-030-attack-never-rejected-by-target.md`; cross-epic: information-visibility-policy (stories not yet written; `CanViewerKnow`), cross-epic: assassination-defence-reversal (stories not yet written; victim resolution)
- Unlocks: `production/epics/input-interaction-framework/story-062-stale-pawnref-falls-back.md`, `production/epics/input-interaction-framework/story-063-pawnref-gate.md`
