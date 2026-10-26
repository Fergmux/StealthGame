# RMF round-end and disconnect batch evidence (stories rmf-048..065), 2026-10-26

## Run (run-and-observe, `.claude/docs/run-and-observe.md`)

- Editor launched with `tools/dev/cycle.ps1`; 3-player PIE (listen server + 2 clients, floating windows) through the Unreal MCP
  (`EditorAppToolset.StartPIE`, `LevelEditorPlaySettings.PlayNumberOfClients` 3); bot and console commands through
  `StealthPlayToolset.RunPlayConsoleCommand`; windows captured with `tools/dev/capture-window.ps1`. Teams: host and client 2 on
  team 0 (Jaguar), client 1 alone on team 1 (Eagle).
- Server log excerpt: `pie-server-log-excerpt.txt`.
  - **Round 1, strike win, two-step end (rmf-052..055):** `StealthBot 0 strike_enemy` assassinates team 1's only Active player.
    `latched at 36.733 s, round-end step at 37.066 s (+0.333 s)`: the latch step at the strike, then the round-end step at the
    first server frame at or after the stand-in death write (AdapterStrikeRoundEndDelaySec 0.25 s; the PIE frame was ~0.33 s with
    three windows). The banner carries the approved reason and both TeamSummary lines (`summary T0 {0, 0, 0} T1 {0, 1, 0}`); the
    crowd and pawns hold. Ended -> Reset (pawns destroyed, lobby travel) -> Lobby -> round 2.
  - **Round 2, disconnect -> grace -> rejoin (rmf-060, rmf-061):** client 2 runs `disconnect`: `participant 2 connection
    Disconnected in phase Active` / `grace records open 1; participant 2 converts at 103.108 s unless it rejoins`. It rejoins with
    `open 127.0.0.1:17777` 13 s later (same unique net id, Reclaimed): `grace records open 0`; the host's report shows team 0
    `act 2 inf 0`: the participant kept its Active role, no transition.
  - **Round 2, last disconnect forfeits (rmf-064):** client 1 (team 1's only member) runs `disconnect` and stays away:
    `participant 1 converts at 135.014 s`, then at the expiry `participant 1 state 0 -> 1 (reason 4 = Disconnect)` and
    `team 0 wins (public reason 2, end reason 2 = ForfeitElimination); latched at 135.300 s, round-end step at 135.300 s (+0.000 s)`:
    a forfeit is not a strike, so the round-end step is the latch step. The banner shows the elimination phrase, never a forfeit one.
- Not representative (recorded): a rejoined participant has no pawn (`pawn=none`): the prototype composition root releases a
  leaver's pawn at logout and the engine destroys it (ADR-0010 S8 spike, PawnLeavingGame; Active pawn-loss recovery is
  rmf-075). The `LogStealthFault` lines of the session are prototype/test-procedure ones: Fault 7 (live binding cleared by the
  engine's pawn destroy at logout) and Fault 5 (Roster calls on the disconnected client's own standalone game instance while it
  re-browsed). The Unresponsive hand-over and supersede (D24) paths have no production caller yet (Session's facts arrive only as
  Roster connection events until the Session epic wires them); they are covered headless.

## Screenshots (checked: not black, looked at)

| File | Shows |
|---|---|
| 01-host-active-round1.png | Host (Jaguar) in round 1, Active |
| 02-client1-active-round1.png | Client 1 (Eagle) in round 1 |
| 03-host-strike-win-ended.png | `JAGUAR WIN (elimination)`; "Jaguar win: every rival eliminated for the round." and both summary lines (Eagle: eliminated 1) |
| 04-client1-victim-ended.png | The victim (now INFORMANT) sees the same Ended banner and summary; `[prod] Round Ended idx 1 changes 4 (replicated)` |
| 05-host-client2-disconnected-grace.png | Host while client 2 is away (grace running) |
| 06-host-client2-rejoined-active.png | Host after client 2's rejoin inside grace |
| 07-client2-rejoined.png | Client 2 back in round 2, ACTIVE, `[prod] Round Active idx 2` |
| 08-host-forfeit-win-ended.png | Round 2 ended by the forfeit: `JAGUAR WIN (elimination)`, "every rival eliminated for the round", Eagle eliminated 1 |
| 09-client2-forfeit-win-ended.png | Client 2's view of the forfeit end |

## Tests

`full-suite.txt`: 207/207 `StealthGame.*` Success headless (`cycle.ps1 -Tests -NoLaunch`); 189 before the batch, +18 Round Logic tests.
`StealthGame.Input.PIE.WalkSlowerModifier` failed in two of the three full runs (informant phase step 5.78 cm/s over the limit; the
test's own comment notes a flaky phase) and passed alone three times, in the `StealthGame.Input.` group and in the final full run: no Round code path is involved (stub
informant state, no Roster transition).

## Code review

Focused review (lead-programmer, sonnet), 11 findings, no high severity. Fixed: the staging pass re-reads the epoch per write;
Reset drops writes staged earlier in the frame (DropStagedWrites); a second RoundEndStep in one slot is refused (fault); Active
entry takes over (or marks AI-driven, gate closed) participants disconnected at Active; parked host commands set the held reset
only for HostCommand items; the host abort's reason entry carries the host's id once (also for the Preparing and pre-P1 aborts);
a grace no-op clears the AI-driven mark; PostRound and the latched summary reset at Lobby entry. Not changed (recorded): no
design cap on a death write's time (Assassination's to bound); Session's Unresponsive/Responsive/Superseded/lobby-travel facts have
no production caller yet (Session epic); GraceExpired is appended after the Applied transition, so a void raised synchronously by
that same transition archives the slice first (the production Roster dispatches in its flush; to revisit with the flush barrier).
