# Story 030: AC 11 AttackNeverRejectedByTarget

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-007`, `TR-iif-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story verifies the "no target-dependent rejection" rule of GDD R6 and R10 and ADR-0011 section 8: after attacker-side legality passes (stage 2a to 2e), an `Attack` is never rejected. It always executes as a strike, and the target outcome is Assassination's hit or whiff, identical in content and timing for NPC, player, informant, concealed pawn and empty spot. If an Attack could be rejected because of what stands at the aimed point, the rejection would be an oracle for that target's identity or concealment.

It follows story 027 (the stage 2 resolver) and is the base of the `PawnRef` stories 061, 062 and 063. The IIF side of the contract is that its stage 2 steps never read the target; the victim resolution itself belongs to Assassination (cross-epic).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; after attacker-side legality passes an Attack is never rejected, and no reply code depends on a target pawn's identity, team, concealment or brace state.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story directly. V3 (story 010) covers server NetGUID resolution of a culled pawn, which is what makes naming a concealed pawn grant nothing; this story tests that execution and reply do not vary by target kind.

**Control Manifest Rules (this layer)**:
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — source: ADR-0011
- Required: The Attack payload comes only from Camera's `GetAimRay()`; `PawnRef` is honoured only through `CanViewerKnow` and `PawnRefAgreementCm` — source: ADR-0011, ADR-0015
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011
- Forbidden: Never treat a resolved `PawnRef` as evidence of visibility — source: ADR-0011
- Forbidden: Never add lock-on, snapping, magnetism or aim assist — source: ADR-0011, ADR-0015
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 11 (R6, Logic, U `AttackNeverRejectedByTarget`): **GIVEN** a legal attacker **WHEN** Attack is aimed at an NPC, a player, an informant, a concealed pawn and an empty spot **THEN** each executes as a strike, with no reply code, at the same tick offset.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.2 and 8 and Implementation Guidelines:*

Stage 2 for `Attack`: legality class (2a), locomotion (2b), slot arbitration (2c), Assassination's `CanBegin` predicate (2d, attacker-side facts such as lockout) and the Attack bucket (2e). None of these reads the target. After they pass the Attack executes: `OnActionStarted(Attack)` fires and a Striking pose is written through `WriteActionState`. The `PawnRef` and `WorldPoint` are hints resolved by Assassination with the same authoritative visibility check for both forms, so naming a concealed pawn gains nothing a `TargetSpot` would not (R10, x-ray oracle guard).

**Rationale.** "After attacker-side legality passes, an `Attack` is never rejected" (ADR-0011 section 8) removes the one place a player could probe the world by watching for a rejection; hit or whiff is Assassination's outcome, decided at execution and identical for every target kind.

**GDD references.** Core Rules R6 (last paragraph of the replies section), R10; Edge Cases "Attack and target payloads (anti-oracle)". Tuning Knobs (defaults): `BucketAttackSize` 2, `BucketAttackPerSec` 1.0; `AttackAttemptLockoutSec` (Assassination-owned, GDD default 1.75) and `AssassinationRangeCm` 200 (Assassination-owned).

**Test seam.** Use a fresh legal attacker per target: the bucket, lockout and Striking slot would reject a reused attacker. Targets are stubs (NPC, player, informant, concealed pawn, empty spot); the Assassination predicate is a stub `Allow` that takes no target input. Control case: an Informant attacker gets `NotNow`, never because of the target.

**Prototype superseded.** Supersedes `ServerAssassinate(Target, StrikePoint)` and `FindAssassinationTarget` lock-on-style targeting in the prototype, where the client named the victim and the server could reject by target. Not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-061-pawnref-same-as-spot.md`: identical victim resolution for `PawnRef` versus `TargetSpot` (AC 21)
- `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`: reply and timing independence of nearby-pawn identity over 50 samples
- `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`: the self-ref strip (must happen before this test's legality)
- `production/epics/input-interaction-framework/story-064-no-ally-attack-no-aim-assist.md`: the client-side ally exclusion and no aim assist

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (Attack execution path; no target-dependent branch)
- `Source/StealthGameplay/Private/Tests/Input/AttackNeverRejectedByTargetTest.cpp` (class `FInputAttackNeverRejectedByTargetTest`, name `StealthGame.Input.AttackNeverRejectedByTarget`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Logic — automated test specs (QL-STORY-READY: ADEQUATE):**

- **AC-1**: AC 11 `AttackNeverRejectedByTarget`
  - Given: a fresh legal attacker per target (bucket, lockout and Striking slot would reject a reused attacker); targets: NPC, player, informant, concealed pawn, empty spot.
  - When: Attack is issued at the same tick offset.
  - Then: each executes as a strike (`OnActionStarted(Attack)` fired, Striking pose written), zero replies, same tick offset for all five.
  - Edge cases: self `PawnRef` stripped first (story 020); attacker rejected at stage 2a-2e is still rejected (control: an Informant attacker gets `NotNow`, never because of the target); reply and timing do not vary with target (story 083).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/AttackNeverRejectedByTargetTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-027-stage-two-at-resolution.md`
  - cross-epic: assassination-defence-reversal (Attack predicate and victim resolution; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-061-pawnref-same-as-spot.md`
