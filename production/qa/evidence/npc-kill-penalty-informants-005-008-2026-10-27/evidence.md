# Evidence: NPC-Kill Penalty & Informants stories 005-008 (+ Roster psr-020), 2026-10-27

Stories: `production/epics/npc-kill-penalty-informants/story-005..008`, `production/epics/participant-state-roster/story-020`.
Build: `tools/dev/cycle.ps1 -Tests -NoLaunch` -> `Result: Succeeded`; `^StealthGame.` suite 409 tests, 408 Success, 1 Fail:
`StealthGame.Assassination.PIE.PrototypeScenariosACF.F_BlindStrikeOnABushKillsTheConcealedEnemyAndOnAnEmptyBushNobody`
(story asn-027's new test, uncommitted work of another agent, not this batch: "the blind strike killed the concealed enemy"
at PrototypeScenariosACFPieTest.cpp(419)). Every new test of this batch and every earlier test passes, including the
ASN 022 PIE fixes (`CulledVictimOpensFalling`, `LateActorSeeksNeverSnaps`). Full list: `full-suite.txt`. Stamp: `stamp.txt`.
No .uproject / Build.cs / Target.cs change (project-files check not needed).

## New tests (all Success)

| Story | Test | Rig |
|---|---|---|
| nkp-005 AC1 | `StealthGame.Penalty.ExileOnLthNpcKill` (L = 1 test preset via TDC F-1, 2..5) | core + recording gateways |
| nkp-005 AC2 | `StealthGame.Penalty.CommandsAfterExile` | real queue + strike resolver + Round core |
| nkp-005 AC3 | `StealthGame.Penalty.SameDrainRaces` (both key orders) | real queue + strike resolver + Round core |
| nkp-005 AC4 | `StealthGame.Penalty.StrikeLimitLatched` | core |
| nkp-006 AC1 | `StealthGame.Penalty.ExileReleasesBody` | real Roster core |
| nkp-006 AC2 | `StealthGame.Penalty.ExileBodyExits` | real crowd agent + ambient schedule (sim) |
| nkp-006 AC3 | `StealthGame.Penalty.ExileExitUsesSlot` (30 min x 2: slots 279 = 279, exits 279 = 279, 10/10 exile exits in slots) | crowd sim |
| nkp-007 AC1 | `StealthGame.Penalty.ExileLatchesRound` (+ contrast) | real Round latch |
| nkp-007 AC2 | `StealthGame.Penalty.TwoExilesOneDrain` (both key orders) | real Round latch |
| nkp-008 AC1 | `StealthGame.Penalty.InformantKillRetiresInline` | Round rig + real Roster core |
| nkp-008 AC2 (server half) | `StealthGame.Penalty.InformantKillParity` | Round rig, production writes |
| psr-020 | `StealthGame.Roster.InformantKillIsNotATransition` | real Roster core |

Fixed during the build (test design, not product): ExileOnLthNpcKill's "later Attack" was an NPC kill, which itself exiles
under L = 1 (now a whiff); ExileBodyExits required absorption, but Crowd's give-up rule may abandon a watched exit (the run
abandoned it once; the assertion is now "absorbed or abandoned under the give-up rule", as any citizen's exit).

## Run and observe (3-player listen-server PIE, L_Prototype, 49 NPCs)

Driven through the Unreal MCP (`EditorAppToolset.StartPIE` floating windows, `StealthPlayToolset.RunPlayConsoleCommand`
with `StealthBot`); windows captured with `tools/dev/capture-window.ps1`; every image looked at, none black. Teams: host #0
and client 2 (#2) Jaguar, client 1 (#1) Eagle. Log excerpt: `pie-exile-and-informant-kill-log.txt`.

- Prototype test A (production path): #2 struck three NPCs (`strike_npc 150 0`, lockout settled between). On the third,
  in its slot: `Round: participant 2 state 0 -> 1 (reason 3), Active after T0 1 T1 1` (StrikeLimit), round still Active.
  - `01-exiled-owner-at-third-kill.png`: the exiled owner's client: "Exiled: too many wrong targets" card (prototype plate
    stand-in from bExiled), INFORMANT HUD, the view kept on the body (factory hand-over keeps the view target).
  - `02-teammate-host-at-exile.png`: the teammate (host): ACTIVE, Roster "T0 2 act 1 inf 1", round continues.
  - `03-exiled-owner-respawned-as-informant.png`: the exiled participant respawned as an informant (stand-in respawn).
- Prototype test B: #1 (Eagle) struck the informant #2 (`strike_informant`): kill resolved with victim kind 2 (Informant),
  RetireBinding in the slot (no Roster event), body released, the informant respawned (`factory pawns` +1, #2 Informant
  with a new pawn), #1 still Active with no strike.
  - `04-informant-kill-attacker-plate.png`: the attacker's HUD reads "Wrong targets: 1", exactly as for an NPC kill.
  - `05-killed-informant-respawned.png`: the killed informant back as an informant.
- No error or ensure in the session log beyond the two known pre-existing ones (sample camera asset import, MCP session).

Run result: OBSERVED — 3-player PIE: the 3rd NPC kill exiles through the production path (StrikeLimit in the slot, round
continues, owner informant + respawn); an informant kill shows the attacker "Wrong targets: 1" and respawns the informant.

## Code review (self, `.claude/skills/code-review/SKILL.md` focus)

ADR-0019 2.1 / 2.3 order and single call stack honoured; no timer in Penalty (the deferred AI possession is the factory's
stand-in); Penalty sources still pass the no-logging, guard-first and no-literal-limit lints; DI throughout (gateways),
doc comments on public APIs. Notes, no required change: a released body killed inside its lockout keeps a pending
Crowd adoption entry until the round resets (the factory skips possessing a body with a death write); the stand-in
respawn timer is guarded by `IsRoundInProgress`. Verdict: APPROVED.

## Integration points for later stories

- nkp-009: implement `IPenaltyRespawnScheduler` (the informant kill already calls `ScheduleRespawn(P, InformantKilled,
  slot time)`; pass it as `FPenaltyServiceInit::Respawns`) and add `OnInformantAwaitingBody` to `IInformantBodyProvider`
  for StrikeLimit / Assassinated / ReversedAgainst. Note: the Roster dispatches the exile's event at the return of
  `RequestTransition` (before steps 2-5 run), so Round's provider call arrives before `ExcludePawn` is set.
- nkp-012: `FPenaltyRecord::ExcludePawn` is set at the exile; clear it at embodiment; replace
  `AStealthGameMode::HandleProductionExile`'s stand-in respawn and the prototype `RespawnInformant` path.
- nkp-021: delete the gated prototype block in `HandleEncounterPawnKilled`, `ExilePlayer`, `FStealthPrototypeStrikeTally`,
  the `AStealthPlayerState` counters, `ShouldRemoveForNpcKills`, `UStealthSettings::NpcKillLimit`, the `npcKills=` report field.
- Character Pool: replace `FStealthPenaltyPawnGateway::ReleaseCard`'s stand-in; SCL hand-over pass: replace the timer in
  `UStealthPawnFactory::RequestHandOver`; IIF stage 2 (iif-025/027/057): NotNow for a pawnless participant's commands.
- HUD: nkp-008 AC3 (`FKillCueInput` reflection) and the PIE-3 parity capture of AC2.
