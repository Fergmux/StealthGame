# Story 012: Respawn commit sequence — pick, pre-check, release, takeover, card

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§4.1-§4.2), with ADR-0010 (`TakeOverCrowdPawn`), ADR-0012 (`TakeNpcCard`), ADR-0014 §7.3 (`PickRespawnPawn`) and ADR-0016 §8 (`TryReleaseNpc`)
**ADR Decision Summary**: When the `InformantRespawn` item resolves, in one call stack: re-validate (phase `Active`, epoch, Roster state `Informant`, no live pawn, connection not `Disconnected`); `PickRespawnPawn(Team, RespawnRequestSeq, Exclude)` (the only random draw, at most one pick per slot); `CheckTakeOverEligibility` pre-check; Crowd `TryReleaseNpc`; `TakeOverCrowdPawn`; only on `Applied`, `TakeNpcCard` then Roster `RecordAppearance`; then bookkeeping. A released pawn not taken over is returned with `AdoptReleasedPawn` in the same stack. No actor is created.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Penalty itself calls no engine API; the commit inherits ADR-0010's HIGH-risk possession parity (spikes S1, S14, H1; reported 2026-10-25 for the prototype's possession paths, `production/qa/spikes/2026-10-25_S1-S2-S14.md`). Several interface confirmations are requested of other ADRs and may not exist yet: `FRespawnPick` with `WideningLevel` (ADR-0014), `CheckTakeOverEligibility` (ADR-0010 amendment request), `AdoptReleasedPawn` exactly reversing `TryReleaseNpc` (ADR-0016), `ReleaseControl` idempotent on an already-released pawn (ADR-0010). Code against the interfaces as the ADRs state them and keep each behind a fake in the Logic tests.

**Control Manifest Rules (this layer)**:
- Required: commit order pick -> eligibility pre-check -> `TryReleaseNpc` -> `TakeOverCrowdPawn` -> (`Applied` only) `TakeNpcCard` -> `RecordAppearance` in one call stack; a released pawn not taken over is handed back with `AdoptReleasedPawn` in the same stack — source: ADR-0019
- Forbidden: never commit a card move for any takeover result other than `Applied`; never move the card before the takeover; never have Penalty call `BindPawn` itself — source: ADR-0019
- Guardrail: respawn pick is ADR-0014's budget, at most one pick per frame — source: ADR-0019

The order is what makes the takeover safe to fail: nothing has to be reverted for a failed takeover because no card moved. The test fakes fail `TryReleaseNpc` once and `TakeOverCrowdPawn` once to prove the order, the one-pick-per-slot rule, the hand-back and the stable `RequestSeq`.

**Prototype replaced:** `AStealthGameMode::RespawnInformant` (picks a random unoccupied crowd NPC and possesses it with no unseen test, no atomic release, no card conservation and no retry). The prototype's takeover is the behavioural reference for "an informant ends up in an existing crowd pawn"; none of its code is promoted.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-30, AC-NKP-33, AC-NKP-42), scoped to this story:*

- [ ] **AC1** (AC-NKP-30, [L] `StealthGame.Penalty.CommitSequence`): **GIVEN** fake gateways where `TryReleaseNpc` fails once and `TakeOverCrowdPawn` fails once **WHEN** a respawn commits **THEN** per attempt the call order is pick, pre-check, release, takeover, card; there are three picks in three different frames (one pick per slot: attempt 1 fails the release, attempt 2 fails the takeover, attempt 3 succeeds); the takeover failure sends exactly one defect to the fault sink (once the pre-check exists a takeover `NotEligible` is a defect) and the pawn goes back through `AdoptReleasedPawn` with no card move; no card moves for a failed attempt and one card move follows attempt 3's `Applied`; and every attempt reuses the same `RequestSeq` (PEN-R9). *(GDD AC-NKP-30 says "two picks"; one release failure plus one takeover failure force three: report to the GDD owner.)*
- [ ] **AC2** (AC-NKP-33, [L] `StealthGame.Penalty.SameTeamDueOneDrain`): **GIVEN** two same-team informants due in one drain and a single eligible pawn **WHEN** both resolve **THEN** the lower key takes it and the other retries; the same pawn is never taken twice (PEN-R9, Edge Cases).
- [ ] **AC3** (AC-NKP-42, [L] `StealthGame.Penalty.TakeoverTickRace`): **GIVEN** a strike on a takeover candidate in the takeover's commit tick, in both key orders **WHEN** the drain resolves **THEN** attack first: the pawn is dead when the respawn slot runs, the pre-check returns `NotEligible`, the pawn is excluded, a retry is scheduled at `ServerNow + 1 ms` (implemented as `ServerNow + 2 ms`, `FPenaltyCore::NextFrameRetryOffsetSec`: the queue stamps every item due at or before DrainStart + 1 ms in the same drain, so 1 ms would re-run in this drain; 2 ms is the smallest offset strictly after it, the ADR's intent; readiness 2026-10-27) with the same `RequestSeq` and no card moves; takeover first: the strike is an informant kill (`WrongTarget`, tally +1, no strike) and a new `T_due` is scheduled (Edge Cases).

