# Story 022: Prototype retirement and static scans

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-1 scope, OC-6; AC-OC-25; the epic's Prototype Disposition)
**Requirement**: `TR-oc-007` (no actor, no timer, no tick: the structural form of the stall-pressure rules), `TR-oc-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Implementation Guidelines; Migration Plan)
**Other ADRs**: ADR-0007: Code modules and include boundaries (module layout, promotion rule: promotion is a rewrite, not a file move)
**ADR Decision Summary**: Objectives must never spawn an actor for a court, objective or site; the prototype `AStealthObjective` and its replicated `bCompleted`/`InteractorCount` are replaced, not ported; no client module, Crowd, IIF or Presentation code may include the Objectives `Public/Server/` headers. The prototype keeps compiling until the Objectives epic lands; deleting its files needs user approval.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (source scans and a deletion; no engine API)
**Engine Notes**: The scans are source-text scans (class-name patterns and include edges) run as automation tests, using the include-lint framework from the TDC epic (`story-011` to `story-016`). A seeded-violation control proves each scan can fail. Deleting prototype files requires explicit user approval, so the deletion step is a process step; the test evidence is the mapping table and the post-removal build.

**Control Manifest Rules (this layer)**:
- Required: Promotion of prototype code is a rewrite, not a file move; never promote prototype code before its governing ADR is Accepted — source: ADR-0007, ADR-0020
- Forbidden: Never port the prototype `AStealthObjective` or its replicated `bCompleted`/`InteractorCount` — source: ADR-0020
- Forbidden: Never include the Objectives `Public/Server/` headers from client modules, Crowd, IIF or Presentation — source: ADR-0020
- Guardrail: none (static)

The production path is complete once stories 001-021 land, so this story closes the prototype. It does two things: it installs the structural guards that keep the old design from coming back (no per-court actors, no replicated completion flags, no client include of Objectives server headers), and it retires the prototype code, first proving that every behaviour the prototype test pinned is covered by a production test, then, only with the user's approval, deleting the prototype symbols and confirming the project still builds. Several earlier stories cite `ObjectivesIncludeLint`; this story owns it.

---

## Acceptance Criteria

*From ADR-0020 Guidelines and Migration Plan, GDD AC-OC-25 and the epic's Prototype Disposition, scoped to this story. QL-STORY-READY revised AC1 (allowlist until removal), AC2 (one lint owner) and AC3 (mapping table, approval is a process step):*

- [ ] **AC1** (ADR Guidelines [U] `ObjectivesStaticScan`): **GIVEN** a source scan of the production module sources with the class-name patterns `AObjective*`, `ACourt*`, `ASite*` and any `AActor` subclass in the Objectives namespace, and a replicated `bCompleted`/`InteractorCount` pattern, **THEN** the real tree reports 0 violations; before the prototype is removed the scan allowlists `AStealthObjective` and the listed prototype symbols by name and nothing else; a seeded `AObjectiveCourt` actor and a seeded replicated `bCompleted` each fail the scan; after removal the scan runs again with **no** allowlist.
- [ ] **AC2** (AC-OC-25, ADR Guidelines [U] `ObjectivesIncludeLint`, the single owner of this lint): **GIVEN** an include scan over client modules, Crowd, IIF and Presentation **THEN** none includes an Objectives `Public/Server/` header, directly or transitively through another header (the `View/Features/Objectives/` helper and struct headers are allowed); a seeded include fails the scan. Stories 001, 003, 005 and 016 cite this test rather than duplicating it.
- [ ] **AC3** (epic Prototype Disposition [U] mapped tests, then removal): the seven assertions of `FStealthRulesObjectiveTest` (`StealthGame.Rules.ObjectiveStages`: own stage-1 objective at stage 1; other team's preliminary; locked stage-2 at stage 1; final locked before preliminaries; shared final for team 0; shared final for team 1; no team) have equivalent `StealthGame.Objectives.*` tests mapped one-to-one in a table in this story's Implementation Notes (a case with no equivalent blocks the deletion); **after the user approves the deletion**, `AStealthObjective`, `AStealthGameMode::OnObjectiveCompleted`/`HandleInteractRequest` objective paths, `FStealthRules::CanCompleteObjective` and the HUD's objective-marker reads are removed or replaced, the project builds and the full `StealthGame.` suite passes; a leftover reference fails the build.

---

## Implementation Notes

*Derived from ADR-0020 Guidelines and Migration Plan and the epic's Prototype Disposition:*

Prototype inventory (`Source/StealthGame`): `Public/StealthObjective.h` and `Private/StealthObjective.cpp` (172 + 75 lines: two team-specific preliminary stages then a shared final revealed after the preliminaries; interacting visibly works the site; replicated `bCompleted`/`InteractorCount`; hold time from `UStealthSettings::ObjectiveInteractTime`); `StealthGameMode.cpp` `HandleInteractRequest` (around line 679), `OnObjectiveCompleted` (around line 722) and the debug auto-interact helper (around line 930); `StealthRules.cpp` `CanCompleteObjective` and its use; `StealthHUD.cpp` marker drawing at the objective actors (around lines 151 and 204); `StealthCharacter.cpp` interact progress (around lines 192 and 665); the generator's objective relocation. Line numbers are approximate and will have moved; the retirement list is produced from a symbol search at implementation time.

Mapping table to fill in (test names are in this epic): own stage-1 objective at stage 1 -> `StealthGame.Objectives.ProgressOnlyByCompletion` (Prelim1 live at Active entry); another team's preliminary cannot be completed -> `StealthGame.Objectives.CompletionGate` row 6; locked stage-2 at stage 1 -> `StealthGame.Objectives.FinalNeedsOwnUnlock` / `StageLifecycle`; final locked before preliminaries -> `FinalNeedsOwnUnlock`; shared final for team 0 and for team 1 -> `StealthGame.Objectives.CompletionGateFinal` (and `StageLifecycle`); no team cannot complete anything -> `StealthGame.Objectives.CompletionGate` rows 2-6 (unbound or ineligible pawn). Confirm each mapped test passes before any deletion. The prototype's `ObjectiveInteractTime` setting retirement belongs to TDC story 095.

The deletion is a user-approved step: list the exact files and symbols, ask for approval (the project rule: deleting prototype files needs user approval; the ADR-0020 Migration Plan says the same), and only then remove them. The HUD marker drawing is replaced by the HUD epic's stage card and ring; if the HUD epic has not landed, replace the marker reads with a no-op or the placeholder card, recorded in the story's changed-file list, so the project builds. Do not commit.

Scans: reuse the TDC include-lint framework and seeded-violation control helper (TDC stories 011-016, 010). The class-name patterns are the QA-lead's definition: `AObjective*`, `ACourt*`, `ASite*`, plus any `AActor` subclass declared in an `Objectives` namespace or folder.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Stories 001-021: the production behaviours the mapped tests cover.
- TDC `story-095-retire-prototype-settings-canonical-fields.md`: retiring `UStealthSettings::ObjectiveInteractTime`.
- HUD epic: the replacement stage card and ring.
- Round epic: the prototype elimination rule promotion (`EvaluateEliminationWinner`).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Tests/Objectives/ObjectivesStaticScanTest.cpp` (class `FObjectivesStaticScanTest`, name `StealthGame.Objectives.ObjectivesStaticScan`)
- `Source/StealthGameplay/Private/Tests/Objectives/ObjectivesIncludeLintTest.cpp` (`StealthGame.Objectives.ObjectivesIncludeLint`)
- `Source/StealthGame/Public/StealthObjective.h` and `Source/StealthGame/Private/StealthObjective.cpp` (delete, with user approval)
- `Source/StealthGame/Private/StealthGameMode.cpp` and `Public/StealthGameMode.h` (remove the objective paths)
- `Source/StealthGame/Private/StealthRules.cpp` and `Public/StealthRules.h` (`CanCompleteObjective`)
- `Source/StealthGame/Private/StealthHUD.cpp`, `StealthCharacter.cpp`, `StealthCityGenerator.cpp` (objective references)
- `Source/StealthGame/Private/Tests/StealthRulesTests.cpp` (`FStealthRulesObjectiveTest`, after mapping)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS; the AC1 allowlist, the single lint owner and the mapping-table rewrite were applied above and the specs cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`ObjectivesStaticScan`): no per-court actors, no replicated completion flags
  - Given: scanner over production module sources; prototype allowlist active until removal
  - When: the scan runs; then with a seeded `AObjectiveCourt` actor and a seeded replicated `bCompleted`
  - Then: real tree reports 0 violations; each seeded violation fails the scan
  - Edge cases: allowlist only names `AStealthObjective` and the listed prototype symbols; the scan runs again after removal with no allowlist.
