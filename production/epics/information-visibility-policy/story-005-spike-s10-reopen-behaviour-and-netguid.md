# Story 005: Spike S10, a re-opened pawn is a new actor that presents correctly from replicated state

> **Epic**: Information Visibility Policy
> **Status**: In Review — spike reported FAIL (production/qa/spikes/2026-10-25_S10.md); awaiting the lead's sign-off of the retained frame sequence (production/qa/evidence/s10-reopen-evidence.md) and the superseding ADR-0003 decision
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

When a culled pawn is re-opened (un-cull, or a death-write re-open) the client creates a new actor instance. Whether the NetGUID is reused, whether the AnimBP initialises correctly from `ActionPose` and the death fast-forwards from `DeathServerTime` without a visible pop, whether client registries tolerate the destroy and re-create, and whether a cull followed by an un-cull before the client acknowledges the close (two channels for one NetGUID in flight) is safe are all unverified (E10 in-flight race, E11). A re-open path that differs by pawn kind, or a NetGUID allocation pattern that tracks events, would be a Pillar-1 detector (L4, L32). The measured `T_reopen` also feeds IVP F4's `T_uncull`.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.6-5.8)
**ADR Decision Summary**: A re-open creates a new client actor; client registries must tolerate a relevancy destroy and re-create; the new actor initialises its presentation from replicated state (`FActionState`, `FDeathState`, `HeadLook`, `AppearanceId`) through one idempotent `RefreshPresentation(EstimatedServerNow)` path, never from arrival; nothing gameplay-visible distinguishes a cull from a real destroy.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E10 (in-flight cull/un-cull race) [U, S10]; E11 (new client actor per re-open [V]; NetGUID reuse and AnimBP initialisation [U, S10]); E15 (client throttle) [V]. This spike owns S10.

**Control Manifest Rules (this layer)**:
- Required: **Client code tolerates a relevancy destroy and initialises a re-created actor from replicated state only** — source: ADR-0003
- Forbidden: **Never re-open a culled pawn in a strike's outcome slot; a killed-concealed victim re-opens only at the death write, with `bDiedConcealed = true`** — source: ADR-0003
- Forbidden: **Never call `ForceNetRelevant()` or `ForceActorRelevantNextUpdate()` on a pawn** — source: ADR-0003
- Guardrail: **Replication**: fixed 15 Hz pawns inside the 36 KB/s worst case — source: ADR-0003

---

## Acceptance Criteria

*From the story's spike definition (ADR-0003 Verification Required, S10), with the QL-STORY-READY clarification:*

- [ ] GIVEN the S9 cull rig, WHEN a culled pawn re-opens (un-cull in the same server step, and a death-write re-open of a culled victim), including a cull followed by an un-cull before the client acknowledges the close, THEN the following are measured and recorded: new client actor created each time; whether the NetGUID is reused; the AnimBP initialises from the replicated ActionPose and the death fast-forwards from DeathServerTime with no visible pop; client registries (significance, audio, decals, pawn lists) tolerate the relevancy destroy and re-create; the in-flight cull/un-cull race (two channels for one NetGUID) is safe; NetGUID allocation gaps are logged against events (L32); T_reopen is measured against the F4 value of 33 ms; the verdict is written to production/qa/spikes/. Clarification (story-level, QL-STORY-READY): "no visible pop" means at the first rendered frame after re-open the root is within 1 cm of the server-predicted position and the animation pose time is within one frame (16.7 ms) of the time derived from FActionState.StartServerTime/DeathServerTime; client registries report zero faults, ensures and stale entries over 100 cull/re-open cycles; a frame sequence is retained and lead-signed in production/qa/evidence/.

---

## Implementation Notes

*Derived from ADR-0003 sections 5.6-5.8 and Risks:*

- Builds on story-004's throwaway cull rig. Three re-open cases: un-cull in the same server step (as `HideExit`/`OnConcealmentBroken` will do), a death-write re-open of a culled victim, and a cull followed by an un-cull before the client acknowledges the close.
- "The in-flight race is safe" is defined for this story as zero crashes, ensures and faults (author to confirm with the reviewer before the run).
- Presentation after a re-open must come from the idempotent `RefreshPresentation(EstimatedServerNow)` path; the rig may stub it but must read only replicated state.
- Record NetGUID allocation against events (controllers, PlayerStates and pawns share one counter; L32) and measure `T_reopen` (channel open plus client actor creation) against the F4 provisional 33 ms; story-006 consumes it.
- Visual evidence: a frame-stepped sequence in `production/qa/evidence/` with lead sign-off (Visual evidence is retained even though the story is Integration).
- A failure is handled by a superseding ADR, not by editing ADR-0003.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-004: the cull rig and channel-close verification (S9)
- story-006: recomputing F3/F4 from measured values (S12)
- story-029: the production un-cull lead bound (AC-IVP-24)

