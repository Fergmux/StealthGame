# Evidence: NPC-Kill Penalty & Informants stories 001-004 (2026-10-27)

Stories: `production/epics/npc-kill-penalty-informants/story-001..004`. Type: Logic (all four).
Build: `tools/dev/cycle.ps1 -Tests -NoLaunch` -> `Result: Succeeded`, `tests exit=0`, 387 / 387 `StealthGame.` tests
Success (13 new `StealthGame.Penalty.*`). Full output: `full-suite.txt`. Stamp: `stamp.txt`.
Project files: `Build.bat -projectfiles ... -game -rocket -progress` -> `Result: Succeeded` (Build.cs changed).
Schema: `Stealth.Tuning.SchemaGen` -> `UNCHANGED` (the hand-added row and fingerprint equal the generator's output);
`StealthGame.Tuning.SchemaVersionBumped`, `SchemaComplete`, `SchemaGenDeterministic` pass at `TuningSchemaVersion` 5.

## What was built

| Story | Built | Tests |
|---|---|---|
| 001 core store, service shell | `FPenaltyCore` (plain C++: records keyed by ParticipantId, latch at Preparing, freeze at OnRosterRoundEnded, reset at OnRosterReset, released bodies) and `UPenaltyService` (server-bound UWorldSubsystem, NotBlueprintType, IStrikeTallySource, OnPawnKilled + Roster event subscriptions) in `Features/Penalty/`; bound by the composition root alongside the prototype penalty | `CarryOverAndReset`, `LatchedLimitRead`, `StructureLint`, `RoundEndedFreeze` |
| 002 strike rule, release grace | F1 strike rule (Strike, Npc, Active, not graced), inclusive `StrikeOutReleaseGraceSec` grace, Informant-killer fault, new ServerOnly key `FPenaltyTuning_Server::StrikeOutReleaseGraceSec` (schema version 5) | `StrikeRuleMatrix` (36 cells), `ReleaseGraceBoundary` (1.999 / 2.000 / 2.001 s), `ReversalAtLimit` |
| 003 tally, FStrikeResult supply | saturating tally on WrongTarget only, k <= t fault, slot-local bExiled from F1's exile predicate, LastResultSeq (via the new `IStrikeTallySource::GetTallyForStrike`), owner re-push on reclaim / rejoin / reset through `IPenaltyOwnerViewFeed` | `TallyRuleAndInvariant` (10,000 seeded sequences), `ResidualBoundary`, `TallyRepushOnReclaim` |
| 004 Event Log rows | five fixed-size rows, encode/decode, `FPenaltyEventLog` (slot-only writer), `ERespawnCause`, the five types PostRoundEligible, include-lint rule (i) names | `EventLogRows`, `EventLogRowSchema`, `NoLoggingLint` |

## Gates watched failing (negative controls, "control detected" in full-suite.txt's test log)

- `SeededLiteralLimit` (LatchedLimitRead): a seeded `StrikeLimit = 3` and `NpcKillLimit` are found.
- `HeaderWithoutGuard` (StructureLint): a header whose first include is not the guard is found.
- `SeededPenaltyLogCall` (NoLoggingLint): a seeded `UE_LOGF` with a row field is found.

## Deferred (asserted by the producing stories)

- nkp-001 AC4 and nkp-002 AC3 respawn halves -> nkp-009 (provider and InformantRespawn resolver).
- The exile sequence behind bExiled -> nkp-005. The real paths of RespawnScheduled / InformantRespawn /
  RespawnCancelled / PawnLost -> nkp-005, 009, 010, 012.

## Integration points for later stories

1. nkp-005: register `UPenaltyService` itself as `FEncounterServiceInit::Tally` (drop `FStealthShadowPenaltyTally`,
   `StealthPenaltyWiring.*`), run the exile from the `EPenaltyKillOutcome::StrikeAndExile` point and call
   `FPenaltyCore::RecordReleasedBody` at the release.
2. nkp-009/010: `UPenaltyService` implements `IInformantBodyProvider` (register it as `FRoundGateways::InformantBodies`)
   and the InformantRespawn resolver; `FPenaltyCore::OnInformantReadmitted` / `OnConnectionChanged` are the empty hooks.
3. roster psr-038: pass its owner-view feed as `FPenaltyServiceInit::OwnerView` and call `UPenaltyService::OnViewRebound`
   from the rejoin rebind.
4. TDC store: pass the frozen set as `FPenaltyServiceInit::Tuning` (today the schema defaults stand-in).
5. nkp-021: delete the prototype path (`HandleEncounterPawnKilled` tally/NpcKills/exile, `FStealthPrototypeStrikeTally`).

## Review

Self code review (`.claude/skills/code-review/SKILL.md` phases 3-6) of the changed files: ADR-0019 compliant (server-only
store keyed by ParticipantId, no replicated property, no timer/tick, every mutation in the slot, O(1)); standards: doc
comments on public APIs, no literals (L and the grace come from tuning), DI over singletons. Verdict APPROVED.

## Run result

Run result: N/A — nothing player-observable changes: the encounter service's tally answer is still the prototype's (the
shadow adapter only keeps the production store in step) and the prototype still exiles.
