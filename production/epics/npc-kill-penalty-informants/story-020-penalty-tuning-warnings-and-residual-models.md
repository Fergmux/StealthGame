# Story 020: Penalty tuning warnings and the residual and hunt model checks

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Config/Data
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-001`, `TR-pen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (validator, cross-field warnings, Reject rule), with ADR-0019 §1 (keys consumed) — the registry rows are TDC's, Penalty owns the meaning
**ADR Decision Summary**: Tuning keys live in the data pipeline with one canonical row each and a validator that clamps, warns on cross-field combinations and rejects unsafe presets. Penalty's five keys are `NpcKillStrikeLimit` (default 3, hard 2-5, safe 2-4, `L = 1` only behind TDC's `bTestAllowsStrikeLimitOne` on a non-selectable test preset), `InformantRespawnDelaySec` (15, 5-60), `ExileBeatSec` (0.8, 0-2.0), `StrikeOutReleaseGraceSec` (2.0, 0-5.0) and `ExileCardTimeoutSec` (2.0, 1.0-4.0); the cross-field checks of Penalty CR-11 are sent to TDC.
**ADR Version**: 2026-10-24 (ADR-0004; ADR-0019 for the consuming side)

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Data-only and pure-function checks: no engine API beyond the project's own tuning service. ADR-0004's engine notes apply to the preset asset pipeline (`SoftRef` tuning type, async preload), which this story does not touch. AC2 (F4/F5 are balance analysis, not runtime code) is evaluated by a test-only helper.

**Control Manifest Rules (this layer)**:
- Required: tuning values are data-driven through the tuning pipeline, never hard-coded; each key has one canonical row in TDC's catalogue — source: ADR-0004
- Forbidden: no informant-only tuning key exists ("deliberately absent": no informant movement, collision, sight or region key) — source: GDD Tuning Knobs, ADR-0019
- Guardrail: validation runs at load, not per frame — source: ADR-0004

TDC owns the registry and the validator; this story proves the Penalty-specific rows and cross-field warnings (Penalty CR-11) behave as the GDD states, and that the balance analysis formulas F4 and F5 return the documented values (so a later change to the knobs shows a visible diff against the model).

**Prototype replaced:** the prototype's `UStealthSettings` NPC-kill and informant-delay fields (`NpcKillLimit`, informant respawn delay) read in `AStealthGameMode`. The canonical-key migration is tuning-data-config `story-095-retire-prototype-settings-canonical-fields.md`; this story only checks the Penalty rows after it.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-18, AC-NKP-35), scoped to this story:*

- [ ] **AC1** (AC-NKP-18, [Config/ADVISORY][Dep:TDC CR-11] `StealthGame.Tuning.StrikeLimitOneWarning`): **GIVEN** presets with `NpcKillStrikeLimit` 1 **WHEN** validated **THEN** a non-selectable test preset loads with a warning and a host-selectable preset or `Default` is rejected; **AND** `InformantRespawnDelaySec` 25 with `RespawnStarvationSec` 10 logs the downtime warning; **AND** `StrikeOutReleaseGraceSec` 1.5 with `AttackAttemptLockoutSec` 1.75 logs the grace warning; **AND** `InformantRespawnDelaySec` 5 with `DeathFallSec` 2.0 and `NoPawnFadeSec` 1.5 (the hard maxima, floor 4.7 s) loads with no floor warning, while the TDC predicate `1.2 + DeathFallSec + NoPawnFadeSec ≤ InformantRespawnDelaySec` called directly with the injected out-of-range value D = 4.0 returns the warning (the guard cannot fire through the clamped hard ranges); **AND** `ExileCardTimeoutSec` 1.0 with `ExileBeatSec` 0.8 logs the card-timeout warning (Penalty CR-11).
- [ ] **AC2** (AC-NKP-35, [Config/ADVISORY] residual and hunt model, F4 and F5): **GIVEN** the formulas **WHEN** evaluated from unrounded inputs **THEN** `P_res` = 0.111 (I = 2, b = 1), 0.288 (I = 2, b = 3), 0.476 (I = 4, b = 3) and 0.659 (L = 5, I = 4, b = 3), and with `c_N` 0.6, `c_I` 0.3, D 15, `L_pick` 0.5, `d_back` 6,000 cm, `T_deny` = 58.36 s and R = 29.18 s per strike, each ± 0.01; AC-NKP-23 (story 022) calibrates `b`. *(The GDD formula tables also use `NpcCount` 50 and walk speed 140 cm/s: list them as inputs in the test.)*

