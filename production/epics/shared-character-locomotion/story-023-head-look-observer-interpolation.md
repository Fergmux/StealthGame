# Story 023: Observers render a continuous eased head, never 9 degree steps

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R8.5 Observer interpolation; AC-SCL-39)
**Requirement**: `TR-scl-005`, `TR-scl-010` (smoothing parity)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 3; with ADR-0001 for the one look path)
**ADR Decision Summary**: Every simulated proxy on every client runs the same F6 filter toward the latest received `HeadLook` (same rate and `LookEaseSec` for every pawn), so a 15 Hz byte stream renders as a continuous eased head. Smoothing parity: every simulated proxy uses the same mode, times and thresholds.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The observer test runs under net emulation (PktLag 100, PktLoss 1 %, fixed loss seed). It is one input to the net-emulation pass (story 055) but is independent of the traversal spike, so it is not Blocked.

**Control Manifest Rules (this layer)**:
- Required: observers run the same pure filter toward the received state for every pawn.
- Forbidden: no kind-specific smoothing path; no client-authoritative position.
- Guardrail: stays inside the pawn-side CPU lines.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-39** [N] `HeadLookObserverInterpolation`: GIVEN a 70° head sweep at 140 °/s under PktLag 100 / PktLoss 1 % (fixed seed), WHEN an observer renders at 60 fps, THEN ≥ 95 % of frames show a new head yaw, the per-frame step is ≤ 140/60 + 0.55 = 2.9° with no step > 5°, and the NPC and player traces have equivalent step distributions (KS D ≤ 0.15, n ≥ 200).

*QA clarification (create-stories 2026-10-25, not a GDD change):* one 70° sweep is about 30 frames, so n ≥ 200 samples per path needs at least 7 repeated sweeps per path; the "≥ 95 % of frames" is measured over the sweep frames only.

---

## Implementation Notes

*Derived from GDD R8.5 and ADR-0003 section 3:*

- Apply the story-021 filter on proxies as a client-local per-frame step toward the latest received yaw and pitch, with the same Ω and ease for every pawn. The test uses the harness's net-emulation profile and records the rendered head yaw on a third client at 60 fps.
- Same trace comparison for NPC and player sweeps (two-sample KS). A scripted human sweep and an AI focus sweep produce the same server-side `HeadLook`, so any difference found is an observer-side bug.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 021: server-side filter. Story 022: pitch write. Story 055: the combined net-emulation pass.

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp` (proxy head-look step)
- `Source/StealthTestHarness/Private/Locomotion/HeadLookObserverInterpolationTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.PIE.HeadLookObserverInterpolation.NpcVsPlayer`.*

Test: observer head-look interpolation (AC-SCL-39)
  Given: at least 7 repeated 70° head sweeps at 140 °/s per path under PktLag 100 / PktLoss 1 % (fixed seed), for an NPC and a player
  When: an observer renders at 60 fps
  Then: ≥ 95 % of sweep frames show a new head yaw; the per-frame step is ≤ 2.9° with no step > 5°; the NPC and player traces have equivalent step distributions (KS D ≤ 0.15, n ≥ 200)
  Edge cases: 7 sweeps of about 30 frames give n ≥ 200

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/HeadLookObserverInterpolationTest.cpp` — must exist and pass (multi-client PIE under net emulation); retained screenshot or short capture of the observed sweep in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-021-look-envelope-700-deg-spin.md`, `story-022-remote-view-pitch-clamped.md`
- Unlocks: `production/epics/shared-character-locomotion/story-055-net-emulation-pass-exit.md`
