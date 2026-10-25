# Story 042: Shipping symbol audit and the character-pool lint set

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-003`, `TR-cpa-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-003 = Server-private streams from `GetServerSubSeed(AppearanceDeal)` (`FSecretStream`, SipHash counter mode) for Replace; `Take` reserved (no draw). TR-cpa-006 = One look path for every viewer and pawn kind; no keyed material parameters.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (with ADR-0012)
**ADR Decision Summary**: `CI.ShippingSymbolAudit` builds Shipping and fails if the entropy-injection hook, the debug overlay or the `Warn` floor policy is linked; lints keep MID creation, per-pawn material parameters and engine RNG out of `Core/SharedPawn` and `Core/CharacterPool`; each gate must be watched failing against a seeded violation.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0008)
**Engine Notes**: Shipping-only properties cannot be checked by an automation test inside Shipping; the CI step inspects the built binary's symbols. The lint harness is a script run in CI (`.github/workflows/tests.yml`, self-hosted Windows runner).

**Why this story**: This is the one registry for every `LINT-CPA-*` the other stories reference (qa-lead finding 4) and the Shipping audit. **AC-1 and AC-2 revised per QL-STORY-READY**: the lint list is named and the `Warn` symbol is named. Read GDD "Acceptance Criteria" preamble ("Shipping-only properties") and ADR-0012 Implementation Guidelines 2, 5, 11.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The `Warn` floor policy, the entropy-injection hook and any `AppearanceId` debug overlay are compiled out of Shipping (ADR-0012)
- Forbidden: Never ship `Warn`, `CharacterPool_InjectEntropy` or `CharacterPool_DebugOverlay`; never `CreateDynamicMaterialInstance`/`SetScalarParameterValue`/`SetVectorParameterValue` under `Core/SharedPawn` and `Core/CharacterPool`; no `FMath::Rand*`/`FRandomStream` there
- Guardrail: CI step time within the CI budget (record)

---

## Acceptance Criteria

*From GDD preamble (`CI.ShippingSymbolAudit`), ADR-0012 Guidelines 2, 5, 11; named lints per QL-STORY-READY finding 4:*

- [ ] **AC-1** GIVEN a Shipping build, WHEN `CI.ShippingSymbolAudit` runs, THEN it fails if `CharacterPool_InjectEntropy` or `CharacterPool_DebugOverlay` is present or the named `Warn`-policy symbol (`EDeckFloorPolicy::Warn`, mangled name recorded in the script) is linked; the audit is first run against a build with a seeded symbol.
- [ ] **AC-2** GIVEN the lint set, THEN these fail the build on a violation: the three material-API lints and the engine-RNG lint over `Core/SharedPawn` and `Core/CharacterPool`, plus `LINT-CPA-NoResort` (story 012), `LINT-CPA-NoTakeTag` (012), `LINT-CPA-FreeFunctionNoStatic` (017), `LINT-CPA-NoTierRef` (024), `LINT-CPA-PoolLoadOnlyInSubsystem` (025), `LINT-CPA-NoSetMeshInBP` (026), `LINT-CPA-ApplyVisualsPurity` (027), `LINT-CPA-NoHashFromRuntimePtr` (028), `LINT-CPA-NoCpaRpcOrTimer` (033).
- [ ] **AC-3** GIVEN one seeded violation per gate, THEN every gate fails on its violation and passes when it is removed, with the evidence recorded in the story.

---

## Implementation Notes

- Put the lints under `tools/lint/` as small scripts with a shared runner; each script has a `--selftest` that runs against a fixture of seeded violations.
- Allowlist test files by path only; comments are excluded by rule.
- The audit needs a Shipping build: run it on the self-hosted runner; locally record one run's log under `production/qa/logs/`.

**Prototype disposition**: none.

---

## Out of Scope

- Each lint's own subject behaviour (the stories that register it); the symbol definitions themselves (stories 012, 013)

---

## Files

- `tools/lint/cpa_lints.py` and fixtures under `tools/lint/fixtures/`
- `tools/ci/shipping_symbol_audit.*`
- `.github/workflows/tests.yml` (CI wiring only)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead) with the finding-4 amendments applied to AC-2.*

**Test: Shipping symbol audit**
- Given: a Shipping build.
- When: `CI.ShippingSymbolAudit` runs.
- Then: it fails if `CharacterPool_InjectEntropy`, `CharacterPool_DebugOverlay` or the named Warn-policy symbol is present.
- Edge cases: the audit is first run against a build with a seeded symbol.

**Test: lints**
- Given: the lint set.
- When: they run over `Core/SharedPawn` and `Core/CharacterPool`.
- Then: `CreateDynamicMaterialInstance`, `SetScalarParameterValue`, `SetVectorParameterValue`, `FMath::Rand*` and `FRandomStream` hits fail the build; each named `LINT-CPA-*` fails on its seeded violation.
- Edge cases: allowlisted test files.

**Test: gates watched failing**
- Given: one seeded violation per gate.
- When: each gate runs.
- Then: every gate fails on its violation and passes when it is removed.
- Edge cases: record the evidence in the story.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: CI run logs for `CI.ShippingSymbolAudit` and the lint self-tests under `production/qa/logs/` (documented CI evidence), plus `tools/lint/` fixtures

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-012-deal-shuffle-canonical-order-and-streams.md`, `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`
- Unlocks: None