---

## Files

*Changed by /dev-story (2026-10-25).*

- `production/qa/spikes/2026-10-25_S10.md` (the verdict)
- `production/qa/evidence/s10-reopen-evidence.md` and `production/qa/evidence/story-005-spike-s10-reopen-behaviour-and-netguid/frames/`
- `Source/StealthTestHarness/Private/Scenarios/StealthHarnessScenarioS10.cpp` (throwaway rig on S9's rule shape, with an optional race guard)
- `Source/StealthTestHarness/Private/Spikes/Visibility/StealthSpikeActionStateComponent.{h,cpp}` (presentation stub, frame capture)
- `tools/ci/mp/analyze_s10.py`, `tools/ci/mp/run-mp.ps1` (render client fix)

---

## QA Test Cases

- **AC-1**: S10 re-open behaviour and NetGUID
  - Given: the S9 cull rig.
  - When: a culled pawn re-opens by an un-cull in the same server step, by a death-write re-open of a culled victim, and by a cull followed by an un-cull before the client acknowledges the close.
  - Then: recorded: a new client actor each time, whether the NetGUID is reused, NetGUID allocation gaps (L32), T_reopen against 33 ms; at the first rendered frame after re-open the root is within 1 cm of the server-predicted position and the pose time is within 16.7 ms of the time derived from StartServerTime or DeathServerTime; client registries (significance, audio, decals, pawn lists) report 0 faults, ensures and stale entries over 100 cull/re-open cycles; a lead-signed frame sequence is retained in production/qa/evidence/.
  - Edge cases: "the in-flight race is safe" means 0 crashes, ensures and faults (author to confirm); verdict goes to production/qa/spikes/. (Tier: T3; visual evidence retained.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a written spike report `production/qa/spikes/<date>_S10.md` plus the retained, lead-signed frame sequence in `production/qa/evidence/`.

**Status**: [x] Report and frame sequence written; lead sign-off pending

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`
- Unlocks: `production/epics/information-visibility-policy/story-006-spike-s12-cull-delay-vs-measured-l99.md`, `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`, `production/epics/information-visibility-policy/story-027-concealment-local-hide-at-swap.md`, `production/epics/information-visibility-policy/story-028-blind-spot-attack-on-culled.md`, `production/epics/information-visibility-policy/story-029-uncull-lead-bound.md`, `production/epics/information-visibility-policy/story-044-raw-bunch-schema-parity.md`, `production/epics/information-visibility-policy/story-052-strike-id-oracle-parity.md`

## Completion Notes
**Status**: In Review (not closed) — 2026-10-25
**Verdict**: FAIL (measured and recorded as the criterion asks): new client actor on every re-open and the same NetGUID every time (no allocation from re-opens), root exact, 0 crashes/ensures, T_reopen ≈ 11.5 ms median / 20 ms p99 (≤ 33 ms) — but (1) the in-flight race leaves ~1 % (StealthLeak1) / ~2 % (StealthLeak2) of race re-opens without the pawn on the client; (2) every re-open's first rendered frame shows an un-evaluated (collapsed) pose; (3) pose and death times are ~150 ms late with the engine's default server clock. A candidate race guard (re-open held until the close is acknowledged) removed every race failure. Needed design changes are listed in the report for a superseding ADR to ADR-0003.
**Criteria**: the measurement criterion is met except the **lead sign-off** of the retained frame sequence (pending) — that is why the story is not Complete.
**Decisions**: "safe" for the race = 0 crashes, ensures and faults, where a client left without a relevant pawn is a fault; registries absent on the prototype reported as a CONCERN; case B every 20th cycle so the crowd lasts the run.
**Deviations**: none in scope.
**Test Evidence**: `production/qa/evidence/story-005-spike-s10-reopen-behaviour-and-netguid/` (analysis JSON of 5 runs, launch lines, frames).
**Run result**: OBSERVED — `production/qa/evidence/story-005-spike-s10-reopen-behaviour-and-netguid/frames/` (render client C3, culled → re-open frames; the first re-open frame shows the pop).
**Reviewed by**: focused self-review (unattended run): fault counter made thread-safe (log devices can be called from any thread); relevancy hook removed in OnFinish; no forced relevancy or forced update used.
**Dependents**: stay Blocked (S10 FAIL; a superseding ADR decides); their status notes cite this report.
