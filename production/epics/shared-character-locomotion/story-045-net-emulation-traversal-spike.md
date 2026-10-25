# Story 045: Net-emulation and traversal spike report (unblocks Exit B and starvation)

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (Open Questions OQ2 net-emulation spike, OQ3 traversal spike; R7, R15; AC-SCL-12, 13, 23, 24, 25 are tagged [SPIKE])
**Requirement**: `TR-scl-004`, `TR-scl-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (Engine Compatibility "Verification Required": traversal spike, ST1; sections 6 and 7)
**ADR Decision Summary**: Traversal is server-authoritative and provisional pending this spike: `TraversalState {Seq, TypeId, PointId}` written inside `TraversalStateWrite`, warp targets derived locally from `PointId`, one proxy root-motion policy for every pawn (`RepRootMotion` or follow replicated movement, E12). Input starvation brakes through `ForcePositionUpdate` and the spike (ST1) decides between the engine's 4 Hz forced-update stepping and a per-frame zero-input update.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: This is the conditioning spike the epic's Build Order and Gates name for Exit B: "traversal under net emulation at 100 ms / 1 % loss". No report exists yet (`production/qa/spikes/` holds only the S1/S2/S14 report). The traversal baseline is the sample's CMC-based traversal (`TryTraversalAction`, `Traversal_ServerImplementation`, replicated `TraversalResult`, `MOVE_Flying` with root motion and motion warping), already in the prototype and unverified under latency (first-playable known issue 4). `UMotionWarpingComponent` is Beta in 5.8.4. Engine forced-position-update values are read from the `AGameNetworkManager` class default object.

**Control Manifest Rules (this layer)**:
- Required: traversal starts only from a server-validated queue slot and writes `TraversalState` inside the `TraversalStateWrite` forced-update scope; one proxy root-motion policy applies to every pawn; starvation zeroes acceleration in `ForcePositionUpdate`.
- Forbidden: never use the sample's `Traversal_ServerImplementation`/`TraversalResult` replication or a multicast for traversal (the spike measures the sample to justify replacing it, it does not adopt it); never replay excess late moves as a catch-up.
- Guardrail: `TraversalState` 7 bytes per change.

---

## Acceptance Criteria

*Derived from ADR-0010 Verification Required and GDD OQ2/OQ3 (the GDD has no acceptance criterion for the spike itself; its [SPIKE] ACs 12, 13, 23, 24, 25 wait on this report):*

- [ ] GIVEN a 3-client PIE session at PktLag 100 / PktLoss 1 %, WHEN the spike is run on the prototype's sample traversal and CMC and its report is written to `production/qa/spikes/`, THEN the report contains: the starvation forced-update cadence in Hz (ST1); the traversal end-position error distribution under the profile with n ≥ 100 per kind (vault, climb) against the 64 cm (`PawnSmoothSnapThresholdCm / 4`) bound; a binary decision on proxy root motion (`RepRootMotion` versus follow replicated movement, E12); the sample's movement-mode name during traversal; and the final traversal heights (vault 0.8 m, climb 1.6 m, optional 1.2 m hurdle), each answered with a number or a decision.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the spike has no pass or fail; the ST1 fallback decision is made by the AC-23/24 results (stories 050/051), not by this spike.

---

## Implementation Notes

*Derived from ADR-0010 sections 6, 7 and its Validation gates:*

- Run on the prototype (`Source/StealthGame`, the sample's traversal course to the north of the prototype map, known issue 9) with the ADR-0008 net-emulation profile; for ST1 measure how often a starved pawn's server position steps with the engine's `MAXCLIENTUPDATEINTERVAL` 0.25 s, and test the per-frame zero-input alternative.
- Record, in the same report: whether `PktLag` applies to one direction or both under the harness profile (needed by story 050's RTT definition) and the observed `RepRootMotion` behaviour of the sample montage on proxies.
- Update `production/qa/spikes/` only; this story changes no game code. Afterwards the Blocked stories (047, 048, 049, 050, 051, 053, 055) can be re-checked with `/story-readiness`, and ADR-0010's conditional gate is updated by its owner.
- **Prototype replaced**: none (the spike reads the prototype's sample traversal; promotion happens in stories 047-049).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Stories 047-049: the production traversal. Stories 050-051: production starvation behaviour. Story 055: the epic's exit-criterion pass.

---

## Files

- `production/qa/spikes/` net-emulation and traversal spike report (new)
- `production/qa/evidence/` captures referenced by the report (new)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). No automated test; the evidence is the report file.*

Test: spike report completeness (ADR-0010 Verification Required, GDD OQ2/OQ3)
  Given: a 3-client PIE setup at PktLag 100 / PktLoss 1 % on the prototype's sample traversal and CMC
  When: the spike runs and the report is written to `production/qa/spikes/`
  Then: the report contains (1) the ST1 starvation forced-update cadence in Hz, (2) the traversal end-position error distribution with n ≥ 100 per kind against the 64 cm bound, (3) a binary E12 decision (`RepRootMotion` versus follow replicated movement), (4) the sample's movement-mode name during traversal, (5) the final traversal heights (vault 0.8 m, climb 1.6 m, optional 1.2 m hurdle); each answered with a number or a decision
  Edge cases: the ST1 fallback decision (4 Hz stepping versus per-frame zero-input update) is made by the AC-23/24 results (stories 050/051), not by this spike; also record whether `PktLag` applies to one direction or both

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the spike report file with the five answered items in `production/qa/spikes/`, with its capture logs in `production/qa/evidence/`; no automated test (a measurement)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/EPIC.md` (Exit A: net-emulation profiles in the harness, ADR-0008); the prototype build
- Unlocks: `production/epics/shared-character-locomotion/story-047-traversal-eligibility-matrix.md`, `story-048-traversal-start-parity.md`, `story-049-traversal-under-emulation.md`, `story-050-loss-burst-decel-no-snap.md`, `story-051-loss-recovery-no-catch-up.md`, `story-053-npc-tell-baseline-non-zero.md`, `story-055-net-emulation-pass-exit.md`
