# Crowd engine verifications (story cnb-001; ADR-0016 verification items 2, 3, 6, 7), 2026-10-26

Engine: UE 5.8.4 (installed). Run headless (`-nullrhi`) by the automation suite; the PIE rig is the fixture map
`L_Prototype` as listen server + 1 client with `AStealthGameMode` and the production crowd (D53). The spike used the
production classes (`AStealthCrowdController`, `UCrowdStateTreeComponent`, `UCrowdDirector`) instead of a throwaway rig
under `Source/StealthGame/Private/Spikes/`: they were written in the same batch, so a separate rig would have measured
different code (deviation from the story's Files list; nothing throwaway was created, so nothing needs deleting).

Tests: `StealthGame.Crowd.PIE.EngineVerifications.CrowdEngineFactsHold` (harness, PIE) and
`StealthGame.Crowd.StateTree.GridWithEvents`, `StealthGame.Crowd.Controller.Shape`, `.ComponentParity`, `.TickOrder`
(headless, a bare game world ticked at a fixed 60 Hz).

| Item | Result | Measurement |
|---|---|---|
| (2) Controller without `PathFollowingComponent` under the visual logger, 60 s of routed motion | **PASS** (no fallback) | `DoNotCreateDefaultSubobject` leaves `GetPathFollowingComponent()` null on the CDO and on 40 live controllers; the visual logger recorded for the whole run; no check, ensure or null dereference in the log (0 "Ensure condition failed" lines in the 275-test run). The gameplay debugger is not available under `-nullrhi` (recorded, not run). |
| (3) Phase grid with events on 20 % of the pawns each second | **PASS** after two fixes (below) | Headless, 50 pawns, 60 Hz, 200 events in 20 s: 4.49 tree ticks/s per pawn, 0 off-grid ticks not explained by an event or a wake, max 5 decisions in a frame (bound ceil(50 / 15) + 2 = 6). PIE, 40 pawns, 58 s of world time (about 60 frames/s of wall time shared by two PIE worlds): 9,989 tree ticks (4.3 Hz each), 600 events, 773 wake or event ticks, 0 unexplained off-grid ticks, max 8 decisions in a frame (the bound scales with frame time: a long PIE frame holds more grid points; not gated in PIE). |
| (6) `FindPathAsync` from a `TG_PrePhysics` tick | **PASS** | PIE, every result at least 2 frames after its request: min 2, p99 2 frames over 157 results (670 in an earlier run); at most 2 path starts in a frame (the story 007 budget). |
| (7) Component parity: AI-driven, player-driven and just-handed-over pawn | **PASS** | 19 components each, identical sets; `UGameplayTasksComponent` present and not replicated on all three (the handed-over pawn inventoried in its first possessed frame); `StateTree.Component.ScheduledTickEnabled` asserted on (the Director also asserts it at `OnWorldBeginPlay`). |

## Findings that changed code (item 3)

1. **The first tick after `StartLogic` puts every tree on one frame.** Enabling a component's tick always runs it once
   next frame, whatever its interval (`FTickTaskLevel::QueueAllTicks`), so at round start all trees decided in the same
   frame (40 in PIE). `UCrowdStateTreeComponent::TickComponent` now skips an off-grid tick nobody asked for (no wake, no
   event) and only re-imposes the grid; the tick counts as `SkippedOffGridTicks`, not a decision.
2. **"On the grid" must allow one frame of lateness.** The tick manager fires a cooldown on the first frame that crosses
   it, so a grid tick lands up to one frame late; the fixed 20 ms tolerance flagged 2,218 PIE ticks as off-grid at
   about 48 fps. The tolerance is now max(20 ms, 1.05 x frame delta), and a tick before the first grid point is off-grid.
3. **Wakes need a per-frame budget for the decision bound.** Due-time wakes (`WakeNextFrame`) and events
   (`SendCrowdEvent`) are admitted by `UCrowdDirector::AdmitWakeDecision`, one a frame (the bound's +2 slack minus the
   stratified grid's +1); a refused wake waits a frame or for its grid point.
4. Headless test worlds: a manually ticked world must advance `GFrameCounter` per step (the tick manager queues a tick
   function once per frame number) and must end play before it is destroyed (`FTestWorldWrapper` order); a begun world
   destroyed without `EndPlay` crashed the next map load in Water's GC path.

E12 (the component overwrites its own schedule after every tree tick) holds: the grid is re-imposed after every tick.
No item needed the ADR's fallback (the Director driving `ScheduleTickFrame` per pawn); no superseding-ADR request.
