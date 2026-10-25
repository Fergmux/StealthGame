# Story 036: Developer commands and debug overlay

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md` (UI Requirements; no UX spec needed, no UX flag)
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. These tools expose the pipeline's stage timings and G-PRE ids that TR-003 requires to be measurable.)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Attempt records, failed predicate ids, `LayoutAttempt`, `LayoutHash` and `Seed` may be logged to `LogStealthLayout`; `FSiteAssignment`, zone -> team, respawn picks and every sub-seed value never are. Developer tools are compiled out of Shipping.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014)
**Engine Notes**: Console commands via `IConsoleManager` (pre-cutoff, stable); the overlay extends the LSC socket overlay (`stealth.layout.debug` in the LSC developer UI) and draws with debug-draw helpers. The Generator has **no player-facing UI**: generation runs behind Round's single neutral briefing screen, which shows no progress bar, attempt count or "generating city" text, because a host-only or per-attempt difference would be a timing tell (Session rule 11.8).

**Control Manifest Rules (this layer)**:
- Required: Layout logging limited to attempt records, failed predicate ids, `LayoutAttempt`, `LayoutHash` and `Seed` — source: ADR-0014
- Forbidden: Never log `FSiteAssignment`, zone -> team, respawn picks or any sub-seed value; never ship the developer commands or overlay in Shipping; never make a gate depend on the MCP toolsets — source: ADR-0014, ADR-0008
- Guardrail: the overlay is default off and costs nothing when off

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (UI Requirements), scoped to this story:*

- [ ] **AC1** (UI Requirements, `Dev.RunCommand`): **GIVEN** `stealth.citygen.run <Seed> [Attempt]` in a test world **THEN** it prints the candidate index, stage timings, G-PRE metrics and `LayoutHash`.
- [ ] **AC2** (UI Requirements, `Dev.OverlayAndShipping`): **GIVEN** `stealth.citygen.debug` (default off) **THEN** one retained screenshot per layer (area type, block, corridor class, repair pass, G-PRE failure id), each with its legend, on a named seed shows cells coloured accordingly, the failure-id layer uses a fixture with a known failing candidate, and the commands and overlay are compiled out of Shipping (symbol check).
- [ ] **AC3** (UI Requirements, `Dev.NoPlayerFacingUi`): **GIVEN** a round start **THEN** the briefing screen shows no progress bar, attempt count or "generating city" text.

---

## Implementation Notes

*Derived from GDD UI Requirements:*

Three developer surfaces, non-Shipping only: `stealth.citygen.run <Seed> [Attempt]` builds a layout in a test world and prints the candidate index, stage timings, G-PRE metrics and `LayoutHash` (a bad argument prints usage and never crashes; two runs of the same input print an identical index and hash); `stealth.citygen.debug` toggles the overlay (default off) with five selectable layers (area type, block, corridor class, repair pass, G-PRE failure id; list the selection mechanism, a cvar value or subcommand, in the dev-story notes) reusing the LSC socket overlay; `stealth.citygen.batch` is story 037. Screenshots: a named seed S for four layers and a fixture seed with a known failing candidate for the G-PRE failure-id layer; legends visible in each. The no-UI check (AC3) captures the briefing screen three times during `Preparing` (start, middle, just before lift) on a normal seed and on one that reseeds (candidate > 0), dumps the widget tree, and finds no progress-bar widget and no text matching attempt, generating, seed or percent; reseed and non-reseed captures are visually identical (retained in `production/qa/evidence/citygen/`).

Prototype replaced: the prototype logged the reachability result to the output log only.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-037-batch-harness-and-f4-f6-audits.md`: `stealth.citygen.batch` and its CSV.
- LSC developer UI and its socket overlay: `level-socket-contract` (validate commandlet and overlay stories).
- The briefing screen itself: Round & Match Framework / HUD epics.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenConsoleCommands.cpp` (non-Shipping)
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenDebugOverlay.cpp` (non-Shipping)
- `Source/StealthGameplay/Private/Tests/CityGen/DevRunCommandTest.cpp` (class `FCityGenDevRunCommandTest`, name `StealthGame.CityGen.Dev.RunCommand`)
- `production/qa/evidence/citygen/overlay-*.png` and `briefing-*.png` (retained screenshots)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC3 ADEQUATE; AC2 revised and ADEQUATE).*

- **AC1** `Dev.RunCommand`
  - Setup: a non-Shipping build and a test world. Capture the console output through an output device (this one is automatable).
  - Verify: run `stealth.citygen.run 12345` and `stealth.citygen.run 12345 1`.
  - Pass condition: output has the candidate index, a timing per stage S1-S12 plus G-PRE, the G-PRE metrics and a hex `LayoutHash`. Two runs of the same input give identical index and hash. A bad argument prints usage and doesn't crash.
- **AC2** `Dev.OverlayAndShipping`
  - Setup: a non-Shipping build and named seed S. For the failure-id layer, use the fixture with a known failing candidate. The layer-selection mechanism is listed in the story's Files.
  - Verify: first confirm `stealth.citygen.debug` is off by default and no overlay draws. Then enable each of the 5 layers in turn (area type, block, corridor class, repair pass, G-PRE failure id) and capture a screenshot with its legend.
  - Pass condition: 5 retained screenshots, one per layer, each with a legend. The colours match the legend and the failure-id layer shows the fixture's known failure id. A symbol check on the Shipping binary finds 0 symbols for the commands and overlay (DEFERRED until a Shipping build exists).
- **AC3** `Dev.NoPlayerFacingUi`
  - Setup: a 2-client PIE. Use one normal seed and one that reseeds (candidate > 0).
  - Verify: capture the briefing screen 3 times during `Preparing` (start, middle, just before lift). Dump the widget tree.
  - Pass condition: no progress-bar widget, and no text matching attempt, generating, seed or percent. Reseed and non-reseed captures are visually identical. Screenshots are retained in `production/qa/evidence/citygen/`.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen touched in `production/qa/evidence/citygen/` (five overlay layers and the briefing captures); the `Dev.RunCommand` test must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-023-candidate-loop-work-unit-cap-and-metrics.md`, `story-030-pure-function-determinism-and-stage-hashes.md`; cross-epic `level-socket-contract/story-027-graybox-validate-commandlet-and-ci-gate.md` (LSC developer UI)
- Unlocks: `story-037-batch-harness-and-f4-f6-audits.md`, `story-044-tier-g1-validation-visual-tests-and-fairness-playtest.md`
