# Story 007: Spike S13, CMC cadence and corrections match for PC- and AI-possessed pawns

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Even with identical replicated properties, the transport texture of a pawn (update cadence, bunch sizes, movement correction counts, pitch and heading distributions) can differ between a pawn driven through a client's `ServerMove` and one ticked locally by an AIController. The F2 classifier gate (AC-IVP-03) is the final judge, but ADR-0003 conditions its fixed-frequency, exponential-smoothing configuration on S13: the engine claim E13 ("CMC simulated-proxy cadence and corrections are identical for PC- and AI-possessed pawns under the fixed frequency") is unverified. This spike measures it early, on the prototype, so a failure is found before the production pawn is built.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-003` (UE default overrides of R5: pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator); also `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 1, 3)
**ADR Decision Summary**: Every pawn uses the same net configuration (`NetUpdateFrequency = MinNetUpdateFrequency = 15`, adaptive frequency off, `NetPriority 3.0`, `DORM_Never`, `SimulatedProxy` for every non-owner) and exponential smoothing with the same tuning values; per-instance or per-kind differences are forbidden.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E6 (`net.UseAdaptiveNetUpdateFrequency` default 0) [V]; E9 (CMC `NetworkSmoothingMode`, listen-server simulated smoothing times) [V]; E13 (cadence and corrections identical for PC- and AI-possessed pawns) [U, S13]; E14 (random per-actor update offset) [V]. This spike owns S13.

**Control Manifest Rules (this layer)**:
- Required: **Pawn net config is set only from tuning values, through the 5.5+ setters, identically for every pawn** — source: ADR-0003
- Required: **Every simulated proxy uses exponential smoothing with the same tuning values**, including the listen-server simulated smoothing times — source: ADR-0003
- Forbidden: **Never give a pawn dormancy, a cull distance, `bOnlyRelevantToOwner`, `bNetUseOwnerRelevancy`, a per-instance priority or team relevancy** — source: ADR-0003
- Guardrail: **Replication**: pawns at a fixed 15 Hz within 36 KB/s worst case — source: ADR-0003

---

## Acceptance Criteria

*From the story's spike definition (ADR-0003 Verification Required, S13), with the QL-STORY-READY clarification:*

- [ ] GIVEN one pawn driven by a PlayerController on a test client and one pawn driven by an AIController through the same command path fed the same recorded command stream, adaptive net update frequency off, both Role SimulatedProxy on a third client, WHEN the third client records inter-arrival times, bunch sizes, speed/acceleration variance, heading-change rate, pitch distribution and correction (snap) counts for 10 minutes under StealthLeak1, THEN the two pawns' distributions agree within the F2 feature tolerance used by AC-IVP-03 (KS D <= 0.1 per feature, equal correction counts within sampling error) or the differences and their cause are recorded for ADR-0003 section 1/3 follow-up in production/qa/spikes/. Clarification (story-level, QL-STORY-READY): "equal correction counts" means a conditional binomial (equal-rate Poisson) test on the two counts over the 10-minute run, p > 0.05.

---

## Implementation Notes

*Derived from ADR-0003 sections 1, 3, E13 and IVP F2:*

- Runs on the prototype pawn first (`StealthNpcController` exists as the AI path); ADR-0010's own S13/H1 re-run applies to `AStealthPawn` later. Behaviour is held constant by replaying one recorded human command stream (`Move`, `Look`, actions) once through a PlayerController on a test client and once through an AIController, so only transport differences remain (F2 method).
- Record per-feature distributions with the third client's capture (mechanism from story-001, or the single-process property-level capture until it lands) and compare with a two-sample KS test per feature (threshold D <= 0.1); correction counts use the conditional binomial test (p > 0.05).
- If both correction counts are zero the test is degenerate (p = 1): report that case explicitly. The criterion allows an "or record" outcome, so the spike cannot fail outright; the recorded differences and cause feed ADR-0003 follow-up (a superseding ADR, not an edit).
- Net profile `StealthLeak1` (PktLag 100 ms, PktLoss 1 %) (ADR-0008 section 5).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-043: the full F2 transport classifier AUC gate (AC-IVP-03)
- story-018: the engine-default-overrides acceptance test (AC-IVP-09)
- shared-character-locomotion epic: the production pawn net configuration (ADR-0010)

---

## Files

- `production/qa/spikes/<date>_S13.md` (the verdict)
- Throwaway measurement code (prototype or harness)

---

## QA Test Cases

- **AC-1**: S13 CMC cadence, PC against AI
  - Given: one PlayerController pawn and one AIController pawn, same recorded command stream, adaptive net update off, both SimulatedProxy on a third client, StealthLeak1.
  - When: inter-arrival times, bunch sizes, speed and acceleration variance, heading-change rate, pitch distribution and correction counts are recorded for 10 minutes.
  - Then: KS D <= 0.1 per feature; a conditional binomial (equal-rate Poisson) test on the two correction counts gives p > 0.05; otherwise the differences and their cause are recorded for ADR-0003 section 1/3 follow-up.
  - Edge cases: if both counts are 0 the test is degenerate (p = 1); report that case explicitly; the criterion allows an "or record" outcome, so this spike cannot fail outright. (Tier: T3.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a written spike report `production/qa/spikes/<date>_S13.md` with the distributions and test statistics (a documented measurement).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None
- Unlocks: `production/epics/information-visibility-policy/story-018-engine-default-overrides.md`, `production/epics/information-visibility-policy/story-043-transport-classifier-auc.md`