---

## Implementation Notes

*Derived from ADR-0019 Decision §4.1, §4.2 and Implementation Guidelines:*

The resolver body for `ETimerKind::InformantRespawn` (registered in story 009) runs steps exactly as ADR-0019 §4.1:
1. **Re-validate**: phase `Active`, epoch matches, Roster state `Informant`, no live pawn, connection not `Disconnected`. Otherwise void and log `RespawnCancelled{reason}`: move to `Absent` (disconnect, story 010) or stop (round over).
2. **Pick**: `ULayoutService::PickRespawnPawn(Team, RespawnRequestSeq, Exclude)` where `Exclude` = the participant's own released exile body (`ExcludePawn`) plus this request's exclusion set. `Deferred` (the per-frame guard) -> story 013's next-frame retry with no widening time consumed; `None` -> story 013's retry.
3. **Pre-check**: `UStealthPawnFactory::CheckTakeOverEligibility(R.Pawn)` (side-effect free; the same predicate as `TakeOverCrowdPawn` step 1). `NotEligible` -> add the pawn to the exclusion set and next-frame retry.
4. **Crowd release**: `ITakeoverEligibilitySource::TryReleaseNpc(R.Pawn, Expected)` (crowd `story-030-takeover-eligibility-and-atomic-release.md`: atomic `Eligible` re-check, sink latched, StateTree stopped, ledger B += 1). Penalty is its **only** caller. `false` -> exclude and next-frame retry.
5. **Takeover**: `UStealthPawnFactory::TakeOverCrowdPawn(R.Pawn, P, ExpectedEpoch)` (shared-character-locomotion `story-028-informant-respawn-takes-over-unseen-crowd-pawn.md`; the factory calls `BindPawn` on Penalty's behalf). On `NotEligible` call `ICrowdControl::AdoptReleasedPawn(R.Pawn, {})` (the exact reverse of `TryReleaseNpc`), exclude and next-frame retry; once the step-3 pre-check exists `NotEligible` here is a defect (fault sink), until then only logged. A `BindPawn` rejection (`Rejected_StaleEpoch` / `WrongState`) means the request is stale: return the pawn the same way and void.
6. **Card**: only after `Applied`, `UAppearanceAllocator::TakeNpcCard(P, R.Pawn)` (`NpcLive -> BoundLive`, `AppearanceId` untouched), then Roster `RecordAppearance(P, AppearanceId)`. If `TakeNpcCard` fails after `Applied` the takeover stands: report `LedgerMismatch` to the fault sink, no rollback.
7. **Bookkeeping**: state `Embodied`, log `InformantRespawn{P, PickLatencySec = now - T_due, WideningLevel}`, clear `ExcludePawn`, the exclusion set and the request, raise `OnDueTimeChanged(P, unset)` (story 011).

Two informants due in one drain resolve in key order (ADR-0002 §4, TieRank for equal due times); with the one-pick-per-frame guard the second gets `Deferred` and retries next frame, where the first's pawn is `Bound` and ineligible (Crowd `Bound` reason; LSC "claimed this tick"). The takeover-tick race (AC3) is decided entirely by queue key order and the factory's atomic check: Penalty adds no ordering code and the test asserts both orders through the stub queue.

Each fake gateway records its call list with the frame index; the test asserts the list, the picks' frames, the absence of any card move on failure, and `RequestSeq` equality across retries (retry mechanics are story 013's). Test seeds: fixed `RequestSeq` sub-seeds for the LSC draw.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-013-starvation-retries-and-widening.md`: retry scheduling, widening, `RespawnStarved`.
- `story-014-one-respawn-path-and-takeover-wire-parity.md`: the PIE proof that no pawn is spawned and one card move occurs, and wire parity.
- level-socket-contract `story-033-respawn-pick-eligibility-unseen-draw.md`: the pick itself.
- crowd-npc-behaviour `story-030-takeover-eligibility-and-atomic-release.md`: `TryReleaseNpc`, `AdoptReleasedPawn`.

---

## Files

*The code and test files this story changed (`/dev-story` 2026-10-27). The commit lives in `FPenaltyCore::RunCommit` (no separate `StealthPenaltyCommit` file: it shares the record and the retry bookkeeping).*

- `Source/StealthGameplay/Features/Penalty/Private/StealthPenaltyCore.h`, `StealthPenaltyCore.cpp` (`RunCommit`, `ScheduleRetry`, `VoidRequest`, `NextFrameRetryOffsetSec`)
- `Source/StealthGameplay/Features/Penalty/Public/Server/StealthPenaltySeams.h` (`IPenaltyRespawnGateway`, `FPenaltyRespawnPick`, `EPenaltyTakeoverResult`, fault details)
- `Source/StealthGameplay/Public/Server/Core/StealthPawnFactory.h`, `Private/Core/PawnFactory/StealthPawnFactory.cpp` (`RequestHandOverToPlayer`: the commit now, the possession at the next frame; stand-in for the SCL hand-over pass)
- Composition: `Source/StealthGame/Private/Composition/StealthPenaltyWiring.h/.cpp` (`FStealthPenaltyRespawnGateway`: STAND-IN pick for LSC story 033, STAND-IN takeover for SCL story 028, STAND-IN card for the Character Pool; Crowd `TryReleaseNpc` / `AdoptReleasedPawn`), `StealthCompositionRoot.cpp`; `Source/StealthGame/Private/StealthGameMode.cpp` (the exile's stand-in respawn and the prototype informant respawn stand down while the service is bound)
- Tests: `Source/StealthGameplay/Private/Tests/Penalty/PenaltyCommitSequenceTest.cpp` (`StealthGame.Penalty.CommitSequence`), `PenaltySameTeamDueOneDrainTest.cpp` (`StealthGame.Penalty.SameTeamDueOneDrain`), `PenaltyTakeoverTickRaceTest.cpp` (`StealthGame.Penalty.TakeoverTickRace`)

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-30, `StealthGame.Penalty.CommitSequence`)
  - Given: fake gateways where `TryReleaseNpc` fails once, then `TakeOverCrowdPawn` fails once, then succeeds.
  - When: a respawn commits.
  - Then: per attempt the order is pick, pre-check, release, takeover, card; three picks in three frames; exactly one defect reaches the fault sink (the takeover failure); `AdoptReleasedPawn` returns that pawn with no card move; one card move follows attempt 3's `Applied`; every attempt reuses the same `RequestSeq`.
  - Edge cases: the GDD's "two picks" is wrong (the story says so); a pick that returns `Deferred` costs a frame but not an attempt.
- **AC2** (AC-NKP-33, `StealthGame.Penalty.SameTeamDueOneDrain`)
  - Given: same-team informants P1 and P2 due in one drain; a fake LSC with one eligible pawn X and the per-frame guard.
  - When: both resolve.
  - Then: P1 (lower key) completes the full commit and becomes Embodied; P2 gets `Deferred` and is rescheduled at `ServerNow + 1 ms` with the same `RequestSeq`; at that retry X is Bound so the pick returns `None` and the next retry is after `EmergenceRetrySec`; `TakeOverCrowdPawn` is called once for X.
  - Edge cases: swap the keys; equal due times resolve by `TieRank`.
- **AC3** (AC-NKP-42, `StealthGame.Penalty.TakeoverTickRace`, both key orders)
  - Given: a strike on the takeover candidate in the commit tick.
  - When: the drain resolves.
  - Then: attack first: the pawn is dead when the respawn slot runs, the pre-check returns `NotEligible`, the pawn is excluded, a retry is scheduled at `ServerNow + 1 ms` with the same `RequestSeq`, no card moves; takeover first: the strike is an informant kill (`WrongTarget`, tally + 1, no strike) and a new `T_due = max(t_lost + D, floor, now)` is scheduled.
  - Edge cases: the takeover-first victim kind comes from the Roster binding.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three tests under `Source/StealthGameplay/Private/Tests/Penalty/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [x] Created and passing (2026-10-27): StealthGame.Penalty.CommitSequence, SameTeamDueOneDrain, TakeoverTickRace; evidence production/qa/evidence/npc-kill-penalty-informants-009-012-2026-10-27/

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-009-respawn-due-time-scheduling-and-cancellation.md`, `story-010-absent-path-readmission-and-lost-body.md`, `story-008-informant-kill-inline-retire.md` (AC3's takeover-first run); cross-epic: level-socket-contract `story-033-respawn-pick-eligibility-unseen-draw.md`, crowd-npc-behaviour `story-030-takeover-eligibility-and-atomic-release.md`, shared-character-locomotion `story-028-informant-respawn-takes-over-unseen-crowd-pawn.md`, character-pool-appearance (`TakeNpcCard`), participant-state-roster (`BindPawn`, `RecordAppearance`)
- Unlocks: `production/epics/npc-kill-penalty-informants/story-013-starvation-retries-and-widening.md`, `story-014-one-respawn-path-and-takeover-wire-parity.md`

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: AC1-AC3 passing (AC1: three picks, as the story corrects the GDD's two)
**Deviations**: next-frame retry ServerNow + 2 ms (1 ms would re-run in the same drain; ADR-0019 note added); STAND-INs behind IPenaltyRespawnGateway: pick (LSC 033), takeover (SCL 028; factory RequestHandOverToPlayer), card (Character Pool), plus a team-start spawn fallback used only in a world with no crowd agent (the 0-NPC harness rounds)
**Test Evidence**: production/qa/evidence/npc-kill-penalty-informants-009-012-2026-10-27/ (`tools/dev/cycle.ps1 -Tests -NoLaunch`: Result: Succeeded; 421/421 StealthGame. tests Success); run-and-observe OBSERVED in 3-player PIE (screenshots 01-05)
**Reviewed by**: self code review 2026-10-27 (APPROVED; notes in the evidence)
