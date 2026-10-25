# Story 014: Include lint rules (g)-(h): server UCLASS specifiers and presentation forbidden APIs

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0007 section 3; traced to architecture.md bootstrap order / epic Exit A) and `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`; keeping server types out of Blueprint reach and out of presentation code supports the client/server face split; GDD CR-10 / AC-28)

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: Server types are never Blueprint-exposed (every server UCLASS declares `NotBlueprintType, NotBlueprintable`, every server UINTERFACE `meta=(CannotImplementInterfaceInBlueprint)`, no Blueprint specifier on a member) and presentation code may not discover servers by searching the world; the include lint enforces both as rules (g) and (h).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The lint is a text script with no engine API. Background facts from ADR-0007 Engine Compatibility: every subsystem gets a "Get <X> Subsystem" Blueprint node unless it is `NotBlueprintType` [V `K2Node_GetSubsystem.cpp:282` -> `EdGraphSchema_K2.cpp:1228`]; `AActor`/`UActorComponent` subclasses inherit `BlueprintType`/`Blueprintable`; a `UINTERFACE` is implementable in Blueprint by default [U]. The Blueprint-graph half of the audit (DataValidation) is out of this story.

**Control Manifest Rules (this layer)**:
- Required: Every UCLASS in a `Server/` header declares `NotBlueprintType, NotBlueprintable`; every server UINTERFACE declares `meta=(CannotImplementInterfaceInBlueprint)` (source: ADR-0007)
- Forbidden: Never put `BlueprintCallable`, `BlueprintPure`, `BlueprintReadOnly`, `BlueprintReadWrite` or `BlueprintAssignable` on a member of a server type (source: ADR-0007)
- Forbidden: Never branch on `IsPlayerControlled()`, controller class or `PlayerState` presence in pawn, component, AnimBP or audio code for anything another client can observe (source: ADR-0001; the (h) rule keeps Presentation from reaching those objects)
- Required: Every lint gate includes a negative control the suite detects; lints emit observations only (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), as revised in `tdc-plan-amendments.md`:*

- [ ] per G6, the lint reports (g) a UCLASS in a `Server/` header lacking `NotBlueprintType, NotBlueprintable`, a UINTERFACE lacking `meta=(CannotImplementInterfaceInBlueprint)`, or any member of a server type carrying BlueprintCallable, BlueprintPure, BlueprintReadOnly, BlueprintReadWrite or BlueprintAssignable; and (h) StealthPresentation source using TActorIterator, TActorRange, GetAllActorsOfClass, GetAllActorsWithInterface, GetPlayerControllerIterator, GetGameMode, GetAuthGameMode, or ANY `GetPlayerState`/`GetController` call on a pawn expression unless that line carries a `// LOCAL-PAWN-OK: <reason>` annotation (a text lint cannot decide "non-local", so every use needs the annotation); zero findings on the real tree; seeded fixtures reported.

---

## Implementation Notes

*Derived from ADR-0007 section 3 (rules g, h and the Blueprint-reach rule), Key Interfaces and Implementation Guidelines; ADR-0008 sections 1 and 3; amendments G2, G5, G6:*

- **Script** `tools/ci/lint/include-lint-gh.sh` on the story 011 emitter and gate; findings as `file:line rule message` with the rule letter (`g`, `h`) as the rule id (story 016 asserts one finding per letter); repository root as an argument (default: the real root) so story 016 can aim it at a fixture tree.
- **Rule (g) scope**: headers under `Public/Server/` in the five production modules (G5). Checks: each `UCLASS(...)` has both `NotBlueprintType` and `NotBlueprintable` in its specifier list; each `UINTERFACE(...)` has `meta=(CannotImplementInterfaceInBlueprint)` (the specifier may share a `meta` list with other keys; handle that); no `UPROPERTY`/`UFUNCTION` member of a server type carries any of the five Blueprint specifiers. Specifier lists can span lines, so parse the macro up to its balanced closing parenthesis. A correctly specified UINTERFACE must not be flagged (spec edge case). Event Log types are plain C++ with no reflection macros, so they never trigger (g).
- **Rule (h) scope**: all source under `Source/StealthPresentation` (Public and Private). Flag the identifiers `TActorIterator`, `TActorRange`, `GetAllActorsOfClass`, `GetAllActorsWithInterface`, `GetPlayerControllerIterator`, `GetGameMode`, `GetAuthGameMode`; and every `GetPlayerState` or `GetController` call on a pawn expression unless the same line carries `// LOCAL-PAWN-OK: <reason>` with a non-empty reason (an annotation with an empty reason is itself a finding, spec edge case). What counts as a "pawn expression" is not decidable by text and the amended AC says every use needs the annotation; the conservative reading is to treat every call named `GetPlayerState`/`GetController` in Presentation source as in scope. Occurrences inside comments or string literals are not flagged (spec edge case; the annotation comment itself is read as the annotation, not as code).
- **Real tree**: StealthPresentation currently has only the story 002 skeleton, so zero findings is expected; the rule matters as the HUD and markers arrive (AC-TMK-06, AC-IVP-17, AC-PSR-42 are extended by (h) per ADR-0007).
- **Seeded fixtures (G6)** under `tools/ci/lint/selftest/`: a Server UCLASS missing `NotBlueprintType, NotBlueprintable`; a Server UINTERFACE missing `meta=(CannotImplementInterfaceInBlueprint)`; a server-type member with each of BlueprintCallable, BlueprintPure, BlueprintReadOnly, BlueprintReadWrite, BlueprintAssignable; Presentation source using each listed API; `GetPlayerState`/`GetController` on a pawn expression with and without a `// LOCAL-PAWN-OK: <reason>` annotation, plus the empty-reason and API-in-a-comment cases. Each non-annotated fixture must be reported as `file:line`; the annotated line passes.
- **G2**: selftest as above (a clean pass AND a seeded violation detected) and the wrapper `StealthGame.Build.IncludeLintGh` (class `FBuildIncludeLintGhTest`, `Source/StealthFoundation/Private/Tests/Build/IncludeLintGhTest.cpp`; name proposed) running the script through `FPlatformProcess::ExecProcess` and asserting exit code and observations. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`.
- Prototype: not scanned by (g) or (h) (G5; the prototype module is scanned only for (e) and (f)).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`: emitter and gate
- `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`: rules (a)-(c)
- `production/epics/tuning-data-config/story-013-include-lint-rules-d-f.md`: rules (d)-(f)
- `production/epics/tuning-data-config/story-015-include-lint-rules-i-j.md`: rules (i)-(j)
- `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`: the combined seeded tree
- The DataValidation Blueprint audit (ADR-0007 section 3, item 3) belongs to the editor module's later work, not to this epic's stories 001-017

