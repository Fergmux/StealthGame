# Evidence: NPC-Kill Penalty & Informants stories 009-012, 2026-10-27

Stories: `production/epics/npc-kill-penalty-informants/story-009..012`. Types: Logic (009, 010, 012), Integration (011).
Build: `tools/dev/cycle.ps1 -Tests -NoLaunch` -> `Result: Succeeded`, `tests exit=0`, **421 / 421** `^StealthGame.` tests Success
(full list: `full-suite.txt`). Stamp: `stamp.txt`. No .uproject / Build.cs / Target.cs change.
Schema: `Stealth.Tuning.SchemaGen` -> `UNCHANGED` (the hand-added `InformantRespawnDelaySec` row and fingerprint
`0x020f2efe6081516b` equal the generator's output); `StealthGame.Tuning.SchemaVersionBumped`, `SchemaComplete`,
`SchemaGenDeterministic` pass at `TuningSchemaVersion` 6.

## New tests (all Success)

| Story | Test | Rig |
|---|---|---|
| nkp-009 AC1 | `StealthGame.Penalty.RespawnDueTime` (six F2 cases 115/115/120/120/118/130, hard-maximum floor for T_v 0.8 and 1.2 at D = 5, reclaim exactly at T_due) | core + fakes |
| nkp-009 AC2 | `StealthGame.Penalty.DisconnectNeverShortens` (reclaim at t + 5 -> t + D; at t + 20 -> the reclaim time) | core + fakes |
| nkp-009 AC3 | `StealthGame.Penalty.RespawnCancellation` (Disconnect / RoundEnded x2 / Reset rows, 0 takeovers; leftover item Rejected_StaleEpoch on the REAL queue) | core + real queue |
| nkp-009 AC4 | `StealthGame.Penalty.SchedulingSources` (payload shape on the real queue for 4 causes; the REAL Round core calls after its check, never when latched or for Disconnect; lint + negative control) | real queue + real Round core |
| nkp-010 AC1 | `StealthGame.Penalty.DisconnectedVictimAbsent` | core + fakes |
| nkp-010 AC2 | `StealthGame.Penalty.LostBodyRepawn` | core + fakes |
| nkp-011 AC1 / AC2 | `StealthGame.Penalty.DueAdapter`, `StealthGame.Penalty.DueAdapterLatched` | core + adapter + recording feed |
| nkp-011 AC3 (server half) | `StealthGame.Roster.OwnerRespawnDueOwnerOnly` | real Roster core + push step |
| nkp-012 AC1 | `StealthGame.Penalty.CommitSequence` (+ Deferred edge, exile body excluded) | core + fake gateways |
| nkp-012 AC2 | `StealthGame.Penalty.SameTeamDueOneDrain` (three key orders incl. TieRank) | real queue |
| nkp-012 AC3 | `StealthGame.Penalty.TakeoverTickRace` (both orders) | core + fakes |

Existing tests adapted: the nkp-005..008 rigs keep `Respawns` on the new timer seam (`IPenaltyRespawnTimers`).
Harness PIE tests adapted to the production respawn (15 s, `InformantRespawnDelaySec`): `OwnerResultShape` and
`KillPlateTellsOneThing` now let the respawned pawn settle 1 s before striking it (a strike on a pawn spawned that frame
hit a MotionWarping NaN ensure on a client).

Fixed during the build: a broken rig edit, a `Calls` shadow, a qualified `FRig`; `bDying` (a retired prototype
identifier the asn-027 scan forbids) renamed; the 0-NPC harness rounds had no crowd pawn to take over (respawn waits
timed out at 300 s), solved by the documented team-start stand-in fallback (decision log).

## Run and observe (3-player listen-server PIE, L_Prototype, 50 NPCs)

Driven through the Unreal MCP (`StartPIE` floating, `StealthPlayToolset.RunPlayConsoleCommand` with `StealthBot`); windows
captured with `tools/dev/capture-window.ps1`; every image looked at, none black. Teams: host #0 and #2 Jaguar (team 0),
#1 Eagle. Log excerpt: `pie-exile-and-informant-respawn-log.txt`.

- 3 NPC kills -> exile -> informant respawn. #2 struck three NPCs (`strike_npc 150 0`): the third exiled it in the slot
  (`Round: participant 2 state 0 -> 1 (reason 3)` at 17:48:08.030). 15.15 s later (17:48:23.178) Penalty's
  InformantRespawn item committed: `[Penalty] informant takeover (stand-in): participant 2 binds BP_CrowdCharacter_C_53 at
  (-505, -3052)`, about 23 m from the team-0 spawn. The factory created no pawn for it (factory pawns 53 + 3 crowd
  replacements = 56). The dump shows `#2 team=0 state=Informant pawn=(-505, -3052)`.
  - `01-exiled-owner-awaiting.png`: the exiled owner: INFORMANT HUD, the view still on the released body (now an NPC).
  - `02-exiled-owner-took-over-crowd-pawn.png`: the same client in its new body (a crowd pawn's look), control epoch 2.
- Informant killed -> respawns as an informant. #1 struck the informant #2 (`strike_informant`) at 17:48:57.178
  (victim kind 2); at 17:49:12.180 (15.0 s) `[Penalty] informant takeover (stand-in): participant 2 binds
  BP_CrowdCharacter_C_27 at (1577, -2336)`, inside 45 m of the team-0 spawn.
  - `03-killed-informant-awaiting.png`: the killed informant's own view on its falling body.
  - `04-killed-informant-respawned-as-informant.png`: back as an informant in another crowd pawn (epoch 3).
  - `05-opponent-view-after-respawn.png`: the attacker: ACTIVE, "Wrong targets: 1" (an informant kill is no strike).
- No Penalty fault in the session; the only errors are the known pre-existing MCP `server/discover` and skeleton
  LoadErrors.

Run result: OBSERVED — 3-player PIE: the 3rd NPC kill exiles and, 15 s later, the informant takes over an unseen crowd pawn near its team spawn; a killed informant respawns the same way 15 s after the kill.

## Code review (self, `.claude/skills/code-review/SKILL.md` focus)

ADR-0019 sections 1-6: Penalty schedules only from Round's provider call (after the elimination check, never latched) and
inline for the informant kill; no engine timer, tick or latent action in Penalty (linted); every mutation in a slot or
the Roster flush; commit order pick -> pre-check -> TryReleaseNpc -> TakeOverCrowdPawn -> (Applied) card, the hand-back in
the same stack, no BindPawn by Penalty; T_due reaches only the owner part through the Roster. Doc comments on public
APIs; values from tuning (pending-owner constants named, TD-TDC-I3). Findings, no required change:
- The round-end `RespawnCancelled{RoundEnded}` rows are refused when Round's round-end step runs outside a queue slot
  (staging pass): `Fault 14 from PenaltyEventLog (RowOutsideSlot)` warnings in 9 harness rounds. The cancellation itself is
  correct; the row needs a slot key from Round's round-end step (follow-up for Round / nkp-013+).
- The takeover stand-in answers Rejected_Stale when the participant has no PlayerController (the request voids).
Verdict: APPROVED.

## Integration points for later stories

- nkp-013: retries and widening. Today None -> `EmergenceRetrySec` regular retry (exclusions cleared at its fire),
  Deferred / excluded -> next-frame retry (+2 ms); `LastWideningLevel` comes from the pick; `PickStartServerTime` is set.
  RespawnStarved is LSC's row.
- nkp-014: the no-spawn PIE proof must remove or gate the team-start fallback (`FStealthPenaltyRespawnGateway`, used only in
  a world with no crowd agent).
- LSC story 033: replace `FStealthPenaltyRespawnGateway::PickRespawnPawn` (radius stand-in, LOS unseen test, immediate
  widening). SCL story 028: replace the takeover stand-in (BindPawn + ReleaseControl + `RequestHandOverToPlayer`) and the
  pre-check. Character Pool: `TakeNpcCard` (answers 0 today) and `ReleaseCard`.
- Roster psr-038: `SetOwnerRespawnDue` exists (push step); its `SetOwnerTally` half is still a no-op, so the Roster feed is
  not passed as the service's `OwnerView`.
- Round rmf-072: call `IInformantBodyProvider::OnInformantPawnLost` from R17.9 and release an embodied informant's pawn at
  R17.5 (Penalty already records Absent on the connection event). Round's round-end step should give Penalty a slot.
- HUD story 018: the owner's C-TeamView owner part now carries `T_due` (0 = none).
- nkp-021: `AStealthGameMode::RespawnInformant` and `HandleEncounterBodyReleased`'s timer only run with no bound service.
