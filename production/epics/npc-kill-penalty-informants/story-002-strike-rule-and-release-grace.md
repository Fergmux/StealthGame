# Story 002: Strike rule, stale-read release grace and reversal at the limit

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-001`, `TR-pen-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (strike rule §2.1; consumes ADR-0018's `OnPawnKilled`)
**ADR Decision Summary**: Penalty subscribes to `OnPawnKilled(Victim, Killer, VictimKind, Cause)`, inline in the Attack's queue slot. It adds one strike only when `Cause = Strike`, `VictimKind = Npc`, the killer's Roster state is `Active` at the slot and the victim is not a graced released body (a released body killed within `StrikeOutReleaseGraceSec` of its release, inclusive). The tally is not changed here, and `StrikeCount` is ServerOnly.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API is used; the rule is pure logic in `FPenaltyCore` over injected clock, Roster and queue seams. The only engine-adjacent value is the server clock, read through ADR-0002's server clock interface (never `UWorld::TimeSeconds` directly).

**Control Manifest Rules (this layer)**:
- Required: `VacateCard`, Roster transitions, strike accounting and holds' server cancels happen in the slot — source: ADR-0018
- Forbidden: never run strike accounting in the staging pass; never let `StrikeCount` reach a client before `Ended` — source: ADR-0018, ADR-0019
- Guardrail: O(1) inside the 0.5 ms queue line — source: ADR-0019

The strike rule is the single most load-bearing rule of the penalty (F1: `k' = k + [Cause = Strike ∧ VK = Npc ∧ S = Active ∧ ¬G]`). The grace window (PEN-R5) means the game is only ever kinder than the tally, never harsher; the boundary is inclusive and is tested at 1.999 s, 2.000 s and 2.001 s.

**Prototype replaced:** `FStealthRules::ShouldRemoveForNpcKills(NpcKillCount, NpcKillLimit)` and the NPC-kill branch of `AStealthGameMode` (every NPC kill counted, an informant kill excluded by a target-kind enum). That pure rule with its unit tests is the *reference for the removal-on-the-limit-th-kill boundary* only; the production rule has the grace, the killer-state check and the cause check the prototype lacks. The prototype function is deleted in story 021, not here.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-01, AC-NKP-07, AC-NKP-32), scoped to this story:*

