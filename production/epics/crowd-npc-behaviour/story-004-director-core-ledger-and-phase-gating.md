# Story 004: Crowd Director core: server-only subsystem, ledger and phase gating

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-001`, `TR-crowd-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-001 = Server-only Crowd Director subsystem. TR-crowd-009 = Dormant until OnRoundActive; Halt in the round-end step.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0002 (queue, round-end step), ADR-0007 (module placement, testable plain-C++ cores)
**ADR Decision Summary**: `UCrowdDirector` is a server-only `UWorldSubsystem` facade over a plain C++ `FCrowdDirectorCore` (no `UWorld`, headless-testable). It owns the population ledger, ambient schedule, per-ordinal draw counters, activity mirror, 2 m spatial hash, ray and path budgets, stimulus fan-out and the Halt latch, with one custom tick function in `TG_PrePhysics`. Every crowd pawn's StateTree is created stopped and started in `OnRoundActive`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: A world subsystem cannot choose its tick group, so the Director registers its own tick function (`TG_PrePhysics`, authority only, in `OnWorldBeginPlay`), the same pattern as ADR-0002's drain. `ShouldCreateSubsystem` must return false on clients. `StateTree.Component.ScheduledTickEnabled` is a CVar that silently forces every-frame ticks when off, so the Director asserts it at init. Use `UE_LOGF`.

**Why this story**: The Director is the anchor everything else hangs from: the ledger, the tick order, the phase rule (nobody acts before the round is Active; everybody starts in one step but out of unison). Read GDD Core Rules CNB-R1, R12, R15 and Acceptance Criteria AC-CNB-33.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: crowd code is server-only in `StealthGameplay/Features/Crowd/` with `Public/Server/` headers carrying the ADR-0007 guard; the driver kind comes from the `ICrowdControl` call; decision cadence is one global `CrowdDecisionPerSec` on a per-pawn phase-aligned grid; decision counters keyed by ordinal in the Director (ADR-0016)
- Forbidden: never include Roster, Objectives-assignment or Visibility Policy fact headers in Crowd code; never call `Possess`/`UnPossess` from Crowd code (ADR-0016)
- Guardrail: Director cost is inside the 1.5 ms p99 / 0.8 ms mean Crowd budget (provisional, story 032); memory under 10 MB

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-33, CNB-R1 and CNB-R12, scoped to this story:*

- [ ] `UCrowdDirector::ShouldCreateSubsystem` is false on clients; `FCrowdDirectorCore` constructs and runs in a headless spec with no `UWorld`; Director init asserts `StateTree.Component.ScheduledTickEnabled` is on.
- [ ] (AC-CNB-33a) GIVEN a round start with 50 NPCs, WHEN commands are logged, THEN the command log is empty before `OnRoundActive`; at `OnRoundActive` all 50 NPCs issue their first command in one server step; first pause and leg lengths pooled over at least 20 fixed-seed rounds pass a KS test at p > 0.01 against their distributions (no unison).
- [ ] (AC-CNB-33b) Decision ticks are spread so no frame holds more than `ceil(N / (FPS / CrowdDecisionPerSec)) + 2` decisions.
- [ ] The live-count ledger `NpcCount - V - B + Rel` (V = vacancies, B = NPCs taken over by informants, Rel = participant pawns released to the crowd, GDD CNB-R1) holds after a scripted sequence of kills, takeovers and releases and reads only kill times; if the navmesh is not ready the Director asserts `NavNotReady` and keeps pawns Dormant.

---

## Implementation Notes

*Derived from ADR-0016 sections 1, 3 and 7 and Implementation Guidelines:*

