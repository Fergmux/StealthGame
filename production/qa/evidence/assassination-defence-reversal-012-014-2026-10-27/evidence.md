# Assassination & Defence Reversal batch evidence (stories asn-012..014 + play integration), 2026-10-27

## What was built

- **Outcome-blind slot write (012)**: `FActionState` / `FDeathState` are `ReplicatedUsing` properties on `AStealthPawn`
  (audience All). `UPawnCommandComponent::WriteActionState` is the only writer: it assigns `Seq`, fills `StartServerTime`,
  writes inside the scoped `FStealthForcedNetUpdate(ActionStateWrite)` (one forced update per pawn per frame) and presents on
  the server at once (the listen host's direct path, ADR-0018 E7): the SCL pose (`EActionPose` -> `EStealthActionPose` map)
  and, for a Strike, the local `StrikeContact` warp target. In the slot the resolver writes exactly one thing, the attacker's
  `{Strike, Variant, Start, Contact, WarpPoint}`; `FStrikeWarpPointCore` derives the WarpPoint only from an honoured PawnRef's
  F1 extrapolation, the WorldPoint, or the WorldPoint clamped to the bush footprint's attacker-side edge, then the 2 r_c +
  gap + warp-limit / vertical clamp, quantised to 0.1 cm. Brace / None are FActionState writes too; Strike and Counter poses
  end after the lockout (1.75 s) / counter (1.2 s) through a HoldTimer in the queue.
- **Death write (013)**: the core's apply-only applier is registered for `DeathWrite` and `CounterPose`; at
  `ContactServerTime - StrikeNetLeadSec` it writes `FDeathState{DeathServerTime, bDiedConcealed}` with the timed movement
  freeze (`UStealthMovementComponent::FreezeCommandedMotionFrom`; a reversed attacker's lunge is never interrupted: its
  freeze starts at its own contact), or `Counter` on the defender (never Disabled); it runs after a latch and is dropped by
  epoch on a Reset. Round's prototype `AdapterStrikeRoundEndDelaySec` stand-in is removed: a strike-decided round's
  `RoundEndStep` binds to the strike's death write.
- **Warp targets (014)**: the sample Blueprint's motion-warping component (the D53 adapter pawns) is unreplicated at
  `PostInitializeComponents`; the Shared component's `WarpTargets` row stays `COND_Never`; the server's lunge target is set
  once from the slot write and never re-tracks the victim.
- **Play integration**: `UEncounterService` is created by the composition root per server world and registered on
  `URoundMatchStore::GetRoundQueue()` (PawnCommand + HoldTimer resolvers, both appliers); every pawn's `IPawnCommandQueue`
  is `FStealthRoundCommandQueue` (participant mailbox + Session connection generation, server-local for AI; the Roster epoch
  stamped). Stand-ins in `StealthCommandWiring.h`: the prototype bush spot query (Concealment) and the Interact / Hide /
  Traverse fallback (IIF stage 2). Deleted: `FStealthPrototypeCommandQueue`, `AStealthGameMode::HandleAssassinationRequest`
  (and its hub broadcast: the slot now announces each strike on `UActionHub::OnActionStarted(Attack)` once, which the
  CrowdDirector's R8 stimulus reads), `HandleDefenceRequest`, `EliminatePlayer`. The game mode binds the presentation
  stand-ins to the service: the prototype ragdoll at `DeathServerTime`, HUD messages, informant respawn and NPC replacement
  at the applied death write; the crouch brace tell from the hub; the NPC-kill exile (Penalty stand-in) on OnPawnKilled.
  `StealthBot strike_* / blindstrike / defend / release` submit PawnCommands.

## Tests

`full-suite.txt`: `cycle.ps1 -Tests -NoLaunch`: build `Result: Succeeded`, **344/344** `StealthGame.*` Success (0 Fail). New: `StealthGame.Assassination.OutcomeBlindWire`,
`.WarpPointBlind`, `.ContactWriteAfterLatch`, `.VictimUntouchedUntilWrite`, `.WarpTargetsNotReplicated`,
`.LungeOutcomeBlind`, `.PIE.OutcomeBlindWire.SlotWriteBlindDeathWriteDeferredOnEveryClient`; updated: `.PlantedBrace` (the
brace through FActionState), `.EncounterServiceSmoke` (pose-end timer, applier registration).

Negative controls watched failing:
- 012 AC2: the `bWriteOutcomeIntoVariant` seam build fails the outcome-blind comparison (inside OutcomeBlindWire).
- 014 AC1: with the `PostInitializeComponents` fix removed, the PIE test failed (`a client copy holds one 1`, 3 replicating
  motion-warping components on live pawns) and `WarpTargetsNotReplicated` failed; restored (`negcontrol-014.txt`).
- The PIE test failed once on a client-clock artefact (receipt measured on the client's lagging server-time estimate); the
  assertion was fixed to measure on each machine's own clock from its Strike receipt. An earlier run's kill was a whiff because
  the host stood near a prototype bush (rule 2 is terminal); the test now moves the host clear of bushes first.

## Run (run-and-observe, `.claude/docs/run-and-observe.md`)

Editor launched by `tools/dev/cycle.ps1`; 3-player PIE (listen server + 2 clients, floating windows, L_Prototype, 50 NPCs)
through the editor's MCP server over HTTP (`EditorAppToolset.StartPIE`, `StealthPlayToolset.RunPlayConsoleCommand`); windows
captured with `tools/dev/capture-window.ps1`; every screenshot looked at, none black. Teams: host and client 2 on Jaguar
(team 0), client 1 alone on Eagle (team 1). PIE ran at about 3 frames per second with three windows.

| File | Shows |
|---|---|
| story-012-.../00-host-round-active.png | Host at round start (ACTIVE, Defence READY) |
| story-012-.../00-client1-round-active.png | Client 1 at round start |
| story-012-.../01-host-npc-strike-0.7s.png | (b) Host after two `strike_npc`: "A civilian. Civilians killed: 1/3 ... 2/3" |
| story-012-.../02-client2-npc-strike-0.7s.png | Client 2 (bystander) 0.7 s after the strike |
| story-013-.../01-client2-npc-body-2s.png | Client 2 two seconds later: the NPC's body on the ground |
| story-012-.../03-client2-defender-braced.png | Client 2 while client 1 braces |
| story-012-.../04-client1-defender-reversal-0.5s.png | (c) Defender (client 1): "Reversal! Your attacker is eliminated."; "Defence: recovering 57s" (cooldown from activation) |
| story-012-.../05-client2-reversal-0.5s.png | Client 2 during the reversal |
| story-013-.../02-host-reversed-attacker-2s.png | The reversed attacker (host): "Your target was ready. You have been eliminated and are now an informant.", INFORMANT |
| story-013-.../03-client2-reversal-body-2s.png | Client 2 after the reversal |
| story-013-.../04-client1-victim-killed-ended.png | (a) Victim (client 1): "You were assassinated. You are now an informant."; JAGUAR WIN (elimination) with both summary lines |
| story-013-.../05-client2-attacker-kill-ended.png | Attacker (client 2): "Target eliminated: an enemy agent."; JAGUAR WIN |
| story-013-.../06-host-round-ended.png | Host at Ended (summary: Jaguar wrong targets 2 = the two NPC kills) |
| story-013-.../07-host-informant-respawned.png | Round 2: after a second reversal the host is an informant again, respawned at the team spawn |

Server log (`story-013-.../pie-server-log-excerpt.txt`): every strike resolved in the slot (`[Encounter] ... killed ... in the
slot`, `... reversed and eliminated ...`), each death write applied 0.067 s before its DeathServerTime (one PIE frame after
T_dw), the round-deciding kill `latched at 128.598 s, round-end step at 128.931 s` (the round-end step ran in the staging
pass of the strike's death write), the Roster transitions once per kill (`participant 1 state 0 -> 1 (reason 1)`), and
**no `LogStealthFault` line**.

## Decisions

Logged in the decision log (dev-story, 2026-10-27): cross-epic proceed (risk accepted); Variant bearing measured to the warp
target; pose ends through HoldTimer(ActionType Attack); FActionState carries the brace stance; the timed freeze flag; Round's
stand-in removed; sample Blueprint motion warping unreplicated at PostInitializeComponents; the prototype strike bridge
deleted; the prototype death presentation bound to the applied death write.
