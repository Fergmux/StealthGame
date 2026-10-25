# Story 062: AC 22 StalePawnRefFallsBack

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
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation, Enhanced Input edges and batched owner replies. An Attack's `PawnRefGuid` is resolved at ingress to a weak pawn handle (unresolvable means absent) and re-validated against live state at the command's queue slot; an Attack never carries a distinct reply or timing for a stale reference.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH, command path MEDIUM)
**Engine Notes**: ADR-0011 E7: a NetGUID may be unassigned (assigned lazily) or stale; an unassigned or stale GUID returns null on the server (verification item V3, story 010). The command carries a weak pawn handle from the end of stage 1, so a pawn removed earlier in the same drain resolves to null at the slot; ADR-0002 requires resolvers to treat actors pending teardown as gone and never to destroy actors inline (`RequestTeardown`).

**Control Manifest Rules (this layer)**:
- Required: The Attack payload comes only from Camera's `GetAimRay()`; `PawnRef` is honoured only through `CanViewerKnow` and `PawnRefAgreementCm` — source: ADR-0011, ADR-0015
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011
- Forbidden: Never put `PawnRef` in an object-pointer field; it travels as a NetGUID value — source: ADR-0011

This story is the second half of the `PawnRef`-is-only-a-hint rule: an Attack whose referenced pawn was killed (or otherwise removed) by an earlier-keyed item in the same drain must not fail, must not produce a distinct reply, and must still strike, at the `WorldPoint`. In one drain several slots can resolve in a row; the attacker pressed Attack aiming at a pawn that, by the time the Attack's slot arrives, no longer exists. Falling back to the point keeps the Attack indistinguishable from any other `TargetSpot`, which is what lets Assassination resolve every Attack as a strike with no reply code (AC 11, story 030).

It depends on story 061 (the reference-equals-spot baseline) because that story establishes how a `PawnRef` is converted to a spot at the slot. It relates to GDD R10 ("An invalid, stale, forged, concealed or unreplicated `PawnRef` always falls back to the `WorldPoint`") and to ADR-0011 sections 3.1 step 4 and 9.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 22 (R10, Logic, U `StalePawnRefFallsBack`): **GIVEN** a PawnRef killed earlier in the drain **WHEN** the Attack resolves **THEN** it strikes at the WorldPoint.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.1 (step 4), 3.2 and the Implementation Guidelines:*

- The weak handle established at ingress is re-checked at the slot. If it no longer resolves (pawn killed, removed, pending teardown), the Attack executes as `TargetSpot` at `WorldPoint`: `OnActionStarted(Attack)` fires once, the strike is delivered to Assassination at the point, and no reply is enqueued.
- The qa-lead's assertions: the Attack resolves as a strike at P; `OnActionStarted(Attack)` once; no reply. An absent (unresolvable) NetGUID gives the same result; there is no distinct reply or timing versus a plain `TargetSpot` (AC 47, story 083); a self `PawnRef` is already stripped at ingress (AC 6, story 020).
- Setup: an earlier-keyed item in the same drain kills V; the stub queue orders the kill before the Attack. The kill is a stub Assassination write that makes V's weak handle null (remove or mark pending teardown); do not destroy an actor inline.
- Prototype disposition: the prototype `ServerAssassinate(Target, StrikePoint)` takes a victim object chosen by the client; it is replaced, not ported. New code, no prototype counterpart.
- Test seams: the stub round queue (key control), an Assassination recorder logging the delivered command, and a pawn-handle fixture whose liveness the test can flip (story 095 doubles; story 002 declares the interfaces).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-061-pawnref-same-as-spot.md`: a concealed pawn's `PawnRef` resolves as a spot.
- `production/epics/input-interaction-framework/story-063-pawnref-gate.md`: the `CanViewerKnow` and `PawnRefAgreementCm` gates.
- `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`: stripping a self `PawnRef` at ingress.
- `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`: timing parity of replies across target kinds.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (re-validation of the weak pawn handle at the Attack slot)
- `Source/StealthGameplay/Private/Tests/Input/StalePawnRefFallsBackTest.cpp` (class `FInputStalePawnRefFallsBackTest`, name `StealthGame.Input.StalePawnRefFallsBack`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC 22 StalePawnRefFallsBack — a stale PawnRef falls back to the WorldPoint (`StealthGame.Input.StalePawnRefFallsBack`)
  - Given: an Attack with PawnRef naming pawn V and WorldPoint P; an earlier-keyed item in the same drain kills V (the handle resolves null or the pawn is removed).
  - When: the Attack resolves.
  - Then: it executes as a TargetSpot at P (strike at WorldPoint), `OnActionStarted(Attack)` fires once, no reply.
  - Edge cases: an unresolvable NetGUID (absent) gives the same result; no distinct reply or timing versus a plain TargetSpot (AC 47); a self PawnRef is already stripped at ingress (AC 6).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/StalePawnRefFallsBackTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-061-pawnref-same-as-spot.md`; cross-epic: assassination-defence-reversal (stories not yet written; strike resolution)
- Unlocks: None
