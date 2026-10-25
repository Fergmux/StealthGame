# Story 027: Instance, entry and shape budget audit

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: spike R1 (ADR-0022: 1,000 / 3,000 / 5,000 runtime ISM instances on a 3060-class PC) has not reported, and the Manifest wire-size spike (LSC Open Question 2, `level-socket-contract/story-010-spike-manifest-wire-size-and-loss.md`) has not reported; `CityGenInstanceCapCount` (5,000) and `CityGenManifestEntriesMaxCount` (3,500) are ASSUMED until they do (the AC carries the GDD's dagger).
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-004`, `TR-citygen-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. TR-004: <= 5,000 instances, <= 20,000 shapes; TR-007: Manifest <= 3,500 entries.)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0022 for the 5,000-instance Nanite budget)
**ADR Decision Summary**: Caps are checked at emission (`ManifestOverflow` fails the candidate and the next is tried): <= 5,000 instances including the decoration reserve, <= 3,500 Manifest entries, <= 20,000 collision shapes. ADR-0022 R1 measures whether 5,000 holds on the 3060-class target.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0022 overall; rendering budget unmeasured on the target)
**Engine Notes**: Counting is plain C++ over the emitted candidate and the Manifest-derived client geometry; the thresholds come from keys and are re-set from the spike reports as data only.

**Control Manifest Rules (this layer)**:
- Required: Layout clients <= 5,000 instances, about 48 ISM components, <= 20,000 collision shapes; Manifest 13.5-31.5 KB — source: ADR-0014
- Forbidden: Never raise a cap in code; caps are tuning data (`CityGenInstanceCapCount`, `CityGenManifestEntriesMaxCount`) — source: ADR-0004
- Guardrail: runtime kit + decoration instances <= 5,000 (target <= 4,000) — source: ADR-0022

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G20 dagger, Edge Cases overflow rate), scoped to this story:*

- [ ] **AC1** (AC-G20, `Budget.InstancesEntriesShapes`, provisional): **GIVEN** 1,000 seeds **THEN** kit + decoration instances <= `CityGenInstanceCapCount` (5,000, <= 4,000 in 95%), Manifest entries <= `CityGenManifestEntriesMaxCount` (3,500) and collision shapes <= 20,000 on the server and on each client (client count derived from the Manifest).
- [ ] **AC2** (Edge Cases, `Budget.OverflowRate`): **GIVEN** a 50-seed batch **THEN** the fraction of candidates failing `ManifestOverflow` is <= 5%, otherwise the audit fails and names the kit or slab tables.
- [ ] **AC3** (`Budget.ThresholdsFromReports`): **GIVEN** the R1 and Manifest-wire-size reports **WHEN** they land **THEN** the cap and entry-max keys change in tuning data files only (no code diff), each new value cites its report section, and the `Budget.InstancesEntriesShapes` re-run output is saved in `production/qa/evidence/citygen/` and passes at the new values.

---

## Implementation Notes

*Derived from GDD F2, R-G9 and the QL-STORY-READY review:*

A nightly stress batch (1,000 consecutive seeds from a fixed first seed, thresholds read from the tuning keys, not from constants) counting kit + decoration instances (decoration up to `CityGenDecorReserveCount` 1,500), Manifest entries and collision shapes on the server and on a Manifest-derived client geometry. At least 950 of 1,000 seeds must be <= 4,000 instances; 5,000 passes and 5,001 fails; the first violating seed is printed. The overflow rate denominator is candidates tried (not seeds); zero candidates fails explicitly (no divide by zero). If more than 5% of a 50-seed batch's candidates overflow, the kit or slab tables must change (GDD Edge Cases). AC3 is a T0 script plus an evidence-file check: the allowed diff list is the tuning data files for the two keys (and `CityGenDecorReserveCount` if R1 requires it); a re-run that fails at the new values keeps the story open and escalates to the technical-artist. The F2 estimate (story 019 AC3) is the sanity check beside the measured counts.

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-placement-bay-stacks-and-manifest-emission.md`: the F2 model and size equality.
- `story-025-failure-modes-and-teardown.md`: the runtime `ManifestOverflow` path.
- `story-043-kit-gpu-cost-and-server-spawn-nav-time.md`: the GPU and time measurements R1 and R4 need.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Tests/CityGen/InstancesEntriesShapesTest.cpp` (class `FCityGenBudgetInstancesEntriesShapesTest`, name `StealthGame.CityGen.Budget.InstancesEntriesShapes`, nightly)
- `Source/StealthGameplay/Private/Tests/CityGen/OverflowRateTest.cpp` (class `FCityGenBudgetOverflowRateTest`)
- `tools/ci/citygen-budget-thresholds-check.ps1` (AC3 T0 script: diff touches tuning data only; evidence file exists)
- `production/qa/evidence/citygen/` (re-run output after the reports land)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC2 ADEQUATE; AC3 revised and ADEQUATE).*

- **AC1** `Budget.InstancesEntriesShapes` (provisional)
  - Given: 1,000 consecutive seeds from a fixed first seed, with thresholds read from tuning keys. This is a nightly stress batch.
  - When: each seed is built, counting server and client (Manifest-derived) shapes.
  - Then: kit + decoration instances <= `CityGenInstanceCapCount` (5,000). At least 950 of 1,000 seeds are <= 4,000. Entries <= 3,500. Shapes <= 20,000 on the server and on each client.
  - Edge cases: 5,000 passes and 5,001 fails. The first violating seed is printed.
- **AC2** `Budget.OverflowRate`
  - Given: a fixed 50-seed batch.
  - When: candidates are counted.
  - Then: `ManifestOverflow` candidates / all candidates tried <= 5%. On failure the message names the kit and slab tables.
  - Edge cases: the denominator is candidates, not seeds. Zero candidates fails explicitly with no divide-by-zero.
- **AC3** `Budget.ThresholdsFromReports`
  - Given: the R1 report, the wire-size report, and the commit or changeset that re-sets the keys.
  - When: the changeset is checked and `Budget.InstancesEntriesShapes` is re-run.
  - Then: the diff touches only the tuning data files (0 code files). Each changed key value carries a report section citation. Re-run output exists in `production/qa/evidence/citygen/` and passes at the new values.
  - Edge cases: if the new values make the audit fail, the story stays open and the failure is escalated. The permitted data-file list is named in the dev-story notes. This is a T0 script plus an evidence-file check.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the two test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later), plus the AC3 evidence once the reports land

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-019-placement-bay-stacks-and-manifest-emission.md`, `story-025-failure-modes-and-teardown.md`, `story-026-server-kit-spawn-from-manifest.md`; spike reports: R1 (ADR-0022, no story owns it yet; performance-analyst), `level-socket-contract/story-010-spike-manifest-wire-size-and-loss.md`
- Unlocks: `story-037-batch-harness-and-f4-f6-audits.md` (overflow rate), `story-043-kit-gpu-cost-and-server-spawn-nav-time.md`
