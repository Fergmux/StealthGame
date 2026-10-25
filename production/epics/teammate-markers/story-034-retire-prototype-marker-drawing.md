# Story 034: Retire the prototype marker drawing

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The first-playable prototype draws teammate markers in `AStealthHUD::DrawTeammateMarkers` (an `AHUD` canvas pass) from `AStealthCharacter::IsTeammateOfLocalViewer()`, which reads a team field replicated on every pawn: the leak that the first-playable report records as known issue 2 (any client could mark every player). Production replaces it with the UMG-leaf, C-TeamView-sourced system and nothing is promoted from the prototype. This story removes the prototype drawing once story 019 has landed (it must not be removed before, or there would be no markers), and re-runs the prototype's networked test E (a teammate sees the marker, an enemy sees an empty bush) against the production system as the behavioural reference. Retiring the prototype's replicated team field itself belongs to the Roster and Policy stories (the pawn allowlist), not this one.

**GDD**: `design/gdd/teammate-markers.md` (TM-R2; Cross-References: `production/first-playable-report.md`, `Source/StealthGame/Private/StealthHUD.cpp`)
**Requirement**: `TR-tmk-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (Implementation Guidelines: the prototype `AStealthHUD` drawing code must be replaced, not ported)
**Also governed by**: ADR-0001, ADR-0007 (`AStealthHUD` only owns the bootstrap)
**ADR Decision Summary**: The prototype `AStealthHUD` drawing code is replaced, not ported; `AStealthHUD` exists only as the owner of the bootstrap (ADR-0007); markers are never drawn through the `AHUD` canvas.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Removing prototype source files is a source change only; no `.uasset` is touched. Build with `tools/dev/cycle.ps1 -Tests -NoLaunch` and check the headless run exits cleanly (`No automation tests matched` is NOT ASSESSED, never a pass).

**Control Manifest Rules (this layer)**:
- Required: `AStealthHUD` and all client-side wiring live in `StealthPresentation` — source: ADR-0007
- Forbidden: Never port the prototype `AStealthHUD` drawing code; never use the `AHUD` canvas for game UI — source: ADR-0021
- Guardrail: Prototype tests that still pass (`StealthGame.Rules.*`) must still pass after the removal

---

## Acceptance Criteria

*From ADR-0021 Implementation Guidelines, GDD TM-R2 and the first-playable report, scoped to this story:*

- [ ] **AC1** (derived): **GIVEN** the production modules **WHEN** story 019 has landed **THEN** `AStealthHUD::DrawTeammateMarkers` and its call (`StealthHUD.cpp:48`) and every use of `AStealthCharacter::IsTeammateOfLocalViewer()` for drawing are removed and nothing is ported; no marker code reads the prototype's per-pawn team field (a grep of the marker module and the leaf for that field returns no hits).
- [ ] **AC2** (reference behaviour): **GIVEN** the prototype's networked test E (a teammate sees the marker, an enemy sees an empty bush; the evidence is `production/qa/evidence/2026-10-24_bush-concealment-teammate-vs-enemy.png` and the first-playable report's test table) **WHEN** it is re-run against the production system **THEN** the same outcome holds on both clients, with one retained screenshot per client in `production/qa/evidence/`.

---

## Implementation Notes

*Derived from ADR-0021 and the Prototype Disposition of the epic:*

Remove `DrawTeammateMarkers` and its declaration (`StealthHUD.h:28`), and any use of `IsTeammateOfLocalViewer()` that exists only for marker drawing; leave other prototype HUD behaviour (kill confirmation, civilian count, defence state, banner) for the hud epic to replace. If `IsTeammateOfLocalViewer()` has other callers, leave the function and list them in the story's completion note (their retirement is the Roster and IVP stories' job). Test E reference: a 3-player listen-server PIE (two allies, one enemy) with the bush concealment scenario; the teammate client sees the hatched marker over the empty bush, the enemy client sees an empty bush and no marker. Capture both clients with `tools/dev/capture-window.ps1`.

Do not delete the prototype tests; confirm `StealthGame.Rules.*` still pass.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-paint-markers-in-world-anchor-leaf.md`: the replacement drawing.
- the Roster and Information Visibility Policy epics: removing the replicated team field from the pawn.
- the hud epic: replacing the rest of `StealthHUD.cpp`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Private/StealthHUD.cpp` and `Source/StealthGame/Public/StealthHUD.h` (remove `DrawTeammateMarkers` and its call)
- `Source/StealthGame/Private/StealthCharacter.cpp` (only if `IsTeammateOfLocalViewer()` has no other caller)
- `production/qa/evidence/tmk-prototype-parity-client*.png`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: the production modules after story 019. When: grep and a build run. Then: `AStealthHUD::DrawTeammateMarkers` and its call, and every drawing use of `AStealthCharacter::IsTeammateOfLocalViewer`, are gone; nothing is ported; no marker code reads the prototype's per-pawn team field. Edge cases: a seeded reference fails the check.
- **AC2**: Given: the prototype networked test E re-run against the production system. When: a teammate and an enemy client view the scene. Then: the teammate sees the marker and the enemy sees an empty bush, on both clients. Edge cases: one retained screenshot per client in `production/qa/evidence/`; the test E definition is the first-playable report's networked test table and the 2026-10-24 bush evidence screenshot named in AC2.

QL-STORY-READY: ADEQUATE (the agent noted AC1 and AC2 depend on story 019 landing; the dependency is listed below)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the grep and build result plus a documented playtest with the two retained screenshots `production/qa/evidence/tmk-prototype-parity-client*.png`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-019-paint-markers-in-world-anchor-leaf.md` (the replacement must land first; story 019 is currently Blocked, so this story cannot be started until it is done), `story-027-leak-audit-marker-objects-and-canaries.md`
- Unlocks: None
