# Story 032: UCharacterPoolWarmup: warm every pool row before the acknowledgement

> **Epic**: Character Pool & Appearance
> **Status**: Blocked — BLOCKED: spikes CPA-V2 and CPA-V3 (story 031) have not reported; whether the warm-up rig makes zero pawn proxies delayed and zero pool loads after reveal in a cooked client is unproven (escalation if not: a bundled PSO cache).
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-005 = All pool assets resident and precached before leaving `Preparing`; no first-draw load.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation (ADR-0022 for the budget)
**ADR Decision Summary**: `UCharacterPoolWarmup : UWorldSubsystem` (clients and listen host, never a dedicated server) runs once per round world behind the loading screen: one local non-replicated `ACharacterPoolWarmupRig` with one `USkeletalMeshComponent` per pool row (all `A` rows, never only the active `K`), templated from the pawn class default's mesh component; it completes when every component has a scene proxy, is not PSO-precaching, has no pending init or streaming, every texture is fully streamed in and at least 2 frames rendered; then it destroys the rig and fires `OnPoolWarm`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012, ADR-0022)
**Engine Notes**: `SetForceMipLevelsToBeResident(WarmupHoldSec)` and `PrestreamTextures(WarmupHoldSec, true)` exist (E4); `WarmupHoldSec = TravelLoadTimeoutSec + RevealPawnWaitMaxSec + 10 s` is derived from existing Session and Round keys, not a new tuning key. `r.PSOPrecache.ProxyCreationStrategy` is read-only and must stay 1. PIE: warm-up completes on the residency conditions alone (component precache is off under `GIsEditor`).

**Why this story**: CP-18 makes a pop a tell, so every pool entry is warmed (a subset-only warm-up would itself reveal the active `K`). **AC-3 revised per QL-STORY-READY** to say what "fails" means. Read GDD CP-18, ADR-0012 §5, risk R3.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Clients and the listen host warm all `A` pool rows before Session's acknowledgement; `r.PSOPrecache.ProxyCreationStrategy` is 1 and asserted at startup (ADR-0012)
- Forbidden: Never warm only the active `K` rows; never strategy 2 (a fallback material is a default look) (ADR-0012)
- Guardrail: warm-up shows as a longer load, never a pop; cost measured in story 031

---

## Acceptance Criteria

*From ADR-0012 §5 and GDD CP-18; AC-3 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN a client or listen host (and, in a second case, a dedicated server), THEN `UCharacterPoolWarmup` spawns one local non-replicated rig on clients and none on the dedicated server; it holds `A` components (never `K`: a `K < A` fixture, `A = 24`, `K = 12`, counts `A`), each templated from the pawn class default's mesh component with `SetMaterial` before the mesh.
- [ ] **AC-2** GIVEN a rig under test control, THEN warm-up completes only when every component has a `SceneProxy`, is not `IsPSOPrecaching()`, is not `HasPendingInitOrStreaming()`, every texture is `IsFullyStreamedIn()` and at least 2 frames rendered with all proxies present; then the rig is destroyed and `OnPoolWarm` fires; `WarmupHoldSec` equals `TravelLoadTimeoutSec + RevealPawnWaitMaxSec + 10` with injected values and no registry key named `WarmupHoldSec` exists.
- [ ] **AC-3** GIVEN `r.PSOPrecache.ProxyCreationStrategy` set to 1, then 2, THEN 1 passes and 2 makes startup log `Error_CPA_PSOStrategyMismatch`, warm-up report failure (`OnPoolWarm` is not fired and the Session acknowledgement is withheld) and nothing crash (author to confirm the log text with Session's owner).

---

## Implementation Notes

- Key Interfaces: `bool IsPoolWarm() const; FSimpleMulticastDelegate OnPoolWarm;` (ADR-0005's acknowledgement builder subscribes, story 033).
- One component stays pending in a test to cover the timeout path (Session's `TravelLoadTimeoutSec` bounds the whole load; CPA adds no timer).
- A pawn that would still be PSO-delayed after warm-up is a defect logged as `PoolNotResident` (story 035).

**Prototype disposition**: none (the prototype loaded on first draw).

---

## Out of Scope

- Story 031: the spike; Story 033: acknowledgement wiring; Story 035: the reveal-time load test

---

## Files

- `Source/StealthGameplay/Public/View/CharacterPool/CharacterPoolWarmup.h`
- `Source/StealthGameplay/Private/View/CharacterPool/CharacterPoolWarmup.cpp`
- `Source/StealthGameplay/Private/View/CharacterPool/CharacterPoolWarmupRig.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/PoolWarmupTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-3 revised with the qa-lead's proposed text; not re-run (unattended; decision logged).*

**Test: rig composition**
- Given: a client or listen host, a dedicated server in a second case, and a `K < A` fixture.
- When: warm-up starts.
- Then: one non-replicated rig on clients, none on the dedicated server; `A` components (never `K`), templated from the pawn default's mesh component, `SetMaterial` before the mesh.
- Edge cases: `A = 24`, `K = 12`.

**Test: completion condition**
- Given: a rig under test control.
- When: conditions are made true one at a time.
- Then: warm-up completes only when every component has a `SceneProxy`, `!IsPSOPrecaching()`, `!HasPendingInitOrStreaming()`, every texture `IsFullyStreamedIn()` and 2 frames rendered with all proxies; then rig destroyed and `OnPoolWarm` fires; `WarmupHoldSec` equals the derived value with injected inputs.
- Edge cases: one component stays pending (the timeout path).

**Test: PSO strategy guard**
- Given: the CVar set to 1, then 2.
- When: startup runs.
- Then: 1 passes; 2 logs the error, withholds `OnPoolWarm` and the acknowledgement, and does not crash.
- Edge cases: the CVar is unset.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/CharacterPool/PoolWarmupTest.cpp` — automation tests `StealthGame.CharacterPool.Net.WarmupRigComposition`, `...Net.WarmupCompletion`, `...Net.PsoStrategyGuard` (OR documented playtest)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-031-spike-cpa-v2-v3-pso-precache-and-residency.md`, `production/epics/character-pool-appearance/story-025-pool-subsystem-residency.md`
- Unlocks: `production/epics/character-pool-appearance/story-033-poolhash-in-session-ack.md`, `production/epics/character-pool-appearance/story-035-no-asset-load-on-reveal.md`
