# NpcCount 50 (user decision 2026-10-25) — evidence, 2026-10-27

## Change

- `UStealthSettings::NpcCount` default 40 -> 50 (`Source/StealthGame/Public/StealthSettings.h`), and an explicit
  `NpcCount=50` line under `[/Script/StealthGame.StealthSettings]` in `Config/DefaultGame.ini` (no `SecurityToken` line).
  The TDC/registry key `NpcCount` (`FCrowdTuning_Server::NpcCount`, schema row, `StealthCrowdTuningKnobs.h`) already
  defaulted to 50; the live game reads `UStealthSettings::NpcCount` (spawn loop, composition-root `Params.NpcCount`), which
  was the only place still at 40.
- Defect found by the run and fixed: `AStealthGameMode::SpawnNpc` asked the Director for start placement index
  `CrowdNpcs.Num()`, so one placement whose spawn collided (a node at Z=410 on the graybox) was retried 29 times and the
  round started with 21 of 50 NPCs. The placement list now holds 1.5 x NpcCount entries and a cursor that moves past a
  colliding placement (`StartPlacementCursor`).

## Run (3-player PIE, L_Prototype, listen server + 2 clients)

- First run (before the fix): `Round started (seed 1417647664, 21 NPCs)` and 29 `SpawnActor failed because of collision`.
- After the fix: `Round started (seed 218620060, 50 NPCs)`, 0 spawn collisions, no `LogStealthCrowd` warning or error,
  no `LogStealthFault` line.

| File | Shows |
|---|---|
| 01-host-50-npcs.png | Host (Jaguar), Active; crowd NPCs at the huts |
| 02-client1-50-npcs.png | Client 1 (Eagle), a group of NPCs walking in the market |
| 03-client2-50-npcs.png | Client 2 |

Screenshots checked: not black, looked at.

## Tests

`full-suite.txt`: `cycle.ps1 -Tests`: Result: Succeeded, 275/275 `StealthGame.*` Success.
