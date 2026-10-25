# Story 055: AC-30 mechanism: owner-invariant rows with Pass/Warn/Reject and context

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-004` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-30 (mechanism half), CR-6, CR-15.6

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. The Formulas table of the TDC GDD lists many owner invariants (I-ASN-4, I-ST-1, W-NKP-3 and so on) beyond the TDC-owned I-1..I-5. ADR-0004 section 3 implements all of them as rows of one table, `TuningInvariants.cpp`, each `{ Id, Owner, Check }` where `Check` is a pure function returning `Pass`, `Warn` or `Reject`; the return value (not a fixed severity column) lets one row express the GDD's mixed rows (a reject clause plus a warn clause, rows that depend on `bIsPublic` or on `PresetId == Solo`). Story 048 builds the table for I-1..I-5; this story proves the general mechanism over a synthetic schema fixture, so each owner epic only has to add rows (CR-15.6) and does not need new machinery. The real per-owner rows and the full-schema closure are not here: the owner epics add the rows and story 096 closes the loop once all 18 owners have registered.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: One invariant table of pure `Check` functions evaluated on the clamped set; a `Reject` row rejects the whole preset and a `Warn` row logs and keeps it; the report names the exact GDD ID, and an invariant never writes a value.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Plain C++ over pre-cutoff APIs; nothing post-cutoff. Warnings use `UE_LOGF` on `LogStealthFoundation`. None of ADR-0004's three Verification Required items is touched.

**Control Manifest Rules (this layer)**:
- Required: invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range); invariants reject or warn, never repair — source: ADR-0004
- Required: every reject/warn invariant carries its GDD ID; a new cross-field constraint goes through a TDC change request and a row in `TuningInvariants.cpp` — source: ADR-0004
- Forbidden: never repair a failed invariant by clamping a dependent key — source: ADR-0004
- Guardrail: tuning validation under 1 ms at `Preparing` — source: ADR-0004

---

## Acceptance Criteria

*From the story plan (GDD AC-30 is split: this story is the mechanism half, the aggregate half is story 096), scoped to this story:*

- [x] **AC-30 (mechanism half, test `StealthGame.Tuning.OwnerInvariantMechanism`)**: over a synthetic schema fixture, a reject-class row that alone is broken rejects the preset and the report names exactly that invariant ID; a warn-class row accepts the preset with exactly one warning naming the ID; mixed rows (reject clause and warn clause) report the worst result and name the clause; context-dependent rows (bIsPublic, PresetId == Solo) behave per scope; offline rows are not evaluated at preset load.

---

## Implementation Notes

*Derived from ADR-0004 section 3 (Validator, invariants) and Implementation Guidelines 5-6:*

- **Where the code goes.** `Source/StealthFoundation/Private/Tuning/StealthTuningInvariants.cpp` (extends the table story 048 created) and `Source/StealthFoundation/Public/Server/StealthTuningInvariants.h`. Row shape per ADR-0004 Key Interfaces: `FTuningInvariant { const ANSICHAR* Id; const ANSICHAR* Owner; ETuningCheck (*Check)(const FStealthServerTuning&, const FTuningDerived&, const FTuningCheckContext&); }`, with `FTuningCheckContext { bool bIsPublic; FName PresetId; }`. `GetTuningInvariants()` returns non-offline rows only.
- **Fixture, not real rows.** The test uses a synthetic schema and a synthetic invariant table (fixture rows) injected through the same seam as story 050's `FTuningResolveInput::Schema`; an equivalent injection point for the invariant table is needed and is open (the ADR names only the schema seam). Do not add real owner invariants here: each owner epic adds its own rows with fixtures (CR-15.6).
- **Result handling.** The pipeline takes the worst result across rows. A `Reject` anywhere rejects the whole preset (the end-to-end "FallbackLevel 1" behaviour is story 097); a `Warn` appends one warning to the `FValidationReport` and to `LogStealthFoundation`, naming the row ID. For a mixed row (such as I-HUD-4 or I-OC2 in the GDD) the report names the clause that produced the worst result: the clause naming scheme is open (the ADR says only that the report "names the clause"); a suffix on the ID is a proposal for `/dev-story`.
- **Context rows.** Fixture rows reproduce the three real scoping patterns in ADR-0004: reject only when `bIsPublic` else warn (I-3, I-4, I-NKP-1 pattern), and not applicable when `PresetId == Solo` (W-RMF-4 pattern). Assert each in both scopes.
- **Offline rows.** A fixture row declared offline (the ADR lists O-CNB-6, O-CNB-7, I-SCL-10/11/14 and the LSC layout predicate as real offline rows) is registered in a separate offline list and must not run at preset load: give the fixture check a side effect (a counter) and assert it stays zero.
- **Purity.** `Check` receives const input and cannot write; add a `static_assert` on the function signature (const reference parameters) in the test (CR-6).
- **Test placement and naming.** `Source/StealthFoundation/Private/Tests/Tuning/OwnerInvariantMechanismTest.cpp`, class `FTuningOwnerInvariantMechanismTest`, name `StealthGame.Tuning.OwnerInvariantMechanism`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` (or `BEGIN_DEFINE_SPEC` for the several cases) with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. Use `FStealthLogCapture` (story 009) for the warning counts and the fault-sink helper (story 008) for teardown. No wall-clock predicate.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`: the I-1..I-5 table and its boundary values
- `production/epics/tuning-data-config/story-097-invariants-reject-whole-preset.md`: end-to-end rejection of the whole preset at FallbackLevel 1
- `production/epics/tuning-data-config/story-096-owner-invariants-full-schema-closure.md`: every real owner invariant over the full schema (blocked on the owner epics)
- `production/epics/tuning-data-config/story-094-safe-range-lint-baseline.md`: the safe-range lint that probes keys against these invariants
- `production/epics/tuning-data-config/story-065-catalog-clean.md`: catalog presets validated clean in CI

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningInvariants.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/OwnerInvariantMechanismTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the owner-invariant mechanism over a synthetic fixture (test `StealthGame.Tuning.OwnerInvariantMechanism`)
  - Given: a synthetic schema fixture with reject-class, warn-class, mixed, context-dependent (bIsPublic, PresetId == Solo) and offline rows
  - When: presets are validated
  - Then: a broken reject row alone rejects the preset naming exactly that ID; a warn row accepts with exactly one warning naming the ID; a mixed row reports the worst result naming the clause; context rows act per scope; offline rows are not evaluated at load
  - Edge cases: `Check` functions are pure and never write a value (CR-6)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/OwnerInvariantMechanismTest.cpp` must exist and pass (`StealthGame.Tuning.OwnerInvariantMechanism`)

