# Assassination & Defence Reversal story asn-027 evidence, 2026-10-27

Story: `production/epics/assassination-defence-reversal/story-027-port-rules-reference-tests-retire-prototype.md`

## What was built

- **AC1, the reference parity**: `StealthGame.Assassination.PrototypeParity`
  (`Source/StealthGameplay/Private/Tests/Assassination/PrototypeParityTest.cpp`). It reruns every assertion of the
  retired `StealthGame.Rules.Assassination` (7) and `StealthGame.Rules.Defence` (12) on the production cores, with the
  same inputs:
  - the strike resolver in its slot (`FAttackResolverCore` over the encounter test rig);
  - `FDefenceStateCore`;
  - `UAssassinationPreparePredicate` and `UAssassinationAttackPredicate`;
  - `UDefenceOwnerComponent`.

  The mapping table, with the AC's intended differences, is in the story's Files section.
- **AC2, the retirement**: the prototype strike and defence code is deleted. That covers `KillCharacter`,
  `BeginEncounterDeath`, `StartDefence`, `StopDefence`, `SetDefenceStance`, `ApplyDeathPresentation`, `OnRep_Dying`,
  `FindAssassinationTarget`, `IsPreparingDefence`, `bDying`, `bPreparingDefence` and `DefenceActivationTime`, plus:
  - in the game mode: the Tick `ShouldEndDefence` check, `HandleEncounterCounterPose`, the hub Cancelled / Completed
    handlers, the dying-flag timer and the round-end `StopDefence` loop;
  - in `FStealthRules`: `ResolveAssassination`, `CanStartDefence`, `DefenceCooldownRemaining` and `ShouldEndDefence`;
  - `EStealthTargetKind` and `EStealthAssassinationOutcome`;
  - `AStealthPlayerState::LastDefenceActivationTime`;
  - the five `UStealthSettings` keys;
  - the two migrated tests in `StealthRulesTests.cpp`. The other four `Rules.*` tests stay.

  The static scan is `StealthGame.Assassination.PrototypeRetiredScan`: 29 identifiers, with comments and strings
  blanked, plus the ini keys. It includes a denominator check and a seeded negative control. Its symbol list was added
  to IIF story 093's.
  - Kept: `AStealthCharacter::IsDying()` now reads `FDeathState`.
  - The crouch brace tell is `ApplyBraceTell`, driven only by the replicated `FActionState` pose on the authority and
    the owner. The Counter keeps the crouch until its contact.
  - The S10 harness case B kills by `AStealthPawn::ApplyDeathWrite`.
- **AC3, the acceptance reference**: `StealthGame.Assassination.PIE.PrototypeScenariosACF.*`
  (`Source/StealthTestHarness/Private/Tests/PIE/Assassination/PrototypeScenariosACFPieTest.cpp`). It covers A, both
  cases of C, and F with the empty bush first. The before-state captures are retained:
  - `production/qa/evidence/2026-10-24_client1-reversal-elimination-win.png`
  - `production/qa/evidence/2026-10-24_bush-concealment-teammate-vs-enemy.png`
  - the other `2026-10-24_*` files.

## Tests

`full-suite.txt` is the output of `tools/dev/cycle.ps1 -Tests -NoLaunch`, run on the working tree. The tree also held
the NKP 009-012 agent's uncommitted edits.

- Build: `Result: Succeeded`.
- 409 tests ran: 407 passed and 2 failed.
  - `StealthGame.Tuning.SchemaComplete` and `StealthGame.Tuning.SchemaGenDeterministic` failed with "the checked-in
    StealthTuningSchema.generated.inl equals the in-memory regeneration".
  - Both come from the NKP agent's uncommitted edits to `StealthTuningSchema.generated.inl` and
    `StealthTuningPublicSubstructs.h`. Neither file belongs to this story or is in its commit.
  - In the NKP agent's earlier run (committed tree plus this story's code), the same suite had 408 of 409 passing.
    The only failure was this story's F scenario, fixed here.
- The baseline count changes by -2 for the two migrated `Rules.*` tests. It gains `PrototypeParity`,
  `PrototypeRetiredScan` and 4 PIE methods. No test was skipped or disabled.

Measurements:

| Check | Result |
|---|---|
| PrototypeRetiredScan | 726 source files and 6 ini files scanned, 0 findings (`production/qa/logs/2026-10-27_asn-027-prototype-retired-scan.log`) |
| PIE A | contact = death 8.260; 1 kill (Player, Strike); the rival is an informant |
| PIE C (last Active attacker) | 1 kill (Reversal); phase Ended, reason 2 (Elimination), winner team 0 = the defender's team |
| PIE C2 (a teammate left) | 1 kill (Reversal); phase stays Active |
| PIE F | striker 190 cm out, bearing 0, not concealed; empty bush 0 kills; occupied bush 1 kill, died concealed 1 |

The PIE log is retained in `production/qa/logs/2026-10-27_asn-027-pie-scenarios-acf.log`, which is gitignored; the scan and PIE logs are also copied into this folder.

- **F fix**: the first run failed. The empty-bush strike's root-motion lunge carried the striker toward the bush, which
  put the occupant behind it, out of the bearing limit. The test now re-places the striker outside the bush before the
  second strike and logs the reach, bearing and concealment.

## Code review

This was a focused review of the changed files, with no defects found beyond the F placement:

- the pose-driven brace tell matches the retired server path: Brace at the Prepare slot, None at CapEnd / debounce, the
  Counter held to contact;
- `IsDying()` now flips at the death write instead of DeathServerTime. That is StrikeNetLeadSec earlier, server-side
  only, and harmless for its callers;
- `ExilePlayer`, which is NKP's, lost only its call to the deleted `StopDefence`.

## Run (run-and-observe)

**Pending.** The editor could not be rebuilt and launched: the tree's build fails in the NKP 009-012 agent's
half-written `PenaltySchedulingSourcesTest.cpp` (`'Roster' / 'Respawns' is not a member of 'FRig'`), which belongs to
another agent and was not edited. The player-visible part of this story is the crouch brace tell, now pose-driven. That
is the PIE suite's NoObserverCue, NoWarningToVictim and DefenceStateServerOnly, plus the ACF scenarios, which all pass.
The retained capture is still owed once the tree builds.
