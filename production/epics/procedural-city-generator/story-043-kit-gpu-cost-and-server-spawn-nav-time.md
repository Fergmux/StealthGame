# Story 043: Kit GPU cost and server spawn plus nav time

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: spike R1 (ADR-0022: kit Lumen + Nanite at 1,000 / 3,000 / 5,000 runtime ISM instances <= 5.0 ms GPU on a 3060-class PC at 1080p) and spike R4 (server kit spawn + collision + navmesh <= 3.0 s on the listen host) have not reported, and no 3060-class capture exists (all figures so far come from an RTX 5090 desktop, development evidence only). The R1 and R4 reports (protocol, reference hardware, synthetic-kit numbers) are the prerequisite; this story repeats them at generator scale on real generated seeds.
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-004`, `TR-citygen-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. TR-004 includes the 5,000-instance Nanite budget; TR-006 the 3 s server spawn + nav.)*

**ADR Governing Implementation**: ADR-0022: Rendering configuration and performance budgets (ADR-0014 for the server side)
**ADR Decision Summary**: Kit Lumen scene, GI, reflections and Nanite visibility/raster/shading <= 5.0 ms GPU at <= 5,000 runtime instances (R1, ADR-0022 table); server kit spawn + collision + navmesh <= 3.0 s and steady AI + nav <= 1.5 ms (ADR-0014, R4). The reference preset is RTX 3060-class, 1080p, `sg.*` at 3.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0022 overall; Lumen, Nanite and VSM at 5,000 runtime instances are unmeasured on the target)
**Engine Notes**: The R1 protocol hides the kit to attribute GPU time: the same scene and camera with the kit visible versus hidden (state how it is hidden so Lumen scene setup is not changed beyond the kit). Warm up before the 300 measured frames. A failed R1 is handled by a superseding ADR, not by editing ADR-0022 (the pre-decided fallbacks in ADR-0022 section 5 are ordered and global).

**Control Manifest Rules (this layer)**:
- Required: ADR-0022 budgets at the reference preset; until a 3060-class capture exists the figures stay labelled ASSUMED and the criterion is NOT ASSESSED, not passed — source: M1 quality gate, ADR-0022
- Forbidden: Never raise `CityGenInstanceCapCount` in code; the cap is data re-set from the report — source: ADR-0004
- Guardrail: kit GPU <= 5.0 ms; server spawn + nav <= 3.0 s; steady AI + nav <= 1.5 ms

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G33, R-G9), scoped to this story:*

- [ ] **AC1** (AC-G33 GPU, `Perf.KitGpu`, provisional): **GIVEN** a golden seed at 1,000, 3,000 and 5,000 instances (R1 protocol) on a 3060-class PC at 1080p **THEN** at each count the mean GPU time over 300 frames with the kit minus the same scene with the kit hidden is <= 5.0 ms (Lumen + Nanite).
- [ ] **AC2** (AC-G33 server, `Perf.ServerBuild`, provisional): **GIVEN** a listen host with 10 players and 50 NPCs **THEN** server spawn + nav build, from spawn start to `HandedOff`, is <= 3.0 s at p95 over 20 seeds, and game-thread AI + nav p95 is <= 1.5 ms over 60 s of `Active`.

---

## Implementation Notes

*Derived from ADR-0022 R1 and ADR-0014 R4, and the QL-STORY-READY review:*

AC1: a Gauntlet or editor-automation capture (`StealthGame.CityGen.MP.KitGpu`, name the R1 protocol file): golden seed truncated or scaled to 1,000 / 3,000 / 5,000 instances; warm up; 300 frames visible versus hidden; compare means; 5.0 passes, 5.01 fails. AC2: `StealthGame.CityGen.MP.ServerBuild` on a listen host with 10 players and 50 NPCs; name the stat scopes counted as "AI + nav" in the test; the `NavBuilding` state is inside the timed span; p95 nearest-rank (the 19th of 20). The results feed `CityGenInstanceCapCount` (story 027 AC3) and the collision strategy decision (merged run boxes if R4 fails, part of `GeneratorKitVersion`), and release stories 041 and 042 (R1/R4 reports). Write one generator-scale report to `production/qa/spikes/` (confirming or amending the R1 and R4 reports) with measured numbers, the machine inventory and the verdict (PASS, or FAIL plus the taken ADR-0022 fallback, or an amendment request). The qa-lead notes that where the hardware is not the reference spec the result reads NOT ASSESSED.

Prototype replaced: none (the prototype's 30 s FPS chart summary was development evidence on a 5090-class desktop).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-041-client-build-time-and-first-frame-completeness.md`, `story-042-attempt-time-budget-and-t-cand-measurement.md`: client build and attempt-level time.
- `story-040-stack-and-slab-visual-equivalence.md`: visual equivalence.
- `story-027-instance-entry-and-shape-budget-audit.md`: the counts audit.
- The ADR-0022 captures R2, R3, RC-1, RC-2: characters and whole-frame, owned by other epics (RC-1's input is recorded by story 046).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/CityGen/KitGpuCaptureMpTest.cpp`
- `Source/StealthTestHarness/Private/CityGen/ServerBuildCaptureMpTest.cpp`
- `production/qa/spikes/[date]_citygen-r1-r4-generator-scale.md` (report)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC2 revised and ADEQUATE; both DEFERRED until the reference hardware exists).*

- **AC1** `StealthGame.CityGen.MP.KitGpu`
  - Given: the reference PC at 1080p with a golden seed at 1,000, 3,000 and 5,000 instances per the R1 protocol.
  - When: GPU time is measured over 300 frames with the kit visible, then with it hidden, in the same scene and camera.
  - Then: mean(with kit) minus mean(hidden) <= 5.0 ms at each of the three counts.
  - Edge cases: warm up before the 300 frames. Hiding must not change the Lumen scene setup beyond the kit, and the R1 protocol says how it is hidden. 5.0 passes and 5.01 fails.
- **AC2** `StealthGame.CityGen.MP.ServerBuild`
  - Given: a listen host with 10 players and 50 NPCs, 20 seeds.
  - When: spawn start to `HandedOff` is timed per seed, and game-thread AI + nav is sampled over 60 s of Active.
  - Then: spawn + nav build p95 <= 3.0 s (nearest-rank, the 19th of 20). AI + nav p95 <= 1.5 ms over the 60 s.
  - Edge cases: name the stat scopes counted as "AI + nav" in the test. The NavBuilding state is inside the timed span.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two Gauntlet captures and the committed report in `production/qa/spikes/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-026-server-kit-spawn-from-manifest.md`, `story-027-instance-entry-and-shape-budget-audit.md`, `story-028-navigation-integration-water-stairs-client-none.md`, `story-040-stack-and-slab-visual-equivalence.md`; spike reports: R1 (ADR-0022), R4 (LSC story 011); the 3060-class PC availability
- Unlocks: `story-041-client-build-time-and-first-frame-completeness.md`, `story-042-attempt-time-budget-and-t-cand-measurement.md`, `story-046-ten-connection-gauntlet-integration-pass.md`
