# RMF Exit B batch evidence: Preparing, spawn order and win rules (stories rmf-030..047), 2026-10-26

## Run (run-and-observe, `.claude/docs/run-and-observe.md`)

- Editor launched with `tools/dev/cycle.ps1`; 3-player PIE (listen server + 2 clients, floating windows) through the Unreal
  MCP (`EditorAppToolset.StartPIE`, `LevelEditorPlaySettings.PlayNumberOfClients` 3); bot commands through
  `StealthPlayToolset.RunPlayConsoleCommand` (`StealthBot <i> report|strike_enemy|interact`); windows captured with
  `tools/dev/capture-window.ps1`.
- Server log excerpt: `pie-server-log-excerpt.txt`. Round 1 and round 2 both went Lobby -> Preparing -> (production readiness
  sequence: tuning ready, layout ready after the nav rebuild, P5 acknowledgements, P6 spawn of D = 3 + 40 pawns in one frame,
  P7 world-ready, StartLeadSec) -> Active.
  - Round 1: `StealthBot 0 strike_enemy` kills team 1's only Active player. The Roster transition's event reached Round's F2
    check: `Round 1 decided by the production rules: team 0 wins (public reason 2)` -> Ended.
  - Reset -> Lobby -> auto-start -> round 2 (epoch 2, factory pawns 86 = 2 x 43, untokened 0: round 1's pawns were destroyed).
  - Round 2: `StealthBot 1 interact` x3 completes team 1's stages 1-3; the final completion went to Round's ObjectiveWin with the
    re-validated tender: `Round 2 decided by the production rules: team 1 wins (public reason 1)` -> Ended -> Reset -> Lobby.
  - No LogStealthFault line in the session.

## Screenshots (checked: not black, looked at)

| File | Shows |
|---|---|
| 01-host-active-production-preparing.png | Host (Jaguar) in round 1 after the production Preparing: `[prod] Round Active idx 1 changes 3 played 1 / Roster 3 / factory pawns 43, untokened 0` |
| 02-client1-active.png | Client 1 (Eagle) in round 1, replicated phase Active |
| 03-host-elimination-win-ended.png | `JAGUAR WIN (elimination)`; dev line `Round Ended ... T1 act 0 inf 1` (production F2 elimination) |
| 04-client1-elimination-ended.png | The eliminated client sees the Ended result |
| 05-client1-objective-win-ended.png | `EAGLE WIN (final objective)` on the completer's client, `Round Ended idx 2 changes 9 (replicated)` |
| 06-host-objective-loss-ended.png | Host side of the objective win; `played 2 ... epoch 2 ... factory pawns 86, untokened 0` |

## Tests

`full-suite.txt`: 189/189 `StealthGame.*` Success (headless, `cycle.ps1 -Tests -NoLaunch`); 171 before the batch (+17 Round
Logic tests, +1 Round PIE test). `stamp.txt`: content hashes of the changed sources at capture time.

## Code review

Focused review (lead-programmer, sonnet) of the production changes; 15 findings. Fixed before the final run: grace records for
every participant Disconnected at Active entry (not only the P6 snapshot); round start pawns (Ai bodies included) destroyed at the
next Preparing; bCalledOff written with the Reset phase and cleared by a new Preparing; the store drains in Ended too (late items
resolve Rejected_Latched); a failed gate opening is kept and retried; NPCs held in Preparing and released by the Crowd start;
Preparing aborts archive under the spawn epoch when known; re-acknowledgement on a reconnection in Preparing; a kick on a live
grace record marks it (no second timer); layout abort when the GameState is missing; teardown order in RegisterGateways; the
called-off message. Not changed (noted): a forced EnterActive (test/dev driver) does not require the sequencer's Done step; a
Player-control entry whose controller vanished spawns Ai without a gate (rare, logged by the factory).
