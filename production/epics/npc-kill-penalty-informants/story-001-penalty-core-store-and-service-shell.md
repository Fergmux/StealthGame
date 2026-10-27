# Story 001: Penalty core store, service shell and round-scoped carry-over

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (with ADR-0007 for module and header placement)
**ADR Decision Summary**: `UPenaltyService` (`StealthGameplay/Features/Penalty`, `Public/Server/`, a server-only `UWorldSubsystem`, `NotBlueprintType`) owns a plain-C++ `FPenaltyCore` holding one `FPenaltyRecord` per `ParticipantId` (strike count, wrong-target tally, life state, `t_lost`, `t_floor`, `T_due`, request sequence, retry bookkeeping, `ExcludePawn`) plus a round-scoped `ReleasedBodies` map. Nothing in it is a replicated property; it is cleared on `OnRosterReset` and frozen on `OnRosterRoundEnded`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0019 uses no engine API directly (Post-Cutoff APIs: none). All engine-facing work goes through `UResolutionQueue` (ADR-0002), `UStealthPawnFactory` (ADR-0010), `ULayoutService` (ADR-0014) and `UAppearanceAllocator` (ADR-0012); the takeover path inherits ADR-0010's HIGH-risk possession parity (spikes S1, S14, H1), which later stories in this epic exercise. This story touches no engine class beyond `UWorldSubsystem`.

**Control Manifest Rules (this layer)**:
- Required: Penalty state lives only in `FPenaltyCore` keyed by `ParticipantId`; every Penalty mutation runs inside a queue slot — source: ADR-0019
- Forbidden: never make Penalty state a replicated property; never let `StrikeCount` reach a client before `Ended`; never use an engine timer, feature tick or latent action in Penalty — source: ADR-0019
- Guardrail: Penalty is O(1) inside the 0.5 ms queue line — source: ADR-0019

This is the structural story of the epic. It creates the module folder, the headless core and the service shell that every later story fills in, and it settles the round-scoped lifetime of the stored values: keyed by `ParticipantId`, unchanged by disconnect and reclaim, cleared only on `OnRosterReset`, frozen at round end. The latched `NpcKillStrikeLimit` read (latched at `Preparing`, never changed mid-round) is part of this story because every later story reads `L` through it; the exile that consumes it is story 005.

**Prototype replaced:** the per-`PlayerState` `NpcKills` counter and the `Settings->NpcKillLimit` read in `AStealthGameMode` (`StealthGameMode.cpp`, the `OnAssassination` NPC branch). Nothing is promoted: the prototype counter lives on a replicated `PlayerState`, which the ADR forbids. Removal of the prototype code is story 021.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-13) and ADR-0019 §1, §3.3 (derived items marked), scoped to this story:*

