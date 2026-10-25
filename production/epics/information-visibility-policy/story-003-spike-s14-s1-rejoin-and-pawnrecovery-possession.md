# Story 003: Spike S14 and S1, rejoin and PawnRecovery possession stay invisible to non-owners

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs the participant-state-roster rejoin/PawnRecovery stories, the session-travel rejoin story and the shared-character-locomotion pawn factory/possession stories built (the prototype has no rejoin or PawnRecovery path).
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

S14 passed for every possession path the prototype has (takeover, AI hand-over, kill, informant respawn). Two paths are still unproven and are the ones most likely to break owner-side behaviour or to leak: a rejoin (the returning human is possessed into a pawn that an AI was driving) and a `PawnRecovery` re-pawn (an Active player whose pawn is gone). Making the three pointers `COND_OwnerOnly` must not break `OnRep_Controller`, `ClientRestart`, CMC `ServerMove` routing, camera or input for the owner, and the new owner must receive the three pointers after the ownership change, while no non-owner may see any off-cadence bunch at the possession frame. This spike reports the S14 and S1 rejoin/PawnRecovery conditions.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-003` (UE default overrides of R5: pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (sections 4.1, 4.2); also ADR-0003: Replication, relevancy and the concealment cull hybrid (section 2)
**ADR Decision Summary**: Possession is invisible to non-owners: the possession `ForceNetUpdate` is dropped and the next fixed-cadence update carries the change, owner relevancy is off, and role changes are visible only to the new owner; on the legacy driver a new owner receives the current pointer values after a takeover (E12 [V]).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `APawn::PossessedBy`/`UnPossessed` call `ForceNetUpdate()` (E16, [V] `Pawn.cpp:434`); ADR-0003 section 2 drops every unscoped call. A new owner receives `Owner`/`PlayerState` values on the legacy driver (E12 [V]). This spike owns the [U] claim S14 (and the rejoin/PawnRecovery conditions of S1) in ADR-0001.

**Control Manifest Rules (this layer)**:
- Required: **`ForceNetUpdate()` on a pawn forwards only inside `FStealthForcedNetUpdate` with an allowed reason, at most once per pawn per frame**; the unscoped possession call is dropped and the 15 Hz cadence carries the change — source: ADR-0003, ADR-0001
- Required: **`AStealthPlayerController` clears `Owner`/`PlayerState` client-locally on its previous pawn** — source: ADR-0001
- Forbidden: **Never read `Owner` or `PlayerState` on a pawn the owner does not currently possess** — source: ADR-0001
- Guardrail: **Never prove multi-process criteria in in-process PIE only** — source: ADR-0008

---

## Acceptance Criteria

*From the story's spike definition (S14/S1 conditions left open in ADR-0001's Amendments), with the QL-STORY-READY clarification:*

- [ ] GIVEN a player pawn and an NPC pawn, WHEN a rejoin and a PawnRecovery re-pawn (and, again, the already-proven takeover and AI hand-over) are performed multi-process under StealthLeak2, THEN for the owner possession, OnRep_Controller, ClientRestart, CMC ServerMove routing, camera and input still work and the new owner receives the three pointers after the ownership change, while no non-owner changelist contains PlayerState, Controller or Owner and no bunch for the possessed pawn is sent at the possession frame; the S14 and S1 rejoin/PawnRecovery conditions are marked reported in production/qa/spikes/. Clarification (story-level, QL-STORY-READY): "possession works" means GetViewTarget() == the possessed pawn on the owner client and the owner's CMC ServerMove is acknowledged with zero corrections over the 60 frames after the possession frame.

---

## Implementation Notes

*Derived from ADR-0001 sections 4.1, 4.2 and Validation Criteria (S1, S14), ADR-0003 section 2:*

- Throwaway spike code is acceptable, but the paths under test (rejoin adoption, `PawnRecovery` re-pawn through the crowd emergence path, takeover) are the production ones built by the roster, session-travel and shared-character-locomotion epics; the prototype has none of them.
- "Zero corrections over 60 frames" under 2 % loss can fail for reasons unrelated to possession: record a `StealthClean` baseline run to tell the two apart (QA note).
- Use the capture mechanism chosen in story-001 for the non-owner side; measure the possession frame explicitly (no bunch for the possessed pawn at that frame; the cadence update carries the change).
- A failure is handled by a superseding ADR (ADR-0001 section 8 written fallback); do not edit ADR-0001.
- Prototype: reference only (`Spikes/StealthIdentitySpike.*`, `stealth.Spike.TakeOver`).
- Unblocks when: the roster rejoin/PawnRecovery stories, the session-travel rejoin story and the shared-character-locomotion factory/possession stories are built.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-001: S1 multi-process capture for spawn, takeover and AI hand-over
- story-002: S2 travel and rejoin PlayerState trims
- story-019: the per-commit possession canary (AC-IVP-37)
- story-018: the engine-default-overrides acceptance test (AC-IVP-09)

---

## Files

- `production/qa/spikes/<date>_S14-S1-rejoin-pawnrecovery.md` (the verdict)
- Spike tooling reused from story-001

---

## QA Test Cases

- **AC-1**: S14 and S1 rejoin and PawnRecovery possession
  - Given: a player pawn and an NPC pawn, multi-process under StealthLeak2.
  - When: a rejoin and a PawnRecovery re-pawn occur, and again the already-proven takeover and AI hand-over.
  - Then: for the owner, possession, OnRep_Controller, ClientRestart and CMC ServerMove routing work: GetViewTarget() == possessed pawn and the owner's ServerMove is acknowledged with 0 corrections over the 60 frames after the possession frame; the new owner receives the three pointers after the ownership change; no non-owner changelist contains PlayerState, Controller or Owner; no bunch for the possessed pawn is sent at the possession frame; the S14 and S1 conditions are marked reported in production/qa/spikes/.
  - Edge cases: "0 corrections" under 2 % loss may fail for reasons unrelated to possession; record a StealthClean baseline run to tell the two apart. (Tier: T3.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a written spike report `production/qa/spikes/<date>_S14-S1-rejoin-pawnrecovery.md` with capture artefacts and the baseline run (a documented measurement).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`, `production/epics/information-visibility-policy/story-002-spike-s2-seamless-travel-and-rejoin-playerstate-trims.md`; participant-state-roster epic (rejoin and PawnRecovery stories); session-travel epic (rejoin story); shared-character-locomotion epic (pawn factory and possession stories)
- Unlocks: `production/epics/information-visibility-policy/story-018-engine-default-overrides.md`, `production/epics/information-visibility-policy/story-021-team-view-never-to-opponents.md`
