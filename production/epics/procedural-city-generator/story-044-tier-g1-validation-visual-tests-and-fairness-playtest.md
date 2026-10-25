# Story 044: Tier G1 validation: visual tests and fairness playtest

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: the tier G1 kit (M07, M08, M09 temple, M11 deck and bridge, M12 awning + counter, the real planter + bush family, the court fixture family, prop families) does not exist until the art-bible mass-production gate (8.x.14) lifts, which needs spikes R1, R2 and R3 on RTX 3060-class hardware (not reported). G0 rounds and layouts do not count for these criteria (GDD: G0 data is non-representative for pillar validation).
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md` (AC-G19, AC-G21, Open Questions 5 and 6; LSC VA-1 to VA-4 and the LSC fairness playtest criterion)
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. The criteria validate Pillar 1 and Pillar 4 on real generated layouts.)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0022 for the G1 content and budgets)
**ADR Decision Summary**: Generated layouts must satisfy every LSC predicate and the LSC human tests (VA-1 bush 25 m legibility, VA-2 kit pieces never read as a person, VA-4 paired socket captures); the pipeline is role-blind so no generated structure identifies a role. Tier G1 is the full pipeline R-G4 with the real kit.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0022; G1 content and rendering at the 5,000-instance budget)
**Engine Notes**: Human tests need testers and retained captures; the playtests need >= 30 G1 rounds. Tester recruitment is an open M1 risk. No editor automation is used as a gate (control manifest).

**Control Manifest Rules (this layer)**:
- Required: Visual/Feel stories have retained screenshots and a lead sign-off; the multi-client leak-parity diff and Pillar 1 gates apply to generated layouts too — source: coding-standards, ADR-0008
- Forbidden: Never count G0 layouts or rounds toward AC-G19, AC-G21 or the Open Question 5 and 6 decision rules — source: GDD Visual/Audio tier table
- Guardrail: none (qualitative and statistical evidence)

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G19, AC-G21, Open Questions 5 and 6), scoped to this story:*

- [ ] **AC1** (AC-G19, `Visual.LscVa1ToVa4OnGenerated`): **GIVEN** 3 generated seeds at tier G1 **THEN** the LSC bush 25 m test, static-figure test and paired socket-capture test pass at their LSC thresholds with evidence retained.
- [ ] **AC2** (AC-G21, ADVISORY): **GIVEN** >= 30 playtest rounds on tier-G1 generated layouts (G0 rounds do not count) **THEN** ObjectiveWin per zone falls within 10-20 of 30.
- [ ] **AC3** (OQ 5 and OQ 6, record only): **GIVEN** the same playtests **THEN** the report records each quantity under a definition fixed in the playtest plan before the first counted round (dated, with a map figure): the correct-enemy-read rate in the first 60 s against the later-round rate (informed-first-strike definition from the M1 D38 operational definition), axis-corner pre-positioning (proposed: >= 3 teammates inside the corner-facing court's region at either axis end before their team's first preliminary completes), and the half-predictability walkthrough result (per tester, the share of the other half's courts and ingresses sketched within 1 cell of their true position, n testers recorded), and the 1.5x decision rule is applied to the recorded rates; no pass/fail.

---

## Implementation Notes

*Derived from GDD AC-G19, AC-G21, OQ5, OQ6 and the LSC acceptance protocols:*

AC1 uses the LSC protocols: bush 25 m greyscale legibility (LSC AC-45: >= 92 of 100 bush judgments correct and <= 5 of 100 decorative plants called bushes, >= 10 testers, >= 20 in-situ captures), static-figure test (AC-47: each kit piece fails < 3 "person" calls and <= 5% over >= 200 trials) and paired socket-capture (AC-49: <= 80 of 150 correct per category, >= 30 pairs, >= 5 reviewers); whether the AC-46 colour-swatch test is in scope is unconfirmed (the GDD AC cites only the three named tests): take the three and flag. Captures retained as `bush-25m-*.png` plus the results sheet in `production/qa/evidence/citygen/`. AC2 is ADVISORY: a miss is logged and escalated to the game-designer, it does not block; the GDD does not say how draws and timeouts count or how to treat more than 30 rounds: take "only decisive ObjectiveWin rounds per zone, the first 30 rounds" and flag. AC3 reproduces the open-question measures without inventing thresholds: the 1.5x rule is the GDD's decision rule (if correct enemy reads in the first 60 s exceed 1.5x the later-round rate, make 4-zone mode the default and accept the lower `p`); pre-positioning above 25% of rounds is the OQ5 telemetry trigger; the D38 informed-first-strike definition is itself still open (M1 risk table), so the playtest plan with its dated definitions is a dependency to keep visible.

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-040-stack-and-slab-visual-equivalence.md`: stack/slab visual equivalence at tier G0.
- `story-038-role-prior-and-lsc-batch-audits.md`: the batch role-prior audits.
- The PROCEED/PIVOT D38 measurement itself: M1 milestone, HUD epic exit evidence.
- Authoring the G1 kit content: art pipeline (`/asset-spec system:procedural-city-generator`).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/evidence/citygen/bush-25m-*.png` and `static-figure-*`, `paired-capture-*` (retained captures)
- `production/qa/evidence/citygen/g1-validation-evidence.md` (results sheet, playtest tallies, sign-off)
- `production/qa/playtests/citygen-g1-playtest-plan.md` (plan with the dated definitions)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC2 ADEQUATE; AC3 was GAPS on missing operational definitions and is revised above with the qa-lead's wording).*

- **AC1** `Visual.LscVa1ToVa4OnGenerated`
  - Setup: 3 G1 seeds (G0 doesn't count). Use the LSC protocols: bush 25 m greyscale (AC-45), static-figure (AC-47), paired capture (AC-49).
  - Verify: run the protocols on the generated layouts.
  - Pass condition: AC-45 gets >= 92 of 100 bush judgments correct and <= 5 of 100 decorative plants called bushes (>= 10 testers, >= 20 in-situ captures). AC-47 has each kit piece failing < 3 "person" calls and <= 5% over >= 200 trials. AC-49 is <= 80 of 150 correct per category (>= 30 pairs, >= 5 reviewers). `bush-25m-*.png` and the results sheet are retained.
- **AC2** (AC-G21, ADVISORY)
  - Setup: >= 30 playtest rounds on G1 layouts only. Record ObjectiveWin per zone per round.
  - Verify: tally wins per zone.
  - Pass condition: each zone's wins are within 10-20 of 30. A miss is logged and escalated to the game-designer, and doesn't block.
- **AC3** (OQ 5 and OQ 6, record only)
  - Setup: the same rounds; the plan with dated definitions exists before round 1; testers for the walkthrough each walk only their own half and then sketch the other.
  - Verify: the report lists the three recorded quantities with n, and applies the 1.5x rule to the recorded rates.
  - Pass condition: all three quantities are present with their definition; no pass/fail is asserted. Missing definitions or an undated plan make the evidence incomplete.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures and the results sheet in `production/qa/evidence/citygen/` plus lead sign-off in `production/qa/evidence/citygen/g1-validation-evidence.md` (BLOCKING for AC1; AC2 and AC3 are ADVISORY/record-only)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-029-client-decoration.md`, `story-038-role-prior-and-lsc-batch-audits.md`, `story-040-stack-and-slab-visual-equivalence.md`, `story-036-developer-commands-and-debug-overlay.md`; the mass-production gate (art bible 8.x.14; spikes R1-R2-R3 reported)
- Unlocks: none (closes Open Questions 5 and 6 by data for the creative-director)
