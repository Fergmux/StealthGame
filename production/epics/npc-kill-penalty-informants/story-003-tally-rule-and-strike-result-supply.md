# Story 003: Wrong-target tally, FStrikeResult supply and owner re-push

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-001`, `TR-pen-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§2.2, §2.2a), with ADR-0001 (C-TeamView owner part) for the carrier
**ADR Decision Summary**: `IStrikeTallySource::GetTallyForResult(Attacker, Result)` is called by Assassination after the fan-out: a `WrongTarget` result does `t := min(t + 1, 255)`, and the return is `{TallyAfter = t, bExiled = (this slot exiled Attacker)}`; every other result returns the current `t` and `bExiled = false`. The invariant `StrikeCount ≤ WrongTargetTally` is asserted after each update. The owner re-push `{WrongTargetTally, LastResultSeq}` goes through the Roster's `SetOwnerTally(P, Tally, LastResultSeq)` only on reclaim, rejoin (`OnViewRebound`) and `OnRosterReset`, never per strike.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Penalty's own logic uses no engine API, but the re-push rides the C-TeamView owner part (ADR-0001, HIGH: owner-only replication on a `PlayerController`, legacy NetDriver with Iris pinned off). Spike S1/S2/S14 reported 2026-10-25 (`production/qa/spikes/2026-10-25_S1-S2-S14.md`): owner-only carriers work on the legacy driver; conditions still open there are rejoin, travel and a multi-process capture under `LeakNetProfile`, which story 016 exercises.

**Control Manifest Rules (this layer)**:
- Required: `FStrikeResult` is sent for every executed Attack, same shape for every result; the per-strike tally travels only in `FStrikeResult` — source: ADR-0018, ADR-0019
- Forbidden: never write the tally to C-TeamView on a strike; `T_due` reaches only its owner through the owner part — source: ADR-0019
- Guardrail: O(1) per Attack inside the queue line — source: ADR-0019

The tally is "what the player was already told": it counts every executed Attack whose `FStrikeResult.Result = WrongTarget`, whatever the victim kind and whether or not a strike was added. It is the only count a player ever sees, and it is delivered inside the plate's own payload so a C-TeamView update can never beat the plate to the screen (HUD-R7 gate, CR-12).

**Prototype replaced:** the prototype's per-kill message `Civilians killed: n/limit` and its `NpcKills` counter on `AStealthPlayerState` (`MessagePlayer`), which put a strike counter on screen and made an informant kill read differently. Removal is story 021.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-02, AC-NKP-39, AC-NKP-27), scoped to this story:*

- [ ] **AC1** (AC-NKP-02, [L] `StealthGame.Penalty.TallyRuleAndInvariant`): **GIVEN** Attack outcomes `RivalEliminated`, `WrongTarget` (NPC victim), `WrongTarget` (informant victim), `WrongTarget` (graced body), `NoContact`, `Reversed` **WHEN** processed **THEN** the tally increases by exactly 1 for each `WrongTarget` and 0 otherwise; **AND** over 10,000 sequences from `kPenaltyTestSeed`, `StrikeCount ≤ WrongTargetTally` after every event and exile occurs only when the tally ≥ L; **AND** a tally of 255 stays 255 after another `WrongTarget` (PEN-R3, F1).
- [ ] **AC2** (AC-NKP-39, [L] `StealthGame.Penalty.ResidualBoundary`): **GIVEN** L = 3, an Active attacker with k = 2 and tally 2 **WHEN** its next kill is (run a) an enemy informant and (run b) an ungraced NPC **THEN** run a gives `TallyAfter = 3`, `bExiled = false`, k = 2, and run b gives `TallyAfter = 3`, `bExiled = true`, k = 3 and the exile; the plates are otherwise identical (PEN-R14.1, F1). *The exile in run b is built in story 005; assert `bExiled` and k against story 005's call and `TallyAfter` here.*
- [ ] **AC3** (AC-NKP-27, [L] `StealthGame.Penalty.TallyRepushOnReclaim`): **GIVEN** tally 2 with `LastResultSeq` n **WHEN** the participant reclaims **THEN** the owner's C-TeamView push carries (2, n) and no other connection receives it; **AND** across three ordinary strikes no C-TeamView owner write of the tally occurs (PEN-R3).

---

## Implementation Notes