- [ ] **AC1** (AC-NKP-13, [L] `StealthGame.Penalty.CarryOverAndReset`): **GIVEN** a participant with 2 strikes and tally 3, parameterised over (a) a reclaim inside `DisconnectGraceSec` (still `Active`, same pawn) and (b) a return after grace expiry (a returning informant) **WHEN** it disconnects, returns, and later `OnRosterReset` fires **THEN** in both cases strikes and tally are 2 and 3 after the return and 0 and 0 after the reset (PEN-R6).
- [ ] **AC2** (derived from AC-NKP-40's first half, PEN-R4 and TDC rule CR-11 Latching; the exile half is AC4 of story 005): **GIVEN** L = 3 latched at `Preparing` **WHEN** `NpcKillStrikeLimit` is changed to 4 during `Active` **THEN** Penalty's latched-limit read still returns 3 for the rest of the round and returns 4 after the next round's `Preparing`; and no literal limit value appears in Penalty sources.
- [ ] **AC3** (derived, ADR-0019 §1 and Implementation Guidelines 1-2): **GIVEN** `FPenaltyRecord` and `UPenaltyService` **WHEN** reflection and the include/source lint scan them **THEN** no property is `Replicated` or carries a replication condition, the service is `NotBlueprintType`, every `Public/Server` header starts with the server-only guard include, and `FPenaltyCore` runs in a test with no `UWorld`, `UObject` or engine singleton.
- [ ] **AC4** (derived, ADR-0019 §3.3 "any" rows): **GIVEN** `OnRosterRoundEnded` has fired **WHEN** a strike, tally write or respawn schedule is attempted **THEN** it is a no-op or rejected and the stored `StrikeCount`, tally and `LifeState` are unchanged (the `RoundEnded` freeze).

---

## Implementation Notes

*Derived from ADR-0019 Decision §1, §3.1 and Implementation Guidelines:*

`FPenaltyCore` (plain C++, `Private/Features/Penalty/`) holds `TMap<FParticipantId, FPenaltyRecord>` and the round-scoped `ReleasedBodies` map `FPawnHandle -> ReleaseServerTime` (`FPawnHandle` is ADR-0002's, in `StealthFoundation/Public/Server/StealthQueuePayloads.h`). The record fields are exactly the ADR-0019 §1 table: `StrikeCount` (uint8, ServerOnly), `WrongTargetTally` (uint8, saturating 255, Owner only through `FStrikeResult.TallyAfter`), `LifeState` (`EInformantLifeState`: `Active`, `Awaiting`, `Picking`, `Embodied`, `Absent`, `RoundEnded`), `LostServerTime`, `FloorServerTime`, `DueServerTime`, `RespawnRequestSeq`, `PickStartServerTime`, `LastWideningLevel`, `TimerHandle`, `ExcludePawn`. Add the fields now so later stories do not reshape the record; only the carry-over behaviour is asserted here.

`UPenaltyService` is a `UWorldSubsystem` in `Public/Server/` (guard include first), declared `UCLASS(NotBlueprintType)`. It owns one `FPenaltyCore` and adapts it. Seams are registered weakly from the composition root (`StealthGame`, ADR-0007): the service **implements** `IStrikeTallySource` (story 003), `IInformantBodyProvider` (stories 009-010) and `IQueueResolver` (story 009), and **subscribes** to `UEncounterService::OnPawnKilled`, the Roster connection events, `OnRosterRoundEnded` and `OnRosterReset`. It does **not** subscribe to `OnParticipantStateChanged` for scheduling (delegate order inside the flush is unspecified; ADR-0019 §1). This story creates the subscriptions as empty handlers with the carry-over and reset behaviour.

Reset and freeze: `OnRosterReset` returns every record to `Active(0)`, tally 0, new `RoundEpoch` (clearing `ReleasedBodies`, `ExcludePawn`, timers). `OnRosterRoundEnded` sets every record `RoundEnded` and every later mutator checks that state first. A disconnect, a reclaim and a travel within the round touch none of the stored counters (PEN-R6). Once a participant is `Informant` both counters are frozen (an informant cannot attack, IIF R7), which needs no Penalty code beyond not mutating them.

`L` is read through the Tuning service's server face (ADR-0004), latched at `Preparing` by TDC's own rule; Penalty never caches it across rounds and never reads a hard-coded 3. Gameplay values are data-driven (project coding standard): no literal limit in code.

Tests: all Logic tests inject a fake server clock, a stub Round queue, a fake Roster and an in-memory Event Log sink (dependency injection, no singletons; GDD "Test seams"). Test names follow `.claude/rules/test-standards.md` (`F<System><Scenario>Test`, name `StealthGame.Penalty.<Scenario>`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-002-strike-rule-and-release-grace.md`: the strike rule and the released-body grace.
- `story-003-tally-rule-and-strike-result-supply.md`: the tally rule and `FStrikeResult` supply.
- `story-005-exile-sequence-in-attack-slot.md`: the exile call stack.
- `story-009-respawn-due-time-scheduling-and-cancellation.md`: the `InformantRespawn` item, its resolver and the provider calls.

---

## Files

*Changed by /dev-story 2026-10-27 (folder layout: `Features/Penalty/{Public/Server,Private}`, like Assassination and Crowd; decision logged).*

- `Source/StealthGameplay/Features/Penalty/Public/Server/StealthPenaltyService.h` (UPenaltyService, server-bound UWorldSubsystem, NotBlueprintType, IStrikeTallySource)
- `Source/StealthGameplay/Features/Penalty/Public/Server/StealthPenaltySeams.h` (FPenaltyPawnKey, FPenaltyParams, the consumer-owned seams, fault details)
- `Source/StealthGameplay/Features/Penalty/Private/StealthPenaltyService.cpp` (adapters, OnPawnKilled and Roster subscriptions)
- `Source/StealthGameplay/Features/Penalty/Private/StealthPenaltyCore.h` and `StealthPenaltyCore.cpp` (FPenaltyRecord, FPenaltyCore: store, latch, freeze, reset, carry-over)
- `Source/StealthGameplay/StealthGameplay.Build.cs` (Features/Penalty include paths)
- `Source/StealthGame/Private/Composition/StealthCompositionRoot.cpp`, `Source/StealthGame/Public/Server/StealthCompositionRoot.h` (bind, Preparing latch, unbind before the queue/encounter core go)
- `Source/StealthGame/Private/Composition/StealthPenaltyWiring.h` and `.cpp` (FStealthShadowPenaltyTally stand-in until nkp-005)
- `tools/ci/lint/log-forbidden-identifiers.txt` (Penalty row types and StrikeCount)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyTestRig.h`, `PenaltySourceScan.h` (fakes, source scans)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyCarryOverAndResetTest.cpp` (`StealthGame.Penalty.CarryOverAndReset`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyLatchedLimitReadTest.cpp` (`StealthGame.Penalty.LatchedLimitRead`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyStructureLintTest.cpp` (`StealthGame.Penalty.StructureLint`, the QA case ServerOnlyShell)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyRoundEndedFreezeTest.cpp` (`StealthGame.Penalty.RoundEndedFreeze`, AC4)

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-13, `StealthGame.Penalty.CarryOverAndReset`, parameterised a/b)
  - Given: `FPenaltyCore` with P at k = 2, t = 3 and a second participant Q with its own counters; (a) P disconnects and reclaims inside `DisconnectGraceSec`; (b) grace expiry (Absent) then `OnInformantReadmitted`.
  - When: P disconnects and returns, then `OnRosterReset` fires.
  - Then: k = 2 and t = 3 after the return in both cases; 0 and 0 after the reset, LifeState Active; Q is untouched by P's disconnect (keyed by `ParticipantId`).
  - Edge cases: reset clears `ReleasedBodies` and `ExcludePawn`; k <= t holds throughout; the accessors are test-only server queries.
- **AC2** (derived from AC-NKP-40 first half, `StealthGame.Penalty.LatchedLimitRead`)
  - Given: fake tuning with `NpcKillStrikeLimit` = 3 latched at `Preparing`.
  - When: the live value changes to 4 during `Active`, then the next round's `Preparing` latches.
  - Then: Penalty's latched-limit read returns 3 for the rest of the round and 4 after the next `Preparing`; a source search finds no literal limit in Penalty sources.
  - Edge cases: assert zero reads of the live knob during the round; the exile consequence is story 005 AC4.
- **AC3** (derived, `StealthGame.Penalty.ServerOnlyShell`)
  - Given: `UPenaltyService`, `FPenaltyRecord`, `FPenaltyCore` and the Penalty `Public/Server` headers.
  - When: reflection, the ADR-0007 include lint and a headless instantiation of `FPenaltyCore` run.
  - Then: no `UPROPERTY` is Replicated, ReplicatedUsing or conditioned; the service is `NotBlueprintType` and `FPenaltyRecord` is not a `USTRUCT`; every header starts with the server-only guard include; the core test runs with no `UWorld`, `UObject` or singleton.
  - Edge cases: watch the lint fail once on a seeded header without the guard (a gate you have not watched fail is not a gate).
- **AC4** (derived, `StealthGame.Penalty.RoundEndedFreeze`)
  - Given: P at k = 1, t = 2, Awaiting with one pending item, then `OnRosterRoundEnded`.
  - When: a strike, a `GetTallyForResult(WrongTarget)`, an `OnInformantAwaitingBody` call and an item resolve are attempted.
  - Then: each is a no-op or `Rejected_Latched`; k, t, LifeState (`RoundEnded`) and the pending-item count (0) are unchanged.
  - Edge cases: a tally write inside the deciding slot before `OnRosterRoundEnded` still succeeds (the exiling kill's `FStrikeResult` needs `TallyAfter`), so fire the round end only after `Resolve` returns; a double `OnRosterRoundEnded` is idempotent; `OnRosterReset` unfreezes.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three tests under `Source/StealthGameplay/Private/Tests/Penalty/` (`tests/unit/penalty/` in the studio docs) — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [x] Created and passing (2026-10-27): StealthGame.Penalty.CarryOverAndReset, StealthGame.Penalty.LatchedLimitRead, StealthGame.Penalty.StructureLint, StealthGame.Penalty.RoundEndedFreeze; evidence production/qa/evidence/npc-kill-penalty-informants-001-004-2026-10-27/

---

## Dependencies

- Depends on: cross-epic: tuning-data-config `story-002-gameplay-presentation-module-skeletons.md`, `story-008-fault-sink-and-test-seams-define.md`, `story-040-canonical-keys-and-faces.md` (the `NpcKillStrikeLimit` key and the server tuning face); participant-state-roster (`OnRosterReset`, `OnRosterRoundEnded`, connection events, stories 022-023); round-match-framework (declares `IInformantBodyProvider` and the provider registration that finds Penalty; no story path yet)
- Unlocks: `production/epics/npc-kill-penalty-informants/story-002-strike-rule-and-release-grace.md`, `story-003-tally-rule-and-strike-result-supply.md`, `story-004-event-log-rows.md`, `story-005-exile-sequence-in-attack-slot.md`, `story-009-respawn-due-time-scheduling-and-cancellation.md`

---

## Dev Notes (2026-10-27)

**Readiness**: READY (QL-STORY-READY ADEQUATE; ADR-0019 Accepted; story manifest 2026-10-25f vs current 2026-10-25g: the g changes touch ADR-0002/0003/0013/0017 only, Penalty rules unchanged). Cross-epic dependencies not built are handled by consumer-owned seams: the Roster owner-view feed (psr-038) is `IPenaltyOwnerViewFeed` (null in production), the TDC frozen store is `IPenaltyTuningSource` (production stand-in: the schema defaults, as the encounter service), `IInformantBodyProvider` / the InformantRespawn resolver arrive with nkp-009/010.

**Deviations**:
- Folder layout `Source/StealthGameplay/Features/Penalty/{Public/Server,Private}` (the Assassination and Crowd layout) instead of `Public/Server/Features/Penalty/`; Build.cs include paths added.
- The production service is bound alongside the prototype penalty (user rule: the prototype stays until nkp-021). The encounter service's tally source is `FStealthShadowPenaltyTally`: the prototype still answers (its exile sets bExiled), and every query also reaches `UPenaltyService` so its tally and rows stay in step with the strikes it counts. nkp-005 registers `UPenaltyService` itself.
- AC3's QA name `ServerOnlyShell` is implemented as `StealthGame.Penalty.StructureLint` (the Files name); AC4 has its own test `StealthGame.Penalty.RoundEndedFreeze`.
- AC4's respawn half (`OnInformantAwaitingBody` and the item resolve returning `Rejected_Latched`, pending-item count 0) needs nkp-009's provider and resolver: asserted there. The freeze gate (`IsFrozen`) is the first check of every mutator.
- AC3 "every Public/Server header starts with the guard": scanned by the test over `Features/Penalty/Public/Server` (the CI include lint (b) scans `Source/<M>/Public/Server` only).

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: 4/4 passing (AC4's respawn-schedule half DEFERRED to nkp-009: provider and resolver do not exist yet)
**Deviations**: advisory, listed under Dev Notes (folder layout, shadow tally wiring, deferred halves)
**Test Evidence**: production/qa/evidence/npc-kill-penalty-informants-001-004-2026-10-27/ (`tools/dev/cycle.ps1 -Tests -NoLaunch`: Result: Succeeded; 387/387 StealthGame. tests Success)
**Reviewed by**: self code review 2026-10-27 (APPROVED; no required changes)
