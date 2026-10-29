# Evidence: Concealment (Bushes) stories 001-026, 2026-10-29

Stories: `production/epics/concealment-bushes/story-001..026`, including spikes 010 (S-CON-1) and 021 (R5). One code commit for
the batch (decision D41: the stories share the core, the service, the component, the cull, the composition-root wiring and the
test rigs, and every pushed commit must be the gate-tested tree). Integration criterion of the batch: the prototype's
`AStealthBush` overlap concealment and `AStealthCharacter::bConcealed` / `bRevealedUntilExit` are replaced by the production
per-viewer cull (ADR-0003 §5 with the S10-R2/R3 re-open guard) and presentation latch (ADR-0017), ASN-006's fake bush query by
the real `IBushSpotQuery`, and Crowd's bush herring (CNB-022) is on.

Confirmed user rules honoured: a concealed pawn is fully invisible to enemies (no actor, no channel on their connections) but can
be blind-attacked (a strike on the bush's spot draws a victim from the occupants); informants cannot see concealed rivals (no
x-ray); D1 share cap unchanged (server query); D26/D33 no ping while concealed including Emerging (`IsInBushOrTransition` in
`HandlePing`); D49 the graybox Manifest's BushSite records are a stand-in gathered from L_Prototype's ten bush actors.

## Build

