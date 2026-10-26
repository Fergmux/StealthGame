# Integration spine evidence (decision D53), 2026-10-26

Batch: PSR 061-062, RMF 018-029, SCL 002 and 006. Goal: the production stores drive L_Prototype.

## Run

- Editor launched with `tools/dev/cycle.ps1`; 3-player PIE (listen server + 2 clients, floating windows) started
  through the Unreal MCP (`EditorAppToolset.StartPIE`, `LevelEditorPlaySettings.PlayNumberOfClients` 3, restored to 2
  afterwards); commands through `StealthPlayToolset.RunPlayConsoleCommand`; windows captured with
  `tools/dev/capture-window.ps1`.
- Server log (Saved/Logs/StealthGame.log), production Round driving the prototype loop:
  - `Round phase Unknown -> Lobby (change 1, round 0)`
  - `Round phase Lobby -> Preparing (change 2, round 1)` then `Preparing -> Active (change 3, round 1)`
  - `[prod] Round Active idx 1 changes 3 played 1 | Roster 3 (T0 2 act 2 inf 0 / T1 1 act 1 inf 0) epoch 1 | factory pawns 43, untokened 0`
  - `StealthBot 0 strike_enemy` -> `Round phase Active -> Ended (change 4)`, `Round over: team 0 wins (elimination)`,
    Roster `T1 act 0 inf 1`
  - `Ended -> Reset (change 5)`, `Reset -> Lobby (change 6)`, `Lobby -> Preparing (change 7, round 2)`, `Preparing -> Active (change 8)`
  - Client: `[prod] ... public: phase Active idx 1 changes 3`, `team view: team 1 state 0 mates 0`
- No LogStealthFault warning, no Roster refusal, no unbound player pawn in the session.

## Screenshots (checked: not black)

| File | Shows |
|---|---|
| 01-host-active.png | Host (Jaguar) in round 1: prototype HUD plus the dev line `[prod] Round Active idx 1 changes 3 played 1 / Roster 3 (T0 2 / T1 1) / factory pawns 43, untokened 0` |
| 02-client1-active.png | Client 1 (Eagle): dev line from the replicated FRoundPublicState and its own C-TeamView (`team 1, 0 teammate(s)`) |
| 03-host-elimination-ended.png | After a bot elimination: `JAGUAR WIN (elimination)`, dev line `Round Ended ... T1 act 0 inf 1` |

## Tests

`full-suite.txt`: 120/120 `StealthGame.*` Success (headless, `cycle.ps1 -Tests -NoLaunch`); was 101 before the batch.
`stamp.txt`: content hashes of the changed sources at capture time.

## Code review (lead-programmer, focused) and re-run

12 findings; fixed before the final run: Round state and host id reset per match (re-host), server-automation host
caller (dedicated server / failed host registration), host `remove` sends the removed player away, superseded logout
keeps the new controller, a leaver's pawn is released before the engine destroys it (no live-binding fault), Start
rolls back on a refused lock/ResetAll, PublicHandle draws no longer repeat across matches, Roster handle maps pruned
at EndPlay and factory tokens pruned, Preparing -> Active deferred one frame (clients now receive Preparing), a
colliding crowd spawn is no longer a fault, GetMotionWarping documented nullable on adapter pawns. Not changed: a
pawn destroyed inside FinishSpawning after BindPawn is cleared by its own EndPlay hook.
After the fixes: 120/120 tests again, PIE re-captured (screenshots above are from the fixed build), round 1 -> Ended
-> Reset -> Lobby -> round 2 Active observed again (changes 1..8).
