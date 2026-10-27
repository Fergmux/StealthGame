# Evidence: NPC-Kill Penalty & Informants stories 013-015, 2026-10-27

Stories: `production/epics/npc-kill-penalty-informants/story-013..015`. Types: Logic (013), Integration (014, 015).
Build: `tools/dev/cycle.ps1 -Tests` -> `Result: Succeeded`, `tests exit=0`, **433 / 433** `^StealthGame.` tests Success (421
before + 12 new; full list `full-suite.txt`). Stamp: `stamp.txt`. No .uproject / Build.cs / Target.cs change; no tuning-schema
change. The run before the final one failed two tests: `StealthGame.Penalty.InformantPawnParity` (a defect in the new test, see
below, fixed) and `StealthGame.Input.PIE.WalkSlowerModifier.KeyboardModifiersScaleTheServerReceivedMove` ("Run + Left Ctrl:
measured speed 406.3 cm/s at the target pace 450.0", a timing measurement that passed in the final run and touches nothing this
batch changed: recorded as a flaky load-sensitive test for the Input epic).

## New tests (all Success in the final run)

| Story | Test | Rig |
|---|---|---|
| nkp-013 AC1 | `StealthGame.Penalty.StarvationWidening` (61 picks one `EmergenceRetrySec` apart with one RequestSeq; stored level 0 / 1 at exactly 10.0 s / 2 at exactly 20.0 s, never decreasing; waiting past level 2; the row carries 2; no cancellation; no spawn / late-join / `RespawnStarved` in Penalty sources, with a negative control) | core + scripted fake LSC |
| nkp-013 AC2 | `StealthGame.Penalty.RetryBookkeeping` (Deferred, excluded, Deferred, None, Picked: same seq; next-frame retries strictly after the drain's stamp window; PickStartServerTime and level unchanged by Deferred / excluded; exclusions survive next-frame retries and clear at the regular retry; a new request gets a new seq) | core + fake gateway |
| nkp-014 AC1 | `StealthGame.Penalty.PIE.OneRespawnPath.{Assassinated, ReversedAgainst, StrikeLimit, InformantKilled, ReturningInformant, BodyLostToKillZ, FailedReleaseRedrawsWithoutCardMove}` | 3-player PIE, 40 NPCs, respawn probe |
| nkp-014 AC2 (partial) | `StealthGame.Penalty.PIE.TakeoverWireParity.NoChannelOpenOrCloseAndSameNetGuid` (LeakNetProfile StealthLeak1) | 3-player PIE |
| nkp-015 AC1 (partial) | `StealthGame.Penalty.InformantPawnParity` (38-field dump equal and equal to the fixture names; Run clamped to 140 cm/s vs Active 450; Attack / Prepare NotNow) | test world + encounter rig |
| nkp-015 AC2 (partial) | `StealthGame.Penalty.PIE.InformantContract.EveryBuiltRowHolds` | 3-player PIE, 40 NPCs |

Measurements logged by the PIE tests: `pie-test-measurements.txt`. Every OneRespawnPath cause committed on its first attempt
except FailedRelease (attempt 0: pick, pre-check, release failed; attempt 1 one frame later: pick, pre-check, release, takeover,
card). BodyLostToKillZ was due 0.08 s after the loss (D = 0). TakeoverWireParity: the observer keeps the same actor, channel 58
and NetGUID 190 across the takeover; the logged content diff shows **`Actor.PlayerState` changes on the taken pawn and nothing
on the control NPC**: the identity leak IVP story 018 must close (engine-default replication), the reason AC2 stays Blocked.

Changes made by this batch that existing tests depend on: the six 0-NPC Assassination PIE rounds switch on the harness-only
crowd-less respawn (`StealthPenaltyHarnessSeams`); all six pass.

## Run and observe (3-player listen-server PIE, L_Prototype, 50 NPCs)

Driven through the Unreal MCP (`StartPIE` floating, `StealthPlayToolset.RunPlayConsoleCommand` with `StealthBot`); windows captured
with `tools/dev/capture-window.ps1`; every image looked at, none black. Teams: host #0 and #2 Jaguar (team 0), #1 Eagle. Log
excerpt: `pie-exile-and-informant-respawn-log.txt`.

- 3 NPC kills -> exile -> informant respawn: #2 struck three NPCs; the third exiled it (`participant 2 state 0 -> 1 (reason 3)` at
  21:28:17.346). 15.05 s later Penalty's commit took over `BP_CrowdCharacter_C_35` at (1047, -2500), inside the team-0 region.
  - `01-exiled-owner-awaiting.png`: INFORMANT HUD, the view still on the released body.
  - `02-exiled-owner-took-over-crowd-pawn.png`: the same client in its new body, control epoch 2.
- Informant killed -> respawns as an informant: #1 struck the informant at 21:28:48.758 (victim kind 2); at 21:29:04.067 (15.0 s)
  it took over `BP_CrowdCharacter_C_9` at (985, -2000), team-0 region.
  - `03-killed-informant-awaiting.png`: "You were silenced. Returning to your team's spawn." over the falling body.
  - `04-killed-informant-respawned-as-informant.png`: back as an informant in another crowd pawn (epoch 3).
  - `05-opponent-view-after-respawn.png`: the attacker: ACTIVE, "Wrong targets: 1" (no strike for an informant kill).
- No harness-only spawn in the session (no `HARNESS-ONLY` line); the dump shows `#2 state=Informant` in the taken pawn; no Penalty
  fault. The only errors are the known engine DLL probes and skeleton LoadErrors.

Run result: OBSERVED — 3-player PIE with a crowd: the 3rd NPC kill exiles, the informant takes over an unseen crowd pawn in its team's region 15 s later; a killed informant respawns the same way, still an informant; no pawn spawned.

## Code review (self, `.claude/skills/code-review/SKILL.md` focus)

ADR-0019 sections 4-5: retries reuse the request's seq; widening comes only from the pick; no spawn on the production path (the
team-start branch needs a test-seam build AND the harness switch AND a world with no crowd agent; `SpawnHarnessCrowdlessRespawnPawn`
returns null otherwise and is compiled to null in Shipping); Penalty adds no informant rule; no BindPawn by Penalty. Seams are
plain process-wide statics behind `WITH_STEALTH_TEST_SEAMS`, game thread only, reset by every test that sets them. Doc comments on
public APIs. Findings:
- Fixed during the build: `Setup` collided with the CQTest base (renamed `SetupRound`); the parity dump read collision channel 32
  (`ECC_MAX` runs past the 32-entry response container), garbage in the last byte: loop bound now `ECC_OverlapAll_Deprecated`.
- Minor, not changed: the takeover probe builds its event even while the probe is off (seam builds only, a few reads per commit).
Verdict: APPROVED.

## Blockers recorded on the stories

- nkp-014 AC2: content and cadence clauses wait on information-visibility-policy story 018 (owner-only `PlayerState` /
  `Controller` / `Owner` on `AStealthPawn`; measured leak above), shared-character-locomotion's pawn differentiators
  (`RemoteViewPitch16` from HeadLook, `CharacterInputState`, `TraversalResult`) and the ADR-0008 section 6 multi-process raw-bunch
  capture for cadence.
- nkp-015: Traverse / Interact / Hide `NotNow` (input-interaction-framework story 057), no x-ray and the concealed ally's marker
  (IVP viewer class `OpponentInformant`, Teammate Markers, concealment AC-CON-04). AC3 trace:
  `production/qa/evidence/penalty-informant-contract-trace.md`.
