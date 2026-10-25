# Story 026: Spike CPA-V1: null-mesh pawn takes its look from the initial OnRep

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-004 = Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation (conditional gate CPA-V1, risk R1)
**ADR Decision Summary**: A pawn class default with no skeletal mesh draws nothing until `ApplyAppearanceVisuals` sets the material then the mesh from the initial RepNotify (possibly before `BeginPlay`); engine source says the AnimBP instance initialises when the mesh is first set (E6). CPA-V1 confirms it end to end; on failure the fallback is a class-default mesh hidden through one visibility arbiter shared with ADR-0015 and ADR-0017, recorded in a superseding ADR, not an edit of ADR-0012.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: `USkeletalMeshComponent::SetSkeletalMeshAsset` (not the deprecated `USkinnedMeshComponent::SetSkeletalMesh`); the reparented Game Animation Sample Blueprint's construction script runs in `FinishSpawning` after the apply and must not set a mesh. The spike vehicle is the existing sample pawn (`CBP_SandboxCharacter`) or `AStealthPawn` if shared-character-locomotion story 001 has landed. The prototype citizens `SK_Citizen02-05` may serve as throwaway test meshes only.

**Why this story**: ADR-0012 risk R1 asks that CPA-V1 "runs in the first Shared Pawn story, before any appearance story". It gates story 027 and 034 (Blocked until it reports). Read ADR-0012 `## Engine Compatibility` (Verification Required, E6) and `### 4. AppearanceId on the pawn`. Do not run `read_graph_dsl` or `find_node_types` on the sample Blueprints in a session that later starts PIE (spike S14 incident; dirties `CBP_SandboxCharacter`).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The pawn class default and its Blueprint child have no skeletal mesh assigned; `ApplyAppearanceVisuals` is the only function that sets a pawn's mesh or material (ADR-0012)
- Forbidden: Never set a mesh in the pawn Blueprint or its construction script; never resave sample assets (`production/never-resave-assets.md`)
- Guardrail: client `ApplyAppearance` about 2 to 5 ms one-off at round start [U] (measure and record)

---

## Acceptance Criteria

*From ADR-0012 Verification Required CPA-V1 and risk R1:*

- [ ] **AC-1** GIVEN a pawn whose mesh component starts with no skeletal mesh, WHEN `SetMaterial(0, MIC)` then `SetSkeletalMeshAsset` run from the initial `OnRep` (including runs where it precedes `BeginPlay`), THEN in 300 of 300 remote-client spawns the AnimBP instance initialises and motion matching evaluates on the first rendered frame, and a per-frame hook records zero frames drawn with another mesh.
- [ ] **AC-2** GIVEN `LINT-CPA-NoSetMeshInBP`, THEN it finds no `SetSkeletalMesh*` or `SetSkinnedAsset*` node in the pawn Blueprint or its construction script, and a seeded node fails the lint.
- [ ] **AC-3** GIVEN the run, THEN `production/qa/spikes/<date>_CPA-V1.md` states PASS, FAIL or INCONCLUSIVE; on FAIL it records the fallback (class-default mesh hidden through the single visibility arbiter shared with ADR-0015 and ADR-0017) for a superseding ADR.

---

## Implementation Notes

- Measure at the remote client; the listen host receives no RepNotify for its own writes (E9), so also record that the authority calls the apply function directly.
- Capture frames with a per-frame hook on the mesh component (mesh asset pointer at each render) rather than screenshots; keep two screenshots for the report.
- Record the first-frame cost of `ApplyAppearanceVisuals` for the guardrail.

**Prototype disposition**: the prototype set the mesh from a Blueprint default and tinted with a MID (`AStealthCharacter::ApplyAppearance`); this spike tests the replacement mechanism; no prototype code is promoted.

---

## Out of Scope

- Story 027: the production property and apply path; Story 034: the 2-client first-replication tests

---

## Files

- `production/qa/spikes/<date>_CPA-V1.md`
- `Source/StealthTestHarness/Private/CharacterPool/CpaV1Probe.cpp` (or a temporary prototype probe under `Source/StealthGame/`, removed after the report)
- `tools/lint/` (the `LINT-CPA-NoSetMeshInBP` script, registered in story 042)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead); spike report.*

**Test: null-mesh initial OnRep**
- Given: a pawn whose mesh component starts with no mesh, spawned on a remote client 300 times.
- When: `SetMaterial(0, MIC)` then `SetSkeletalMeshAsset` run from the initial OnRep.
- Then: in 300 of 300 spawns the AnimBP instance initialises and motion matching evaluates on the first rendered frame; the per-frame hook records zero frames drawn with another mesh.
- Edge cases: OnRep before BeginPlay is forced in some runs.

**Test: lint**
- Given: `LINT-CPA-NoSetMeshInBP`.
- When: it scans the pawn Blueprint and construction script.
- Then: no `SetSkeletalMesh*` or `SetSkinnedAsset*` node.
- Edge cases: a seeded node fails the lint.

**Test: report**
- Given: the run.
- When: the report is written.
- Then: `production/qa/spikes/<date>_CPA-V1.md` states PASS or FAIL; on FAIL it records the fallback for a superseding ADR and does not edit ADR-0012.
- Edge cases: a partial run is recorded INCONCLUSIVE.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `production/qa/spikes/<date>_CPA-V1.md` (documented playtest/spike report) OR automation test `StealthGame.CharacterPool.PIE.NullMeshFirstFrame`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: `production/epics/character-pool-appearance/story-027-appearance-id-property-and-apply-path.md`, `production/epics/character-pool-appearance/story-034-id-before-first-replication-no-default-frame.md`
