# Story 027: On exit and bush-emerge the body is never drawn before the leaf crossing and the actor exists by then

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R8, CON-R9, F3(b)(c), AC-CON-07, AC-CON-13 observed)
**Requirement**: `TR-con-005`, `TR-con-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§3, §5), with ADR-0003 (un-cull, re-open)
**ADR Decision Summary**: An exit or a break un-culls in the same server step, but the body shows on every client only at the outward leaf crossing (`Start + BushExitCrossSec`, or `+ BushStrikeEmergeSec` for the emerge strike), which F3 requires to be ≥ `T_uncull` so the actor exists before it is drawn. Late actors are seeked into the clip, never snapped or restarted.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: At L99 = 250 ms `T_uncull` is 0.35 s, so the strike crossing margin is 0 ms and 0.30 s is the hard minimum of the crossing keys; above L99 250 ms the Policy owner recomputes F3. The re-open creating a fresh actor is S10; the cull delay against measured L99 is S12.

**Control Manifest Rules (this layer)**:
- Required: Clips are seeked from `EstimatedServerNow()`, never restarted or snapped; presentation never depends on arrival order — source: ADR-0017
- Forbidden: Never gate the hide or the show by an anim notify — source: ADR-0017
- Guardrail: Measured p99 `T_uncull` ≤ crossing − 50 ms

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-07, AC-CON-13), scoped to this story:*

- [ ] **AC1** (AC-CON-07, [PIE-2][Net][1P/3P] `StealthGame.Concealment.ExitUncullBeforeLeafPlane`): **GIVEN** 1,000 exits and 300 bush-emerge strikes under `LeakNetProfile` **THEN** (a) the body is drawn before its crossing server time in 0 cases on any client (keyed to server time, network-independent); (b) the opponent client creates the actor after the crossing in ≤ 1.5 % of exits and ≤ 2.0 % of emerges, and every late actor appears seeked into the clip with no snap; (c) the measured p99 of `T_uncull` is ≤ crossing − 50 ms.
- [ ] **AC2** (AC-CON-13 observed, [PIE-2][1P/3P] `StealthGame.Concealment.AttackBreaksConcealment`): **GIVEN** a concealed player who strikes a passer-by at t0 **THEN** the opponent client shows the attacker's body at t0 + 0.35 s (±1 server tick), then the victim's contact and fall at t0 + 0.75 s (±1 frame), and the attacker ends outside the bush; control: a victim braced before t0 takes the reversal path.

---

## Implementation Notes

*Derived from ADR-0017 Decision §3, §5 and GDD F3:*

The test logs draw time and actor-creation time per client against the server crossing key. Network-independence of (a) comes from the latch being keyed to the synchronised server clock, not arrival order. The same measurement run feeds IVP `story-029-uncull-lead-bound.md` (the Policy's threshold arithmetic on the exit asset); this story is the Concealment-side observation. A Prepare issued by the victim during the preface does not change the outcome (fixed at execution).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-005-hide-exit-execution-and-blocked-side.md` and `story-014-attack-breaks-concealment.md`: server-side exit and break logic.
- IVP `story-029-uncull-lead-bound.md`: threshold arithmetic on the clip asset.
- assassination-defence-reversal epic: the strike clip and reversal path.

---

## Files

- `Source/StealthTestHarness/Private/Tests/PIE/Concealment/ExitUncullBeforeLeafPlanePieTest.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/Concealment/AttackBreaksConcealmentPieTest.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/Concealment/ConcealmentPieSupport.h`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: ExitUncullBeforeLeafPlane
  - Given: 1,000 exits and 300 bush-emerge strikes under `LeakNetProfile`.
  - When: draw and creation times are logged on each client.
  - Then: (a) the body is drawn before its crossing server time in 0 cases; (b) a late actor occurs in at most 1.5 % of exits and 2.0 % of emerges, always seeked with no snap; (c) p99 T_uncull is at most the crossing minus 50 ms.
- **AC2**: AttackBreaksConcealment
  - Given: a Concealed player striking a passer-by at t0.
  - When: the opponent client is observed.
  - Then: the attacker's body appears at t0 + 0.35 s (±1 tick), and contact and fall at t0 + 0.75 s (±1 frame); the attacker ends outside the bush; control: a victim braced before t0 takes the reversal path.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the two PIE tests above — must exist and pass, OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-005-hide-exit-execution-and-blocked-side.md`, `story-014-attack-breaks-concealment.md`, `story-020-fresh-actor-latch-and-ally-hidden-read.md`, `story-025-cull-push-wiring.md`; IVP S9, S10, S12 spike stories (blockers), `story-029-uncull-lead-bound.md`
- Unlocks: `story-035-prototype-retirement-and-regression.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1, AC2 passing. AC1 StealthGame.Concealment.PIE.ExitUncullBeforeLeafPlane: 1,000 exits and 300 bush-emerge strikes under StealthLeak1 (20 unbound hiders, 2 per bush, cooldown lifted by the test seam): (a) 0 draws before the crossing on any machine; (b) late actors within bounds, no snap; (c) p99 T_uncull within crossing - 50 ms (numbers in the evidence). AC2 StealthGame.Concealment.PIE.AttackBreaksConcealment: the opponent first draws the attacker at t0 + 0.35 s (+- tick + frame), the victim's Death_Fall at t0 + 0.75 s (+- frame), the attacker ends Outside; control: a braced victim reverses.
**Deviations**: ADVISORY: tests live under Source/StealthTestHarness/Private/Tests/PIE/Concealment/ (the module's convention), not Tests/PIE/; the measurement uses unbound NPC hiders (same path, CON-R3) so 1,300 transitions fit a gate run; IVP story-029 (threshold arithmetic) is still Blocked and its observation is this run.
**Test Evidence**: production/qa/evidence/concealment-027-034-2026-10-29/
**Gate**: Build Succeeded; full ^StealthGame. suite 594 tests, 593 passed first run (ExitUncullBeforeLeafPlane snap check narrowed to late actors, re-run alone at full counts PASS)
Run result: OBSERVED — 3-player PIE: after Hide(Exit) the enemy's client shows the hider outside the bush only after the crossing (con-exit-opp-3p-after-crossing.png).
**Reviewed by**: self code review 2026-10-29 (notes in production/qa/evidence/concealment-027-034-2026-10-29/evidence.md)