**Status**: [x] Created and passing (`StealthGame.Tuning.OwnerInvariantMechanism`)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`
- Unlocks: `production/epics/tuning-data-config/story-096-owner-invariants-full-schema-closure.md`
- Open verification claims: the plan Note says the real per-owner rows are added by each owner epic (TDC CR-15.6) and that full-schema closure is story 096 (a scope statement, not an engine claim). Open items found while writing: the injection point for a substituted invariant table, and the clause-naming scheme for mixed rows.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (over a synthetic invariant table injected through FTuningResolveInput::Invariants: the baseline passes with no warning; a broken reject-class row alone rejects the preset (Default active) and the report names exactly X-REJ-1; a warn-class row accepts with exactly one warning, naming X-WARN-1; the mixed row reports Warn naming the warn clause at 52 and Reject naming the reject clause at 48 (worst wins); the bIsPublic row rejects in a HostSelectable preset and warns in a non-selectable one; the Solo row is not applicable in Solo and warns elsewhere; the offline row's call counter stays 0 and the real preset-load table holds no offline row; a static_assert pins the const-only Check signature)
**Decisions**: FTuningInvariant gains optional RejectClause/WarnClause (findings carry the clause of the produced result; the Id stays the exact GDD ID) and bOffline (GetTuningInvariants() returns non-offline rows, GetTuningOfflineInvariants() lists offline ones, EvaluateInvariants skips offline rows of an injected table); the invariant-table injection point is FTuningResolveInput::Invariants (test seams only)
**Files changed beyond the list**: `Source/StealthFoundation/Public/Server/StealthTuningInvariants.h`, `Source/StealthFoundation/Private/Tuning/StealthTuningValidate.cpp`, `Source/StealthFoundation/Public/Server/StealthTuningPipelineCore.h`
**Deviations**: None (no real owner row added; story 096 closes the full schema)
**Test Evidence**: `Source/StealthFoundation/Private/Tests/Tuning/OwnerInvariantMechanismTest.cpp`; `production/qa/evidence/story-055-owner-invariant-mechanism/stamp.txt`; full suite 70/70 green (`tools/dev/cycle.ps1 -Tests -NoLaunch`: Result: Succeeded, tests exit=0)
**Run result**: N/A — validator core; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects left