*Derived from ADR-0019 Decision §2.2, §2.2a and Implementation Guidelines:*

`UPenaltyService` implements ADR-0018's `IStrikeTallySource` and is registered weakly from the composition root. `GetTallyForResult` is called by Assassination after the fan-out for every executed Attack; it must be O(1), idempotent per `(Attack, Result)` call (the caller calls it once) and must never read `VictimKind`: the result alone decides the increment. The saturating add is a `uint8` clamp at 255. Assert `StrikeCount ≤ WrongTargetTally` after every update: development builds fault through the fault sink, Shipping logs once (no crash).

`bExiled` is true only for the Attack whose slot exiled the attacker (story 005 sets a slot-local flag that `GetTallyForResult` consumes); it is false in every other result and for every other Attack. Keep the flag slot-scoped, never a persistent record field, so a later `FStrikeResult` can never carry a stale `true`.

The re-push: on reclaim, rejoin (`OnViewRebound`) and `OnRosterReset`, call the Roster server face `SetOwnerTally(P, WrongTargetTally, LastResultSeq, ExpectedEpoch)` where `ExpectedEpoch` is the participant's current `RoundEpoch` (a stale write after a reset is `Rejected_StaleEpoch`). `LastResultSeq` is the `ClientCommandSeq` of the last `FStrikeResult` that changed the tally; store it with the tally. Never call `SetOwnerTally` per strike: the Roster cannot police this, so the discipline is Penalty's and AC3 is its test (the roster `story-038` asserts the Roster side, precedence and owner-only audience). HUD applies a re-push only when its `LastResultSeq` is at least the last plate shown and never lowers the displayed tally (CR-12; that is HUD's story).

Test seeds: randomised sequences use the fixed fixture seed `kPenaltyTestSeed`; the 10,000-sequence run must be deterministic. Fakes for the Roster owner-view feed record the call list, so "no other connection receives it" is asserted at the Roster-feed seam here and on the wire in story 016.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-002-strike-rule-and-release-grace.md`: the strike rule itself.
- `story-005-exile-sequence-in-attack-slot.md`: setting the slot-local exile flag and running the exile.
- `story-016-strike-count-never-leaves-and-removal-audiences.md`: the wire-level audit of what each connection receives.
- `story-017-owner-presentation-timing.md`: the client-side tally gate (AC-NKP-46).

---

## Files

*Changed by /dev-story 2026-10-27.*

- `Source/StealthGameplay/Features/Penalty/Private/StealthPenaltyCore.cpp` (tally, saturation, k <= t invariant, `LastResultSeq`, slot-local bExiled, re-push on reclaim, rejoin, reset)
- `Source/StealthGameplay/Features/Penalty/Private/StealthPenaltyService.cpp` (`IStrikeTallySource`: `GetTallyForResult` / `GetTallyForStrike`)
- `Source/StealthGameplay/Features/Penalty/Public/Server/StealthPenaltySeams.h` (`IPenaltyOwnerViewFeed`, the Roster owner-view feed seam until roster psr-038)
- `Source/StealthGameplay/Features/Assassination/Public/Server/StrikeTallySource.h` (`GetTallyForStrike` with the ClientCommandSeq, default forwards) and `Features/Assassination/Private/AttackResolverCore.cpp` (one call site)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyTallyRuleAndInvariantTest.cpp` (`StealthGame.Penalty.TallyRuleAndInvariant`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyResidualBoundaryTest.cpp` (`StealthGame.Penalty.ResidualBoundary`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyTallyRepushOnReclaimTest.cpp` (`StealthGame.Penalty.TallyRepushOnReclaim`)

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-02, `StealthGame.Penalty.TallyRuleAndInvariant`)
  - Given: the six outcomes (`RivalEliminated`, three `WrongTarget` variants, `NoContact`, `Reversed`) and `kPenaltyTestSeed`.
  - When: each is processed, then 10,000 seeded sequences run.
  - Then: tally + 1 only for the three `WrongTarget` cases; k <= t after every event; exile occurs only when t >= L; a tally of 255 stays 255.
  - Edge cases: the seed is a fixture constant, not inline; check the 256th `WrongTarget` and the uint8 wrap.
- **AC2** (AC-NKP-39, `StealthGame.Penalty.ResidualBoundary`)
  - Given: L = 3, k = 2, t = 2.
  - When: run a kills an enemy informant, run b kills an ungraced NPC.
  - Then: run a gives `TallyAfter` = 3, `bExiled` = false, k = 2; run b gives `TallyAfter` = 3, `bExiled` = true, k = 3 and the exile; the two `FStrikeResult`s differ in `bExiled` only once `ClientCommandSeq` is normalised.
  - Edge cases: a graced NPC kill at t = 2 gives tally 3 and no exile; repeat with L = 1 on the test preset.
- **AC3** (AC-NKP-27, `StealthGame.Penalty.TallyRepushOnReclaim`)
  - Given: t = 2 with `LastResultSeq` = n and a fake Roster that records `SetOwnerTally` calls.
  - When: P reclaims; separately, three ordinary strikes run.
  - Then: the reclaim records exactly one `SetOwnerTally(P, 2, n)` and none for other participants; three strikes record zero tally writes.
  - Edge cases: "no other connection receives it" is the Roster's audience job at the wire (story 016 and roster story 038); also assert the re-push on rejoin (`OnViewRebound`) and on `OnRosterReset` (ADR-0019 section 2.2a).

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three tests under `Source/StealthGameplay/Private/Tests/Penalty/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [x] Created and passing (2026-10-27): StealthGame.Penalty.TallyRuleAndInvariant, StealthGame.Penalty.ResidualBoundary, StealthGame.Penalty.TallyRepushOnReclaim; evidence production/qa/evidence/npc-kill-penalty-informants-001-004-2026-10-27/

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-001-penalty-core-store-and-service-shell.md`, `story-002-strike-rule-and-release-grace.md`; cross-epic: assassination-defence-reversal (`IStrikeTallySource` and `FStrikeResult { ClientCommandSeq, Result, TallyAfter, bExiled, ContactServerTime }`, ASN-R19), participant-state-roster `story-038-owner-view-feed.md` (`SetOwnerTally`)
- Unlocks: `production/epics/npc-kill-penalty-informants/story-005-exile-sequence-in-attack-slot.md`, `story-016-strike-count-never-leaves-and-removal-audiences.md`, `story-017-owner-presentation-timing.md`

---

## Dev Notes (2026-10-27)

**Readiness**: READY (depends on nkp-001/002 in this batch; `IStrikeTallySource` exists; roster psr-038 `SetOwnerTally` not built: asserted at the `IPenaltyOwnerViewFeed` seam, as the story says).

**Deviations**:
- `LastResultSeq` needs the Attack's `ClientCommandSeq`, which `GetTallyForResult(Attacker, Result)` does not carry: `IStrikeTallySource::GetTallyForStrike(Attacker, Result, Seq)` was added (default forwards to `GetTallyForResult`, so existing fakes are unchanged) and the resolver calls it (one line in `FAttackResolverCore::DeliverOwnerResults`). `LastResultSeq` moves only when the tally actually changed (not at 255).
- bExiled: the core computes F1's `Exile = (k' >= L) AND (S = Active)` in the strike path and holds it slot-local (consumed by the same slot's tally query); the exile sequence itself is nkp-005. In production the prototype's bExiled is still the one the plate carries (shadow wiring, see nkp-001).
- The `k <= t` invariant is asserted at the tally update (after the Attack's fan-out), since the strike lands one call before the tally in the same slot.
- Production has no owner-view feed yet (psr-038): `OnParticipantReclaimed` / `OnViewRebound` / `OnRosterReset` re-push into a null seam. Integration point for psr-038: pass its feed as `FPenaltyServiceInit::OwnerView`, and call `UPenaltyService::OnViewRebound` from the Roster's rejoin rebind.

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: 3/3 passing (AC2 asserts the core's F1 exile flag; the exile sequence is nkp-005; AC3 at the IPenaltyOwnerViewFeed seam until roster psr-038)
**Deviations**: advisory, listed under Dev Notes (folder layout, shadow tally wiring, deferred halves)
**Test Evidence**: production/qa/evidence/npc-kill-penalty-informants-001-004-2026-10-27/ (`tools/dev/cycle.ps1 -Tests -NoLaunch`: Result: Succeeded; 387/387 StealthGame. tests Success)
**Reviewed by**: self code review 2026-10-27 (APPROVED; no required changes)
