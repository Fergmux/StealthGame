# Story 003: Core chain validation and court binding

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-3 the chain, OC-17 lifecycle, AC-OC-01, AC-OC-25)
**Requirement**: `TR-oc-002` (court binding from `FSiteAssignment` at `OnLayoutReady`, server-only), `TR-oc-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 2 and 3)
**ADR Decision Summary**: `FObjectivesCore` is plain C++ with no `UWorld`. `ValidateChain` fails the round at `Preparing` with `ObjectiveChainInvalid` unless each team has exactly `ObjectivesPerStageCount` (1) Prelim1 and Prelim2 rows with its eligibility, there is exactly one `Final` row with `Both`, `SiteRole` matches `StageKind`, ids are unique, card text is non-empty, `ObjectivesPerStageCount` is 1, `CompletionsRequiredCount` is 1 and there are at most 16 rows. At `OnLayoutReady` the core binds `Prelim1(T)` to `P1[T]`, `Prelim2(T)` to `P2[T]` and `Final` to `Final`, all `Locked`, reading `FSiteAssignment` only through the server query API.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0020 rules and data; plain C++ core)
**Engine Notes**: No engine API in the core. The assignment arrives through an injected interface so headless tests inject a stub; the real source is `UStealthLayoutService::GetSiteAssignment_Server()` (ADR-0014 section 7.4). No ADR-0020 verification item gates it.

**Control Manifest Rules (this layer)**:
- Required: `FSiteAssignment` is read only through `UStealthLayoutService::GetSiteAssignment_Server()` inside `UObjectivesService`/`FObjectivesCore` — source: ADR-0020
- Forbidden: Never log site ids, the assignment or draws outside the Event Log — source: ADR-0020, ADR-0014 (Guideline 13)
- Forbidden: Never spawn an actor for a court, objective or site, replicated or not — source: ADR-0020
- Guardrail: Objectives store under 2 KB, no tick — source: ADR-0020

The core is where the rules live and where headless tests drive them with stubs. This story gives it two things: validation of the chain asset against the rules the GDD states (so a malformed or future multi-stage chain fails loudly at `Preparing`, never silently), and the binding of definitions to the secret courts. The assignment is the single most sensitive secret Objectives touches (a modified host client could read it, which the GDD states honestly as a code discipline until the dedicated server), so the story's third criterion turns "server-only" into three mechanical checks.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-01, AC-OC-25), scoped to this story. QL-STORY-READY revised AC3 into three checks:*

- [ ] **AC1** (AC-OC-01 [U] `ChainBindAndValidate`): **GIVEN** the MVP chain **WHEN** validated at `Preparing` **THEN** per team it has one `Prelim1` and one `Prelim2` with distinct ids and text, one `Final` with `TeamEligibility = Both`, and it binds to `P1[T]`, `P2[T]`, `Final` of a fixture `FSiteAssignment`; every bound site state is `Locked`.
- [ ] **AC2** (AC-OC-01 reject clauses): **GIVEN** the valid chain plus one mutation each (`ObjectivesPerStageCount` 2; `CompletionsRequiredCount` 2; 17 rows; duplicate `ObjectiveId`; `SiteRole` not matching `StageKind`; empty `CardTitle`; empty `CardLine`) **THEN** `ValidateChain` returns the **named enum value** `EChainValidation::ObjectiveChainInvalid` (never a string) and nothing is bound; 16 rows is valid.
- [ ] **AC3** (AC-OC-25 [U, static] `AssignmentServerOnly`): (1) a reflection scan finds no replicated property or RPC parameter of type `FSiteAssignment` or carrying its fields; (2) a fake Event Log sink records every entry written during bind and the only entry carrying site data is the server-only header entry; (3) the include scan (`ObjectivesIncludeLint`, story 022) shows only `UObjectivesService`/`FObjectivesCore` reference `GetSiteAssignment_Server` and no client module includes `FSiteAssignment`; no actor is spawned (stub world spawn count 0).

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 2-3 and Guidelines:*

`FObjectivesCore::ValidateChain(const UObjectiveChainData&, const FObjectivesTuning_Server&)` implements the five validation rules of section 2 (rule 4, the removed `bOverrideInteractSec`, stays a numbered placeholder and checks nothing). It returns `EChainValidation` (the ADR's Key Interfaces name); the `ObjectiveChainInvalid` value is what the service maps to Round's `PreparingFailed(ObjectiveChainInvalid)` in story 005; confirm the exact enum name against the Round `PreparingFailed` contract when that epic's story lands. The first failing clause should travel with the result so the log is useful.

`Bind` takes the assignment through an injected `IObjectivesAssignmentSource` (the real implementation wraps `UStealthLayoutService::GetSiteAssignment_Server()`; the fake returns a fixture `FSiteAssignment {Final, P1[2], P2[2], ZoneTeam}`). It stores one `FChainBinding` per (team, DefIndex) holding the court's `FCourtFixtureId` (the LSC SocketId, public geometry; the id names a court and nothing about its role) and the site state `Locked`. The binding is never copied anywhere a client could reach and never logged outside the server-only Event Log header (ADR-0014 Guideline 13). In the same call ADR-0020 section 3 also computes each bound site's clue and the shared-clue count; that part is story 013 and hooks in after this story's `Bind`.

Edge cases to cover in the implementation: an assignment missing a P2 site fails validation rather than binding a hole; `Final` is bound once for both teams; team 0 and team 1 get different P1 and P2 sites (LSC guarantees no court holds two roles, GDD Edge Cases).

Prototype replaced: the prototype placed objectives as replicated actors per site (`AStealthObjective`) and the city generator relocated them; none of that is ported (ADR-0020 Alternative 1).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-002-chain-data-types-and-cacao-asset.md`: the types and the shipped asset.
- `story-004-core-site-state-and-progress.md`: `Locked -> Live -> Completed`, progress and unlocks.
- `story-005-service-shell-and-composition-root.md`: the `PreparingFailed` mapping and the `OnLayoutReady` hook.
- `story-013-clue-function-and-too-broad-log.md`: clue computation and `ClueTooBroad`.
- `story-022-prototype-retirement-and-static-scan.md`: owns `ObjectivesIncludeLint`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesCore.h` (`FObjectivesCore`, `EChainValidation`, `FChainBinding`, `IObjectivesAssignmentSource`)
- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesCore.cpp`
- `Source/StealthGameplay/Private/Tests/Objectives/ChainBindAndValidateTest.cpp` (class `FObjectivesChainBindAndValidateTest`, name `StealthGame.Objectives.ChainBindAndValidate`)
- `Source/StealthGameplay/Private/Tests/Objectives/AssignmentServerOnlyTest.cpp` (`StealthGame.Objectives.AssignmentServerOnly`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS; the AC3 rewrite and the named-enum point were applied above and the specs cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`ChainBindAndValidate`): valid MVP chain binds
  - Given: the MVP chain fixture; fixture `FSiteAssignment` via an injected stub assignment source; 1v1 teams
  - When: `ValidateChain` then `Bind` run at Preparing
  - Then: per team one Prelim1 and one Prelim2 with distinct ids and text; one Final with `TeamEligibility` Both; binding is `P1[T]`, `P2[T]`, `Final`; every bound site state is Locked
  - Edge cases: team 0 and team 1 get different P1 and P2 sites; Final bound once for both teams; an assignment missing a P2 site fails validation.
