# Story 005: Exile sequence inside the killing Attack's slot

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§2.3; ADR-0002 for the slot and key order)
**ADR Decision Summary**: When `k' ≥ L` and the killer is `Active`, one call stack inside the Attack's slot runs: Roster `RequestTransition(P, Informant, StrikeLimit, ExpectedEpoch)` (the Roster releases the pawn to the crowd inside the transition), `ReleaseCard(Pawn)` (`BoundLive -> NpcLive`, same `AppearanceId`), Crowd `AdoptReleasedPawn(Pawn, {RestrictedUntilServerTime = strike lockout end, bExitAtNextScheduledSlot = true})`, `RequestHandOver(Pawn, Ai, ExpectedEpoch)`, then `ReleasedBodies[Pawn] = slot time`, `ExcludePawn = Pawn` and `bExiled` for this slot's result. The respawn is not scheduled in this stack: Round's `OnInformantAwaitingBody(P, StrikeLimit, slot time)` does that after its elimination check.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The sequence itself uses no engine API; it calls Roster, Character Pool, Crowd and the pawn factory. The factory's `RequestHandOver(Ai)` commits the control-epoch bump now but the possession executes after the pawn's strike lockout (a root-motion pose), an amendment request to ADR-0010 ("never during a traversal" extended to root-motion action poses); the PlayerController's unpossess must not cut the owner's view (ADR-0019 §4.4, ADR-0015). Inherits ADR-0010's HIGH-risk possession parity (spikes S1, S14, H1).

**Control Manifest Rules (this layer)**:
- Required: the exile runs `RequestTransition(StrikeLimit)`, `ReleaseCard`, `AdoptReleasedPawn` and `RequestHandOver(Ai)` in one call stack inside the killing Attack's slot — source: ADR-0019
- Forbidden: never leave a surviving exiled body `Retired`; never schedule a respawn from a Penalty subscription to Roster state events, or when latched — source: ADR-0019
- Guardrail: O(1) inside the 0.5 ms queue line — source: ADR-0019

The exile is the penalty's visible consequence: the `NpcKillStrikeLimit`-th ungraced NPC kill removes the player from `Active`. The wrong read has "its lasting cost" only if the exile is exact (it fires on that kill, not the next), single-slot (no frame in which the body is `Retired` and walking) and silent to everyone but the exiled owner.

**Prototype replaced:** `AStealthGameMode::ExilePlayer` and the NPC-kill branch of `AStealthGameMode` (limit check, `ExilePlayer(Attacker)`). The prototype's behaviour is the acceptance reference: prototype networked test A (third civilian kill exiles the player while a teammate is Active) passes today and is ported as a CQTest PIE test in story 021. The prototype keeps the exiled pawn as the informant; production releases it to the crowd and gives the participant a different body.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-05, AC-NKP-16, AC-NKP-34, AC-NKP-40), scoped to this story:*

