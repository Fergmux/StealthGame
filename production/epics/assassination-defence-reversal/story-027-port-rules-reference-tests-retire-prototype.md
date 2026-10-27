# Story 027: Port the StealthRules reference tests to the production cores and retire the prototype strike and defence code

> **Epic**: Assassination & Defence Reversal
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (Tuning Knobs "Prototype keys retired"; ASN-R9, ASN-R19)
**Requirement**: `TR-asn-001`, `TR-asn-002`, `TR-asn-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (Migration Plan), with ADR-0001 (the prototype's pawn bools are marked for replacement)
**ADR Decision Summary**: The prototype uses separate pawn bools (`bDying`, `bPreparingDefence`) and `Server*` RPCs on the pawn; nothing is ported: Assassination is written fresh against ADR-0018 and its dependencies, and the prototype code is retired once the production path exists.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (deleting prototype code and keeping the build and tests green; no post-cutoff API)
**Engine Notes**: The prototype lives in the single `StealthGame` module (`Source/StealthGame`), marked PROTOTYPE in `production/first-playable-report.md`; production code goes to `StealthFoundation`/`StealthGameplay` per ADR-0007. The `StealthGame.Rules.*` automation tests are **kept as reference tests for this epic** until the ported tests pass; they are removed only in the same change that adds their replacements. The other four prototype rule tests (`Rules.NpcKillRemoval`, `Rules.ObjectiveStages`, `Rules.EliminationVictory`, `Rules.TeamsAndMovement`) belong to the Penalty, Objectives and Round epics and are untouched here.

**Control Manifest Rules (this layer)**:
- Required: Prototype code is promoted only after its ADR is Accepted and built against; nothing is ported by copy — source: ADR-0018 Migration Plan, ADR-0001 migration list
- Forbidden: Never leave two paths that resolve an assassination (the prototype's game-mode resolver and the queue resolver); never keep the prototype's replicated `bDying` / `bPreparingDefence` — source: ADR-0018, ADR-0001
- Required: Each ported test keeps its scenario and gets a production-core name `StealthGame.Assassination.*` (naming per `.claude/rules/test-standards.md`) — source: ADR-0008
- Guardrail: The build and every `StealthGame.` test stay green at each step

The prototype's `StealthGame.Rules.Assassination` and `StealthGame.Rules.Defence` tests encode the proven behaviour: unprepared enemy eliminated, prepared enemy reverses, NPC kill, informant kill, teammate and informant-attacker rejected; defence first use allowed, blocked while running, in parkour or as an informant, cooldown from activation (69.9 blocked, 70.0 allowed at t = 10 + 60), remaining-cooldown arithmetic, window expiry and cancel on run or parkour. This story re-expresses each row on the production cores and records what changed by design; then retires the prototype strike and defence code.

---

## Acceptance Criteria

*From the epic's Prototype Disposition (`production/epics/assassination-defence-reversal/EPIC.md`), the GDD's "Prototype keys retired" and the six prototype `StealthRules` tests, scoped to this story (the GDD has no numbered AC for retirement):*

- [x] **AC1** ([Logic/BLOCKING] reference parity): **GIVEN** every assertion of `StealthGame.Rules.Assassination` and `StealthGame.Rules.Defence` **WHEN** each scenario is re-run on the production cores **THEN** each has an equivalent passing `StealthGame.Assassination.*` test with the same inputs, with exactly these documented, intended differences: a teammate target is `NoContact` (no rejection, ASN-R9 and story 008) instead of `Rejected`; an informant attacker is rejected `NotNow` by the IIF (not by the resolver); the informant-victim row yields `OnPawnKilled(Informant, Strike)` with a `WrongTarget` result identical to an NPC kill; running or parkour blocking the brace is the IIF's legality (stories 033 and 034 there); and the window length is `DefenceDurationSec` 4.0 s.
- [x] **AC2** ([Logic/BLOCKING] retirement): **GIVEN** the production queue path **WHEN** the project is built and the `StealthGame.` automation filter is run **THEN** `KillCharacter`, `StartDefence`, `StopDefence`, `SetDefenceStance`, `ApplyDeathPresentation`, `ServerAssassinate`, `ServerSetDefence`, `HandleAssassinationRequest`, `HandleDefenceRequest`, `bDying`, `bPreparingDefence`, `AStealthPlayerState::LastDefenceActivationTime` and the five retired `UStealthSettings` keys (`AttackerRecoveryTime`, `BodyLifetime`, `BlindStrikeRadius`, `AssassinationConeHalfAngle`, `AssassinationServerTolerance`) no longer exist, the build has no reference to them (static scan), and no test is skipped or disabled to achieve this.
- [x] **AC3** ([Integration/BLOCKING] acceptance reference): **GIVEN** the networked prototype tests A, C and F **WHEN** replayed as `StealthGame.Assassination.PIE.*` scenarios on the production path **THEN** A (unprepared strike on an Active rival kills it), C (a braced defender's reversal eliminates the attacker and, when it is the team's last Active player, ends the round by elimination) and F (a blind strike on a bush kills a concealed enemy) pass, with the before-state screenshots in `production/qa/evidence/2026-10-24_*` retained alongside new captures.

---

## Implementation Notes

*Derived from the epic Prototype Disposition and ADR-0018 Migration Plan:*

Ordering: do this story last in the implementation order of the logic stories (it needs 003, 007, 009, 010, 011). Steps: (1) add the ported tests next to the cores (`Source/StealthGameplay/Private/Tests/Assassination/`), keeping the same scenario names in comments with the old test name for traceability; (2) run both suites and record the mapping table in the story's Files section; (3) delete the prototype code in one change with `StealthRulesTests.cpp`'s two migrated tests; (4) keep `FStealthRules` members still used by the other epics (`ShouldRemoveForNpcKills`, `CanCompleteObjective`, `EvaluateEliminationWinner`, `ChooseTeamForNewPlayer`, `IsRunningSpeed`) until their epics retire them; (5) run the static scan from IIF story 093 (`static-scan-and-prototype-retirement`), adding this story's symbols to its list.

Do not copy `FStealthRules::ResolveAssassination` or `CanStartDefence` into the new code: re-express them under the queue slot, the key-compare braced test and the component stamp (stories 003, 007, 009). `FStealthRules::DefenceCooldownRemaining` becomes the owner component's display datum (story 016). The prototype's `EStealthAssassinationOutcome` and `EStealthTargetKind` enums in `StealthTypes.h` are removed when nothing else uses them.

Closes known issues 5 (assassination presentation placeholder) and 6 (the brace is the crouch) of `production/first-playable-report.md` in combination with stories 018, 020 and 011.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Penalty, Objectives and Round epics: their prototype rules tests and code.
- IIF story 093: the overall prototype static scan (this story adds its symbols).
- `story-001-assassination-tuning-keys-and-invariants.md`: the new keys (this story only removes the old ones).

---

## Files

Delivered (2026-10-27):
- `Source/StealthGameplay/Private/Tests/Assassination/PrototypeParityTest.cpp` — `FAssassinationPrototypeParityTest`, `StealthGame.Assassination.PrototypeParity` (AC1)
- `Source/StealthGameplay/Private/Tests/Assassination/PrototypeRetiredScanTest.cpp` — `FAssassinationPrototypeRetiredScanTest`, `StealthGame.Assassination.PrototypeRetiredScan` (AC2 static scan; this story's symbol list, added to IIF story 093's)
- `Source/StealthTestHarness/Private/Tests/PIE/Assassination/PrototypeScenariosACFPieTest.cpp` — `StealthGame.Assassination.PIE.PrototypeScenariosACF.{A_UnpreparedStrikeOnAnActiveRivalKillsIt, C_ReversalOfTheLastActiveAttackerEndsTheRoundByElimination, C_ReversalWithAnActiveTeammateLeftKeepsTheRoundActive, F_BlindStrikeOnABushKillsTheConcealedEnemyAndOnAnEmptyBushNobody}` (AC3; the test root `Source/<Module>/Private/Tests/` replaces the planned `Private/Assassination/` path)
- Retired: `StealthCharacter.{h,cpp}` (`KillCharacter`, `BeginEncounterDeath`, `StartDefence`, `StopDefence`, `SetDefenceStance`, `ApplyDeathPresentation`, `OnRep_Dying`, `FindAssassinationTarget`, `IsPreparingDefence`, `bDying`, `bPreparingDefence`, `DefenceActivationTime`, the Tick `ShouldEndDefence` check); `StealthGameMode.{h,cpp}` (`HandleEncounterCounterPose`, the hub Cancelled / Completed handlers, the dying-flag timer at the death write, the round-end `StopDefence` loop); `StealthRules.{h,cpp}` (`ResolveAssassination`, `CanStartDefence`, `DefenceCooldownRemaining`, `ShouldEndDefence`); `StealthTypes.h` (`EStealthTargetKind`, `EStealthAssassinationOutcome`); `StealthPlayerState.{h,cpp}` (`LastDefenceActivationTime`); `StealthSettings.h` (the five keys); `StealthRulesTests.cpp` (`Rules.Assassination`, `Rules.Defence`). `ServerAssassinate`, `ServerSetDefence`, `HandleAssassinationRequest` and `HandleDefenceRequest` were already gone (IIF / asn stories); the scan proves it.
- Kept by design: `AStealthCharacter::IsDying()` now reads `FDeathState` (`HasDeathWrite()`); the crouch brace tell is `AStealthCharacter::ApplyBraceTell`, presentation of the replicated `FActionState` Brace pose only (authority and owner, Counter held to its contact), so first-playable known issue 6 (the brace is the crouch) stays open until the authored Brace clips land; `HoldInPlace` (Round adapter); `DefenceDuration`, `DefenceCooldown`, `AssassinationRange`, `NpcKillLimit` settings (not in AC2's list; spike session, aim range, Penalty).
- S10 harness (`StealthHarnessScenarioS10.cpp`, `StealthHarnessScenarioS10Rerun.cpp`): case B kills by the production death path (`AStealthPawn::ApplyDeathWrite`) instead of `KillCharacter`.
- `NoObserverCuePieTest.cpp`: `.bDying` dropped from the allowed replicated properties (the property no longer exists).

Mapping table (AC1; prototype assertion -> production row in `PrototypeParity`; D = documented, intended difference):

| Prototype assertion | Production row |
|---|---|
| Rules.Assassination: unprepared active enemy is eliminated | A1: Kill, `RivalEliminated`, `OnPawnKilled(Player, Strike)`, Roster `Assassinated` |
| prepared active enemy reverses | A2: Reversal, `Reversed`, `OnPawnKilled(attacker, defender, Player, Reversal)`, Roster `ReversedAgainst` |
| NPC target is an NPC kill | A3: Kill, `WrongTarget`, `OnPawnKilled(Npc, Strike)`, no transition |
| enemy informant kill is not an NPC kill | A4 (D): `OnPawnKilled(Informant, Strike)` with a `WrongTarget` result identical to an NPC kill's |
| teammates cannot be attacked (Rejected) | A5 (D): Executed, `NoContact` (ASN-R9, story 008) |
| informants cannot attack (Rejected) | A6 (D): `NotNow` by the IIF legality (the slot's stand-in re-check); Assassination's CanBegin allows |
| informants cannot attack NPCs | A7 (D): the same `NotNow`, the NPC untouched |
| Rules.Defence: first use is allowed | D1: Begin at t = 10 executes (core and slot) |
| blocked while running / during parkour / for informants | D2 / D3 / D4 (D): the IIF legality's `NotNow` (IIF R7; stories 033 / 034 there) |
| cooldown runs from activation (69.9) | D5: `Reject(Cooldown)`, stamp unchanged |
| usable again once per minute (70.0) | D6: Allow / Executed, inclusive; default still 60 s |
| cooldown remaining 30 | D7: `FDefenceStateCore::CooldownRemaining` and `UDefenceOwnerComponent::GetCooldownRemaining` = 30 |
| no cooldown before first use | D8: 0 with no stamp; 60 (full) at activation |
| defence holds within its window (13.9) | D9: a strike at T_act + 3.9 is reversed |
| defence expires after its duration (14.0) | D10 (D): `DefenceDurationSec` 4.0 s; `HoldTimer(CapEnd)` due at exactly T_act + 4.0 ends it; a later strike kills |
| running / parkour cancels the defence | D11 / D12 (D): the brace is planted (PlantedBrace); a Traverse during it is `Busy`, the window stays open; the cancel is the IIF's |

Planned:
- `Source/StealthGame/Private/Tests/StealthRulesTests.cpp` (remove `Rules.Assassination` and `Rules.Defence` after their replacements pass; keep the other four tests)
- `Source/StealthGame/Private/StealthCharacter.cpp` and `Source/StealthGame/Public/StealthCharacter.h` (remove strike and defence code and the two replicated bools)
- `Source/StealthGame/Private/StealthGameMode.cpp` and `Source/StealthGame/Public/StealthGameMode.h` (remove `HandleAssassinationRequest`, `HandleDefenceRequest`)
- `Source/StealthGame/Private/StealthRules.cpp` and `Source/StealthGame/Public/StealthRules.h` (remove `ResolveAssassination`, `CanStartDefence`, `DefenceCooldownRemaining`, `ShouldEndDefence` when unused)
- `Source/StealthGame/Public/StealthPlayerState.h` (remove `LastDefenceActivationTime`) and `Source/StealthGame/Public/StealthSettings.h` (remove five keys)
- `Source/StealthGameplay/Private/Tests/Assassination/PrototypeParityTest.cpp` (class `FAssassinationPrototypeParityTest`, name `StealthGame.Assassination.PrototypeParity`)
- `Source/StealthTestHarness/Private/Assassination/PrototypeScenariosACFTest.cpp` (`StealthGame.Assassination.PIE.PrototypeScenariosACF`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS (resolved 2026-10-27 by /dev-story readiness, unattended: the qa-lead's proposed rewrites below are adopted as the test design; decision log 2026-10-27)
- Adopted AC1: one named `StealthGame.Assassination.PrototypeParity` row per prototype assertion (mapping table in Files); the production `DefenceCooldownSec` default is still 60 s, so 69.9 s blocked / 70.0 s allowed at t = 10 + 60 holds unchanged.
- Adopted AC2: `StealthGame.Assassination.PrototypeRetiredScan` finds zero code references (comments and strings blanked) to the listed symbols in `Source/` and zero `Config/*.ini` lines setting the five keys, with a denominator check and a seeded negative control; the run reports 0 failed and no test is disabled or skipped (count = baseline - 2 migrated + the 2 new headless tests + the PIE scenarios).
- Adopted AC3: A, C and F as defined in the rewrite below (note: `production/first-playable-report.md`'s test "A" was the third-civilian exile, a Penalty test; this story's A is the rival kill, as the AC says). Edge cases: C with an Active teammate left (round stays Active), F on the empty bush first (no kill).
- AC1: "every assertion" is not enumerated and the cooldown boundary may differ; proposed rewrite (unconfirmed): each assertion row in the mapping table (recorded in the Files section) has a named `StealthGame.Assassination.*` test with identical inputs; the cooldown boundary row asserts 69.9 s blocked and 70.0 s allowed at t = 10 + `DefenceCooldownSec`, and the story states whether the production default is still 60 s (if it changed, that is a further documented difference).
- AC2: the symbol scan and "no test skipped" need a method; proposed rewrite (unconfirmed): static scan result is zero references to the listed symbols and zero matches in config or ini files for the five retired keys; the build is warning-free for removed symbols; the `StealthGame.` run reports 0 failed with test count = baseline − 2 migrated + their replacements and no `DISABLED`, `Skip` or filtered-out tests.
- AC3: tests A, C and F are named only by letter; proposed rewrite (unconfirmed): copy the scenario definitions into the story. A: an unprepared strike kills an Active rival. C: a braced defender's reversal eliminates the attacker and, when it is the team's last Active player, the round state becomes `Ended` with reason Elimination and the defender's team as winner. F: a blind strike on a concealed enemy in a bush kills it. The `2026-10-24_*` before-state screenshots are verified present before the run.

- **AC-1**: Reference parity
  - Given: the mapping table of every prototype `Rules.Assassination` and `Rules.Defence` assertion to a production test
  - When: both suites run
  - Then: each row passes on the production core with the same inputs, with the documented differences: teammate `NoContact`; informant attacker `NotNow` by the IIF; informant victim `OnPawnKilled(Informant, Strike)` with `WrongTarget`; brace legality by the IIF; window 4.0 s
  - Edge cases: the exact 69.9 s and 70.0 s boundary; remaining-cooldown arithmetic at 0 and at full; window expiry at exactly `DefenceDurationSec`; cancel on run or parkour is now the IIF's
- **AC-2**: Retirement
  - Given: the production queue path is live and the prototype code is deleted in one change
  - When: the project builds, the static scan runs and the `StealthGame.` filter runs
  - Then: all listed symbols and the five keys are gone (code and config), the build is green and no test is disabled
  - Edge cases: `FStealthRules` members still used by other epics remain; `EStealthAssassinationOutcome` and `EStealthTargetKind` go only if unused; the four other `Rules.*` tests are untouched; the scan list is added to IIF story 093's
- **AC-3**: Acceptance reference
  - Given: PIE scenarios A, C and F on the production path with the before-state screenshots retained
  - When: each scenario is replayed
  - Then: A, C and F pass as defined above, and new captures are retained alongside the `2026-10-24_*` files
  - Edge cases: C when the defender's team still has other Active players (the round does not end); F on an empty bush (no kill); both runs retained in `production/qa/logs/`

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `PrototypeParityTest.cpp` — must exist and pass; plus the AC2 static-scan output and the AC3 harness run (PIE) retained in `production/qa/logs/`

**Status**: [x] Created and passing 2026-10-27 (`PrototypeParityTest.cpp`, `PrototypeRetiredScanTest.cpp`, `PrototypeScenariosACFPieTest.cpp`). Evidence: `production/qa/evidence/assassination-defence-reversal-027-2026-10-27/` (evidence.md, full-suite.txt: build Succeeded, 409 tests, 407 pass; the 2 Tuning.Schema* failures come from another agent's uncommitted schema edit). Logs: `production/qa/logs/2026-10-27_asn-027-prototype-retired-scan.log` (726 files, 0 findings) and `production/qa/logs/2026-10-27_asn-027-pie-scenarios-acf.log` (A, C, C2 and F pass). Run result: PASS (2026-10-27, 3-player PIE through the MCP): the pose-driven brace tell, its end at the window's end, and a reversal with the tell held to contact. Captures are in `production/qa/evidence/story-027-port-rules-reference-tests-retire-prototype/` (6 files), alongside the retained `2026-10-24_*` before-state captures.

---

## Dependencies

- Depends on: `story-003-defence-state-component-and-cooldown.md`, `story-007-attack-outcomes-resolved-in-slot.md`, `story-009-same-drain-ordering-and-braced-key-compare.md`, `story-010-reversal-counter-and-chained-strikes.md`, `story-011-npc-brace-parity-never-strike-planted.md`, `story-018-strike-clip-variants-warp-and-contact-scheduling.md`; cross-epic `production/epics/input-interaction-framework/story-093-static-scan-and-prototype-retirement.md`
- Unlocks: closes the Assassination prototype disposition; clears the way for removing `StealthRules` entirely once the Penalty, Objectives and Round epics retire their rows