- Split exactly as ADR-0007 asks: `FCrowdDirectorCore` plain C++ (ledger, schedule state, counters, activity mirror, latch) behind `UCrowdDirector`; the composition root (`AStealthGameMode`) registers the Director's `ICrowdControl` and `ITakeoverEligibilitySource` faces weakly at world start. No lower layer names a Crowd type.
- `FCrowdDirectorTickFunction`: `TG_PrePhysics`, game thread, authority only; prerequisites are ADR-0010's hand-over pass; the StateTree component and controller depend on it (story 012 and 006 wire their side).
- `OnRoundActive()` starts every AI-driven pawn in Round's Active-entry step, each from its own drawn first phase (`ECrowdDecision::TickPhase`, first pause or leg) so the 4 Hz decisions spread evenly across frames.
- The ledger tracks V (vacancies: in-flight ambient cycles and unserved kills), B and Rel; it records kill times only, never who died (CNB-R1, R9.3). The kill, schedule and takeover entry points are added by stories 026, 027, 030 and 029; this story supplies the store and the invariant.
- Register the six scope names (`Crowd.Director`, `Crowd.StateTree`, `Crowd.Follower`, `Crowd.Perception`, `Crowd.NavQuery`, `Crowd.Dispatch`) as `TRACE_CPUPROFILER_EVENT_SCOPE` macros from day one (non-Shipping), so story 032 only reads them.
- Spawn ordinals come from the pawn factory (ADR-0010), assigned to every pawn including participants; the Director never invents ordinals.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the crowd-population parts of `Source/StealthGame/Private/StealthGameMode.cpp` (`SpawnCrowd`, `SpawnCrowdWhenNavReady`, `ScheduleNpcReplacement`). Those stay compiling until story 039 retires them.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002 (`story-002-crowd-draws-and-streams.md`): the draw functions the counters feed
- Story 026 and 027: ambient schedule items, replacement and kill handling
- Story 031: `Halt` semantics and handover epoch
- Story 008: spatial hash and ray budget contents

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdDirector.h`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdDirectorCore.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdDirector.cpp`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdDirectorCore.cpp`
- `Source/StealthGameplay/Public/Server/Core/CrowdControl.h` (consumer-owned `ICrowdControl`, `ITakeoverEligibilitySource`, `FCrowdAdoptParams`)
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdDirectorCoreTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). AC-2 revised after the GAPS verdict (sample size and significance level added).*

- **AC-1**: `StealthGame.Crowd.DirectorServerOnly`
  - Given: a client world and a server world; a headless core spec
  - When: subsystems are created and the core is constructed without a `UWorld`; init runs with the CVar on then off
  - Then: no Director on the client; the core runs headless; the assert fires with the CVar off
  - Edge cases: listen-server host (is authority, creates it); dedicated server
- **AC-2**: `StealthGame.Crowd.PhaseGating`
  - Given: 20 fixed-seed rounds of 50 NPCs, command log captured
  - When: rounds start and `OnRoundActive` fires
  - Then: command log empty before it; all 50 first commands in one server step; pooled first pause/leg lengths pass KS at p > 0.01
  - Edge cases: never reseed a failing seed; a late-spawning NPC before Active stays Dormant
- **AC-3**: `StealthGame.Crowd.DecisionSpread`
  - Given: 50 NPCs at 60 fps and `CrowdDecisionPerSec` 4
  - When: decisions per frame are counted over 60 s
  - Then: maximum is at most `ceil(50 / 15) + 2` = 6
  - Edge cases: N = 70 at 30 fps; a hitch frame
- **AC-4**: `StealthGame.Crowd.LedgerInvariant`
  - Given: a scripted sequence of kills, takeovers and releases; a not-ready navmesh case
  - When: events apply
  - Then: the live count equals `NpcCount - V - B + Rel` after each event; kind-blind; `NavNotReady` asserted and pawns Dormant when not ready
  - Edge cases: release of a never-taken-over pawn; kill and takeover in the same tick

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdDirectorCoreTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-002-crowd-draws-and-streams.md`, `production/epics/crowd-npc-behaviour/story-003-crowd-tuning-keys-and-profile-data.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-007-path-source-and-start-budget.md`, `production/epics/crowd-npc-behaviour/story-008-spatial-hash-and-perception-filter.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`, `production/epics/crowd-npc-behaviour/story-017-density-and-start-placement.md`, `production/epics/crowd-npc-behaviour/story-026-ambient-schedule-and-emergence.md`, `production/epics/crowd-npc-behaviour/story-032-spike-60-character-server-cost.md`, `production/epics/crowd-npc-behaviour/story-036-no-crowd-ui.md`, `production/epics/crowd-npc-behaviour/story-039-retire-prototype-crowd.md`
