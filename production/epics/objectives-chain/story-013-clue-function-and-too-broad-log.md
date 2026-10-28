# Story 013: Clue function and the too-broad clue log

> **Epic**: Objectives Chain
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-28

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-15 presentation and discovery; Edge Cases on `ClueMatchMaxCount`; AC-OC-13, AC-OC-28; Open Question 3)
**Requirement**: `TR-oc-003` (the clue rides the team view as a role-blind tag)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 3 and 4)
**ADR Decision Summary**: At `OnLayoutReady` the core computes, for each bound site, its clue (`FClueTag {AreaType, FeatureKind}`, taken from LSC's per-court public `ClueTag` record) with a role-blind function and counts how many courts of the layout share it; more than `ClueMatchMaxCount` logs `ClueTooBroad` once per `LayoutHash` and changes nothing else. A court with no feature has `FeatureKind = None` and the clue is area type only. The phrase ("A court in the market, by the water steps") is built client-side from a localised phrase table; no string travels.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (plain C++ function over public layout data)
**Engine Notes**: None. Depends on the graybox layout asset carrying `ClueTag` data (LSC R3 court row and V-CLUE, ADR-0014 section 7.4): check that data exists at story start; if it does not, the fixture layout in the tests stands in and the graybox check waits for LSC.

**Control Manifest Rules (this layer)**:
- Required: Card text and clue phrases are rendered client-side from cooked data; only indices and enums travel — source: ADR-0020
- Forbidden: Never replicate card text (`FText`) or clue strings; never log site ids, the assignment or draws outside the Event Log — source: ADR-0020
- Forbidden: Never let the clue depend on a court's role (role-blind function of public geometry) — source: ADR-0020, GDD OC-15
- Guardrail: computed once at `OnLayoutReady`; no per-frame cost — source: ADR-0020

A team finds its live court from the stage card: a title, one line and a clue built from the court's public `ClueTag` (area type plus nearest distinctive public feature), then confirms it by tending, never by a prompt at reach and never by a world marker (OC-15). The clue is the only help a team gets, so it must be a pure function of public geometry (a clue that read the role would be a leak) and it must be vague enough to match more than one court: at most `ClueMatchMaxCount` (3). A layout that cannot meet that is a data defect for LSC to fix, never a reason to show a sharper hint, so the round proceeds with the clue as it is and one `ClueTooBroad` entry is logged for LSC data fixing.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-13, AC-OC-28, OC-15) and ADR-0020 section 4, scoped to this story:*

- [x] **AC1** (AC-OC-13 [U] `ClueBoundAndRoleBlind`): **GIVEN** the graybox layout and 1,000 pinned assignments **THEN** every live site's clue matches 1 to `ClueMatchMaxCount` courts and the clue string is a function of public `ClueTag` data only (no role input; perturbing role inputs leaves the string unchanged).
- [x] **AC2** (AC-OC-28 [U] `ClueTooBroadLogged`): **GIVEN** a fixture layout in which a live site's clue matches 4 courts with `ClueMatchMaxCount` 3 **THEN** the round proceeds, the stage card shows the same clue string (never a sharper one), and exactly one `ClueTooBroad` entry is logged per layout hash (a second preparation with the same hash logs nothing; a different hash logs a new entry; a match count of exactly 3 logs nothing).
- [x] **AC3** (ADR-0020 section 4 [U] `ClueFallbackAreaOnly`): `FClueTag` is `{uint8 AreaType, uint8 FeatureKind}`; a court record with no feature has `FeatureKind` None and the clue is area type only; the phrase is built client-side from a localised phrase table (a missing key returns a safe fallback and a warning); no string member exists in the replicated view.

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 3 and 4 and GDD OC-15:*

`FClueTag` is declared with the other View/Core layout types (`StealthGameplay/Public/View/Core/`, ADR-0014 owns the declaration alongside `FCourtFixtureId`); use it, do not redeclare it. The clue function maps a court's public `ClueTag` record to an `FClueTag`: it reads nothing but that record, so AC1's role-perturbation test (permute `FSiteAssignment` roles and check the output string for every court is unchanged) is the proof. The "matches" count for a site is the number of courts in the layout whose `FClueTag` equals the site's; the bound is `ClueMatchMaxCount` (default 3, hard 1-6, safe 2-3).

`ClueTooBroad` is a Round Event Log entry (server-only), appended through the Round Event Log `Append`, never `UE_LOG`. The "once per `LayoutHash`" rule means a per-hash set in the core store; the log entry may carry the hash and the match count but never a court id or role (site ids must not be logged outside the Event Log's server-only header). The card still shows the same clue: nothing sharper is ever substituted.

Client text: `BuildClueText(FClueTag)` (story 016's helper) builds "A court [phrase]" from a localised phrase table keyed by area type and feature kind; this story defines the tag, the server computation and the fallback rule, and supplies the table's key set; the localised strings are authored with the chain asset's gather target (story 002). Open Question 3 of the GDD (can LSC publish a role-blind `ClueTag` per court with every clue matching at most 3 courts on the graybox; fallback area type only) is answered by AC1 on the real layout once LSC delivers the tags.

Prototype replaced: nothing. The prototype had no clue; objectives were visible markers on the HUD (`StealthHUD.cpp`), which OC-15 forbids (no compass, waypoint, map marker or world plate).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-016-presentation-view-helpers-and-ring-rule.md`: the client-side `BuildClueText` function and the ring rule.
- `story-010-team-view-struct-and-builder.md`: the view struct that carries `FClueTag`.
- LSC epic: producing the `ClueTag` record and its V-CLUE validation.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/ClueTag.h` (key set EClueAreaType x EClueFeatureKind)
- `Source/StealthGameplay/Features/Objectives/Private/ObjectivesClue.h / .cpp` (ClueOf, CountMatches, FindRecord)
- `Source/StealthGameplay/Features/Objectives/Public/Server/ObjectivesSeams.h` (FCourtClueRecord, IObjectivesCourtClues)
- `Source/StealthGameplay/Features/Objectives/Private/ObjectivesCore.h / .cpp` (BindClues, ClueTooBroad once per LayoutHash, view clue)
- `Source/StealthGameplay/Features/Objectives/Public/View/ObjectiveViewHelpers.h, Private/ObjectiveViewHelpers.cpp` (BuildClueText phrase table)
- `Source/StealthGame/Private/Composition/StealthObjectivesWiring.h / .cpp` (role-blind clue stand-in over the court sockets)
- `Source/StealthGameplay/Private/Tests/Objectives/ClueBoundAndRoleBlindTest.cpp, ClueTooBroadLoggedTest.cpp, ClueFallbackAreaOnlyTest.cpp, ObjectivesClueFixture.h`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; depends on the graybox `ClueTag` data existing). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`ClueBoundAndRoleBlind`): clue match bound and role-blind
  - Given: graybox layout data; 1,000 pinned assignments (fixed seed list in a constant file); `ClueMatchMaxCount` from tuning
  - When: the clue is computed for every live site per assignment
  - Then: each clue matches between 1 and `ClueMatchMaxCount` courts; the clue depends only on public `ClueTag` data; perturbing role inputs leaves the string unchanged
  - Edge cases: depends on the graybox layout asset existing with `ClueTag` data (check at story start); a site with no feature uses the fallback clue.
- **AC-2** (`ClueTooBroadLogged`): over-broad clue handled
  - Given: fixture layout where a live site's clue matches 4 courts; `ClueMatchMaxCount` 3; fake Event Log sink
  - When: the round prepares, twice with the same layout hash
  - Then: the round proceeds; the stage card shows the same clue string (not a sharper one); exactly one `ClueTooBroad` entry logged for that layout hash
  - Edge cases: a different layout hash logs a new entry; match count exactly 3 logs nothing.
- **AC-3** (`ClueFallbackAreaOnly`): clue tag shape and client-built text
  - Given: `FClueTag {AreaType, FeatureKind}`; court record with no feature (`FeatureKind` None); localised phrase table stub
  - When: the clue text is built client-side
  - Then: the clue is area type only for `FeatureKind` None; the phrase comes from the table; no string member exists in the replicated view
  - Edge cases: a missing phrase-table key returns a safe fallback and a warning; the same inputs give the same text.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [x] Created and passing (2026-10-28): StealthGame.Objectives.ClueBoundAndRoleBlind, ClueTooBroadLogged, ClueFallbackAreaOnly; evidence production/qa/evidence/objectives-chain-013-023-2026-10-28/

Run result: OBSERVED - the stage card shows the clue line on the team clients (run-and-observe captures in production/qa/evidence/objectives-chain-013-023-2026-10-28/)

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md`, `production/epics/objectives-chain/story-003-core-chain-validation-and-court-binding.md`; cross-epic: level-socket-contract (`ClueTag` court records and V-CLUE; stories not yet written)
- Unlocks: `production/epics/objectives-chain/story-010-team-view-struct-and-builder.md`, `production/epics/objectives-chain/story-016-presentation-view-helpers-and-ring-rule.md`

## Completion Notes
**Completed**: 2026-10-28
**Criteria**: AC1-AC3 passing. AC1 runs on a 12-court fixture layout (1,000 pinned assignments, 0 violations, 0 role-perturbation changes); the graybox half waits for the Level Socket Contract's ClueTag records (LSC epic, not built) - until then the composition root's role-blind stand-in (quadrant round the courts' centroid + water steps / shrine by distance) fills the L_Prototype clues.
**Deviations**: the phrase table is a C++ LOCTEXT table (namespace ObjectiveClues), gathered by the Game target; the ClueTooBroad entry carries the hash and the worst / maximum counts only.
**Test Evidence**: production/qa/evidence/objectives-chain-013-023-2026-10-28/
**Reviewed by**: self code review 2026-10-28 (notes in the evidence)
