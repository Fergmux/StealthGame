# Story 010: Spike S-CON-1, choose the owner-side warp mechanism that meets AC-CON-31

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The server executes `Hide` at a Round queue drain, later than the owner's press, so a server-only root-motion source reaches the owner as a correction (`ClientAdjustRootMotionSourcePosition` [V]) and would hitch every hide. Which of the candidate mechanisms avoids the hitch, and whether an owner-started source with the same `InstanceName` is duplicated then corrected (E4 [U]), are unverified engine claims owned by this spike. Story 012 is gated on its report.

**GDD**: `design/gdd/concealment-bushes.md` (CON-R4a, Game Feel, AC-CON-31)
**Requirement**: `TR-con-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§4; Verification Required S-CON-1)
**ADR Decision Summary**: The warp is `FRootMotionSource_MoveToForce` to the fixed anchor, applied on the server for every pawn. Owner smoothness is chosen by this spike in the order of trial: (a1) the owner starts the identical source (same `InstanceName`, parameters and start server time) when its `FActionState` arrives, blending the residual correction, with no new message; then (b) server `bIgnoreClientMovementErrorChecksAndCorrection` plus owner `bClientIgnoreMovementCorrections` for the transition plus one RTT, then one normal correction; an owner-only start message (a2) is tried last because it is a new owner wire element and needs an IVP allowlist row. `bServerAcceptClientAuthoritativePosition` stays banned.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E4: `FRootMotionSource_MoveToForce` exists [V]; `bIgnoreClientMovementErrorChecksAndCorrection` only stops the server generating corrections; `bClientIgnoreMovementCorrections` stops the client applying them; an owner-started source that does not match the server's (same `InstanceName`) would be duplicated, then corrected [U, this spike].

**Control Manifest Rules (this layer)**:
- Required: Spike verdicts are recorded in `production/qa/spikes/`; a failure is handled by a superseding ADR, never by editing an Accepted ADR — source: Manifest "Conditional acceptance"
- Forbidden: Never use `bServerAcceptClientAuthoritativePosition` — source: ADR-0003 §3, ADR-0017
- Guardrail: Press-to-motion ≤ 50 ms at 60 fps; no owner correction > 5 cm during the transition

---

## Acceptance Criteria

*From the story's spike definition (ADR-0017 Verification Required S-CON-1, GDD AC-CON-31):*

- [ ] **AC1** (spike on throwaway code, AC-CON-31): **GIVEN** a throwaway `FRootMotionSource_MoveToForce` to a fixed anchor and each candidate (a1: owner starts the identical source when its `FActionState` arrives; b: server `bIgnoreClientMovementErrorChecksAndCorrection` plus owner `bClientIgnoreMovementCorrections` for the transition plus one RTT, then one correction; a2: owner-only start message) under `LeakNetProfile` (100 ms, 2 % loss) with 100 hides and 100 exits **THEN** at least one mechanism gives press-to-motion ≤ 50 ms at 60 fps and no owner frame shows a positional correction > 5 cm during the transition; the duplicate-source behaviour (E4) is observed and recorded. "Motion" means the first owner frame with at least 1 cm displacement after the input timestamp. If no mechanism passes, the verdict is "none" and a superseding ADR follows.
- [ ] **AC2** (verdict): **GIVEN** the spike finished **THEN** `production/qa/spikes/<date>_S-CON-1.md` names the chosen mechanism in ADR-0017 §4's order of trial (a1, then b, then a2 last), contains the measurements, and leaves ADR-0017 unedited (a failure is handled by a superseding ADR).

---

## Implementation Notes

*Derived from ADR-0017 Decision §4 and the Validation Criteria:*

Throwaway code on the prototype map `L_Prototype` is acceptable, but it must follow the rule shape production will take: the same server source for NPCs and players. Test each candidate across at least 100 hides and 100 exits under `LeakNetProfile` (ADR-0008 §5); packet loss is not seedable, so assert bounds. Record the duplicate-source behaviour of an owner-started source with a matching `InstanceName` (duplicated then corrected, or merged). The result is recorded by amending ADR-0017 §4 through the normal ADR route after the spike, not by this story.

**Prototype**: reuse only the map and pawn; the prototype's concealment code is not touched.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-server-warp-to-anchor-and-exit-motion.md`: the production server warp.
- `story-012-owner-warp-no-snap.md`: the production owner-side mechanism.

---

## Files

- `production/qa/spikes/<date>_S-CON-1.md` (the verdict)
- Throwaway spike code (prototype or harness): `Source/StealthGame/Private/Spikes/` or `Source/StealthTestHarness/Private/Scenarios/`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: OwnerWarpSpike
  - Given: throwaway code for candidates a1, b and a2 under `LeakNetProfile`, with 100 hides and 100 exits each.
  - When: press-to-motion and per-frame owner correction are measured.
  - Then: at least one mechanism gives press-to-motion of 50 ms or less at 60 fps and no owner correction above 5 cm; the E4 duplicate-source behaviour is recorded.
  - Edge cases: "motion" is the first owner frame with at least 1 cm displacement after the input timestamp; if none passes the verdict is "none" and a superseding ADR follows.
- **AC2**: Verdict
  - Given: the spike finished.
  - When: `production/qa/spikes/<date>_S-CON-1.md` is reviewed.
  - Then: it names the mechanism, the order of trial (a1, then b, then a2 last), the measurements, and leaves ADR-0017 unedited.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a written spike report `production/qa/spikes/<date>_S-CON-1.md` with capture artefacts (a documented measurement)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (uses the SCL pawn and the prototype map)
- Unlocks: `story-012-owner-warp-no-snap.md`
