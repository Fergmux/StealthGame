# Story 039: All D pawns spawn in one server frame in sigma order and a Connecting participant's pawn is gated

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-011` (Single shuffled spawn list in one frame, server-secret order), `TR-round-006` (Preparing readiness sequence, spawn gate and watchdog); GDD AC 41; rules R6 P6 and "Spawn gate" (Level Socket Contract R4.8), R23 (a).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams; ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity; ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: ADR-0006: 128-bit `ServerSecret`, in-project SipHash-2-4 as `H64`, counter-mode secret streams for hidden draws, PCG32 for the public layout stream; domain registry append-only. ADR-0010: one `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator/possessor, kind-blind possession and hand-over pass, disconnect never destroys a pawn. ADR-0003: uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus client-local hide, fixed-width serialisers, and an allowlist audit of every replicated property. This story applies P6: one list of `D` entries shuffled by `GetServerSubSeed(SpawnOrder, LayoutAttempt)` (ADR-0006), every pawn created through `SpawnSharedPawn` (ADR-0010 section 3.1), and the spawn gate: no pawn is spawned for, possessed by or made relevant to a client before Session has moved that participant to `Connected` (relevancy half: ADR-0003 section 5.4).
**ADR Version**: ADR-0006 2026-10-24; ADR-0010 2026-10-24; ADR-0003 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 and ADR-0003 knowledge risk; ADR-0006 is LOW)
**Engine Notes**: Touches `UWorld::SpawnActorDeferred` + `AActor::FinishSpawning` and possession through the factory's parity wrapper (ADR-0010 E1, E5), and `APawn::IsNetRelevantFor` as the per-connection hook for the gate (ADR-0003 E1, E8). Open items touched: ADR-0010 E14 and the ADR-0002 E4 spike (a CMC pawn spawned inside `TG_PostPhysics` ticks the same frame, [V queueing, U behaviour]); Open: confirm against ADR-0002 section 8.6 whether P6 runs in Round's Preparing poll or inside the queue drain (the queue is not open in `Preparing`, so the E4 spike may not apply). ADR-0003's relevancy-side spikes (S9 explicit close at the deadline, S10 re-open, S12, S13) gate the "not relevant to its connection until `Connected`" assertion; the manifest status for this story is Ready, but Open: confirm against ADR-0003 section 5.4 and control-manifest "Conditional acceptance" whether S9 and S10 must report before that second half of the assertion is run in PIE. ADR-0010's spikes (traversal, S8, S14, H1, P1) are not touched by spawn-time possession.

**Control Manifest Rules (this layer)** (Core Layer Rules, Shared Pawn, movement and possession):
- Required: Every pawn is created by `UStealthPawnFactory::SpawnSharedPawn`: deferred spawn, `ApplyAppearance`, `BindPawn` (if a participant), `FinishSpawning`, possess, in one call stack; a bind rejection destroys the pawn before `FinishSpawning` (ADR-0010).
- Forbidden: Never create or possess a pawn with `RestartPlayer`, `SpawnDefaultPawnFor`, `SpawnDefaultPawnAtTransform`, `SpawnDefaultController` or a `SpawnActor` of a pawn class outside the factory (ADR-0010).
- Guardrail: Replication (Foundation Layer Rules): pawns at a fixed 15 Hz within 36 KB/s worst case for 60 pawns (ADR-0003); all `D` pawns replicate from the spawn frame.

