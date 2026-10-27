# Assassination & Defence Reversal batch evidence (stories asn-015, asn-016, asn-019), 2026-10-27

## What was built

- **Owner results (015)**: after the OnPawnKilled fan-out every executed Attack asks `IStrikeTallySource` (once, whatever
  the result) and appends `FCommandReply::MakeStrikeResult` to the attacker's reply sink; a reversal by a bound defender
  appends `MakeDefenceResult{Reversed}` (answering the defender's Prepare seq); voided commands append their Rejected reply.
  `FStrikeResult` / `FDefenceResult` moved into `View/CommandReplyTypes.h`; `FCommandReply` carries them and has a
  fixed-width NetSerialize per Kind (a StrikeResult entry is 137 bits for every Result). The reply-sink table is flushed
  once per connection after each drain (`URoundMatchStore::OnRoundQueueDrained` -> composition root ->
  `AStealthPlayerController::ClientCommandReplies`). STAND-INs: the Penalty tally (`FStealthPrototypeStrikeTally` over the
  server-only `WrongTargetTally` / exile flag; `NpcKills` and `LastDefenceActivationTime` no longer replicate) and the owner
  plates (one HUD message per result at its contact time, "Wrong targets: n" line) until story 024.
- **Owner cooldown (016)**: `UDefenceOwnerComponent` (COND_OwnerOnly `CooldownEndServerTime`) on `AStealthPlayerController`,
  published in the Prepare(Begin) slot, synced on possession, `AdoptForRecovery` for PawnRecovery, cleared on Reset. The
  prototype's replicated `bPreparingDefence` is server-only; the HUD's "PREPARED" and the owner's predicted crouch follow the
  FActionState Brace pose; "Defence: recovering Ns" reads the owner component.
- **Body sequence and Reset (019)**: the kill slot schedules `TimerFired{BodyRelease}` at DeathServerTime + DeathFallSec and
  `TimerFired{BodyRemoval}` at + BodyLingerSec + BodyRemovalGraceSec for every victim kind; release = capsule ignores pawns +
  `UStealthPawnFactory::ReleaseBody` (control epoch bump, UnPossess, crowd controller destroyed, PlayerController keeps its
  view target and never spectates); removal = `IBodyRemovalObserver` then teardown; after the latch both items are
  Rejected_Latched and the body lies until Reset. `FAttackResolverCore::Reset` clears roles, defence states, owner cooldowns,
  pending writes and bodies (generation bump); the composition root calls it on Round's Reset phase. The informant respawn
  follows the release. The prototype ragdoll remains the fall STAND-IN (`BeginDeathFallPresentation`, no unpossess / lifespan).

## Tests

`full-suite.txt`: `cycle.ps1 -Tests -NoLaunch`: see the header line for the build Result and the counts.
New: `StealthGame.Assassination.ExileFlagAndTally`, `.OwnerResultShape`, `.DefenceOwnerCooldown`, `.ResetClearsState`,
`.BodySequenceParity`, `.PIE.OwnerResultShape.EveryResultReachesOnlyItsOwnerAtOneSize`,
`.PIE.DefenceStateServerOnly.OnlyTheBracersOwnControllerHoldsItsCooldownEnd`,
`.PIE.PossessionThroughFall.KilledBodiesKeepTheirControllerUntilReleaseAlike`. Updated: `StealthGame.Input.CommandReplyTypes`
(FCommandReply's two new members, the result structs' shapes), `.Assassination.EncounterServiceSmoke` (four timers incl. the
body items; the body timer kinds unregister on Shutdown).

PIE measurements (PossessionThroughFall, headless run): NPC death 8.043 -> last held 9.234, released 9.243, removed 17.242;
player (reversed) 10.878 -> 12.072 / 12.080 / 20.082; informant 15.305 -> 16.500 / 16.508 / 24.509 (frames <= 31 ms); no
early change, no Spectating, both informants respawned after their release. The first PIE OwnerResultShape run failed (P2's
facing reverted to its client's control rotation over a 2.4 s settle, so its strike was a whiff): the helper now sets the
control rotation and the strike follows the brace after 0.4 s; it then passed.

## Run (run-and-observe, `.claude/docs/run-and-observe.md`)

Editor launched by `tools/dev/cycle.ps1`; 3-player PIE (listen server + 2 clients, floating windows, L_Prototype, 50 NPCs)
through the Unreal MCP (`EditorToolset.EditorAppToolset.StartPIE`, `StealthGameEditor.StealthPlayToolset.RunPlayConsoleCommand`
with `StealthBot`); windows captured with `tools/dev/capture-window.ps1`; every screenshot looked at, none black. Teams: host
(#0) and client 2 (#2) Jaguar, client 1 (#1) alone on Eagle. In round 1 the brace expired before client 2's strike (MCP round
trips at ~3 fps), so the strike killed client 1 and ended the round; round 2 repeated it with the commands back to back.

| File | Shows |
|---|---|
| story-015-.../01-host-npc-kill-wrong-target-plate.png | Host after `strike_npc`: plate "Wrong target. Wrong targets: 1" and the tally line |
| story-015-.../02-client1-victim-eliminated-round1-ended.png | Round 1: client 1 killed (brace expired): own removal message, Jaguar win |
| story-015-.../03-client2-rival-kill-target-eliminated-plate.png | Round 1: client 2's plate "Target eliminated" |
| story-015-.../04-client1-defender-reversal-result.png | Round 2: defender client 1 "Reversal! Your attacker is eliminated." (FDefenceResult) and "Defence: recovering 55s" |
| story-015-.../05-client2-attacker-reversed-plate.png | Round 2: reversed attacker client 2: plate "Reversed", own removal message, INFORMANT |
| story-015-.../06-client1-informant-kill-wrong-target-plate.png | Client 1 kills the informant: "Wrong target. Wrong targets: 1" (same plate as an NPC kill) |
| story-016-.../01-client1-braced-prepared.png | Client 1 braced: "Defence: PREPARED" (Brace pose), crouch tell |
| story-016-.../02-client1-recovering-cooldown.png | Client 1 after its brace: "Defence: recovering 55s" from its owner component |
| story-019-.../01-host-view-after-reversal.png, 02-client1-view-after-reversal.png | Host and client 1 views around the reversal ("recovering 36s") |
| story-019-.../03-client2-informant-respawned.png | Client 2 respawned at its team spawn after its body's release (INFORMANT) |
| story-019-.../04-host-npc-body-on-ground-2s.png | NPC body falling / on the ground 2 s after the host's strike |
| story-019-.../05-client2-npc-body-on-ground-2s.png | Client 2's view at the same time (the host in the distance) |
| story-019-.../06-host-npc-body-removed-after-9s.png | The same spot about 11 s later: the body has been removed |
| story-019-.../07-client2-informant-killed-falls.png | Client 2 (informant) killed by client 1: its own fall, "You were silenced" |
| story-019-.../08-client2-informant-respawned-after-release.png | Client 2 respawned (epoch 4) after the release |

Server log (`story-019-.../pie-server-log-excerpt.txt`): every death write at DeathServerTime - 0.067 s, every body released
at DeathServerTime + 1.2 s plus one PIE frame (e.g. DeathServerTime 31.868 -> released 33.134; 78.251 -> 79.517; frames of
0.066 s in the 3-window editor), with the controller that held it
(`StealthCrowdController_*` for NPCs, `StealthPlayerController_2` for client 2), and **no `LogStealthFault` line**.

## Decisions

Logged in the decision log (dev-story, 2026-10-27): cross-epic proceed; FCommandReply entries with fixed-width serialisation;
the drain-end flush; Rejected replies for voided commands; the Penalty tally stand-in and server-only counters; the owner
plate stand-in; the 016 QA t = 170 interpretation; bPreparingDefence server-only; the factory ReleaseBody; body timers gated
on the hand-over; respawn after release; Reset on Round's Reset phase; the 019 test split; the fall presentation stand-in.