---

## Files


- `tools/ci/lint/include-lint-gh.sh`, `tools/ci/lint/cpplex.awk` (shared C++ lexer), `tools/ci/lint/rule-g.awk`, `tools/ci/lint/rule-h.awk`
- `tools/ci/lint/selftest/run-include-gh-selftest.sh`, `tools/ci/lint/selftest/include-gh/fixtures/**` (g/h seeded and clean fixtures)
- `Source/StealthFoundation/Private/Tests/Build/IncludeLintGhTest.cpp` (G2 wrapper)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: the lint reports rules (g) and (h), annotated lines pass, seeded fixtures are reported, and the real tree has zero findings
  - Given: seeded fixtures: a UCLASS in `Server/` without `NotBlueprintType, NotBlueprintable`; a UINTERFACE without `meta=(CannotImplementInterfaceInBlueprint)`; a server-type member with each of BlueprintCallable, BlueprintPure, BlueprintReadOnly, BlueprintReadWrite, BlueprintAssignable; Presentation source using each listed API; `GetPlayerState`/`GetController` on a pawn expression with and without a `// LOCAL-PAWN-OK: <reason>` annotation
  - When: include-lint-gh runs on the fixtures and the real tree
  - Then: each non-annotated fixture is reported as `file:line`; the annotated line passes; the real tree reports zero findings
  - Edge cases: an annotation with an empty reason fails; an API name in a comment is not flagged

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/lint/selftest/` (run by the T0 gate step) and the G2 wrapper `StealthGame.Build.IncludeLintGh` (`Source/StealthFoundation/Private/Tests/Build/IncludeLintGhTest.cpp`) must exist and pass (qa.level is full, so tests are required)

**Status**: [x] Created and passing (`tools/ci/lint/selftest/run-include-gh-selftest.sh` 5/5 cases; `StealthGame.Build.IncludeLintGh`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`
- Unlocks: `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`
- Sequencing note (not a plan dependency): the G2 wrapper test needs the StealthFoundation module of `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`; the Presentation scan scope exists only after story 002.
- Open verification claims from the plan's Note lines: none (no Note line on 014). Open design point: the meaning of "pawn expression" is handled conservatively (every call needs the annotation) because the inputs state a text lint cannot decide it.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: seeded (g) UCLASS with no specifiers / only one of the two / the other one, UINTERFACE with no meta and with an unrelated meta, each of BlueprintReadOnly, BlueprintReadWrite, BlueprintCallable, BlueprintPure, BlueprintAssignable on a member and a specifier list spanning lines: 11 findings `:N g`; seeded (h) each of the seven APIs, unannotated GetPlayerState/GetController, an empty-reason annotation (on a call and alone), and GetGameMode carrying an annotation (still flagged): 12 findings `:N h`; annotated lines and API names in comments/strings pass; correct UCLASS/UINTERFACE (also multi-line and shared meta list) and an Event-Log-like plain C++ type pass; real tree `GATE: PASS (11 file(s) scanned, 0 findings)`)
**Decisions**: rule ids `g`/`h`; parsing is C++-aware (cpplex.awk blanks comments and string content, keeps offsets, so specifier lists are read to their balanced parenthesis); (g) UINTERFACE needs the `meta` token and `CannotImplementInterfaceInBlueprint` in the same list, any UPROPERTY/UFUNCTION in a Public/Server header is checked (the whole header is server types); (h) the LOCAL-PAWN-OK annotation must be on the same line and exempts only GetPlayerState/GetController, never the other APIs; an empty-reason annotation is a finding even with no call on the line; every call named GetPlayerState/GetController in Presentation source is in scope (text cannot decide 'pawn expression'). Logged in the decision log.
**Deviations**: none
**Test Evidence**: `production/qa/evidence/story-013-016-include-lint-rules-d-j/test-run.txt` (build `Result: Succeeded`; 34/34 `StealthGame.` tests Success), `selftest-gh.txt`, `stamp.txt`
**Run result**: N/A — CI tooling; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no further defects found
