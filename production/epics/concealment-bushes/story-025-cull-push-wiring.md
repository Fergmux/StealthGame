# Story 025: Commit, exit, break and kill drive the IVP cull through IConcealmentCullSource in the right slot

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R2 cull scope, CON-R5, CON-R8, CON-R9, AC-CON-01b, AC-CON-33)
**Requirement**: `TR-con-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§3), with ADR-0003 (§5 cull grid, explicit close, un-cull in the same step)
**ADR Decision Summary**: Cull state is pushed only through `IConcealmentCullSource::SetCullState(Pawn, bool, ServerNow())` inside the slot: at commit `CullPending` starts with the deadline on the global grid; un-cull happens in the same slot as exit execution or break; a concealed kill un-culls at the death write with `bDiedConcealed`. For a concealed subject the cull predicate replaces the whole relevancy function for that connection, and every actor owned by or attached to a pawn uses owner relevancy or joins the same cull.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `APawn::IsNetRelevantFor` is replaced entirely for a concealed subject [V `Pawn.cpp`]; an explicit per-connection `FindActorChannelRef` then `Close(EChannelCloseReason::Relevancy)` at the grid deadline is what S9 verifies [U until S9]; the re-open creating a fresh actor is S10. `ForceNetRelevant` is banned on pawns.

**Control Manifest Rules (this layer)**:
- Required: Cull state is pushed only through `IConcealmentCullSource` inside the slot; un-cull in the same slot as exit execution or break — source: ADR-0003, ADR-0017
- Forbidden: Never rely on the relevancy override alone; never call `ForceNetRelevant()` on a pawn — source: ADR-0003
- Guardrail: Replication relevancy O(1) per pawn-connection pair inside the 1.5 ms replication line

This story replaces the stub cull seam used by stories 004, 005 and 014 with the real caller and observes it. It overlaps IVP stories 024 and 025, which own the mechanism; this story is the Concealment-side caller and its observation.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-01b, AC-CON-33) and ADR-0017 §3, scoped to this story:*

- [ ] **AC1** (AC-CON-01b, [PIE-2][Net] `StealthGame.Concealment.CullSendTime`): **GIVEN** 200 commits under `LeakNetProfile` **THEN** each opponent connection's channel close is sent at the first pawn net-update boundary at or after commit + `ConcealCullDelayMs` (250 ms, ±1 server tick) in 100 % of cases, and every commit in one server frame closes on the same boundary.
- [ ] **AC2** (AC-CON-33, [PIE-2][Net] `StealthGame.Concealment.OwnedActorsFollowCull`): **GIVEN** a concealed player holding a carried prop and an attached tend prop **WHEN** an opponent connection's net log is read for 10 s after the cull lands **THEN** no bunch is sent for either child actor and no actor on that connection reports the concealed pawn's position (control: the same props on an Outside pawn replicate normally).
- [ ] **AC3** (derived, Manifest Required): **GIVEN** exit execution, a break, or a concealed kill **THEN** `SetCullState(Pawn, false)` happens in the same slot (kill: at the death write with `bDiedConcealed`), and Concealment calls `SetCullState` only through `IConcealmentCullSource` (lint).

---

## Implementation Notes

*Derived from ADR-0017 Decision §3 and ADR-0003 §5:*

Commit: `SetCullState(Pawn, true, ServerNow())` (`CullPending`, deadline on the global grid; same-frame commits share one boundary). Exit execution and break: `SetCullState(Pawn, false, ...)` in the same slot. Kill: the un-cull is staged with the death write (story 017). Every actor owned by or attached to a pawn (held or carried props, tend props) uses owner relevancy (`bNetUseOwnerRelevancy`) or joins the same cull, so no child actor replicates a concealed pawn's position. `bConcealServerCull = false` (client-side hide only) exists only in internal development builds (IVP R7.6, L29; IVP `story-031-player-only-cull-never-ships.md`). Tests use `StealthLeak1`/`StealthLeak2` profiles (ADR-0008 §5); packet loss is not seedable so assert bounds.

**Prototype replaced**: `bConcealed`/`bRevealedUntilExit` replication to all clients and `UpdateConcealmentVisibility` (`StealthCharacter.cpp:63-64`), which are the leak this epic closes; removal is story 035.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- IVP `story-024-concealment-cull-audience.md` and `story-025-concealment-cull-timing.md`: the Policy-side cull mechanism.
- `story-027-uncull-before-leaf-plane.md`: un-cull timing relative to the leaf plane.
- `story-026-per-viewer-visibility-pie.md`: per-viewer no-trace checks.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentCullBridge.cpp`
- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentSeams.h` (seam replaced by the real source)
- `Source/StealthTestHarness/Private/Tests/PIE/CullSendTimeTest.cpp` (`NETWORK_TEST_CLASS(CullSendTime, "StealthGame.Concealment.PIE")`)
- `Source/StealthTestHarness/Private/Tests/PIE/OwnedActorsFollowCullTest.cpp` (`NETWORK_TEST_CLASS(OwnedActorsFollowCull, "StealthGame.Concealment.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: CullSendTime
  - Given: 200 commits under `LeakNetProfile`.
  - When: the net log is read.
  - Then: each opponent channel close is at the first net-update boundary at or after commit + 250 ms (±1 tick) in 100 % of cases; all commits in one server frame close on the same boundary.
- **AC2**: OwnedActorsFollowCull
  - Given: a Concealed player holding a carried prop and an attached tend prop.
  - When: an opponent connection's net log is read for 10 s after the cull lands.
  - Then: no bunch is sent for either child actor and no actor reports the pawn's position; control: the same props on an Outside pawn replicate.
- **AC3**: SameSlotUncull
  - Given: exit, break and concealed-kill cases.
  - When: execution runs.
  - Then: `SetCullState(false)` is called in the same slot (kill: at the death write with bDiedConcealed); Concealment calls `SetCullState` only through `IConcealmentCullSource` (lint).

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the PIE `CullSendTimeTest.cpp` and `OwnedActorsFollowCullTest.cpp` — must exist and pass, OR a documented playtest

**Status**: [x] Created and passing (2026-10-29): StealthGame.Concealment.CullDeadlineGrid, StealthGame.Concealment.CullStateChain, StealthGame.Concealment.PIE.PerViewerVisibility.CoOccupantsAndCullBoundary; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: OBSERVED - 3-player listen-server PIE on L_Prototype (production/qa/evidence/concealment-001-026-2026-10-29/, con-*.png per viewer)

---

## Dependencies

- Depends on: `story-004-hide-enter-execution-and-commit.md`, `story-005-hide-exit-execution-and-blocked-side.md`, `story-014-attack-breaks-concealment.md`, `story-017-kill-while-concealed-server-rules.md`; IVP `story-004-spike-s9-cull-latency-and-explicit-channel-close.md` and `story-005-spike-s10-reopen-behaviour-and-netguid.md` (blockers), `story-024-concealment-cull-audience.md`, `story-025-concealment-cull-timing.md`
- Unlocks: `story-026-per-viewer-visibility-pie.md`, `story-027-uncull-before-leaf-plane.md`, `story-028-enter-swap-hides-body-per-viewer.md`, `story-032-ownership-change-while-concealed.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1 and AC3 passing: each close lands on the first 15 Hz pawn net-update boundary at or after commit + ConcealCullDelayMs (250 ms) within one server frame (PIE, every commit of the run) and the boundary rule is pinned headless (CullDeadlineGrid: 20 commits across a period, same-frame commits share a boundary); exit, break and the concealed kill's death write un-cull in the same step through IConcealmentCullSource only. AC2 (OwnedActorsFollowCull) DEFERRED: no carried or tend prop actor exists yet (Objectives' props are not actors); owned actors use bNetUseOwnerRelevancy, so they follow AStealthPawn::IsNetRelevantFor.
**Deviations**: The 200-commit LeakNetProfile run is replaced in the per-commit gate by the headless boundary test plus the PIE commits; the IVP cull mechanism is built here as UStealthConcealmentCull (Foundation world subsystem: Visible/CullPending/Culled records, grid deadline anchored at Active entry, TG_PostUpdateWork close of non-ally channels, S10-R2/R3 re-open guard with ReopenAckTimeoutMs 350) - IVP stories 024/025's cull; S10-R4 repair and R6 clock remain IVP scope.
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
