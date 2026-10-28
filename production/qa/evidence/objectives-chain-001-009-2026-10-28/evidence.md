# Evidence: Objectives Chain stories 001-009, 2026-10-28

Stories: `production/epics/objectives-chain/story-001..009`. Types: Logic (001-004, 006-009), Integration (005). One commit for the
batch (decision D41; logged: the stories share the core, the service, the composition-root wiring and the test rigs, and every
pushed commit must be the gate-tested tree).

Build: `tools/dev/cycle.ps1 -Tests -NoLaunch` -> `Result: Succeeded`. StealthGameplay.Build.cs gained the Objectives feature
include paths: `Build.bat -projectfiles ... Result: Succeeded`. `Stealth.Tuning.SchemaGen`: `WROTE` (ten new rows),
TuningSchemaVersion 7 -> 8 (fingerprint 0x3fd12248f6497fd9). The chain asset was authored in the editor by
`Stealth.Objectives.BuildCacaoChain` (`STEALTH OBJECTIVES CHAIN: PASS ... (5 rows, serial 1)`); no other asset was saved
(`git status Content/` shows only the new folder). Stamp: `stamp.txt`.

## Full-suite gate runs (`^StealthGame.`, 481 tests: 449 + 32 new)

| Run | Result | Failures |
|---|---|---|
| 1 (`full-suite-run1.txt`) | 479 / 481 | `Tuning.InvariantBoundaries`: it pinned the real schema at exactly 18 range pairs, all Crowd-owned; Objectives' `RivalSignalDelayMinSec` / `MaxSec` are a new Min/Max pair (TDC's I-5 now also checks Min <= Max, beside I-OC2): the test now expects 19 pairs (18 Crowd + this one) - a real consequence of story 001, fixed. `Assassination.PIE.KillAtArmsLength` (machine 0: Death_Fall later than ContactServerTime +- one frame): the known load-sensitive timing test; re-run filtered: Success (`killatarmslength-filtered.txt`) |
| **2 FINAL (`full-suite.txt`)** | **481 / 481** | none (`tests exit=0`) |

New tests (32): StealthGame.Objectives.{InteractSecFloor, RivalSignalBound, TendCueDelayInvariant, ObjectivesTuningRows,
ChainDefShape, CacaoChainContent, ChainAssetId, ChainTextLocalisable, ChainBindAndValidate, AssignmentServerOnly,
ProgressOnlyByCompletion, FinalNeedsOwnUnlock, StageLifecycle, PreparingFailsOnBadChain, ServiceHasNoTickTimerActor,
CompositionRootRegistrations, InteractHoldSecOneValue, ClaimLifecycle, ClaimReleaseOutsideSlot, ClaimConsistencyCheck,
PredicateSiteAndClassOnly, FeintMatchesDecoy, DecoyIncludesTendedCourts, CompletionGate, CompletionGateControlEpoch,
StageCompletionEffects, DebugCompleteStage, CompletionGateFinal, FollowOnInSlot, NoStackNoCancel, FinalSameFrameOrdering} and
StealthGame.Objectives.PIE.ServiceBoots.ServerOnlyServiceRegisteredWithNoFaultAtActive.

Other builds: the Development game cooked and staged (`BuildCookRun ... BUILD SUCCESSFUL`, OBJ-V3 `production/qa/evidence/oc-cook-check.md`);
the Shipping game built and linked (`production/qa/evidence/oc-shipping-build.md`).


## What was built