- [ ] **AC1** (AC-NKP-05, [L] `StealthGame.Penalty.ExileOnLthNpcKill`, parameterised L = 2 to 5, plus L = 1 on a test preset with TDC's `bTestAllowsStrikeLimitOne`): **GIVEN** an Active participant with L − 1 strikes **WHEN** its next ungraced NPC kill resolves **THEN** in the same queue slot the fake gateways record, in order, `RequestTransition(P, Informant, StrikeLimit, epoch)` (Applied) once, `ReleaseCard(Pawn)`, `AdoptReleasedPawn(Pawn, {RestrictedUntilServerTime = lockout end, bExitAtNextScheduledSlot = true})` and `RequestHandOver(Pawn, Ai, epoch)`; `ReleasedBodies[Pawn]` and `ExcludePawn` are set and the body is never left Retired; `bExiled = true` in that Attack's `FStrikeResult` and in no other; `StrikeCounted` is written with that slot's key; and, after the stub Round's `OnInformantAwaitingBody(P, StrikeLimit, slot time)`, `RespawnScheduled` carries that slot's key (PEN-R4, F1; ADR-0019 §2.3 steps 1-5; the `StrikeLimit` row is Round's and is asserted in story 007's integration run with the real Round; the mid-round latch is AC4).
- [ ] **AC2** (AC-NKP-16, [L] `StealthGame.Penalty.CommandsAfterExile`): **GIVEN** an exiled participant **WHEN** its client sends `Attack`, `Prepare`, `Interact`, `Hide` or `Traverse` before its new body is bound **THEN** each is voided `NotNow` and the released body receives none of them (PEN-R4).
- [ ] **AC3** (AC-NKP-34, [L] `StealthGame.Penalty.SameDrainRaces`): **GIVEN**, in both key orders, P's L-th NPC kill and P's own assassination in one drain **WHEN** the drain resolves **THEN** kill first: exile, then the rival's strike is a graced `WrongTarget` with tally +1 and no strike; assassination first: P's Attack is voided `NotNow` and the NPC lives (Edge Cases; the takeover-tick race is story 012).
- [ ] **AC4** (AC-NKP-40, [L] `StealthGame.Penalty.StrikeLimitLatched`): **GIVEN** L = 3 latched at `Preparing` **WHEN** `NpcKillStrikeLimit` is changed to 4 during `Active` and the participant's third ungraced NPC kill resolves **THEN** the exile fires on that kill; **AND** the next round's `Preparing` latches 4 (PEN-R4, TDC rule CR-11 Latching).

---

## Implementation Notes

*Derived from ADR-0019 Decision §2.3 and Implementation Guidelines:*

Extend `UPenaltyService::OnPawnKilled` (story 002): when the strike rule leaves `k' ≥ L` (with `L` from story 001's latched read) and `State = Active`, run the exile as one function, `RunExile(P, Pawn, SlotTime)`, with no `await`, no timer and no early return after step 1. Step order, exactly as ADR-0019 §2.3:
1. `Roster.RequestTransition(P, Informant, StrikeLimit, ExpectedEpoch)`; the Roster performs `ReleaseToCrowd(Pawn)` inside it (roster `story-014-transition-active-to-informant.md`: `StrikeLimit` is "release, not retire"), so the pawn is `Unbound` from here on. If the transition is rejected, report to the fault sink and run nothing else.
2. `UAppearanceAllocator::ReleaseCard(Pawn)` (`BoundLive -> NpcLive`, same `AppearanceId`).
3. `ICrowdControl::AdoptReleasedPawn(Pawn, {RestrictedUntilServerTime = the pawn's strike lockout end, bExitAtNextScheduledSlot = true})`. The lockout end is `contact slot time + AttackAttemptLockoutSec` read from Assassination's action state, never recomputed here.
4. `UStealthPawnFactory::RequestHandOver(Pawn, Ai, ExpectedEpoch)`.
5. `ReleasedBodies[Pawn] = slot time`; `ExcludePawn = Pawn`; set the slot-local `bExiled` flag that story 003's `GetTallyForResult` consumes.
6. Do **not** schedule the respawn here: the Roster event of step 1 is dispatched in the slot's Roster flush after `Resolve` returns, where Round runs its elimination check and, if the round did not latch, calls `IInformantBodyProvider::OnInformantAwaitingBody(P, StrikeLimit, slot time)` (story 009). After a latch Round makes no call (story 007).
7. Event Log: `StrikeLimit` is Round's row, written with the transition; `RespawnScheduled` when story 009's path runs. `StrikeCounted` is story 002's.

The participant's commands after the exile are voided `NotNow` because it has no pawn (IIF legality, `input-interaction-framework` `story-057-roster-legality-class.md`); AC2 is a regression guard that Penalty adds nothing that lets a command reach the released body. The released pawn is now `Unbound`, so the command ingress finds no binding for P.

Same-drain ordering is the queue's key order (ADR-0002 §4): do not add any ordering logic in Penalty; AC3 asserts the queue's behaviour through Penalty's handlers. For AC4 the exile reads the *latched* `L` captured at `Preparing`, never the live tuning value.

`L = 1` is only reachable through the TDC dev/test flag `bTestAllowsStrikeLimitOne` on a non-selectable test preset; the parameterised test uses a test preset for that case.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-006-exile-releases-body-to-crowd.md`: the released body's behaviour after the call (resolve kind, exit routine, exclusion from `PickRespawnPawn`).
- `story-007-exile-latches-round-and-double-exile.md`: exile of a team's last Active player and two exiles in one drain.
- `story-009-respawn-due-time-scheduling-and-cancellation.md`: the respawn item scheduled from Round's provider call.
- `story-017-owner-presentation-timing.md`, `story-018-tally-countdown-and-exile-card-ui.md`: the owner's plate, card and camera release.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Penalty/StealthPenaltyService.cpp` (`RunExile`)
- `Source/StealthGameplay/Private/Features/Penalty/StealthPenaltyCore.cpp` (exile decision, `ReleasedBodies`, `ExcludePawn`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyExileOnLthNpcKillTest.cpp` (class `FPenaltyExileOnLthNpcKillTest`, name `StealthGame.Penalty.ExileOnLthNpcKill`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyCommandsAfterExileTest.cpp` (class `FPenaltyCommandsAfterExileTest`, name `StealthGame.Penalty.CommandsAfterExile`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltySameDrainRacesTest.cpp` (class `FPenaltySameDrainRacesTest`, name `StealthGame.Penalty.SameDrainRaces`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyStrikeLimitLatchedTest.cpp` (class `FPenaltyStrikeLimitLatchedTest`, name `StealthGame.Penalty.StrikeLimitLatched`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-05, `StealthGame.Penalty.ExileOnLthNpcKill`, L = 2..5 plus the L = 1 test preset)
  - Given: fake Roster, Character Pool, Crowd and factory gateways that record call order; an Active P at k = L - 1.
  - When: its next ungraced NPC kill resolves in one slot.
  - Then: recorded in order `RequestTransition(P, Informant, StrikeLimit, epoch)` once, `ReleaseCard`, `AdoptReleasedPawn({lockout end, bExitAtNextScheduledSlot = true})`, `RequestHandOver(Ai, epoch)`; `ReleasedBodies[Pawn]` and `ExcludePawn` set; the body is never Retired; `bExiled = true` in this Attack's `FStrikeResult` only; `StrikeCounted` carries the slot key; after the stub Round's `OnInformantAwaitingBody(P, StrikeLimit, t)`, `RespawnScheduled` carries the same key.
  - Edge cases: a graced kill at k = L - 1 does not exile; L = 1 needs `bTestAllowsStrikeLimitOne`; the `StrikeLimit` row is not asserted here (story 007).
- **AC2** (AC-NKP-16, `StealthGame.Penalty.CommandsAfterExile`)
  - Given: an exiled P with no new body bound, a fake legality gateway and a command sink.
  - When: its client sends `Attack`, `Prepare`, `Interact`, `Hide` and `Traverse`.
  - Then: each is voided `NotNow` and the released body's sink records 0 deliveries.
  - Edge cases: commands sent in the exile's own frame; the same commands after rebinding as an informant.
- **AC3** (AC-NKP-34, `StealthGame.Penalty.SameDrainRaces`, both key orders)
  - Given: P's L-th NPC kill and a rival's assassination of P in one drain.
  - When: the drain resolves.
  - Then: kill first: exile, then the rival's strike is a graced `WrongTarget` (rival tally + 1, no strike); assassination first: P's Attack is voided `NotNow` and the NPC lives.
  - Edge cases: the graced strike is inside the 2.0 s inclusive window; P's tally is unchanged in the assassination-first order.
- **AC4** (AC-NKP-40, `StealthGame.Penalty.StrikeLimitLatched`)
  - Given: L = 3 latched at `Preparing`, P at k = 2, a fake TDC.
  - When: the live value changes to 4 during `Active` and P's third ungraced NPC kill resolves.
  - Then: the exile fires on that kill; after the next `Preparing` latches 4 the third kill does not exile and the fourth does.
  - Edge cases: assert zero reads of the live knob mid-round.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the four tests under `Source/StealthGameplay/Private/Tests/Penalty/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-001-penalty-core-store-and-service-shell.md`, `story-002-strike-rule-and-release-grace.md`, `story-003-tally-rule-and-strike-result-supply.md`, `story-004-event-log-rows.md`; cross-epic: participant-state-roster `story-014-transition-active-to-informant.md`, character-pool-appearance (`ReleaseCard`), crowd-npc-behaviour (`AdoptReleasedPawn`), shared-character-locomotion (`RequestHandOver`)
- Unlocks: `production/epics/npc-kill-penalty-informants/story-006-exile-releases-body-to-crowd.md`, `story-007-exile-latches-round-and-double-exile.md`, `story-009-respawn-due-time-scheduling-and-cancellation.md`
