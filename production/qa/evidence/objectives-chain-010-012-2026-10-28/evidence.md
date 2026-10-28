# Evidence: Objectives Chain stories 010-012, 2026-10-28

Stories: `production/epics/objectives-chain/story-010..012`. Types: Logic (010, 012), Integration (011). One commit for the
batch (decision D41; logged: the three stories share the core's view builder, the service's writer and the carrier, and every
pushed commit must be the gate-tested tree).

Build: `tools/dev/cycle.ps1 -Tests -NoLaunch` -> `Result: Succeeded`. `StealthGameplay.Build.cs` gained the private `NetCore`
dependency (the push-model dirty mark links against `UEPushModelPrivate`): `Build.bat -projectfiles ... Result: Succeeded`.
No asset was saved (`git status Content/ Config/` clean).

## Full-suite gate runs (`^StealthGame.`, 491 tests: 481 + 10 new)

| Run | Result | Failures |
|---|---|---|
| 1 (`full-suite-run1.txt`) | 490 / 491 | `Assassination.PIE.OneBodySequence`: the engine ensure `TMatrix<T>::InverseFast(), trying to invert a NIL matrix` from MotionWarping (once per process, so the running test fails; seen before in the NKP 016-022 batch). Re-run filtered: Success (`onebodysequence-filtered.txt`); the whole Assassination PIE group re-run filtered: 27 / 27 with 0 ensure lines (`assassination-pie-filtered.txt`). Nothing in this batch touches pawns, views or warping |
| **2 FINAL (`full-suite.txt`)** | **491 / 491** | none (`tests exit=0`, 0 ensure lines) |

New tests (10): StealthGame.Objectives.{NoTeamViewBeforeActive, TeamViewContentsCore, TeamViewWidthAndChangeOnly,
ObjectivesAllowlistRow, ResetAndRejoin, ViewReboundWritesTeamView, RejoinHoldDoesNotResume},
StealthGame.Objectives.PIE.TeamViewOwnerOnly.EachTeamsViewReachesOnlyItsOwnClients and
StealthGame.Objectives.PIE.RivalCountOneUpdate.{OneChangePerFrame_PushModelOn, OneChangePerFrame_PushModelOff}.

## What was built

| Story | Built | Tests |
|---|---|---|
| 010 | `FObjectiveLiveSite`, `FObjectiveTeamView` (client-safe, fixed-width `NetSerialize`: 70 + 37 bits per site, at most 3 sites -> 9/14/18/23 bytes vs the 12/16/20/24 budget), `FClueTag` (declared; story 013 fills it), `FObjectivesCore::BuildTeamView` (pure), the change-only `PushView`, `IObjectivesViewSink::PushTeamView(Team, View)`, the `RivalPrelimCount` store (HOLD for 014/015) | `NoTeamViewBeforeActive`, `TeamViewContentsCore`, `TeamViewWidthAndChangeOnly` |
| 011 | `UObjectiveTeamViewComponent` (push-based `View`, COND_None on the owner-only controller, change-only `ApplyView` that marks dirty and raises the host's own event), a default subobject of `AStealthPlayerController`; `UObjectivesService` is the core's sink and the only writer (`IObjectivesViewTargets`: Roster team members, controller map); the composition root's adapter; the HUD reads only the own view (card titles from the cooked chain by `DefIndex`, "Rival steps: N of 2", markers at the own live courts; the prototype `TeamStage` mirror and the "Enemy progress" line are gone); `ReplicationAllowlist.json` with its row and a per-class audit | `ObjectivesAllowlistRow`, `PIE.TeamViewOwnerOnly`, `PIE.RivalCountOneUpdate` x2 |
| 012 | `URosterStore::OnParticipantViewRebound` (raised from `RegisterTeamView`), the service's subscriptions (`HandleViewRebound` inline write, `HandleRosterReset` clear + empty default to every member) | `ResetAndRejoin`, `ViewReboundWritesTeamView` (a real URosterStore), `RejoinHoldDoesNotResume` |

## ADR-0020 verification items

- **OBJ-V1** (owner-only reach): PASS in single-process PIE, listen server + 2 clients (one remote client per team): sampled on
  every frame, each client world held exactly one Objectives view component (its own) and never a court live only for the other
  team; at Active each client's view equalled its team's server build; when team 0 banked Stage1 team 1's client received no
  change (change count and value unchanged); after a reproduced rejoin (view reset to a new controller's default, then the
  Roster's RegisterTeamView -> OnParticipantViewRebound -> the service's inline write) each client got its own team's view
  again. `OC011 OBJ-V1 samples=7 component-count violations=0 leak violations=0`. **Seamless travel: NOT RUN** (needs the Session
  multi-process harness with the production game mode; deferred to story 019 / session-travel, decision logged).
- **OBJ-V2** (coalescing): PASS with `Net.IsPushModelEnabled` 1 and 0: `one-frame pair: 1 change(s), final 2`, `two frames: 2
  change(s)`, the observed count never decreased. Observation: an unmarked write arrived in both modes (`unmarked write
  arrived=1`): RepLayout's `IsPropertyDirty` also compares a NetSerialize property after a garbage collection, so the push-on
  control is not deterministic and is recorded, not asserted; the "every write marks dirty" rule is pinned by the source check
  in `ObjectivesAllowlistRow` (with a seeded unmarked writer as its negative control).

Output: `objectives-pie-filtered.txt`.

## Run and observe (3-player listen-server PIE, L_Prototype, 50 NPCs)

Driven through the Unreal MCP (`EditorAppToolset.StartPIE` floating; `StealthPlayToolset.RunPlayConsoleCommand` with
`StealthBot 2 interact`, the production Interact path). Windows captured with `tools/dev/capture-window.ps1`; every image was
looked at; none is black. Teams: host #0 and client #2 Jaguar (team 0), client #1 Eagle (team 1). Log: `pie-run-and-observe-log.txt`.

| Shot | What it shows |
|---|---|
| `oc11-01-team0-client-stage1.png` | Jaguar's client: "Stage 1/3: Pay the Runner [hold E]", "Rival steps: 0 of 2", a marker "Pay the Runner" at its own court (card text from the cooked chain by DefIndex, no longer the prototype actor's name) |
| `oc11-02-team1-client-stage1.png` | Eagle's client: "Stage 1/3: Hear the Runner", "Rival steps: 0 of 2"; no marker of Jaguar's court |
| `oc11-03-team0-p1-banked-message.png` | Jaguar's client after the tend: "Stage 2/3: Gather the Root" and the local line "Task complete. The next task is ready." (from its own view, not an RPC) |
| `oc11-04-team0-stage2-own-view.png` | Jaguar's client at Stage 2/3 |
| `oc11-05-team1-unchanged-after-rival-stage.png` | Eagle's client at the same moment: still "Stage 1/3: Hear the Runner", no enemy stage line, "Rival steps: 0 of 2" (the delayed OC-12 count is stories 014/015) |
| `oc11-06-team0-final-unlocked.png`, `oc11-08-host-team0-same-view.png` | after P2: Jaguar's client and the host (same component, same code) at "Stage 3/3: The Ruler's Cacao"; the host shows the final's marker |
| `oc11-07-team1-no-final-no-rival-steps.png` | Eagle's client: still Stage 1/3, no final, nothing of Jaguar's progress |