`tools/dev/cycle.ps1 -Tests -NoLaunch` -> `Result: Succeeded` (Development editor). StealthGameplay.Build.cs gained the
Features/Concealment include paths and StealthGame.Build.cs the RenderCore dependency (spike R5's render-thread sample):
`Build.bat -projectfiles ... Result: Succeeded` after each. `Stealth.Tuning.SchemaGen`: `WROTE` (eleven Concealment rows, the
first enum key), TuningSchemaVersion 8 -> 9 (fingerprint 0x21e2825b8dc4aec5). Config/DefaultEngine.ini gained the BushVolume
(GameTraceChannel3, object) and PresentationAim (GameTraceChannel4, trace) channels; no SecurityToken line.

## Full-suite gate (`^StealthGame.`, 565 tests: 517 + 48 new)

| Run | Result | Failures |
|---|---|---|
| 1 (aborted at ~110 tests) | - | `Assassination.PIE.CulledVictimOpensFalling` and `LateActorSeeksNeverSnaps`: a fresh client actor was hidden at BeginPlay and shown a tick later, and the show overwrote the S10 warm-up's anim tick option, so the warm-up timed out; an informant teammate with no pawn also held every fresh actor hidden. Fixed (latch evaluated at PostNetInit, tick option restored, hold only for a concealed unresolved teammate); both re-run filtered with the Concealment PIE suite: 9/9 |
| **2 FINAL (`full-suite.txt`)** | **565 / 565** | none (`tests exit=0`) |

Earlier filtered runs during the batch: the Concealment suite and Tuning/Roster/Penalty/Assassination-invariants 142/144 then
fixed (`ExitSideMapping` row -135 deg belongs to side 3 under the inclusive lower bound - a test-table error; `CollisionReplay`:
the motion gate registered at BeginPlay, moved to OnRegister); the two prototype PIE tests that teleported into the leaves
re-pointed to the production Hide(Enter) and passing (`Assassination.PIE.NoBushTargetingUi`, `PrototypeScenariosACF` F).

## What was built

| Story | Built | Tests |
|---|---|---|
| 001 | `FConcealmentTuning_Public` (7 keys, `All`) and `FConcealmentTuning_Server` (4 keys incl. the enum `BushVictimMode`) on both faces and `UStealthTuningData`; the schema's first enum key (SchemaGen, fingerprint, ApplyF1); F3 as TDC invariant rows I-CON-F3a..f; TuningSchemaVersion 9 | TuningRows, TimingChain, NoDwellKey, Tuning.SchemaComplete |
| 002 | `UBushRuntimeSubsystem` (FBushSiteRecord, V-BUSH-1 `ValidateSites`, `BuildFromSites`, `FindEnterableBush`, `FindBushAt`), `ABushVolumeSet` (one local box set, BushVolume object channel), the D49 stand-in site gatherer, V-BUSH-1 layout abort | ManifestReject, SingleVolumeSet, BushRuntimeHash, PIE.BushSiteParity |
| 003 | `FConcealmentCore` records and occupancy, `UConcealmentComponent` (pawn feature component, never replicated) | StateMachine, OccupancyRecord, TakeoverEligibility |
| 004 | Hide(Enter) resolver, Commit/End timer items, the commit guard and `ConcealGuard` fault + internal exit | EnterExecution, GuardFaultPath |
| 005 | Hide(Exit) side choice (nearest open, blocked-side fallback, 30 cm depenetration cap), un-cull in the exit's slot | ExitSideAndBlocked, ExitExecution, ExitSideMapping |
| 006 | latch exemption for in-flight transitions, reset, stale-item voiding, no dwell timer | RoundEndAndReset, NoDwellTimer, StaleItemsVoided |
| 007 | `ICommandPredicate` HideEnter / HideExit (reach, side, gait, informant, phase, busy, cooldown); Interact gate stand-in | EntryLegality, SamePathForNpcs |
| 008 | `ComputeConcealCollision`, `IStealthMotionGate` on `UStealthMovementComponent` (gate evaluated per move, saved in `FSavedMove_Stealth`, not during replay), change-only collision writer | CollisionAndAvoidance, CollisionReplay, MoveZeroed |
| 009 | `ICrowdPerceptionFilter` (perceivable unless Concealed, steering obstacle only Outside), Crowd herring sites from bushes, cooldown from tuning | PerceptionFilter |
| 010 | spike S-CON-1 (`production/qa/spikes/2026-10-28_S-CON-1.md`) | StealthSpike.Concealment (not in the gate) |
| 011 | the one server `FRootMotionSource_MoveToForce` (ConcealEnter / ConcealExit) for every pawn | PIE.CollisionAndStillness.CollisionAndAvoidance |
| 012 | mechanism (b) PredictOnPress as the default (`Stealth.Concealment.OwnerWarp` 2), the server and owner correction windows; `PressHide` starts the owner's source | PIE.OwnerWarpNoSnap |
| 013 | stillness inside (Move zeroed, yaw kept), Prepare/Interact refused | PIE.CollisionAndStillness.InsideViewAndStillness, InsideInputs |
| 014 | Attack-from-inside break (Emerging, un-cull in the slot, emerge side from aim) | BreakAtExecution, EmergeTiming, InsideInputs |
| 015/016 | spot zone (75 cm, rounded corners), non-ally occupants via `IBushSpotQuery` into Assassination's VictimResolverCore and the BushSpotPick stream | SpotZoneAndVictim, SpotOccupants, SpotPick |
| 017/018 | kill-while-concealed slot rules, death-write un-cull with `bDiedConcealed`, body removal / rebind clears, no immunity while Entering | KillSlotRules, DeathWrite, BodyRemovalOrRebind, NoImmunityWhileEntering |
| 019 | `EvaluateBodyHidden` (pure, fail-closed) | BodyHiddenLatchTable |
| 020 | the client latch on `UConcealmentComponent` (fresh actor hidden from OnRegister, evaluated when net-initialised, unresolved-ally hold), `IsAllyBodyHidden` facade, HUD hatched-marker stand-in | PIE.PerViewerVisibility.* |
| 021 | spike R5 (`production/qa/spikes/2026-10-29_R5.md`) | Stealth.Spike.R5 (console, not in the gate) |
| 022 | `ApplyHidden` (`SetVisibility(false, true)` on the mesh and root-attached props, presentation-aim channel ignore, pose ticking), the three gating flags pinned on `AStealthPawn` | Pawn.RenderFlagParity, PIE.PerViewerVisibility.ListenHostParity |
| 023 | `UConcealmentOwnerViewComponent` (owner-only `bIsConcealed`), Roster `SetTeammateConcealed` | OwnerAndTeamFlags, Roster.TeammateConcealed |
| 024 | `IsConcealedFrom` | IsConcealedFromTruthTable, CoOccupantRivalsBlind |
| 025 | `UStealthConcealmentCull` (Foundation; IVP 024/025's cull with the S10-R2/R3 guard) through `IConcealmentCullSource`; `AStealthPawn::IsNetRelevantFor` | CullDeadlineGrid, CullStateChain, PIE.PerViewerVisibility.CoOccupantsAndCullBoundary |
| 026 | per-viewer visibility end to end | PIE.PerViewerVisibility.HiderCulledForEnemyKeptForAllyAndOwner, CoOccupantsAndCullBoundary |

Prototype retired (integration): `AStealthBush` keeps only its placement (NoCollision, no overlap handlers);
`AStealthCharacter::bConcealed`, `bRevealedUntilExit`, `SetInBush`, `RevealFromConcealment`, `UpdateConcealmentVisibility`,
`OverlappingBushes`, `bHiddenForLocalViewer` removed (`IsConcealed()` is now a server read of `UConcealmentService`); the
prototype bush query and body observer in the command wiring removed (the production service is the Encounters `Bushes` /
`BodyObserver`); `HandleHubActionStarted`'s prototype reveal is a no-op; the HUD reads the owner view flag. The two prototype
PIE tests that teleported a pawn into the leaves (`Assassination.PIE.NoBushTargetingUi`, `Assassination.PIE.PrototypeScenariosACF`
scenario F) now hide through the production Hide(Enter) (`Tests/PIE/Concealment/ConcealmentPieSupport.h`) and still pass.

## Stand-ins (each named in code and on its story)

- **Level Socket Contract / D49**: BushSite records from L_Prototype's ten `AStealthBush` actors (sorted by name, ground-traced,
  every side open) - `StealthConcealmentWiring::GatherBushSites`.
- **IIF Interact owner**: `StealthBushGatedPredicate` answers NotNow for Interact while the pawn is in a bush or transition.
- **Teammate Markers TM-R5**: the prototype HUD draws a hatched box around a teammate chevron whose body `IsAllyBodyHidden`.
- **IVP-owned key** `ConcealCullDelayMs`: a pending-owner constant (250 ms) until IVP registers its row.

## Spikes

- **S-CON-1** (story 010): `production/qa/spikes/2026-10-28_S-CON-1.md` — mechanism (b) PredictOnPress PASS (100 + 100
  transitions under StealthLeak2: press-to-motion p95 9.3 ms, 0 corrections during a transition); (a1) fails (one RTT, E4
  duplicated-then-corrected). ADR-0017 amended ("S-CON-1 resolution 2026-10-28"). Story 012 unblocked and closed.
- **R5** (story 021): `production/qa/spikes/2026-10-29_R5.md` — PASS for the six software-Lumen cells (no proxy for any hidden
  subject primitive incl. groom and Niagara; Hidden-vs-Empty equals noise in the subject region); hardware-RT cells NOT RUN
  (`r.RayTracing=False`); hide/show 0.024 ms GT, 0.016 ms under the D56 PROXY affinity. Stories 022 (closed), 028 and 034
  unblocked.

## Run-and-observe (3-player PIE, the batch's integration criterion)

- Run on the FINAL build (after the gate fixes; a first observation on the pre-fix build showed the same). Editor launched with `tools/dev/cycle.ps1`; 3-player PIE on L_Prototype (listen server + 2 clients, floating windows, 50
  NPCs) through the editor's MCP server over HTTP (`EditorAppToolset.StartPIE`; `StealthPlayToolset.RunPlayConsoleCommand`
  with `StealthBot`, the production command path); windows captured with `tools/dev/capture-window.ps1`; every screenshot
  looked at, none black. Teams: host (#0) and client 2 (#2) on Jaguar (team 0), client 1 (#1) alone on Eagle (team 1).
- `StealthBot 2 hide`: client 2's pawn stands at the nearest bush's entry and submits the one Hide(Enter)
  (`[Bot] #2 ... concealed=1`); `StealthBot 1 watch2 450` / `StealthBot 0 watch2 350` stand the enemy and the ally facing it.
  - `con-01-enemy-client1-hider-concealed.png` — **the enemy's client**: the bush, nothing of the hider (no body, no shadow,
    no marker; its client holds no actor for it).
  - `con-02-ally-host-hider-concealed-hatched.png` — **the teammate (listen host)**: the hatched marker over the bush,
    labelled "(hidden)"; the body itself is not drawn (the latch).
  - `con-03-hider-client2-own-view-inside.png` — **the hider's own client**: "Hidden from enemies" (the owner-only flag);
    its default boom camera sits in the foliage (the dome-clamped concealed boom and grade are Camera story 014's, not built).
- `StealthBot 1 blindstrike`: the enemy strikes the bush's spot with no target pawn (`[Bot] #1 blind-strikes at ...`);
  `[Encounter] ... killed ... in the slot`, `death write applied ... (concealed 1)` (bDiedConcealed); the Roster shows team 0
  `act 1 inf 1`; client 2 becomes an informant.
  - `con-04-enemy-client1-blind-strike-kill.png` — the enemy: "Target eliminated" plate.
  - `con-05-hider-client2-killed-in-bush.png` — the victim: the informant banner and "Returning in 12".
  - `con-06-ally-host-after-kill.png` — the teammate: the marker is gone (no living concealed teammate).
- Server log excerpt: `pie-run-and-observe-log.txt`; 0 `LogStealthFault` lines in the session.

## Not done in this batch (reported)

- Stories 027-030, 032-034 are Ready (028 and 034 unblocked by R5, 033 by the built Assassination resolver); 031 Blocked
  (needs 10 logged playtest matches); 035 Blocked on 028, 033, 034.
- Deferred ACs inside closed stories: 004 AC4 (no BushEnter clip asset), 009 AC2 (100-route NPC bit-identity run), 013 AC2
  (dome-clamped concealed boom: Camera story 014), 020 AC3 (clip seek under LeakNetProfile: no clips yet), 023 AC2 (PIE rejoin:
  Session's PIE rejoin not built), 025 AC2 (no carried/tend prop actors exist); R5's hardware-RT cells (story 034).

## Code review (self, per `.claude/skills/code-review/SKILL.md`)

Verdict: **APPROVED** with the fixes below applied before the gate.

- Layering: Concealment lives in `StealthGameplay/Features/Concealment` (View/ and Server/ faces); Core never names it (pawn
  feature components registered through `StealthPawnFeatureComponents`, the motion gate through `IStealthMotionGate`); the cull
  is in Foundation behind `IConcealmentCullSource`; Crowd reads only `ICrowdPerceptionFilter` (header scan with a seeded
  control in PerceptionFilter). The composition root is the only place that wires Concealment to Round, Encounters, Crowd,
  Roster and the cull.
- Server authority: every transition is a queue item on the server; clients derive state only from `FActionState` /
  `FDeathState` and the owner-only flag; `bServerAcceptClientAuthoritativePosition` is never set true (asserted in
  OwnerWarpNoSnap); no replicated field was added (allowlist: one owner-only row for `bIsConcealed`, the prototype's
  `bConcealed` / `bRevealedUntilExit` replication removed).
- Data-driven: every number is a tuning row (eleven keys) or a named pending-owner constant (`ConcealCullDelayMs`); no magic
  numbers in the rules (the 2 s unresolved-ally hold is ADR-0017 §5's bound, named in code).
- Testability: `FConcealmentCore` is headless (fake pawn world, queue rig, recording sinks); the engine adapters are injected.
- Fixes made in review: the motion gate registered at `OnRegister` instead of `BeginPlay` (a move can run before BeginPlay;
  CollisionReplay caught it); the cull's close log is bounded (1024, halves when full); `ApplyHidden` also suppresses anim
  notify dispatch (footstep decals, foley, notify Niagara, sounds) and pauses attached audio emitters (story 022 AC1 had it
  only by implication); the three gating render flags pinned on `AStealthPawn`'s primitives (they were engine defaults only).
- Fixes from the first full-gate attempt (Assassination.PIE.CulledVictimOpensFalling and LateActorSeeksNeverSnaps failed): a
  fresh client actor was hidden at BeginPlay and shown a tick later, and the show overwrote the S10 warm-up's anim tick
  option, so the warm-up timed out; and an informant teammate with no pawn held every fresh actor hidden for 2 s. Now the
  latch is evaluated at PostNetInit, the tick option is restored not overwritten, and only a concealed unresolved teammate
  holds. Both tests pass again.
- Residual risks (recorded on the stories): the S10-R4 lost-pawn repair and the R6 clock are IVP scope, not built here (the R5 warm-up is the existing EncounterPresentation one);
  PredictOnPress mispredicts when the server refuses a Hide the owner thought legal (one normal correction after the window);
  `PawnNetUpdateHz` for the cull grid is read from the Locomotion defaults, not the live tuning store.
