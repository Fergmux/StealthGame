# Story 002: Chain data types and the Cacao chain asset

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-2 the objective definition, OC-3 the chain, OC-16 "The Ruler's Cacao", AC-OC-01)
**Requirement**: `TR-oc-001` (chain data asset of `FObjectiveDef` rows)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 1 and 2)
**ADR Decision Summary**: One cooked `UObjectiveChainData` (`UPrimaryDataAsset`, primary asset type `ObjectiveChain`, cook rule `AlwaysCook`, `GetPrimaryAssetId()` overridden) holds `ChainSerial` and up to 16 `FObjectiveDef` rows. Row identity on the wire is the row index, never the `FName`. Clients render card text from their own cooked copy; only indices travel. MVP ships exactly one chain, `DA_ObjectiveChain_Cacao`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0020 rules and data; `UPrimaryDataAsset` and the Asset Manager are pre-cutoff)
**Engine Notes**: ADR-0020 E5: `UPrimaryDataAsset` reports its native class name as primary asset type unless `GetPrimaryAssetId()` is overridden; the override must return an invalid id for the CDO and a type name that matches the `DefaultGame.ini` scan entry exactly; `EPrimaryAssetCookRule::AlwaysCook` exists. E6 [U]: `FText` in a cooked data asset is localisable only if the asset's path is inside a localisation gather target. Verification item **OBJ-V3** (the asset cooks with `AlwaysCook` and resolves by `FPrimaryAssetId` on a cooked client) is verified here; ADR-0012's CPA-V check covers the same mechanism for the character pool.

**Control Manifest Rules (this layer)**:
- Required: Card text and clue phrases are rendered client-side from cooked data; only indices and enums travel — source: ADR-0020
- Forbidden: Never set a per-court hold duration (`FObjectiveDef` has no hold-time field since 2026-10-25) — source: ADR-0020
- Forbidden: Never replicate card text (`FText`) or clue strings — source: ADR-0020
- Guardrail: the chain asset is a few KB of text, loaded once at session start — source: ADR-0020

This story creates the data shape and the one shipped chain. The shape is the reusable `FObjectiveDef` record from OC-2: id, stage kind, team eligibility, site role, completions required (1 in MVP), card title and line, and the clue form. It deliberately has no hold-time field: a per-definition hold time would make `D` depend on a court's role, a timing oracle (IVP R7.4a, AC-OC-08), and the user removed `InteractSecOverride` on 2026-10-25. The MVP chain is the five rows of OC-16, "The Ruler's Cacao": two team-specific preliminaries each, then one shared final. Card text follows the framing rules in OC-16 and the IIF language rule (the prompt verb is always Tend; card text never says objective, target, enemy, player or NPC, and never names a place because any court can hold any role).

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-01 static clause, OC-16) and ADR-0020 sections 1-2, scoped to this story. QL-STORY-READY revised AC2 (match rule), AC3 (split) and AC4 (checks):*

