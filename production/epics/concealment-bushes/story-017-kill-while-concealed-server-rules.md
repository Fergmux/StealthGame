# Story 017: Killing a concealed pawn leaves the record at the slot, re-opens only at the death write and holds the owner and team flags until body removal

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R9 kill row, CON-R10 tail, AC-CON-26 server part)
**Requirement**: `TR-con-012`, `TR-con-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§3 kill of a concealed pawn, §8 B), with ADR-0018 (outcome slot, death write, `IBodyRemovalObserver`)
**ADR Decision Summary**: At Assassination's outcome slot the victim leaves the occupancy record and stops being a candidate; the un-cull happens only at the death write (`ContactServerTime − StrikeNetLeadSec`) in the same step as `FDeathState {DeathServerTime, bDiedConcealed = true}`; Owner/Team `bIsConcealed` is not cleared until the body's server removal (`IBodyRemovalObserver::OnBodyRemovalCommitted`, implemented by `UConcealmentService::OnConcealedBodyRemoved`) or the participant's rebind, whichever comes first.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; plain server logic over staged writes)
**Engine Notes**: `FDeathState` serialises to exactly 65 bits including `bDiedConcealed` (ADR-0018). `MarkKilledConcealed` is retired. The flags are per participant, so they must never carry concealed framing or the ally latch onto a new body.

**Control Manifest Rules (this layer)**:
- Required: A concealed kill un-culls at the death write together with `FDeathState.bDiedConcealed = true`; Owner/Team `bIsConcealed` for a killed-concealed pawn clears at server body removal or participant rebind, whichever first — source: ADR-0003, ADR-0017
- Forbidden: Never re-open a killed-concealed pawn before the death write, and never draw its body on a non-owner client — source: ADR-0017
- Guardrail: `bDiedConcealed` is set only in the death write, only for a victim in Concealed or CullPending, identically for NPC and player victims

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (CON-R9, AC-CON-26) and TR-con-012, scoped to this story:*

- [ ] **AC1** (CON-R9, AC-CON-26 server part): **GIVEN** a concealed victim killed in Assassination's outcome slot, with a second strike on the same bush in the same tick **THEN** it leaves the occupancy record in that slot (the second strike draws from the survivors), stops being a candidate, no cull change is made in the slot, and Owner/Team `IsConcealed` stay true.
- [ ] **AC2** (`TR-con-012`): **GIVEN** the death write at `ContactServerTime − StrikeNetLeadSec`, for an NPC victim and a player victim **THEN** the un-cull and `FDeathState {DeathServerTime, bDiedConcealed = true}` happen in the same step; `bDiedConcealed` is false at every earlier point; it is set only for a victim in Concealed or CullPending; `FDeathState` serialises to exactly 65 bits and the replicated-property allowlist is unchanged from baseline (no new field).
- [ ] **AC3** (CON-R9, ADR-0017 §3): **GIVEN** a killed-concealed pawn **WHEN** `OnConcealedBodyRemoved` fires in one run, and `BindPawn` or `TakeOverCrowdPawn` fires first in another **THEN** Owner and Team flags clear on whichever comes first (exactly once if both on one tick); a victim's own `Hide(Exit)` arriving between the slot and the death write is rejected `NotNow`.

---

## Implementation Notes

*Derived from ADR-0017 Decision §3 (Kill of a concealed pawn) and §8 B:*

`OnConcealedPawnKilled(Victim)` runs in the outcome slot: leave the record only. At the death write push `bConcealed = false` in the staging pass so the victim re-opens carrying `bDiedConcealed = true`; nothing re-opens in the outcome slot. The removal instant is a server event: ADR-0018's `BodyRemoval` resolver calls `IBodyRemovalObserver::OnBodyRemovalCommitted(Body)` in the same slot, just before ADR-0002 `RequestTeardown`; `UConcealmentService::OnConcealedBodyRemoved` clears both flags. The rebind path (`BindPawn`/`TakeOverCrowdPawn`, ADR-0010/ADR-0019) also clears them. A body already on an ally client stays hidden after the clear by CON-R6b rule 4. ADR-0004 is asked to add the cross-field warning `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < InformantRespawnDelaySec` so the normal order is removal first. A victim that was Entering or Outside never gets `bDiedConcealed`; the fixture needs a pending-cull (CullPending) victim from the IVP seam.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-033-blind-strike-parity-and-kill-timing.md`: the PIE timing and never-drawn-on-non-owner checks.
- `story-020-fresh-actor-latch-and-ally-hidden-read.md`: the client-side hold of the body.
- assassination-defence-reversal epic: the outcome slot, death write and `FDeathState` carrier.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentCore.cpp` (kill path)
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentService.cpp` (`IBodyRemovalObserver`)
- `Source/StealthGameplay/Private/Tests/Concealment/KillWhileConcealedTest.cpp` (classes `FConcealmentKillSlotRulesTest`, `FConcealmentDeathWriteTest`, `FConcealmentBodyRemovalOrRebindTest`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS on AC2 of the first draft; revised as above, re-check ADEQUATE).*

- **AC1**: SlotRules
  - Given: a Concealed victim killed in the outcome slot, with a second strike on the same bush in the same tick.
  - When: the slot runs.
  - Then: the victim leaves the record in that slot, the second strike draws from the survivors, no cull change is made, Owner and Team IsConcealed stay true.
  - Edge cases: the victim was the only occupant, so the second strike is a whiff.
- **AC2**: DeathWrite
  - Given: the death write at ContactServerTime minus StrikeNetLeadSec, for an NPC victim and a player victim.
  - When: it executes.
  - Then: the un-cull and `FDeathState{DeathServerTime, bDiedConcealed=true}` happen in the same step; bDiedConcealed is false at every earlier point; `FDeathState` serialises to exactly 65 bits; the replicated-property allowlist is unchanged.
  - Edge cases: a victim that was Entering or Outside never gets bDiedConcealed; the fixture needs a pending-cull victim.
- **AC3**: BodyRemovalOrRebind
  - Given: a killed-concealed pawn.
  - When: `OnConcealedBodyRemoved` fires, in one run, and `BindPawn` or `TakeOverCrowdPawn` fires first in another.
  - Then: Owner and Team flags clear on whichever comes first; a Hide(Exit) from the victim between slot and death write replies NotNow.
  - Edge cases: both events on the same tick clear exactly once.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `KillWhileConcealedTest.cpp` under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-003-conceal-state-machine-and-occupancy-record.md`, `story-004-hide-enter-execution-and-commit.md`, `story-023-owner-and-team-concealed-flags.md`; assassination-defence-reversal epic (outcome slot, death write; stories not yet written); Roster `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md`
- Unlocks: `story-033-blind-strike-parity-and-kill-timing.md`