**Prototype disposition**: Replaces the prototype round-start spawn in `Source/StealthGame/Private/StealthGameMode.cpp`: `RestartPlayer` at round start (the `RestartPlayer` calls in `StealthGameMode.cpp`) and NPCs through `SpawnActor` plus a separate controller (ADR-0010 context item 3; the prototype's spawn paths "must be replaced, not ported"). Production code spawns every pawn through `UStealthPawnFactory`.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 41, scoped to this story:*

- [ ] GIVEN P6, THEN all `D` pawns spawn in one server frame in sigma order, and a `Connecting` participant's pawn is unpossessed and not relevant to its connection until `Connected`.

---

## Implementation Notes

*Derived from ADR-0002 section 8.6, ADR-0010 section 3.1 and GDD R6 P6:*

- **P6 sequence.** Round calls `RequestStartPlacement(PlayersByTeam, NpcCount)` once, builds one spawn list of `D = players + NpcCount` entries, shuffles it with `GetServerSubSeed(SpawnOrder, LayoutAttempt)` (stories 036 and 037), then spawns all `D` pawns through the one factory in one frame: `UStealthPawnFactory::SpawnSharedPawn(Transform, AppearanceId, ControlKind, Participant?, ExpectedEpoch)`. NPC entries go through Crowd; player entries by deferred spawn, `BindPawn`, finish spawning, possess. Every pawn carries `Instigator == Self`. Each participant's `AssignedAppearanceId` is read from the allocator's `ParticipantToAppearance` map and copied to the Roster field; `AppearanceId` is on the pawn before it first replicates (Character Pool CP-10).
- **One frame, sigma order.** Record the spawn frame number and the order in which pawns are created; assert one frame number for all `D` and creation order equal to `sigma`. The shuffle protects the server's spawn and iteration order only; arrival order at clients is decided by net priority, which is why P7 exists (story 032).
- **Spawn gate (Level Socket Contract R4.8).** No pawn is spawned for, possessed by or made relevant to a client before Session has moved that participant to `Connected` for this round. A participant that is `Connecting` (or otherwise not `Connected`) at P6 gets its pawn spawned AI-driven and unpossessed (`ControlKind = Ai` in the factory); after `Connected` the pawn becomes relevant and possessed. A participant `Connected` from the start is not gated. The per-connection relevancy hook is `AStealthPawn::IsNetRelevantFor` driven by the Policy's cull record, which does not call `Super` (ADR-0003); this story uses it, it does not redesign it, and never calls `ForceNetRelevant()` or `ForceActorRelevantNextUpdate()` on a pawn.
- **Possession path.** Possession runs only through the factory with the parity wrapper (`FScopedPossessionParity`); nothing here calls `RestartPlayer`, `SpawnDefaultPawnFor` or `HandleStartingNewPlayer` (a no-op, ADR-0005 guideline 2).
- **A bind rejection** destroys the pawn before `FinishSpawning` and aborts `BindFailed` (story 034 owns the abort); the fully Disconnected-at-P6 pawn (bound, unpossessed, AI-driven, grace from `ActivePhaseStart`) is story 040.
- **Test shape.** A 2-client PIE session (`StealthTestHarness`, CQTest `NETWORK_TEST_CLASS`) with one participant held `Connecting` at P6; capture spawn frame and creation order on the server, and the pawn's possession and per-connection relevancy for the gated participant until `Connected`. A single-process PIE result is non-representative for travel (ADR-0005 guideline 10).
- Manifest 2026-10-25f: the per-connection spawn gate in `AStealthPawn::IsNetRelevantFor` stays closed until Session moves that connection to `Connected`, so every P6 pawn is non-relevant to a connection until then.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 036 (`production/epics/round-match-framework/story-036-spawn-order-uniform.md`) and 038 (`production/epics/round-match-framework/story-038-placement-invariant.md`): the spawn-order draw and the placement invariant (dependencies).
- Story 037: sigma independent of player flags.
- Story 040 (`production/epics/round-match-framework/story-040-disconnected-at-start-spawned.md`): a participant `Disconnected` at P6, its AI-driven bound pawn and the grace basis.
- Story 041 (`production/epics/round-match-framework/story-041-active-entry-unbound-aborts.md`): the Active-entry `BindFailed` check.
- Story 032 and 091: the P7 world-ready stage and the scheduled reveal.
- Shared Character & Locomotion internals (`SpawnSharedPawn`, `BindPawn`, possession parity), Crowd internals (NPC placement and AI control) and Concealment/cull policy internals (ADR-0003 spikes).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Start/RoundStartSpawner.h`
- `Source/StealthGameplay/Private/Round/Start/RoundStartSpawner.cpp`
- `Source/StealthTestHarness/Private/Round/SpawnOneFrameAndGateTest.cpp` (CQTest `NETWORK_TEST_CLASS(SpawnOneFrameAndGate, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.SpawnOneFrameAndGate.<Method>`; the GDD scenario name is `StealthGame.Round.SpawnOneFrameAndGate`)
- `Source/StealthGame/Private/StealthGameMode.cpp` (prototype round-start spawn with `RestartPlayer` and `SpawnActor` NPCs: retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: All D pawns spawn in one server frame in sigma order, and a Connecting participant's pawn is gated.
  - Given: a 2-client PIE session, with one participant still `Connecting` at P6.
  - When: P6 runs.
  - Then: all D pawns have the same spawn frame number and were spawned in sigma order.
  - Then: the `Connecting` participant's pawn is unpossessed and not net-relevant to its connection until `Connected`.
  - Edge cases: after `Connected` the pawn becomes relevant and possessed; a participant `Connected` from the start is not gated.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/SpawnOneFrameAndGateTest.cpp` (CQTest `NETWORK_TEST_CLASS(SpawnOneFrameAndGate, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-036-spawn-order-uniform.md`, `production/epics/round-match-framework/story-038-placement-invariant.md`
- Unlocks: `production/epics/round-match-framework/story-040-disconnected-at-start-spawned.md`, `production/epics/round-match-framework/story-041-active-entry-unbound-aborts.md`
- Cross-epic: `production/epics/shared-character-locomotion` (`UStealthPawnFactory::SpawnSharedPawn`, possession parity; stories not yet created), `production/epics/crowd-npc-behaviour` (NPC spawn entries and AI control; stories not yet created), `production/epics/participant-state-roster` (`BindPawn`, `Connected` state, `AssignedAppearanceId`; stories not yet created), `production/epics/level-socket-contract` (`RequestStartPlacement`; stories not yet created), `production/epics/session-travel` (`GetServerSubSeed`, `Connected` transition; stories not yet created), `production/epics/character-pool-appearance` (`ParticipantToAppearance`; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
