# Smoking Mirror — First Playable Report

*Written: 2026-10-24, at the end of an unattended autonomous run. Source plan: `GAME_BUILD_HANDOFF.md`.*
*Status: PROTOTYPE. The gameplay code in `Source/StealthGame` is promoted to production only after its ADR exists (`design/gdd/game-concept.md`, Pending User Decisions).*

## How to launch

1. Open `StealthGame.uproject` in **UE 5.8**. The C++ modules are built; if the editor asks to rebuild, say Yes. You need VS 2022 17.14 / MSVC 14.44.35229 or later.
2. The editor opens `/Game/StealthGame/Maps/L_Prototype`.
3. **Single player:** press Play. A round starts after 5 s with 40 NPCs and a freshly seeded city.
4. **Multiplayer test:** set *Play → Net Mode = Play As Listen Server* and *Number of Players = 2–4*, then Play. Each extra window is a client.
5. **Standalone:** `UnrealEditor.exe StealthGame.uproject -game -windowed -ResX=1280 -ResY=720`.
6. **Performance capture:** add `-StealthPerfSeconds=30`. The FPS chart summary goes to `Saved/Logs/StealthGame.log`.
7. **Dev loop scripts** (PowerShell):
   - `tools/dev/cycle.ps1 [-Tests]` closes the editor, builds, optionally runs all `StealthGame.*` tests, and relaunches with MCP.
   - `tools/dev/capture-window.ps1 -TitleMatch "Client 1" -Out x.png` captures a PIE client window.

## Controls

| Action | Key |
|---|---|
| Move (walk by default, like the crowd) | WASD |
| Run / sprint (a tell; blocks the defence) | Shift (Ctrl toggles jog) |
| Climb / vault (sample traversal) | Space |
| Assassinate (target in front, or blind strike at the spot) | Left mouse / F |
| Prepare defence (~4 s, once per 60 s) | hold Q |
| Objective task (~3 s, in range) | hold E |
| Ping your position for teammates | T |
| First/third-person camera | V |
| Crouch | C |

**Console (dev):**
- Round control (host only, production Round since 2026-10-26, D53): `stealth.round.start`, `stealth.round.abort` / `stealth.round.reset`, `stealth.round.swapteam <participant> [<participant>]`, `stealth.round.remove <participant>`. The old `StealthStartRound`, `StealthEndRound`, `StealthSwitchTeam` are retired. `StealthDumpProd` logs the production Round phase and Roster counts; a yellow `[prod]` line at the bottom of the HUD shows them in development builds.
- Bot driver (server rule paths): `StealthBot <player#> <strike_npc|strike_enemy|strike_informant|blindstrike|defend|interact|hide|watchN|ping|report>`.
- Camera: `StealthDebugCam <0|1|2>`.

## What was built (by handoff checklist step)

