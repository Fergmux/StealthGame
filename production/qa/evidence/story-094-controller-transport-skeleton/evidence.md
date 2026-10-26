# Production input path in play (story iif-094 + D53 integration), 2026-10-26

Batch: IIF 001-006, 094, 095, spikes 009/010/011, 012-019, 013, 071, 092. Goal: human input reaches the pawn through
the production path: `IA_*`/`IMC_*` (story 007) → `UPawnCommandClient` on `AStealthPlayerController` → (Move: the pawn's
`UPawnCommandComponent` intent → CMC `ServerMove`) / (discrete: `ServerPawnCommand` → `IngestCommand` → entry point →
`FPawnCommandCore` → the D53 prototype queue adapter → the prototype game mode rules).

## Run

- Editor launched with `tools/dev/cycle.ps1`; 3-player PIE (listen server + 2 clients, floating windows) through the
  Unreal MCP (`EditorAppToolset.StartPIE`, `LevelEditorPlaySettings.PlayNumberOfClients` 3).
- Keys were injected through the real Enhanced Input pipeline with the development exec `StealthInputKey <Key> <1|0>`
  (`APlayerController::InputKey` with a simulated key event, as a key press would arrive), sent per instance with
  `StealthPlayToolset.RunPlayConsoleCommand`. Windows captured with `tools/dev/capture-window.ps1`.
- No `LogStealthFault` line in the session log.

## Screenshots (checked: not black, looked at)

| File | Shows |
|---|---|
| 01-client1-idle.png | Client 1 (Eagle) in round 1; the new `[cmd]` dev line: `move 0.00 Walk pace 1.00 epoch 1 last seq 0 ctx Active` (epoch 1 = `ClientSetPossessionEpoch` delivered before `ClientRestart`) |
| 02-client1-walking-ctrl-walk-slower.png | Client 1 holding W + Left Ctrl through production input: walking forward, `[cmd] move 0.80 Walk pace 0.80` (MVP item 11 walk-slower; the sample's Ctrl jog toggle does not fire, D37) |
| 03-client1-prepare-held-via-ServerPawnCommand.png | Client 1 after pressing Q: `last seq 1` (one `ServerPawnCommand`, PrepareBegin) and the server's prototype defence rule ran through the production entry path (`Defence: recovering 54s`) |
| 04-host-walking-alt-walk-slower.png | The listen host (Jaguar) holding W + Left Alt: `[cmd] move 0.90 Walk pace 0.90` (the host's discrete commands go through `IngestCommand(HostLocal)`, its Move through the same component) |

The PIE level style changes between captures (the sample's editor "Game Animation Widget" `SetLevelStyle`, an
editor-only artifact noted on 2026-10-24).

## Automated evidence

Headless suite `cycle.ps1 -Tests -NoLaunch`: see `production/qa/evidence/iif-command-path-2026-10-26/full-suite.txt`.
The server-observed numbers are in the PIE tests: `StealthGame.Input.PIE.WalkSlowerModifier` (server-received Move
magnitude 0.800 / 0.900 / 0.800 / 1.000, paces 160 / 180 / 160 / 200 cm/s with the sample's 200 cm/s walk speed, informant
the same), `NoSampleCtrlToggle`, `DefaultBindings` (25 rows), `FlushSendsEnd`, `CommonUiMidBraceSingleEnd`,
`ContextSwapOnStateAndPossession`, `ControllerTransportSkeleton`, `CommandTestSeams`.
