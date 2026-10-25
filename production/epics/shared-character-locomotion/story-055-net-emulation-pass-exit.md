# Story 055: Net-emulation pass over traversal, the look path and pawn replication (epic exit criterion)

> **Epic**: Shared Character & Locomotion
> **Status**: Blocked — BLOCKED: the net-emulation / traversal spike (story 045) has not reported and the traversal story 049 it depends on is itself Blocked; this is the epic's cross-cutting exit criterion and the final gate of Exit B
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (Acceptance Criteria AC-SCL-21, AC-SCL-25, AC-SCL-39; Open Question OQ2); epic Build Order and Gates ("This epic owns the cross-cutting net-emulation pass ... as an exit criterion")
**Requirement**: `TR-scl-004`, `TR-scl-005`, `TR-scl-009`, `TR-scl-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (Verification Required: traversal under net emulation), with ADR-0003 and ADR-0008 (net-emulation profiles)
**ADR Decision Summary**: The pawn's network behaviour is accepted only when traversal, the look path and pawn replication hold together under the project's net-emulation profile; the CMC prediction, smoothing and starvation settings are one system, so they are verified in one pass.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses the ADR-0008 harness profiles (`PktLag`, `PktLoss`, fixed loss seed). Run on the legacy driver with the pinned `AGameNetworkManager` and `NetClientTicksPerSecond` values.

**Control Manifest Rules (this layer)**:
- Required: tests run under the replication system actually shipped (legacy, Iris off).
- Forbidden: never relax a profile to make a test pass; never mark an INCONCLUSIVE result as Pass.
- Guardrail: pawn replication at most 36 KB/s per client (story 034).

---

## Acceptance Criteria

*Derived from the epic's exit criterion (about 4 days, net-emulation pass at 100 ms / 1 % loss over traversal, look path and pawn replication) and reconciled with AC-SCL-21's own profile:*

- [ ] GIVEN a 3-client round, WHEN the net-emulation pass runs, THEN AC-SCL-25 (`TraversalUnderEmulation`) and AC-SCL-39 (`HeadLookObserverInterpolation`) pass at PktLag 100 / PktLoss 1 %, AC-SCL-21 (`NoObserverSnap`) passes at its own PktLag 50 / PktLoss 1 % profile and is additionally recorded, non-gating, at PktLag 100 / PktLoss 1 %, and an INCONCLUSIVE result from AC-SCL-21 counts as not Pass.

*QA clarification (create-stories 2026-10-25, not a GDD change):* AC-SCL-21 is specified at PktLag 50, so running it at 100 ms would be a stricter test than the GDD states; the epic's exit-criterion text ("one profile ... together") should be amended by its owner to match this restatement.

---

## Implementation Notes

*Derived from the epic's Build Order and Gates:*

- This story orchestrates stories 049, 023 and 035 under one harness run and writes the pass report to `production/qa/spikes/` (or `production/qa/evidence/`); it adds no game code. Include the host frame-time capture of I-SCL-10 in the same run.
- **Exit gating**: Exit A of the epic (locomotion, look path, animation, net parity, factory and possession lifecycle) can close without this story; Exit B (traversal replication) cannot.
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The individual tests (stories 023, 035, 049) and the spike (045).

---

## Files

- `Source/StealthTestHarness/Private/Locomotion/NetEmulationPassExitTest.cpp` (planned orchestrator)
- `production/qa/spikes/` pass report (new)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; BLOCKED on spike 045 and story 049). Test `StealthGame.Locomotion.PIE.NetEmulationPassExit.ProfileRun`.*

Test: net-emulation pass exit criterion
  Given: a 3-client round
  When: the pass orchestrates stories 049, 023 and 035 under the harness
  Then: AC-SCL-25 and AC-SCL-39 pass at PktLag 100 / PktLoss 1 %; AC-SCL-21 passes at its own PktLag 50 / PktLoss 1 % profile and is additionally recorded, non-gating, at 100 / 1; an INCONCLUSIVE result from AC-SCL-21 counts as not Pass
  Edge cases: the epic's exit-criterion wording ("one profile ... together") should be amended by its owner to match this restatement

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/NetEmulationPassExitTest.cpp` — must exist and pass; the pass report in `production/qa/spikes/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-045-net-emulation-traversal-spike.md`, `story-049-traversal-under-emulation.md`, `story-023-head-look-observer-interpolation.md`, `story-035-no-observer-snap.md`
- Unlocks: epic completion (`production/epics/shared-character-locomotion/EPIC.md` Definition of Done)