| Story | Built | Tests |
|---|---|---|
| 001 | `FObjectivesTuning_Public` (+ `TendCueDelaySec`, `FixtureInteractSideCount`) and `FObjectivesTuning_Server` (8 ServerOnly keys) on both tuning faces and `UStealthTuningData`; I-OC1..I-OC3 as TDC Check rows (owner Objectives; I-OC2 a mixed row with both clauses); the GDD table as a constant fixture, pinned by SchemaComplete | `StealthGame.Objectives.InteractSecFloor`, `RivalSignalBound`, `TendCueDelayInvariant`, `ObjectivesTuningRows` |
| 002 | `FObjectiveDef` (no hold-time field), `UObjectiveChainData` (primary type `ObjectiveChain`, CDO id invalid), `DA_ObjectiveChain_Cacao` (OC-16), the Asset Manager scan with `AlwaysCook`, the Game gather target, `UObjectivesChainRegistry` | `ChainDefShape`, `CacaoChainContent`, `ChainAssetId`, `ChainTextLocalisable`; OBJ-V3 `production/qa/evidence/oc-cook-check.md` |
| 003 | `FObjectivesCore::ValidateChain` (rules 1-5, first failing clause), `Bind` through `IObjectivesAssignmentSource::GetSiteAssignment_Server` | `ChainBindAndValidate`, `AssignmentServerOnly` |
| 004 | per (team, row) Locked/Live/Completed, StageIndex, CompletedDefMask, `ApplyCountedCompletion` (only writer), Active entry, Ended freeze | `ProgressOnlyByCompletion`, `FinalNeedsOwnUnlock`, `StageLifecycle` |
| 005 | `UObjectivesService` (server-only world subsystem), the composition root's adapters and registrations, Preparing's `ObjectiveChainInvalid` abort, the Interact hold STAND-IN | `PreparingFailsOnBadChain`, `ServiceHasNoTickTimerActor`, `CompositionRootRegistrations`, `StealthGame.Objectives.PIE.ServiceBoots` |
| 006 | `IInteractTargetOwner` (Core) + `UActionHub::RegisterInteractTargetOwner`, the claim table, the drain-end consistency check, `UStealthPawnFactory::IsHandOverPending` | `InteractHoldSecOneValue`, `ClaimLifecycle`, `ClaimReleaseOutsideSlot`, `ClaimConsistencyCheck` |
| 007 | the Interact predicate in its own translation unit | `PredicateSiteAndClassOnly`, `FeintMatchesDecoy`, `DecoyIncludesTendedCourts` |
| 008 | the gate (a)-(e), ObjectiveStage / TendCompleted rows, view push and release hooks, DebugCompleteStage through Round's DebugItem | `CompletionGate`, `CompletionGateControlEpoch`, `StageCompletionEffects`, `DebugCompleteStage`; Shipping `production/qa/evidence/oc-shipping-build.md` |
| 009 | the final branch (FinalCompleted, DeclareObjectiveWin in the slot), latched-final TendCompleted(NoEffect, Latched), TeamSummary from the production store | `CompletionGateFinal`, `FollowOnInSlot`, `NoStackNoCancel`, `FinalSameFrameOrdering` |

Rigs: `ObjectivesTestRig.h` (stubs of every seam, the Cacao chain built in code, a core over them), `ObjectivesQueueRig.h` (the
REAL resolution queue on a fake clock, a bare world's UActionHub and UObjectivesService, the hold stand-in, Round hooks that latch the
queue, a strike stub), `ObjectivesGateRows.h` (the six-row table), `ObjectivesSourceScan.h` (comment-blind name scans and the
non-Shipping guard scan, each with a seeded negative control).

## Stand-ins (each named in code and on its story)

- **IIF stage 2** (`FInteractHoldStandIn`, Core): InteractBegin / End and HoldTimer(Complete) for Interact on Round's queue,
  following the ADR-0020 5.1 owner contract (predicate at Begin, the court from the command or the nearest court in reach, D read
  once, OnInteractExecuted / OnInteractEnded inline before the hub's events, latched Complete -> OnInteractCompleteLatched,
  CancelAll at Ended / Reset). Missing: the release debounce, condition cancels, the owner's reply batch and FHoldStatus.
- **Level Socket Contract**: `FSiteAssignment` and its query are declared by Objectives; the composition root reads L_Prototype's
  five AStealthObjective actors (court id = 1-based index by name, role from OwningTeam/Stage, four faces at 120 cm). The actors
  keep only their placement; no prototype objective gameplay runs on the production path.