---

## Implementation Notes

*Derived from GDD Tuning Knobs, W-NKP warnings and Formulas F3-F5:*

The five Penalty keys already exist as canonical rows in TDC (tuning-data-config `story-040-canonical-keys-and-faces.md`; the cross-field rows and warnings are in `story-047-derived-indicators.md` and `story-048-invariants-i1-i5.md`). This story adds the Penalty-owned checks: preset fixtures for each AC1 case (checked in as data under the project's test preset folder, no inline magic numbers beyond the AC's boundary values), a Config/Data smoke test that loads them through the real validator and asserts the exact warnings, and the test-only evaluator for F4/F5 (`P_res`, `T_deny`, `R`) in the test module.

Warnings are advisory (ADVISORY gate level) except the rejection of a host-selectable `L = 1` preset, which is the validator's Reject rule. The floor guard `1.2 + DeathFallSec + NoPawnFadeSec <= InformantRespawnDelaySec` cannot fire through the clamped hard ranges (5 >= 1.2 + 2.0 + 1.5 = 4.7), so it is tested by calling the TDC predicate directly with an injected out-of-range D = 4.0.

Check the warning identifiers: the W-NKP-3 (grace vs lockout) and W-NKP-5 (`0.4 + 0.35 + ExileBeatSec + 0.3 <= ExileCardTimeoutSec` at the default timeout) wording in the GDD Tuning Knobs table; use the TDC warning ids as defined there, never new ones. Latch rules are Round for `InformantRespawnDelaySec`, `NpcKillStrikeLimit`, `StrikeOutReleaseGraceSec`; Action for `ExileCardTimeoutSec` (GDD table).

Smoke evidence: a `production/qa/smoke-[date].md` entry with the test names and results (Config/Data evidence).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- tuning-data-config (all stories): the validator, the registry, preset assets and the Reject rule implementation.
- `story-022-telemetry-and-playtest-report.md`: calibration of `b` from internal matches.
- `story-001-penalty-core-store-and-service-shell.md`: Penalty's runtime read of the latched `L`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyTuningWarningsTest.cpp` (class `FPenaltyTuningWarningsTest`, name `StealthGame.Tuning.StrikeLimitOneWarning`)
- `Source/StealthGameplay/Private/Tests/Penalty/PenaltyResidualHuntModelTest.cpp` (class `FPenaltyResidualHuntModelTest`, name `StealthGame.Tuning.ResidualHuntModel`)
- Test preset data assets under the project's test-preset folder (authored through the tuning pipeline; no `Content/*.uasset` edits are made by this story's authors outside the editor workflow)
- `production/qa/smoke-[date].md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-18, `StealthGame.Tuning.StrikeLimitOneWarning`, smoke evidence)
  - Given: presets for the five combinations in the AC.
  - When: each is validated through the real validator.
  - Then: `NpcKillStrikeLimit` = 1 on a non-selectable test preset loads with a warning, on a selectable preset or `Default` is rejected; D = 25 with `RespawnStarvationSec` = 10 logs the downtime warning; grace 1.5 with lockout 1.75 logs the grace warning; D = 5, `DeathFallSec` 2.0, `NoPawnFadeSec` 1.5 loads with no floor warning while the predicate called with D = 4.0 returns the warning; `ExileCardTimeoutSec` 1.0 with `ExileBeatSec` 0.8 logs the card-timeout warning.
  - Edge cases: the floor guard cannot fire through the clamped ranges, hence the direct predicate call.
- **AC2** (AC-NKP-35, `StealthGame.Tuning.ResidualHuntModel`, pure functions)
  - Given: the F4 and F5 formulas, `NpcCount` 50, walk speed 140 cm/s and the AC's inputs.
  - When: evaluated from unrounded inputs.
  - Then: `P_res` = 0.111, 0.288, 0.476, 0.659; `T_deny` = 58.36 s and R = 29.18 s per strike; each +/- 0.01.
  - Edge cases: F4/F5 are not runtime code: name the test-only evaluator.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass (`production/qa/smoke-*.md`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic: tuning-data-config `story-040-canonical-keys-and-faces.md`, `story-047-derived-indicators.md`, `story-048-invariants-i1-i5.md`, `story-095-retire-prototype-settings-canonical-fields.md`
- Unlocks: None
