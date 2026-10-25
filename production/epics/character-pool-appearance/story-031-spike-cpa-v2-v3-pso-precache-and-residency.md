# Story 031: Spike CPA-V2/V3: PSO precache and residency after warm-up in a cooked client

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-005 = All pool assets resident and precached before leaving `Preparing`; no first-draw load.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation (conditional gates CPA-V2 and CPA-V3, risks R2, R3); ADR-0022 for the 3060-class budget
**ADR Decision Summary**: A hidden per-round warm-up rig of one skeletal-mesh component per pool row makes PSOs compile and textures stream before the reveal; with `r.PSOPrecache.ProxyCreationStrategy = 1` (delay) a pawn proxy is not created while its PSOs compile, so after warm-up zero proxies may be delayed between reveal and reveal + 10 s (V2), zero pool-asset package loads occur in that window and every pool texture is `IsFullyStreamedIn` at reveal (V3).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012, ADR-0022)
**Engine Notes**: Post-cutoff APIs: `UPrimitiveComponent::PrecachePSOs` / `IsPSOPrecaching()`, `r.PSOPrecache.ProxyCreationStrategy` (read-only; 1 delay is the default, 2 draws a fallback material and is forbidden), `HasPendingInitOrStreaming()` (the `bool` overload is deprecated in 5.8). **Component PSO precache is off whenever `GIsEditor`**: PIE results are not evidence (E11); the cooked half is DEFERRED to a cooked Development client (3060-class reference PC). The prototype citizens may serve as throwaway test meshes.

**Why this story**: It builds a minimal prototype rig only as the vehicle for the measurement; the production subsystem (story 032) is Blocked until this reports, and the load test (story 035) too. Read ADR-0012 Verification Required CPA-V2, CPA-V3, §5 (client warm-up), risks R2 and R3.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Clients and the listen host warm all `A` pool rows before Session's acknowledgement; `r.PSOPrecache.ProxyCreationStrategy` is 1 (ADR-0012)
- Forbidden: Never warm only the active `K` rows; PIE is not evidence for PSO precache (manifest Engine API Constraints)
- Guardrail: warm-up cost on 3060-class hardware measured and recorded; the load bound is Session's `TravelLoadTimeoutSec`

---

## Acceptance Criteria

*From ADR-0012 Verification Required CPA-V2 and CPA-V3:*

- [ ] **AC-1** (V2) GIVEN a cooked Development client after a prototype warm-up rig reports complete, THEN zero pawn proxies are delayed by `r.PSOPrecache.ProxyCreationStrategy = 1` between reveal and reveal + 10 s (`IsPSOPrecaching()` on every pawn mesh at first registration, first run on a cold shader cache); PIE results are recorded separately and are not evidence.
- [ ] **AC-2** (V3) GIVEN the same run, THEN zero pool-asset package loads occur between reveal and reveal + 10 s and every pool texture is `IsFullyStreamedIn` at reveal (with the streaming pool sized for the pool).
- [ ] **AC-3** GIVEN the 3060-class reference PC, THEN the report records the rig's cost and, if any delay or load is non-zero, the escalation (a bundled PSO cache via `FShaderPipelineCache`), as `production/qa/spikes/<date>_CPA-V2-V3.md`.

---

## Implementation Notes

- Build the rig exactly as ADR-0012 §5 describes (template each component from the pawn class default's mesh component, material first then mesh, `SetForceMipLevelsToBeResident`, `PrestreamTextures`) so the measurement transfers to story 032; do not ship it.
- Count delays and loads with hooks, not by eye; keep the logs under `production/qa/logs/`.
- Start from the spike S2/S14 build and cooked-build recipe if one exists; otherwise record the cook command used.

**Prototype disposition**: the prototype's `SK_Citizen02-05` and `BP_CrowdCharacter` are test content for the rig only (throwaway).

---

## Out of Scope

- Story 032: the production warm-up subsystem; Story 035: the acceptance test on the reveal; Story 050: T5 frame budget

---

## Files

- `production/qa/spikes/<date>_CPA-V2-V3.md`
- `Source/StealthTestHarness/Private/CharacterPool/CpaV2V3Probe.cpp`
- `production/qa/logs/<date>_CPA-V2-V3-*.txt`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; DEFERRED to a cooked Development client); spike report.*

**Test: V2 PSO precache on reveal**
- Given: a cooked Development client after the warm-up rig reports complete.
- When: the reveal happens and 10 s pass.
- Then: zero pawn proxies are delayed by `ProxyCreationStrategy = 1`; PIE results are recorded separately and do not count.
- Edge cases: first run on a cold shader cache.

**Test: V3 no loads after reveal**
- Given: the same run.
- When: counting package loads.
- Then: zero pool-asset package loads between reveal and reveal + 10 s; every pool texture `IsFullyStreamedIn` at reveal.
- Edge cases: a texture streaming pool too small for the pool.

**Test: rig cost and escalation**
- Given: the 3060-class reference PC.
- When: the rig runs.
- Then: the rig cost is recorded; if any delay is non-zero the escalation (bundled PSO cache) is recorded.
- Edge cases: none.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `production/qa/spikes/<date>_CPA-V2-V3.md` (documented spike report with logs from a cooked Development client)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-025-pool-subsystem-residency.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: `production/epics/character-pool-appearance/story-032-pool-warmup-subsystem-and-rig.md`, `production/epics/character-pool-appearance/story-035-no-asset-load-on-reveal.md`