- [ ] **AC1** (AC-NKP-01, [L] `StealthGame.Penalty.StrikeRuleMatrix`): **GIVEN** `OnPawnKilled` for every combination of `Cause` {Strike, Reversal} × `VictimKind` {Player, Npc, Informant} × killer {Active participant, Informant participant, NPC (no participant: a bracing NPC's reversal, `GetParticipant` returns none)} × grace {in, out} **WHEN** Penalty processes each **THEN** `StrikeCount` increases by 1 only for (Strike, Npc, Active, out of grace) and is unchanged for every other combination; the Informant-killer cells expect no strike and exactly one `ensure` through the injected fault sink; the NPC-killer cells expect no strike, no `ensure` and no crash (PEN-R2, F1).
- [ ] **AC2** (AC-NKP-07, [L] `StealthGame.Penalty.ReleaseGraceBoundary`): **GIVEN** `StrikeOutReleaseGraceSec` 2.0 on a fake clock and a released body **WHEN** a rival kills it at 1.999 s, 2.000 s and 2.001 s after the release (three runs) **THEN** all three produce `WrongTarget` and tally +1, the first two add no strike (the boundary is inclusive) and the third adds one (PEN-R5).
- [ ] **AC3** (AC-NKP-32, [L] `StealthGame.Penalty.ReversalAtLimit`): **GIVEN** k = 2 and L = 3 **WHEN** P's Attack is reversed **THEN** k stays 2, the tally is unchanged (`Reversed` is not `WrongTarget`), no `StrikeLimit` is requested, and a normal respawn is scheduled for `ReversedAgainst` (Edge Cases). *The respawn scheduling half is asserted against story 009's provider call; until it exists assert only k, tally and the absence of a `StrikeLimit` request.*

---

## Implementation Notes

*Derived from ADR-0019 Decision §2.1 and Implementation Guidelines:*

`UPenaltyService::OnPawnKilled` runs inline in the Attack's slot (ADR-0018 §2.1 step 6). Resolve the killer with the Roster's `GetParticipant(KillerPawn)` (the attacker's pawn is still `BoundLive` in the slot). Evaluate, in this order: (1) `VictimKind = Informant` -> no strike, hand the kill to story 008's path; (2) the strike rule as the F1 predicate; (3) `Graced(Victim)` = `ReleasedBodies[Victim]` exists and `slotTime - ReleaseServerTime <= StrikeOutReleaseGraceSec` (inclusive, double arithmetic on server time; test on both sides of the boundary with a fake clock, never a real wait). A `Player` victim (Assassinated / ReversedAgainst) is handled by the Roster state event the same slot already raised: Penalty adds nothing here.

The killer-state check is an assert backed by ASN-R2 (an attacker who left `Active` earlier in the drain is already voided): an `Informant` killer reaching `OnPawnKilled` is a defect, so it reports once to the fault sink (the TDC fault-sink seam) and adds no strike. A killer with no participant (a bracing NPC's reversal) adds no strike and reports nothing. Log `StrikeCounted{Killer, StrikeAfter, TallyAfter, Graced}` through story 004's writer once it exists; until then call the writer interface with the in-memory sink.

`StrikeCount` is ServerOnly and must never be copied to a replicated type, a reply struct, a log line that reaches a client or a debug draw. The tally is not touched here (story 003 owns it); the invariant `StrikeCount ≤ WrongTargetTally` is asserted by story 003 after each update. `L` comes from the latched-limit read of story 001.

`StrikeOutReleaseGraceSec` is a Tuning key (default 2.0, hard range 0-5.0, ServerOnly, latch Round): read it through the server tuning face, never hard-coded. The grace table `ReleasedBodies` is filled by the exile (story 005); here the tests seed it directly.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-003-tally-rule-and-strike-result-supply.md`: the tally and `FStrikeResult` supply.
- `story-005-exile-sequence-in-attack-slot.md`: the exile sequence that fires when `k' ≥ L`, and the writes to `ReleasedBodies`.
- `story-008-informant-kill-inline-retire.md`: the `VictimKind = Informant` path (retire binding, schedule respawn).
- `story-009-respawn-due-time-scheduling-and-cancellation.md`: the `ReversedAgainst` respawn scheduling.

---

## Files

*Changed by /dev-story 2026-10-27.*

- `Source/StealthGameplay/Features/Penalty/Private/StealthPenaltyCore.cpp` (strike rule F1, grace predicate, InformantKiller fault, StrikeCounted pending row)
- `Source/StealthGameplay/Features/Penalty/Private/StealthPenaltyService.cpp` (`OnPawnKilled` subscription, killer resolution)
- `Source/StealthFoundation/Public/Server/StealthTuningSubstructs.h` (new `FPenaltyTuning_Server::StrikeOutReleaseGraceSec`), `Public/Server/StealthServerTuning.h`, `Public/Server/StealthTuningData.h` (`Penalty_Server`)
- `Source/StealthFoundation/Private/Tuning/StealthTuningSchema.generated.inl` (one row, fingerprint), `Public/Server/StealthTuningVersion.h` (schema version 5)
- `Source/StealthFoundation/Public/Server/Testing/StealthPenaltyTuningKnobs.h`, `Source/StealthGameEditor/Private/Tests/Tuning/SchemaCompleteTest.cpp` (the Penalty pin)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyStrikeRuleMatrixTest.cpp` (`StealthGame.Penalty.StrikeRuleMatrix`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyReleaseGraceBoundaryTest.cpp` (`StealthGame.Penalty.ReleaseGraceBoundary`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyReversalAtLimitTest.cpp` (`StealthGame.Penalty.ReversalAtLimit`)

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-01, `StealthGame.Penalty.StrikeRuleMatrix`)
  - Given: all 36 cells of Cause {Strike, Reversal} x VictimKind {Player, Npc, Informant} x killer {Active, Informant, NPC with no participant} x grace {in, out}.
  - When: `OnPawnKilled` is processed for each cell.
  - Then: k + 1 only for (Strike, Npc, Active, out of grace); Informant-killer cells have no strike and exactly one `ensure` through the fault sink; NPC-killer cells have no strike, no `ensure`, no crash.
  - Edge cases: impossible combinations (Reversal with an Npc victim, Strike by an NPC) must still run; "grace in" means a `ReleasedBodies` entry on the victim handle, whatever the victim kind.
- **AC2** (AC-NKP-07, `StealthGame.Penalty.ReleaseGraceBoundary`)
  - Given: fake clock, `StrikeOutReleaseGraceSec` = 2.0, a body released at t0.
  - When: a rival kills it at t0 + 1.999, + 2.000 and + 2.001 (three runs).
  - Then: all three give `WrongTarget` and tally + 1; the strike is +0, +0, +1.
  - Edge cases: pick a t0 that makes + 2.000 compare exactly (the boundary is inclusive and doubles drift); a body never released is never graced; reset clears the grace.
- **AC3** (AC-NKP-32, `StealthGame.Penalty.ReversalAtLimit`)
  - Given: L = 3, P at k = 2.
  - When: P's Attack is reversed.
  - Then: k stays 2, tally unchanged, no `StrikeLimit` request; after the stub Round calls `OnInformantAwaitingBody(P, ReversedAgainst, t)`, exactly one respawn is scheduled.
  - Edge cases: depends on story 009 for scheduling (assert only the no-strike half until it exists); an assassination at k = 2 also leaves k unchanged.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three tests under `Source/StealthGameplay/Private/Tests/Penalty/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [x] Created and passing (2026-10-27): StealthGame.Penalty.StrikeRuleMatrix, StealthGame.Penalty.ReleaseGraceBoundary, StealthGame.Penalty.ReversalAtLimit; evidence production/qa/evidence/npc-kill-penalty-informants-001-004-2026-10-27/

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-001-penalty-core-store-and-service-shell.md`; cross-epic: assassination-defence-reversal (`OnPawnKilled` with `VictimKind` and `Cause`, ASN-R10, ASN-R11), participant-state-roster (`GetParticipant`, Roster state)
- Unlocks: `production/epics/npc-kill-penalty-informants/story-003-tally-rule-and-strike-result-supply.md`, `story-005-exile-sequence-in-attack-slot.md`

---

## Dev Notes (2026-10-27)

**Readiness**: READY (depends on nkp-001, implemented in the same batch; ASN `OnPawnKilled` exists).

**Deviations**:
- `StrikeOutReleaseGraceSec` registered as a schema key in a new `FPenaltyTuning_Server` (2.0, hard 0-5.0, safe 1.75-2.5, ServerOnly, latch Round, owner Penalty); `TuningSchemaVersion` 5. The generated table was hand-edited in the generator's exact format; regenerate with `Stealth.Tuning.SchemaGen` after the build and keep the result.
- Order of the checks: an Informant killer faults before the victim-kind dispatch, so every Informant-killer cell of the matrix (whatever the cause or victim) reports exactly one fault.
- AC3's scheduling half (one respawn after `OnInformantAwaitingBody(P, ReversedAgainst, t)`) is nkp-009's; only the no-strike half is asserted (as the story allows).
- `StrikeCounted` is written for every NPC or informant kill by an Active participant, completed with `TallyAfter` at the same slot's tally query.

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: 3/3 passing (AC3's respawn-scheduling half DEFERRED to nkp-009, as the story allows)
**Deviations**: advisory, listed under Dev Notes (folder layout, shadow tally wiring, deferred halves)
**Test Evidence**: production/qa/evidence/npc-kill-penalty-informants-001-004-2026-10-27/ (`tools/dev/cycle.ps1 -Tests -NoLaunch`: Result: Succeeded; 387/387 StealthGame. tests Success)
**Reviewed by**: self code review 2026-10-27 (APPROVED; no required changes)
