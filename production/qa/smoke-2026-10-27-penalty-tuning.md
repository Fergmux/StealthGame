# Smoke check: Penalty tuning warnings and residual / hunt model (AC-NKP-18, AC-NKP-35)

- **Date**: 2026-10-27
- **Story**: `production/epics/npc-kill-penalty-informants/story-020-penalty-tuning-warnings-and-residual-models.md` (TR-pen-001, TR-pen-003)
- **Type / gate**: Config/Data, ADVISORY
- **Schema change**: `TuningSchemaVersion` 6 to 7 (fingerprint `0x3a54113f358da039`): Penalty's public `ExileBeatSec` and `ExileCardTimeoutSec` registered (W-NKP-5 reads both).
- **Rows registered** (owner Penalty, `Source/StealthFoundation/Private/Tuning/PenaltyInvariants.cpp`): I-NKP-1, W-NKP-2, W-NKP-3, W-NKP-4, W-NKP-5.

## Tests

| Test name | Covers | Result |
|---|---|---|
| `StealthGame.Tuning.StrikeLimitOneWarning` | AC1 / AC-NKP-18: L = 1 warns on a test preset and rejects a host-selectable preset and Default; D 25 + starvation 10 (W-NKP-2); grace 1.5 vs lockout 1.75 (W-NKP-3); D 5 / fall 2.0 / fade 1.5 no floor warning and the predicate at D = 4.0 warns (W-NKP-4); card timeout 1.0 with beat 0.8 (W-NKP-5); defaults raise none | Success (full-suite runs 1-3, 2026-10-28) |
| `StealthGame.Tuning.ResidualHuntModel` | AC2 / AC-NKP-35: F4 `P_res` 0.111 / 0.288 / 0.476 / 0.659; F5 `T_deny` 58.36 s and R 29.18 s per strike, each +/- 0.01 (test-only evaluator, NpcCount 50, walk speed 140 cm/s) | Success (full-suite runs 1-3, 2026-10-28) |

## Notes

- Presets are built in code through a test-local `ITuningPresetSource` and validated by the real `FTuningPipelineCore::Resolve` over the production invariant table; the project holds no checked-in test preset data.
- `RespawnStarvationSec` (10) and `NoPawnFadeSec` (0.5) are pending-owner constants (TD-TDC-I3 pattern); the 1.5 fade case injects the input through `StealthPenaltyInvariants::Evaluate`.
- Fixture files: `Source/StealthGameplay/Private/Tests/Penalty/PenaltyTuningWarningsTest.cpp`, `Source/StealthGameplay/Private/Tests/Penalty/PenaltyResidualHuntModelTest.cpp`.
