# Story 082: The host HUD module compiles with includes of FRoundPublicState and C-TeamView only

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-009` (`FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal); GDD AC 30b ([STATIC]); rules R23 (g) host-view parity, R20.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: Four runtime modules (StealthFoundation, StealthGameplay, StealthPresentation, StealthGame) plus Editor and StealthTestHarness; `Public/View/` (client-safe) versus `Public/Server/` (server-only, compile-guarded); consumer-owned interfaces wired by the composition root; plain-C++ cores behind thin facades. This story applies section 2 (a service with both faces is split into a `View/` facade and a `Server/` implementation) and section 3 (three enforcement layers for the include boundary) to the one Round surface the HUD may read.
**ADR Version**: ADR-0007 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0007 knowledge risk)
**Engine Notes**: Touches ADR-0007 Verification Required item (1) in spirit (the `STEALTH_PRESENTATION_MODULE` guard define covers C++ translation units only) but adds a separate source scan; it does not depend on `PrivateDefinitions` reaching every translation unit. No post-cutoff API is used (`UE_LOGF` and the C++20 floor are not touched). Item (3), the DataValidation Blueprint audit, is a different check (Blueprint reach) and is not this story.

**Control Manifest Rules (this layer)** (Round-relevant module rules sit in the Foundation Layer Rules):
- Required: A service with both faces is split into a `View/` facade and a `Server/` implementation (ADR-0007).
- Forbidden: Never let a `Public/View/` header include a `Server/` header (ADR-0007).
- Guardrail: Tests: per-commit pipeline <= 10 min (ADR-0008); the scan reads source files only and must stay well inside it.

**Prototype disposition**: None (new [STATIC] check). The prototype's `AStealthHUD` and its direct reads of the GameMode and GameState are not promoted and not touched by this story.

**Where it sits**: Exit B (Core). It is the build-time half of Round's host-parity rule (R23 (g)): on the listen host, calls such as `GetGameMode` return authoritative objects, so the host's HUD must read Round state only through `FRoundPublicState` and the viewer's own C-TeamView, the same inputs every client has.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 30b, scoped to this story:*

- [ ] The host HUD module compiles with includes of `FRoundPublicState` and C-TeamView only.

---

## Implementation Notes

*Derived from ADR-0007 sections 2-3 and Implementation Guidelines, and GDD R23 (g):*

- **What is scanned.** The sources of the host HUD module (the client-side wiring that ADR-0007 places in `StealthPresentation`; `AStealthHUD` lives there). The only Round headers it may include are the one for `FRoundPublicState` (a `Public/View/` wire type) and the one for the C-TeamView Roster face. Any include of a Round `Public/Server/` header, the Round game-instance store, the phase core or the queue fails the scan; the compile guard in ADR-0007 section 2 already stops a `Server/` include from `StealthPresentation` at compile time, so this scan adds the allowlist-of-Round-headers rule on top of it.
- **Non-vacuity (QA requirement).** The scan fails if the module or its sources are not found, and fails if it finds zero includes of the two permitted headers. Keep one planted-violation fixture directory outside the scanned globs that includes a forbidden Round header, to prove the scan can fail.
- **"Compiles" half.** The module must still compile: the story's test runs the scan inside the Automation framework, and the build gate (`/smoke-check`) proves the compile. Do not assert compilation from inside the test.
- **Module name and path.** The AC says "the host HUD module" without a path. Open: confirm the module name and path against ADR-0007 section 1 (`StealthPresentation` and its HUD sources) and the HUD epic; record the confirmed path as a fixture constant in the test constant files (story 005, `StealthRoundTestConstants.h`), not inline.
- **Sequencing note.** The HUD module is built by the HUD epic (`production/epics/hud`). Until its sources exist the scan reports "module not found" and fails by design; write the scan against the fixture path and the planted-violation fixture first, and expect the first green run when the HUD sources land.
- **Include lint relation.** ADR-0007 section 3 lint rule (c) (a `View/` header must not include a `Server/` header) and rule (h) (no `GetGameMode` and similar in `StealthPresentation`) are separate CI checks owned by the module-layout work; do not reimplement them.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 029 (`production/epics/round-match-framework/story-029-public-state-replication.md`): `FRoundPublicState` itself (this story's dependency) and its `Public/View/` placement.
- Story 080 (`production/epics/round-match-framework/story-080-no-leak-before-ended.md`): runtime replicated-property audit and byte identity.
- Story 083 (`production/epics/round-match-framework/story-083-no-clock-no-count.md`): the widget-tree scan for clock and count bindings.
- The HUD module and widgets (hud epic); the repo-wide include lint and Blueprint audit (ADR-0007 section 3, owned by the module-skeleton work in tuning-data-config).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/HostParityIncludesTest.cpp` (class `FRoundHostParityIncludesTest`, test name `StealthGame.Round.HostParityIncludes`; a [STATIC] scan run as a Simple automation test)
- `Source/StealthFoundation/Private/Tests/Fixtures/StealthRoundTestConstants.h` (fixture constants file from story 005; this story adds the scanned module path and permitted header names)
- planted-violation fixture directory outside the scanned globs (path chosen by `/dev-story`; story 005 uses `tests/static-fixtures/` for the same purpose)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: The host HUD module's includes of Round headers are `FRoundPublicState` and C-TeamView only.
  - Given: a [STATIC] scan of the host HUD module sources.
  - When: the build runs it.
  - Then: the only Round headers included are those for `FRoundPublicState` and C-TeamView.
  - Then: the module still compiles.
  - Edge cases: the scan fails if the module or its sources are not found, or if zero includes are found (non-vacuity). Open: confirm the module name and path against ADR-0007 section 1 and the HUD epic.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/HostParityIncludesTest.cpp` (class `FRoundHostParityIncludesTest`, test name `StealthGame.Round.HostParityIncludes`) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-029-public-state-replication.md`
- Unlocks: None
- Cross-epic: `production/epics/tuning-data-config` (module skeleton, include-lint scaffolding and test bootstrap; stories not yet created)