- [ ] **AC1** (AC-OC-01 [U] static check `ChainDefShape`): the `FObjectiveDef` type has the fields `ObjectiveId`, `StageKind`, `TeamEligibility`, `SiteRole`, `CompletionsRequiredCount`, `CardTitle`, `CardLine`, `ClueForm` and **no hold-time field** (a reflection check fails on any property whose name contains `Hold` or `InteractSec`; a seeded control struct with a `HoldSec` member fails the same check).
- [ ] **AC2** (OC-16, OC-3 [U] `CacaoChainContent`): **GIVEN** `DA_ObjectiveChain_Cacao` loaded **THEN** it holds exactly the five rows of the OC-16 table (`Cacao.T0.PayRunner`, `Cacao.T0.GatherRoot`, `Cacao.T1.HearRunner`, `Cacao.T1.TakeGourd`, `Cacao.Final` with their stage, eligibility, site role, card title and card line equal to a constant fixture of the table), `ChainSerial` is set, there are at most 16 rows, card text is non-empty (whitespace-only counts as empty) and no title or line contains **objective, target, enemy, player or NPC as a case-insensitive whole word**.
- [ ] **AC3** (ADR-0020 sections 1-2, OBJ-V3): (a) [U] **GIVEN** the primary asset type `ObjectiveChain` **THEN** `GetPrimaryAssetId()` is invalid for the CDO and the type name equals the `DefaultGame.ini` scan entry exactly (case-sensitive); (b) [OBJ-V3 verification] a cooked-client run resolves `FPrimaryAssetId("ObjectiveChain", DA_ObjectiveChain_Cacao)` with cook rule `AlwaysCook` and loads it; the log line, cook command and build id are saved to `production/qa/evidence/oc-cook-check.md`.
- [ ] **AC4** (ADR-0020 E6, section 2): (a) a config test parses the localisation gather-target configuration and asserts the chain asset folder path is included; (b) `CardTitle` and `CardLine` are `FText`, `FObjectiveTeamView` (story 010) carries only indices and enums, and the cooked client reads card text from the asset, with no network read path for it.

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 1-2 and Key Interfaces:*

Types (all `StealthGameplay/Public/View/Features/Objectives/`, client-safe because card text is read client-side): `EObjectiveStage {Prelim1, Prelim2, Final}`, `EObjectiveEligibility {Team0, Team1, Both}`, `EObjectiveSiteRole {P1, P2, Final}`, `EClueForm {AreaAndFeature}`, `FObjectiveDef`, and `UObjectiveChainData : UPrimaryDataAsset { uint16 ChainSerial; TArray<FObjectiveDef> Definitions; GetPrimaryAssetId() -> ("ObjectiveChain", GetFName()) }`. `ChainSerial` is authored, stable and unique per chain; it tells the client which cooked chain the wire indices refer to. `Definitions` is bounded by 16 (the width of `CompletedDefMask`, story 010); validation of that bound is story 003, but the asset editor metadata should clamp the array.

The OC-16 table is the content authority. Rows: Prelim1 team 0 `Cacao.T0.PayRunner` "Pay the Runner" / "Leave the beans for the steward's runner."; Prelim2 team 0 `Cacao.T0.GatherRoot` "Gather the Root" / "Collect the bitter root set aside for your house."; Prelim1 team 1 `Cacao.T1.HearRunner` "Hear the Runner" / "Hear the palace runner's news at a quiet stone."; Prelim2 team 1 `Cacao.T1.TakeGourd` "Take the Gourd" / "Take the gourd left for your house."; Final, Both, `Cacao.Final` "The Ruler's Cacao" / "Spoil the ruler's cup before the rival house does." Both houses see identical final card text. Re-read OC-16 at implementation time; it wins over this snapshot. Cultural read of the card text before the Vertical Slice is a CD-GDD-ALIGN condition, not this story's gate.

The asset is created in the editor (MCP toolset or editor), not hand-written as a binary: do not hand-edit `.uasset` files. Project configuration needed (ADR-0020 Migration Plan): the Asset Manager primary asset type `ObjectiveChain` with `AlwaysCook` in `DefaultGame.ini`, via the TDC Asset Manager story (057); the localisation gather-target entry for the chain folder is part of this story's AC4. One cooked chain is selected for every round; a second cooked chain without a selection rule is a `Preparing` failure (story 005).

