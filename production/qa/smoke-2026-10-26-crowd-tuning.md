# Smoke check: Crowd tuning keys and motion-profile data (story cnb-003), 2026-10-26

**Result: PASS**

| Check | Result |
|---|---|
| 108 `Crowd*` keys in `FCrowdTuning_Server` (49 GDD + 59 OQ13 constants), schema regenerated (`Stealth.Tuning.SchemaGen`) | PASS: `StealthGame.Crowd.Tuning.KeysRegistered`, `StealthGame.Tuning.SchemaComplete`, `.SchemaGenDeterministic` |
| `TuningSchemaVersion` 2 -> 3, history `{3, 0x6c226685555a3908}` | PASS: `StealthGame.Tuning.SchemaVersionBumped` |
| Invariants I-CNB-1..6 and the 18 Crowd Min/Max range pairs | PASS: `StealthGame.Crowd.Tuning.Invariants`, `StealthGame.Tuning.InvariantBoundaries` |
| Crowd keys are ServerOnly and enter the F-2 hash | PASS: `StealthGame.Crowd.Tuning.ServerOnlyHashed` |
| Asset keys resolve: `DA_CrowdMotionProfile_Default`, `ST_CrowdFull`, `ST_CrowdRestricted` under `/Game/StealthGame/AI` (built by `Stealth.Crowd.BuildAssets`) | PASS: `StealthGame.Crowd.Tuning.AssetKeys`, `StealthGame.Crowd.VirtualInput.ProfileAsset`, `StealthGame.Crowd.StateTree.AssetsBootCheck` |
| Full suite | 275/275 (`production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`) |

Notes: there is no runtime tuning store yet, so the Director freezes the schema defaults at round start (the
`SetTuningForRound` seam refuses writes while Active). The asset soft paths live in `FCrowdAssetKeys` outside the schema
(a soft object path is not a schema row type yet). The pending Level Socket Contract keys (`EmergenceExitCancelMaxPct` 25,
`AmbientEmergencePerSocketMinPerMin` 0.5, `EmergenceSocketCooldownSec` 20) are read through
`StealthTuningPendingOwnerKeys` until that epic registers them.