| # | Step | Result |
|---|---|---|
| 1 | Inspect project, prove toolchain | Done. Rebased onto Epic's Game Animation Sample (5.5 → 5.8). Unreal MCP enabled (AllToolsets + LiveCoding); Meshy, Blender and Unreal MCP verified. C++ module + editor module added. |
| 2 | Multiplayer round framework | Done. Server-authoritative GameMode, GameState and PlayerState; 2 teams × 5; active/informant/round-over; objective and elimination wins; auto-restart; dev start/reset commands. |
| 3 | Movement, cameras, traversal | Partly done. Sample motion matching and traversal (CharacterMovement-based, already replicated by the sample) are in. Walk is the default. The first/third-person toggle works. Informants are speed-capped and blocked from jump/traversal in code. **Not verified:** traversal replication under latency; informant traversal block in play. |
| 4 | Shared character pool and crowd | Done. Four Meshy citizens (man, woman, elder, labourer) are bound to the sample skeleton through Blender. Players and NPCs draw from the same pool, dealt from a shuffled balanced deck each round so no model is over-represented (the art bible's Pillar 1 rule). 40 NPCs wander the nav mesh with acceleration-based paths. Teammate-only markers are drawn from this client's team. |
| 5 | Assassination, reversal, informants | Done. Range/cone targeting, blind strikes, a prepared defence (crouch "brace" as the visible tell), reversal eliminates the attacker, the 3rd NPC kill exiles, informants respawn when killed and that doesn't count as an NPC kill, and kills are confirmed. |
| 6 | Objective chain | Done. Two team-specific preliminary stages per team, then one shared final target (revealed only after the preliminaries). Interacting visibly "works" the site. |
| 7 | Modular map generation | Done (prototype kit). A server seed replicates; every machine builds the identical grid city (houses, halls, temple, stalls, gardens, low walls); objectives and bushes move to seeded sites; the nav mesh rebuilds dynamically; reachability is validated and logged. Meshy kit: house, stepped temple, market stall, with hidden box collision underneath. |
| 8 | Bush concealment | Done. Hidden per viewer (teammates still see you and your marker; enemies see nothing). Blind strikes can kill a hidden player. Striking from a bush reveals you until you leave. NPCs passing through bushes are concealed the same way. |
| 9 | Validate and deliver | Mostly done. See the test results below. Functional at 10 players (5v5, in-process); not yet with humans over a real network. |

## Test results

**Automated:**
- 12/12 tests pass, run headless with `UnrealEditor-Cmd … -ExecCmds="Automation RunTests StealthGame.; Quit" -nullrhi`.
- `StealthGame.Rules.*` (6): assassination outcomes, NPC-kill removal on the 3rd kill, defence cooldown and expiry and blocking while running or climbing, objective stage gating, elimination victory, team choice.
- `StealthGame.EditorTools.*` (6): the new MCP toolsets.

**Networked play tests:**
- Setup: 3-player listen-server PIE (2 Jaguar vs 1 Eagle), driven through the real server rule paths by the `StealthBot` test driver.
- Evidence is in `production/qa/evidence/`.

| Test | Result |
|---|---|
| A. 3rd civilian kill exiles the player; the round continues while a teammate is active | PASS |
| B. Killing an enemy informant respawns them as an informant; not counted as an NPC kill | PASS |
| C. Prepared defender reverses an attack → attacker eliminated → elimination win | PASS |
| D. Jaguar completes stage 1 → 2 → final → objective win (all clients get the result) | PASS |
| D2. Eagle completes the full chain on a procedurally generated city | PASS |
| E. Per-viewer bush concealment: teammate sees the marker, enemy sees an empty bush | PASS |
| F. Blind strike at a bush kills the hidden enemy | PASS |
| Procgen: seeds 29755 / 8924 / 25482 / 17906 / 13609 identical on all 3 instances; all objectives reachable from both spawns | PASS |
| HUD: kill confirmation, civilian count, defence state/cooldown, stage progress, enemy-stage alert, round banner, teammate markers | PASS (seen on clients) |
| First-person toggle | PASS (a sliver of own shoulder shows at the bottom edge) |
| Mixed Meshy pool: different clients get different models; motion matching drives them | PASS |
| **6-player (3v3) listen server:** correct team spawns; identical seed-19972 city on all 6 instances; Jaguar eliminates all 3 Eagles → win only after the 3rd; host + 5 clients receive the result | PASS |
| **10-player (5v5) listen server, in-process PIE (crowd lowered to 20 NPCs for the 10 worlds):** 5/5 teams; identical seed-13135 city on all 10 instances; each player on their own team start (after a stacking fix); Eagle completes the full chain → objective win received by all 10 players | PASS |

**Performance (standalone, 1080p, RTX 5090 / 16-core CPU — a high-end machine):**

| Characters | Average FPS | Game thread | Render thread | GPU | 60 FPS misses |
|---|---|---|---|---|---|
| 41 (1 player + 40 NPCs) | ~105 | 9.5 ms | 4.0 ms | 2.1 ms | — |
| 50 (1 player + 49 NPCs) | 89.6 | 11.2 ms | 4.0 ms | 2.1 ms | 0 % |

Not yet measured on the assumed RTX 3060-class target.

## Assets produced, by tool

**Meshy MCP** (240 credits spent of the 2,000 cap; balance 3,070):
- `citizen_01` (A-pose test). Arms bent, so unusable for rigging and superseded.
- `citizen_02` T-pose man, `citizen_03` woman, `citizen_04` elder, `citizen_05` labourer.
- Kit: house, temple, stall.
- Sources are under `assets/characters/meshy/` and `assets/environment/kit/`.

**Blender MCP** (scenes `StealthPipeline` and `StealthKit` in your open Blender session — **unsaved**; the outputs are on disk):
- The mannequin is posed to a T-pose. Each citizen was fitted to the mannequin's height, had skin weights transferred from it, and was bound to the UEFN skeleton. Exported as `SK_Citizen0X.fbx`.
- Kit pieces were joined, transforms applied, and pivots set to the base centre. Exported as `SM_Kit_*.fbx`.
- The reusable script is `tools/asset-pipeline/blender_bind_to_mannequin.py`.

**Unreal MCP:**
- **Characters:** `/Game/StealthGame/Characters/Pool/SK_Citizen02–05` (physics asset PA_UEFN_Mannequin); `/Game/StealthGame/Characters/BP_CrowdCharacter` (child of the sample character).
- **Kit:** `/Game/StealthGame/Environment/Kit/SM_Kit_House|Temple|Stall`.
- **Materials:** `/Game/StealthGame/Materials/M_CrowdTint` (tint fallback).
- **Map:** `/Game/StealthGame/Maps/L_Prototype` (copy of the sample map, plus team starts, objectives, bushes, crowd centre, nav bounds and the city generator).

**New MCP toolsets (C++, `Source/StealthGameEditor`):**
- `StealthPlayToolset`: `RunPlayConsoleCommand`, `ListPlayInstances`.
- `StealthAssetToolset`: `ExportAssetToFile`.

## Changed project content

- The project folder was rebased onto the Game Animation Sample. The originals are in `_backup_pre_rebase/`.
- **The sample's `CBP_SandboxCharacter` was reparented onto `AStealthCharacter`.** The sample's internal casts require it. A pre-change snapshot is in `_snapshots/2026-10-24_sample-blueprints-pre-resave/`.
- The sample camera assets were resaved once (that broke an import) and restored from the snapshot. **Never let the editor save them in 5.8.**
- Config:
  - `DefaultEngine.ini`: default map and GlobalDefaultGameMode.
  - `DefaultGame.ini`: `[/Script/StealthGame.StealthSettings]`, all tuning values plus the character pool.
- Editor: autosave disabled; "Auto Start Server" for MCP on.

## Known issues and limitations (honest list)

1. **The 10-player test was functional only, in one process.** 5v5 was verified with 10 in-process PIE instances at 20 NPCs; there were no humans, no real network latency, and the full 40-NPC crowd was not used at 10 players. Matchmaking and sessions over the internet are not implemented; listen server only.
2. **Network-level identity leak.** `APawn::PlayerState` and `bConcealed` replicate to all clients, so a modified client could tell players from NPCs and see hidden players. The UI never does. The fix (replication filtering via Iris or a ReplicationGraph) is ADR-001 work.
3. **Meshy proportions:** the citizens' arms are about 17% shorter than the mannequin's. Scaling by height means arms stretch slightly in animation. The fix is to generate characters to mannequin proportions (an image-to-3D reference).
4. **Traversal under latency** and the **informant traversal block** were not exercised in play (the code paths exist).
5. **Assassination presentation is a placeholder:** the victim ragdolls and the attacker is held for 1 s. There is no paired kill animation.
6. **The defence "brace" is the sample's crouch,** and players can crouch with C at any time, which is a deliberate red herring but undesigned.
7. **The sample's editor "Game Animation Widget"** pops up and randomises the visual style per PIE window (`ke * SetLevelStyle`). This is editor-only; it doesn't happen in standalone.
8. **Bushes are opaque placeholder spheres;** the foliage art is pending.
9. **The prototype map** still contains the sample's traversal course to the north. Generated cells avoid it.
10. **First person:** part of your own shoulder is visible at the screen edge.
11. **No version control.** Git with LFS was recommended; commits need your go-ahead.
12. **Unused duplicate asset** `/Game/StealthGame/Characters/BP_StealthCharacter`. Deleting it needs your OK.
13. **Bot camera control** (`StealthBot … watchN`) doesn't rotate client cameras. This is the test harness only.

## Design pipeline status (rigor: full)

- **Concept:** `design/gdd/game-concept.md`, with all four director gates run.
- **Art bible:** `design/art/art-bible.md`.
- **Systems index and GDDs:** 18 MVP GDDs in `design/gdd/`, each design-reviewed with blocking fixes applied (review logs in `design/gdd/reviews/`). `/review-all-gdds` failed on 5 blockers; after fixes it now rates CONCERNS.
- **Architecture:** `docs/architecture/architecture.md`. ADR-0001 to 0023 are written and all **Proposed**; 0024 and 0025 are deferred to Alpha.
- **Architecture review:** CONCERNS, the highest possible while ADRs are Proposed. 175 of 177 requirements are covered and 2 are partial, both waiting on your decisions.
- **Control manifest:** `control-manifest.md`, saved as DRAFT.
- **Not done:**
  - independent second reviews of each GDD
  - 3 low-severity ADR/architecture doc inconsistencies
  - `docs/engine-reference` corrections (outside the agent's write scope)

## Decisions waiting for you

**Start with `production/pending-decisions-2026-10-25.md`**, a prioritised list (blockers first) with recommendations.

See `design/gdd/game-concept.md` → Pending User Decisions, `production/session-logs/decision-log.md`, and the "Pending user decisions (design pipeline)" list in `production/session-state/active.md`.
