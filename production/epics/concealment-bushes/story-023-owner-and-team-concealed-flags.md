# Story 023: Commit and crossing set and clear the owner and ally concealed flags through their only writers

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R2 owner component placement, CON-R11, AC-CON-32)
**Requirement**: `TR-con-001`, `TR-con-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§1 `UConcealmentOwnerViewComponent`, §3), with ADR-0001 (C-TeamView and owner data)
**ADR Decision Summary**: The owner learns its own `IsConcealed` from `UConcealmentOwnerViewComponent`, a replicated component on the owner's PlayerController (Owner audience, `COND_OwnerOnly`); allies learn it from the Roster C-TeamView teammate entry `bIsConcealed`, which Concealment sets only through the Roster server face `SetTeammateConcealed(FParticipantId, bool)` (the Roster stays the only writer of the replicated entry). The flags are set at commit and cleared at the crossing. `BushId` is never replicated.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; replicated sub-object on the PlayerController follows ADR-0001 owner-data family)
**Engine Notes**: The PlayerController is relevant only to its owner, so the owner bool never reaches another connection. The component needs an IVP allowlist row (process, owner sign-off).

**Control Manifest Rules (this layer)**:
- Required: Owner/Team `bIsConcealed` is set at commit and cleared at the crossing; Concealment is the only caller of `SetTeammateConcealed` — source: ADR-0017, ADR-0001
- Forbidden: Never add a concealment field to the pawn's replicated set; never replicate a concealed flag on the pawn — source: ADR-0017
- Guardrail: One owner-only bool on the PlayerController

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (CON-R2, CON-R11, AC-CON-32), scoped to this story:*

- [ ] **AC1** (derived, CON-R2, ADR-0017 §1/§3): **GIVEN** a commit **THEN** `UConcealmentOwnerViewComponent` (replicated, `COND_OwnerOnly`, on the PlayerController) sets `bIsConcealed = true` for the owner only and Roster `SetTeammateConcealed(Participant, true)` is the sole call into the C-TeamView entry; at the crossing both clear; `BushId` is never replicated (an owner derives it from its position and the Manifest).
- [ ] **AC2** (AC-CON-32, [PIE-2] `StealthGame.Concealment.RejoinConcealed`): **GIVEN** a concealed player who rejoins within grace **THEN** its owner component reports `IsConcealed`, the camera enters CS-R11.3, and the packets to every non-ally connection are unchanged.
- [ ] **AC3** (derived, ADR-0017 §3): **GIVEN** an unbound (AI) pawn **THEN** the setters are no-ops (every unbound pawn counts as Active) and the IVP allowlist has exactly the one owner-only row for this component.

---

## Implementation Notes

*Derived from ADR-0017 Decision §1, §3 and CON-R11:*

`UConcealmentOwnerViewComponent` lives in the root module's PlayerController (ADR-0017 placement) with `UPROPERTY(Replicated) bool bIsConcealed`. `SetTeammateConcealed` is called from the Commit and Cross resolver steps of stories 004/005; the Roster's push step writes the replicated entry. For a killed-concealed pawn the clear is at server body removal or participant rebind (story 017). A rejoiner resumes its concealed pawn and learns its state from the owner component (pose is None); the camera reads the owner `IsConcealed` (Camera story-014).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-017-kill-while-concealed-server-rules.md`: the killed-concealed clear timing.
- `story-032-ownership-change-while-concealed.md`: disconnect and release.
- Teammate Markers epic: the hatched marker itself.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentOwnerViewComponent.h` (placement per ADR-0017 §1: the root-module PlayerController)
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentOwnerViewComponent.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/OwnerFlagsTest.cpp` (class `FConcealmentOwnerFlagsTest`)
- `Source/StealthTestHarness/Private/Tests/PIE/RejoinConcealedTest.cpp` (`NETWORK_TEST_CLASS(RejoinConcealed, "StealthGame.Concealment.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: FlagWriters
  - Given: a commit.
  - When: flags are written, then the crossing arrives.
  - Then: the owner component (OwnerOnly) sets bIsConcealed for the owner only; Roster `SetTeammateConcealed` is the sole call into C-TeamView; both clear at the crossing; BushId is never replicated.
- **AC2**: RejoinConcealed (verbatim AC-CON-32)
  - Given: a Concealed player rejoining within grace.
  - When: the owner component is read and non-ally traffic is captured.
  - Then: the owner reports IsConcealed, the camera enters CS-R11.3, and non-ally packets match the pre-rejoin baseline.
- **AC3**: UnboundPawn
  - Given: an unbound (AI) pawn.
  - When: the setters are called.
  - Then: they are no-ops, and the IVP allowlist has exactly the one owner-only row.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `OwnerFlagsTest.cpp` and the PIE `RejoinConcealedTest.cpp` — must exist and pass, OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-003-conceal-state-machine-and-occupancy-record.md`, `story-004-hide-enter-execution-and-commit.md`; Roster `production/epics/participant-state-roster/story-038-owner-view-feed.md` and `story-041-team-view-repush-on-controller-swap.md`
- Unlocks: `story-013-inside-the-bush-stillness-and-view.md`, `story-017-kill-while-concealed-server-rules.md`, `story-032-ownership-change-while-concealed.md`
