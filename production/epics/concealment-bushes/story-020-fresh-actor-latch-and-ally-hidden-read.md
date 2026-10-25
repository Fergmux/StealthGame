# Story 020: A fresh or late-arriving pawn actor starts hidden, evaluates in PostNetInit and exposes IsAllyBodyHidden

> **Epic**: Concealment (Bushes)
> **Status**: Blocked — BLOCKED: spikes S9 (`production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`) and S10 (`production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`) have not reported; re-open creating a fresh actor that initialises through the latch is what they verify
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R6a, CON-R6b rule 1, AC-CON-08)
**Requirement**: `TR-con-006`, `TR-con-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§5 fresh actor, unresolved ally evidence), with ADR-0003 (S9/S10 re-open behaviour)
**ADR Decision Summary**: Every non-owner pawn actor is created presentation-hidden at construction (before any RepNotify can run); the latch is first evaluated in `PostNetInit` from the full replicated state and clears only if no set rule applies. While any living teammate entry on an ally client has an unresolved `PawnRef`, a fresh actor does not clear; it re-evaluates when the `PawnRef` resolves. `IsAllyBodyHidden(Pawn)` is the public client-side read that Teammate Markers consumes.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E8: `AActor::PostNetInit` runs after the initial replicated properties are applied; RepNotifies from the first bunch may fire before it; cross-actor references (C-TeamView `PawnRef` on the PlayerController) are not resolved there [U, RepNotify ordering]. S9/S10 verify that the close lands at the deadline and a re-open creates a fresh actor.

**Control Manifest Rules (this layer)**:
- Required: Every non-owner pawn actor is created presentation-hidden and evaluated in `PostNetInit`; clips are seeked, never restarted — source: ADR-0017
- Forbidden: Never use `SetActorHiddenInGame`/`bHidden` for concealment — source: ADR-0017
- Guardrail: A fresh non-owner actor must start hidden before any RepNotify and must not clear while a living teammate entry's `PawnRef` is unresolved

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-08, CON-R6a, CON-R6b) and ADR-0017 §5, scoped to this story:*

- [ ] **AC1** (AC-CON-08, [PIE-2] `StealthGame.Concealment.FreshActorLatch`): **GIVEN** a client that creates the pawn (rejoin or re-open) (i) during BushEnter after the swap, (ii) during BushExit before the crossing, (iii) after the crossing time has passed, (iv) while Concealed with pose None on an ally client, (v) after a death with `bDiedConcealed` **THEN** the body is hidden from the first rendered frame in (i), (ii), (iv) and (v) and shows seeked in (iii); GIVEN an ally client watching the whole enter, no body frame is drawn when the pose goes BushEnter → None at +0.9 s.
- [ ] **AC2** (ADR-0017 §5 unresolved ally evidence): **GIVEN** an ally re-open while a teammate is Concealed with pose None and a living teammate entry has an unresolved `PawnRef` **THEN** no body frame is shown, and the latch re-evaluates when the `PawnRef` resolves or the entry changes (the TM-R4a one-update bound).
- [ ] **AC3** (CON-R6a, OQ9): **GIVEN** a hide clip **WHEN** a client creates the actor late under `LeakNetProfile` **THEN** the clip is seeked to ServerNow − Start (never restarted or snapped) and stays within one frame of the server key.
- [ ] **AC4** (derived, ADR-0017 §5): **GIVEN** any client **THEN** `IsAllyBodyHidden(const AStealthPawn*)` (a `Public/View` facade) returns the latch value, for Teammate Markers TM-R5.

---

## Implementation Notes

*Derived from ADR-0017 Decision §5:*

Construct non-owner pawn actors presentation-hidden in `PostInitializeComponents` (before any RepNotify; no RepNotify before `PostNetInit` may clear it). In `PostNetInit` evaluate the latch from the full action state, the death state (`bDiedConcealed`) and, on ally clients, the C-TeamView `IsConcealed` flag; clear only if no set rule applies, so an actor whose exit already completed (pose None, no evidence) shows at once. On an ally client a Concealed teammate's only evidence after its pose returns to None is the C-TeamView flag, reached through a `PawnRef` that cannot resolve before the new actor's `PostNetInit`, so hold hidden until it resolves (the fallback feed covers the marker meanwhile). The single idempotent `RefreshPresentation(EstimatedServerNow())` runs on RepNotify of `FActionState`/`FDeathState`, on C-TeamView change, and each frame while a hide pose is within its key window. Clips are seeked to `Now − Start`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-body-hidden-latch-function.md`: the pure function.
- `story-022-presentation-hide-application-and-host-parity.md`: what hiding applies and the host path.
- Teammate Markers epic: the hatched marker itself.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentComponent.cpp` (`RefreshPresentation`, `PostNetInit`)
- `Source/StealthGameplay/Features/Concealment/Public/View/AllyBodyHidden.h`
- `Source/StealthTestHarness/Private/Tests/PIE/FreshActorLatchTest.cpp` (`NETWORK_TEST_CLASS(FreshActorLatch, "StealthGame.Concealment.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: FreshActorLatch (verbatim AC-CON-08)
  - Given: a client that creates the pawn in cases (i)–(v).
  - When: the first frame is rendered.
  - Then: hidden in (i), (ii), (iv) and (v), shown and seeked in (iii).
  - Edge cases: also the whole-enter ally case (+0.9 s, no body frame).
- **AC2**: UnresolvedAllyEvidence
  - Given: a re-open while an ally is Concealed with pose None and an unresolved PawnRef.
  - When: the client renders, then the PawnRef resolves.
  - Then: no body frame appears, and the latch re-evaluates within one update (TM-R4a).
- **AC3**: SeekedNotRestarted
  - Given: a late actor under `LeakNetProfile`.
  - When: the clip time is sampled.
  - Then: it equals ServerNow minus Start within 1 frame, never restarted or snapped.
- **AC4**: IsAllyBodyHidden
  - Given: any client.
  - When: `IsAllyBodyHidden(Pawn)` is read.
  - Then: it returns the latch value.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the PIE `FreshActorLatchTest.cpp` — must exist and pass, OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-019-body-hidden-latch-function.md`, `story-004-hide-enter-execution-and-commit.md`, `story-023-owner-and-team-concealed-flags.md`; IVP `story-004-spike-s9-cull-latency-and-explicit-channel-close.md` and `story-005-spike-s10-reopen-behaviour-and-netguid.md` (blockers); Roster `production/epics/participant-state-roster/story-042-team-view-pawn-ref-ordering.md`
- Unlocks: `story-026-per-viewer-visibility-pie.md`, `story-028-enter-swap-hides-body-per-viewer.md`, `story-032-ownership-change-while-concealed.md`
