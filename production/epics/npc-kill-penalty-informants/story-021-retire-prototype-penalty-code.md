# Story 021: Retire the prototype penalty code behind the production path

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-001`, `TR-pen-002`, `TR-pen-003`, `TR-pen-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (migration of the prototype), with ADR-0007 (the production code lives in the module layout; the prototype is replaced, not promoted)
**ADR Decision Summary**: The production Penalty service replaces the prototype's penalty and informant code in `AStealthGameMode`; nothing in the prototype is promoted until the ADR that governs it is built against. The prototype's networked tests A (third civilian kill exiles the player while a teammate is Active) and B (killing an enemy informant respawns them and does not count as an NPC kill) are the behavioural acceptance reference.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No new engine API. The PIE tests are CQTest network tests (ADR-0008 CQTest exemption: named by scenario, `StealthGame.Penalty.PIE.<Scenario>.<Method>`); the prototype's `StealthBot` driver exists for the manual runs and is *not* a test dependency. Editor builds must compile after deletion: run the project's `commands.test` (`Automation RunTests ^StealthGame.`) and read the output, not the exit code (`tests/README.md`).

**Control Manifest Rules (this layer)**:
- Required: Penalty replaces, not wraps, the prototype's path; production code follows ADR-0007 modules — source: ADR-0007, ADR-0019
- Forbidden: never keep a replicated `NpcKills` counter or a distinct informant-kill message — source: ADR-0019, ADR-0001
- Guardrail: tests that are replaced are not "skipped to pass CI": the replacement tests ship in the same change — source: `.claude/docs/coding-standards.md` CI rules

This is the last story of the epic. It removes the prototype penalty path only after the production path passes the same behavioural acceptance tests, so the project never loses a working penalty. It must run after stories 005, 008, 012 and 014.

**Prototype replaced:** `AStealthGameMode::EliminatePlayer`, `ExilePlayer`, `RespawnInformant`, `MessagePlayer`/`MessageTeam` penalty uses, the `NpcKills` counter on the replicated `AStealthPlayerState` and its use in `AStealthHUD`, and `FStealthRules::ShouldRemoveForNpcKills` with its test `StealthGame.Rules.NpcKillRemoval` (`StealthRules.cpp`, `StealthRules.h`, `StealthRulesTests.cpp`). `MessagePlayer`/`MessageTeam` themselves may stay if the objectives code still uses them (`StealthGameMode.cpp` lines using them for stage messages): remove only the penalty call sites.

---

## Acceptance Criteria

*From the GDD's Prototype Disposition (prototype tests A and B as the behavioural reference) and the first-playable report; all three derived:*

- [ ] **AC1** (derived, GDD Prototype Disposition, prototype test A): **GIVEN** the production Penalty build with L = 3 **WHEN** prototype networked test A (a third civilian kill exiles the player while a teammate is Active; `production/first-playable-report.md`) is ported as a CQTest PIE test **THEN** it passes: the exile happens exactly on the third ungraced NPC kill, kills 1 and 2 do not exile, the teammate stays Active and the round continues.
- [ ] **AC2** (derived, GDD Prototype Disposition, prototype test B): **GIVEN** the production build **WHEN** prototype test B (killing an enemy informant respawns them as an informant and does not count as an NPC kill) is ported as a CQTest PIE test **THEN** it passes: the victim is re-bodied by takeover of an existing crowd pawn after the test preset's delay, the attacker's tally is +1, `StrikeCount` is unchanged and the attacker is not exiled (the prototype spawned a pawn: the intent is kept, the mechanism is not).
- [ ] **AC3** (derived, ADR-0019 migration): **GIVEN** the migration is complete **WHEN** `Source/StealthGame` is searched for `EliminatePlayer`, `ExilePlayer`, `RespawnInformant`, the `NpcKills` counter, `ShouldRemoveForNpcKills` and the test name `StealthGame.Rules.NpcKillRemoval` **THEN** there are no matches and the project builds; the HUD and PlayerState uses migrated first (the tally now comes from `FStrikeResult`, story 003 and 018).

---

## Implementation Notes

*Derived from ADR-0019 and `production/first-playable-report.md`:*