- **AC-2** (`ChainBindAndValidate` reject clauses): each invalid input rejects
  - Given: the valid chain plus one mutation each: `ObjectivesPerStageCount` 2; `CompletionsRequiredCount` 2; 17 rows; duplicate `ObjectiveId`; `SiteRole` not matching `StageKind`; empty `CardTitle`; empty `CardLine`
  - When: `ValidateChain` runs on each
  - Then: each returns `ObjectiveChainInvalid` and nothing is bound
  - Edge cases: 16 rows is valid (boundary); two simultaneous defects still give one reject; the result carries the first failing clause for the log.
- **AC-3** (`AssignmentServerOnly`): `FSiteAssignment` access is server-only
  - Given: stub assignment source; fake Event Log sink; reflection and include scan
  - When: bind runs and the scans execute
  - Then: reads occur only via `GetSiteAssignment_Server` through the injected interface; no replicated type carries the assignment; only the server-only header entry logs site data; no actor is spawned (stub world spawn count 0)
  - Edge cases: a seeded replicated copy of the struct fails the scan; a seeded log call outside the header entry fails the sink check.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the two test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md`, `production/epics/objectives-chain/story-002-chain-data-types-and-cacao-asset.md`; cross-epic: level-socket-contract (`FSiteAssignment` and `GetSiteAssignment_Server()`; stories not yet written)
- Unlocks: `production/epics/objectives-chain/story-004-core-site-state-and-progress.md`, `production/epics/objectives-chain/story-005-service-shell-and-composition-root.md`, `production/epics/objectives-chain/story-013-clue-function-and-too-broad-log.md`
