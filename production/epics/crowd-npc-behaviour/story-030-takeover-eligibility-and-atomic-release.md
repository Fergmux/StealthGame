# Story 030: Takeover eligibility supply and atomic NPC release for informants

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-007 = Participant-bound AI driving and takeover eligibility supply.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0019 (Penalty is the only caller of `TryReleaseNpc`, in its commit slot; commit order pick, eligibility pre-check, `TryReleaseNpc`, `TakeOverCrowdPawn`), ADR-0010 (control epoch), ADR-0014 (`PickRespawnPawn`)
**ADR Decision Summary**: `ITakeoverEligibilitySource::GetTakeoverEligibility(Pawn)` returns `Eligible`, `Tending`, `Bracing`, `Hiding`, `Emerging`, `Exiting`, `Reacting`, `Conversing`, `Bound` or `AlleyExit`, computed from authoritative sources at call time. `TryReleaseNpc(Pawn, FEpochToken)` re-checks `Eligible` atomically, latches the pawn's sink, stops the StateTree, removes the pawn from the ledger (B += 1) and leaves the pawn moving as it was; on failure Penalty re-picks.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff API. The invisibility check (no pose change, speed continuity over 1 s with 100 ms RTT) is a multi-process test under network emulation (ADR-0008 T3).

**Why this story**: A killed informant is replaced by taking over a live NPC mid-stride; if that pawn visibly stopped, changed pose or came from a recognisable state, the takeover itself leaks identity (Penalty ADR-0019, T15). Read GDD CNB-R11 and AC-CNB-23, ADR-0016 section 8.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: eligibility from authoritative sources (IIF slot for Tending/Bracing, Concealment for Hiding, the ledger for Emerging/Exiting/AlleyExit/Bound; the mirror only for Reacting and Conversing); `TryReleaseNpc` called only by Penalty inside its commit slot; the StateTree is stopped and the epoch bumped in the same tick (ADR-0016, ADR-0019)
- Forbidden: reading appearance, team or who is watching; `Possess`/`UnPossess` from Crowd code; rescheduling a `Rejected_StaleEpoch` item (ADR-0016)
- Guardrail: eligibility query O(1) per pawn; release inside the Penalty commit slot

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-23 and CNB-R11, scoped to this story:*

- [ ] (AC-CNB-23a) GIVEN 100 `PickRespawnPawn` calls, WHEN candidates are released, THEN every released pawn was `Eligible` at the commit tick and none was tending, bracing, in a bush or bush transition, emerging, exiting, reacting or conversing, and the StateTree is stopped and the control epoch bumped in the same tick.
- [ ] (AC-CNB-23b) 0 old-epoch commands reach the pawn afterwards (an async path callback is injected after the commit in 20 of the calls) and the pawn shows no pose change at the release.
- [ ] (AC-CNB-23c) In a multi-process run with network emulation (100 ms RTT), the pawn's speed never drops below 90 % of its pre-release speed in the 1 s after possession unless the new driver commanded it.
- [ ] `GetTakeoverEligibility` is computed from authoritative sources and never reads appearance, team or viewers; `AlleyExit` covers any pawn whose current leg is an `AmbientExit`, including a released exile body in pending exit, so a participant can never re-adopt its own released body.

---

## Implementation Notes

*Derived from ADR-0016 section 8 and GDD CNB-R11:*

- `TryReleaseNpc` is called only inside Penalty's commit slot, on the pawn `PickRespawnPawn` returned; it re-checks eligibility atomically, latches the sink, stops the tree, removes the pawn from the ledger (B += 1) and leaves the pawn moving as it was for the SCL AI to player handover. Penalty's same-slot `TakeOverCrowdPawn` / `RequestHandOver` bumps the control epoch (ADR-0010).
- `Bound` is reported for any participant-bound pawn; `Exiting` and `Emerging` come from the ledger.
- Reversal: `AdoptReleasedPawn(Pawn, {})` (story 029) undoes it.
- LSC `PickRespawnPawn` applies the region and unseen tests and the uniform draw; Crowd does not choose.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype respawns informants by spawning a fresh pawn. Not ported (ADR-0019 forbids spawning for a respawn).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The Penalty epoch (`production/epics/npc-kill-penalty-informants/EPIC.md`): commit order, `T_due`, card moves
- Story 028 and 029: bound driving and release
- Story 031: the handover/Halt epoch gate for 100 transitions

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdTakeoverEligibility.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdInformantReleaseTest.cpp`
- `Source/StealthTestHarness/Private/CrowdInformantReleaseMPTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.InformantReleaseAtomic` (eligibility)
  - Given: 100 `PickRespawnPawn` calls over varied NPC states
  - When: candidates are released
  - Then: every released pawn was `Eligible` at the commit tick; none tending, bracing, in a bush or transition, emerging, exiting, reacting or conversing; StateTree stopped and epoch bumped in the same tick
  - Edge cases: state changes in the commit tick
- **AC-2**: `StealthGame.Crowd.InformantReleaseAtomic` (epoch)
  - Given: the same calls, an async path callback injected after the commit in 20 of them
  - When: commands are monitored after release
  - Then: 0 old-epoch commands reach the pawn; no pose change at the release
  - Edge cases: callback in the same frame as the commit
- **AC-3**: `StealthGame.Crowd.MP.InformantReleaseContinuity`
  - Given: a multi-process run with 100 ms RTT network emulation
  - When: speed is measured for 1 s after possession
  - Then: speed never drops below 90 % of its pre-release speed unless the new driver commanded it
  - Edge cases: needs multi-process emulation (deferred to the harness)
- **AC-4**: `StealthGame.Crowd.EligibilitySources`
  - Given: `GetTakeoverEligibility` with fixtures for the IIF slot, Concealment, ledger and activity mirror, and a released exile body
  - When: it is evaluated
  - Then: results come only from the authoritative sources (the mirror only for Reacting and Conversing); never reads appearance, team or viewers; `AlleyExit` covers a released exile body
  - Edge cases: the mirror disagrees with authority

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdInformantReleaseTest.cpp` plus `Source/StealthTestHarness/Private/CrowdInformantReleaseMPTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`, `production/epics/crowd-npc-behaviour/story-028-bound-pawn-restricted-driver.md`
- Unlocks: None