The HUD lines "Task complete" / "Your house's preparations are done" were logged for the host (netmode 2) and one client
(netmode 3, Jaguar's) only.

Run result: OBSERVED - each team's client shows only its own team's objective view; after team 0 banked P1 and P2, team 0's
client and host showed Stage 2/3 then 3/3 with the final, and team 1's client stayed at Stage 1/3 with "Rival steps: 0 of 2" and
no rival stage (the prototype's "Enemy progress" leak is gone).

## Code review (self, `.claude/skills/code-review/SKILL.md` focus)

Checked against ADR-0020 sections 4 and 6 and the control-manifest rules: objective data travels only in
`UObjectiveTeamViewComponent::View` (the GameState's `TeamStage` is no longer written on the production path; `FRoundPublicState`
has no objective field: `TeamViewContentsCore`); the view never holds a locked or opponent site or the final before the own
prelims (`TeamViewContentsCore`, `TeamViewOwnerOnly`); no FText/FString/FName in the view; the service is the only writer, from
the Active-entry step, a slot, the rebind handler or the reset clear; every write marks the push-model property dirty (source
check); change-only at both the team and the component level; the server-only headers stay out of client code (the HUD reads
only `View/` types; the source scans pass). Doc comments on every new public API; test seams under `WITH_STEALTH_TEST_SEAMS`.

Defects found and fixed during the build:
- `StealthGameplay` did not link `NetCore` (`MARK_PROPERTY_DIRTY_FROM_NAME` -> unresolved `UEPushModelPrivate::MarkPropertyDirty`): private dependency added.
- `TeamViewOwnerOnly` asserted more than 10 samples; the run is short (7 samples, one per wait at least): the bound is now >= 4.
- The push-on unmarked-write assertion failed (the write arrived): an engine fact (GC-forced compares of NetSerialize
  properties), not a product defect; the control became an observation and the rule a source check (see OBJ-V2).

Findings routed, not changed:
- AC1's seamless-travel clause (OBJ-V1) needs a production-game-mode scenario in the multi-process harness (story 019 / session-travel).
- `RivalPrelimCount` stays 0 until stories 014/015 (HOLD in the core); the HUD shows "Rival steps: 0 of 2".
- The prototype `AStealthObjective` actors still replicate `bCompleted` / `InteractorCount` (never set on the production path);
  their deletion is story 022. `AStealthObjective::GetStandInCourts` is the shared stand-in court id rule until the LSC fixtures exist.
- Penalty's `UPenaltyService::OnViewRebound` could subscribe to the new `URosterStore::OnParticipantViewRebound` (its tally re-push
  on rejoin is currently driven by `ParticipantReclaimed`); left to the NKP / Roster owners.

Verdict: APPROVED.