Prototype replaced: nothing in the prototype has a data asset. `AStealthObjective` tracked stage per team in `AStealthGameState` with no asset (see story 022 for removal).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-003-core-chain-validation-and-court-binding.md`: `ValidateChain` rules (counts, ids, role/stage match, 16-row bound) and binding.
- `story-010-team-view-struct-and-builder.md`: `FObjectiveTeamView` and the wire-width test.
- `story-013-clue-function-and-too-broad-log.md`: `FClueTag` and the clue text.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Features/Objectives/ObjectiveChainData.h` (enums, `FObjectiveDef`, `UObjectiveChainData`)
- `Source/StealthGameplay/Private/Features/Objectives/ObjectiveChainData.cpp`
- `Content/StealthGame/Objectives/DA_ObjectiveChain_Cacao.uasset` (created in the editor)
- `Config/DefaultGame.ini` (Asset Manager primary asset type; shared with TDC story 057, coordinate)
- `Config/Localization/` gather-target entry for the chain folder
- `Source/StealthGameplay/Private/Tests/Objectives/ChainDefShapeTest.cpp` (class `FObjectivesChainDefShapeTest`, name `StealthGame.Objectives.ChainDefShape`)
- `Source/StealthGameplay/Private/Tests/Objectives/CacaoChainContentTest.cpp` (`StealthGame.Objectives.CacaoChainContent`)
- `Source/StealthGameplay/Private/Tests/Objectives/ChainAssetIdTest.cpp` (`StealthGame.Objectives.ChainAssetId`)
- `production/qa/evidence/oc-cook-check.md`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS; the AC2 match rule and the AC3/AC4 rewrites were applied to the criteria above and the specs cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`ChainDefShape`): `FObjectiveDef` field set and no hold-time field
  - Given: UE reflection over `FObjectiveDef`
  - When: properties are enumerated
  - Then: the set is exactly `ObjectiveId`, `StageKind`, `TeamEligibility`, `SiteRole`, `CompletionsRequiredCount`, `CardTitle`, `CardLine`, `ClueForm`; no property name contains `Hold` or `InteractSec`
  - Edge cases: a seeded control struct with a `HoldSec` field must fail the same check; field order is not asserted.
- **AC-2** (`CacaoChainContent`): `DA_ObjectiveChain_Cacao` content
  - Given: `DA_ObjectiveChain_Cacao` loaded in the automation context
  - When: rows are read
  - Then: exactly 5 rows with ids as listed, each with stage, eligibility, site role, title and line equal to the OC-16 table (constant fixture); `ChainSerial` set; rows <= 16; card text non-empty; no forbidden whole word (case-insensitive) in any title or line
  - Edge cases: whitespace-only text counts as empty; 16 rows ok, 17 fails; ids unique; "targets" or "Players" outcomes decided by the whole-word rule.
- **AC-3** (`ChainAssetId` + OBJ-V3): primary asset registration
  - Given: `ObjectiveChain` primary asset type with `AlwaysCook`; `DefaultGame.ini` scan entry
  - When: T1 asks the CDO for `GetPrimaryAssetId`; OBJ-V3 loads the id on a cooked client
  - Then: CDO id invalid; type name equals the ini entry exactly (case-sensitive); the cooked client resolves `FPrimaryAssetId("ObjectiveChain", DA_ObjectiveChain_Cacao)` and loads it, recorded in `oc-cook-check.md`
  - Edge cases: a misspelled ini type fails T1; an uncooked asset yields a load failure recorded in the evidence file.
- **AC-4** (localisation gather + client-only text): card text localisable and off the wire
  - Given: Localization config (gather target paths) and the chain asset folder path
  - When: config is parsed
  - Then: the folder is inside a gather target; `CardTitle`/`CardLine` are `FText`; the cooked client reads them from the asset (no network read path)
  - Edge cases: folder moved outside the gather path fails; a `CardLine` declared `FString` fails the `FText` check.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)
- OBJ-V3: `production/qa/evidence/oc-cook-check.md` (cooked-client resolution log, cook command, build id)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic: `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`; cross-epic: `production/epics/tuning-data-config/story-057-asset-manager-primary-types.md` (the `DefaultGame.ini` primary-type mechanism)
- Unlocks: `production/epics/objectives-chain/story-003-core-chain-validation-and-court-binding.md`, `production/epics/objectives-chain/story-010-team-view-struct-and-builder.md`, `production/epics/objectives-chain/story-016-presentation-view-helpers-and-ring-rule.md`