- **Team view (stories 010/011)**: the view sink mirrors the production StageIndex into the prototype HUD's
  `AStealthGameState::TeamStage` and tells the completing team. The prototype HUD still prints "Enemy progress" from that
  replicated array (the prototype's own leak, unchanged; story 011 replaces the HUD source, 022 deletes it). Card text on the HUD
  is still the prototype actor's DisplayName.
- **Rival signal (014/015)**: no release is scheduled; the StageSignal resolver is registered and voids anything.
- **Tuning**: the schema defaults (as Crowd and Assassination) until the TDC store feeds features the frozen set.

## Run and observe (3-player listen-server PIE, L_Prototype, 50 NPCs)

Driven through the Unreal MCP (`StartPIE` floating; `StealthPlayToolset.RunPlayConsoleCommand` with the new StealthBot `interact`
command: the bot stands at a court and submits `InteractBegin` through its pawn command component, so the hold resolves in Round's
queue and completes in UObjectivesService's slot after ObjectiveInteractSec 3.0 s). Windows captured with
`tools/dev/capture-window.ps1`; every image was looked at; none is black. Teams: host #0 and client #2 Jaguar (team 0), client #1
Eagle (team 1). Log lines: `pie-run-and-observe-log.txt`.

| Shot | What it shows |
|---|---|
| `oc-01-team0-client-stage1.png`, `oc-02-team1-client-stage1.png` | Active entry: both teams at Stage 1/3 (`[Bot] Objectives: bound=1 team0 stage=1 ... team1 stage=1`) |
| `oc-03-team0-tending-p1.png` | #2 (Jaguar) at its live P1 court after `InteractBegin` (court 1) |
| `oc-04-team0-p1-banked.png` | 3 s later: Stage 2/3 (`team0 stage=2 prelims=1`) |
| `oc-05-team1-feint-at-completed-court.png` | #1 (Eagle) tends court 1, Jaguar's completed P1 (a feint): nothing changes for Eagle (`team1 stage=1 prelims=0` afterwards); Jaguar's P2 banked meanwhile |
| `oc-06-team0-final-unlocked.png` | Jaguar at Stage 3/3 (`team0 stage=3 prelims=2`) |
| `oc-07-team1-view-during-final-tend.png` | Eagle's view while the host tends the shared final (court 5) |
| `oc-08-team0-objective-win.png`, `oc-09-team1-sees-loss.png` | "JAGUAR WIN (final objective)", "Jaguar: steps 3 of 3", "Eagle: steps 0 of 3" (TeamSummary now reads the production store) on both teams' clients; `Round over: team 0 wins (objective).` |

Run result: OBSERVED - 3-player PIE on L_Prototype: one team completed both preliminaries and then the shared final through the
production chain (UObjectivesService's gate in the HoldTimer(Complete) slot, DeclareObjectiveWin), ending the round with that
team's objective win; a feint at a completed court had no effect.

## Code review (self, `.claude/skills/code-review/SKILL.md` focus)

ADR-0020 guidelines checked against the code: no actor, tick, timer manager or latent action in Objectives (source scan in
`ServiceHasNoTickTimerActor`); `FSiteAssignment` read only through `GetSiteAssignment_Server` (scan in `AssignmentServerOnly`);
the predicate reads only court, claims and legality class and never returns AllowNoEffect (scan in `PredicateSiteAndClassOnly`);
one chain-wide D, read once at execution (`InteractHoldSecOneValue`); completion judged only in `OnInteractEnded(Completed)` with
the slot's key, the final's `DeclareObjectiveWin(team, CurrentKey())` in that slot (`FollowOnInSlot`); a completion never cancels
another hold (`NoStackNoCancel`); Event Log rows only through Round's writer, never `UE_LOG` (the only `UE_LOG` in the feature is
the non-Shipping `Stealth.Objectives.ResolveChain` dev command); the server-face tuning names absent from client modules
(`ObjectivesTuningRows`); the debug path compiled out of Shipping (`DebugCompleteStage` scan, `oc-shipping-build.md`). Doc
comments on every public API. Server-only headers carry `StealthServerOnlyGuard.h`; the service is created on server worlds only
(`ServiceBoots` checks the clients).

Defects found and fixed during the build:
- Unity-build collision: adding `ObjectivesInvariants.cpp` moved `PenaltyInvariants.cpp` into the same unity blob as
  `AssassinationInvariants.cpp`, whose anonymous-namespace helpers have the same names: Penalty's helpers now sit in a named
  namespace (and the new files use named local namespaces).
- `FRoundMatchCore::SubmitDebugObjectiveStage` was first declared in the private section (moved public).
- The Development GAME build (BuildCookRun) failed on the existing test `WarpTargetsNotReplicated` reading the editor-only
  `UClass::ClassGeneratedBy`: it now tests `CLASS_CompiledFromBlueprint` (same meaning in every configuration).
- `Tuning.InvariantBoundaries` pinned exactly 18 Crowd range pairs; the rival-delay Min/Max is a new pair (I-5 applies to it):
  the test expects 19 (gate run 1).
- Test timing: two queue tests aligned their "same frame" by elapsed-time sums; they now step to the frame before the hold's
  due time (`RunToFrameBefore`), and the voluntary-release case of `NoStackNoCancel` releases before the second hold's D.

Findings routed, not changed:
- The prototype HUD's "Enemy progress" line reads the replicated `AStealthGameState::TeamStage`, which the stand-in view sink now
  feeds from the production store: the prototype's existing opponent-progress leak is kept until stories 011 (team view) and
  022 (retirement) replace the HUD source. The rival signal (014/015) is the designed replacement.
- The stand-in IIF hold has no release debounce, condition cancels, reply batch or FHoldStatus (IIF stage 2).
- `UObjectivesService` is bound once per server world (as Encounters / Penalty): a second match on the same world after the
  queue is released keeps no StageSignal registration until rewired (same limitation as the other services).

Verdict: APPROVED.