Order of work inside the story: (1) port tests A and B as CQTest PIE tests against the production path (they use the production bot/test driver of the harness, not the prototype `StealthBot` console driver); confirm they pass; (2) migrate the remaining prototype readers (`AStealthHUD`'s read of `NpcKills`, the prototype `PlayerState` field) to the production tally path; (3) delete the prototype penalty functions, the counter, `ShouldRemoveForNpcKills` and its test; (4) build and run `Automation RunTests ^StealthGame.` and read the output: `Result={Fail}` is a failure and "No automation tests matched" is NOT ASSESSED, never a pass.

Deleting `StealthGame.Rules.NpcKillRemoval` is not skipping a failing test: AC1 and the strike-rule tests of story 002 replace it, and the boundary it checked (removal exactly on the limit-th kill, zero limit handling) is covered by story 005's parameterised exile test. Note that the prototype's "zero limit disables the penalty" behaviour is not carried over: TDC clamps an authored 0 to the hard minimum 2.

Check what else reads the prototype fields before deleting: the code search (AC3) is the guard; at the time of writing the search term set matches in `StealthGameMode.cpp`, `StealthHUD.cpp`, `StealthPlayerState.*`, `StealthRules.*` and `StealthRulesTests.cpp`. Do not touch Content assets or `.uasset` files (blueprints that call the prototype functions are routed to the project's blueprint cleanup, not edited here).

Commits reference the story id in the body (`Story: NKP-S021` or the project's id scheme); no commit is made by this skill.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- stories 001-020: the production Penalty implementation (this story removes the prototype, it adds no behaviour).
- tuning-data-config `story-095-retire-prototype-settings-canonical-fields.md`: the prototype settings fields.
- Other epics' prototype code (assassination, concealment, objectives): each epic retires its own.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Private/StealthGameMode.cpp` and `Source/StealthGame/Public/StealthGameMode.h` (remove the prototype penalty paths)
- `Source/StealthGame/Private/StealthRules.cpp` and `Source/StealthGame/Public/StealthRules.h` (`ShouldRemoveForNpcKills`)
- `Source/StealthGame/Private/Tests/StealthRulesTests.cpp` (the `NpcKillRemoval` test)
- `Source/StealthGame/Private/StealthHUD.cpp`, `Source/StealthGame/Private/StealthPlayerState.cpp` and its header (the `NpcKills` readers and field)
- `Source/StealthTestHarness/Private/Penalty/PrototypeParityExileTest.cpp` (CQTest, `StealthGame.Penalty.PIE.PrototypeParityExile.*`)
- `Source/StealthTestHarness/Private/Penalty/PrototypeParityInformantKillTest.cpp` (CQTest, `StealthGame.Penalty.PIE.PrototypeParityInformantKill.*`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyPrototypeRetiredTest.cpp` (class `FPenaltyPrototypeRetiredTest`, name `StealthGame.Penalty.PrototypeRetired`, the search lint)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (derived, `StealthGame.Penalty.PIE.PrototypeParityExile`)
  - Given: the prototype test A scenario (`production/first-playable-report.md`), L = 3, a teammate Active.
  - When: a player makes three ungraced NPC kills.
  - Then: the exile happens exactly on the third kill; kills 1 and 2 do not exile; the teammate stays Active and the round continues.
  - Edge cases: uses the harness test driver, not the prototype `StealthBot` console driver.
- **AC2** (derived, `StealthGame.Penalty.PIE.PrototypeParityInformantKill`)
  - Given: prototype test B: an enemy informant killed, test preset delay.
  - When: the delay elapses.
  - Then: the victim is re-bodied by takeover of an existing crowd pawn; the attacker's tally is + 1, `StrikeCount` unchanged, attacker not exiled.
  - Edge cases: the prototype spawned a pawn, production must not: same intent, different mechanism.
- **AC3** (derived, `StealthGame.Penalty.PrototypeRetired`, search lint plus build)
  - Given: `Source/StealthGame`, which currently matches the term set in `StealthGameMode.cpp`, `StealthHUD.cpp`, `StealthPlayerState.*`, `StealthRules.*` and `StealthRulesTests.cpp`.
  - When: it is searched for `EliminatePlayer`, `ExilePlayer`, `RespawnInformant`, `NpcKills`, `ShouldRemoveForNpcKills` and `StealthGame.Rules.NpcKillRemoval`.
  - Then: 0 matches and the project builds; `Automation RunTests ^StealthGame.` is read from output, not exit code.
  - Edge cases: HUD and PlayerState readers migrate first; deleting the obsolete test is not skipping a failing test because AC1/AC2 and story 002/005 tests replace it.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two CQTest PIE tests under `Source/StealthTestHarness/Private/Penalty/` and the search lint under `Source/StealthGameplay/Private/Tests/Penalty/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-005-exile-sequence-in-attack-slot.md`, `story-008-informant-kill-inline-retire.md`, `story-012-respawn-commit-sequence.md`, `story-014-one-respawn-path-and-takeover-wire-parity.md`, `story-018-tally-countdown-and-exile-card-ui.md`; cross-epic: tuning-data-config `story-003-split-prototype-composition-root.md`, hud (the production tally readout)
- Unlocks: None