- **AC-2** (`ObjectivesIncludeLint`): no client include of Objectives `Public/Server` headers
  - Given: include scanner over client modules, Crowd, IIF, Presentation
  - When: the scan runs; then with a seeded include
  - Then: 0 violations; the seeded include fails
  - Edge cases: transitive include through another header is detected; `View/Features/Objectives` helper header is allowed.
- **AC-3** (prototype retirement): prototype tests ported before deletion
  - Given: the six cases of `FStealthRulesObjectiveTest` (`StealthGame.Rules.ObjectiveStages`); a mapping table to `StealthGame.Objectives.*` tests
  - When: the mapped tests are run, then after user approval the prototype symbols are removed and the project builds
  - Then: all six mapped cases pass before deletion; `AStealthObjective`, `OnObjectiveCompleted`/`HandleInteractRequest` and `FStealthRules::CanCompleteObjective` are gone; the project builds and the suite passes
  - Edge cases: a case with no equivalent blocks the deletion; a leftover reference fails the build.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the two test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)
- Prototype retirement: the filled mapping table in this story and the post-removal build log noted at close; the user's approval of the deletion recorded in the story

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-021-pie2-end-to-end-cacao-chain.md` (the production path proven before the prototype goes; also stories 001-020 transitively); cross-epic: `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`, `story-012-include-lint-rules-a-c.md` through `story-016-lint-seeded-violation-controls.md`; `production/epics/tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`
- Unlocks: None
