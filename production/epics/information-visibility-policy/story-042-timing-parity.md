# Story 042: Rejection reply, ActionPose and kill-confirmation delays are statistically identical for player and NPC targets

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs IIF rejection replies, ActionPose forced update U and kill-confirmation delay (input-interaction-framework, assassination-defence-reversal, shared-character-locomotion); nightly statistical harness.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Even with identical data, a player target and an NPC target could differ in how long the game takes to answer: a command rejection reply, the delay before the target's `ActionPose` update, and the delay before the kill is confirmed. A measurable difference in any of the three is a timing channel that identifies players. This story is the statistical gate that compares, per delay class, at least 1236 samples from player-target and NPC-target runs on effect size, variance ratio and a two-sample Kolmogorov-Smirnov distance, with a negative control that injects a 3 ms player-only shift in the `ActionPose` delay and must fail. It protects Pillar 1 at the timing level, where the byte-level gates (story-044) cannot see.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test automation and leak parity (sections 1 T4, 3, 8 and 9); ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 1 and 2, parity config and forced-update policy)
**ADR Decision Summary**: T4 statistical gates run offline analysis over T3 captures with fixed, checked-in seed sets and pre-registered thresholds, each with a negative control the suite detects. The pawn net config is identical for every pawn and `ForceNetUpdate` on a pawn forwards only inside `FStealthForcedNetUpdate` with an allowed reason, at most once per pawn per frame, so `ActionPose` update timing is the same for every pawn kind.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The captures are T3 multi-process runs under `StealthLeak1` (cooked Development, Gauntlet); packet loss is not seedable, so bounds are asserted. The `ActionPose` delay `U` depends on the forced-update policy of ADR-0003 section 2 (E4, E8, E16 in ADR-0003) and on S13's cadence result (story-007). `WITH_DEV_AUTOMATION_TESTS` in cooked Development builds is [U] (ADR-0008 Verification 7).

**Control Manifest Rules (this layer)**:
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Required: **`ForceNetUpdate()` on a pawn forwards only inside `FStealthForcedNetUpdate` with an allowed reason, at most once per pawn per frame**; the unscoped possession call is dropped and the 15 Hz cadence carries the change — source: ADR-0003, ADR-0001
- Forbidden: **Never use wall-clock time as a test pass/fail predicate** (only as a failure ceiling); never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Forbidden: **Never give a pawn dormancy, a cull distance, `bOnlyRelevantToOwner`, `bNetUseOwnerRelevancy`, a per-instance priority or team relevancy; never change net config per instance, kind or on possession** — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-26), scoped to this story:*

- [ ] **AC-IVP-26** [Stat][Net] `StealthGame.Visibility.TimingParity`: GIVEN ≥ 1236 samples per class for rejection reply delay, `ActionPose` delay U and kill-confirmation delay, WHEN player-target and NPC-target samples are compared, THEN `|d| ≤ 0.1`, variance ratio in [0.8, 1.25], `KS D ≤ 0.1`. Negative control: a 3 ms player-only shift in U fails. (F7, L9)

---

## Implementation Notes

*Derived from ADR-0008 sections 1, 3, 8, 9 and ADR-0003 sections 1-2, with IVP F7/L9:*

- Three delay classes, each with at least 1236 samples per target kind (player-target and NPC-target): (1) rejection reply delay (input-interaction-framework replies for a rejected command; the same reply code, timing class and budget for every target); (2) the `ActionPose` update delay `U` (the forced update on the action write, shared-character-locomotion/ADR-0003 section 2); (3) the kill-confirmation delay (assassination-defence-reversal). For each class compute effect size `d`, the variance ratio and the two-sample KS distance `D`; all three thresholds must hold for all three classes: `|d| <= 0.1`, variance ratio within [0.8, 1.25], `KS D <= 0.1`.
- Pinned command streams: both kinds replay the same recorded command streams (the ADR-0008 / ADR-0016 replay mechanism), so the only difference between runs is the target kind. Seeds are fixed and checked in; thresholds and sample sizes are pre-registered in `tools/ci/stats/` before data collection (ADR-0008 section 9). Never reseed to pass.
- Negative control (criterion, ADR-0008 section 3): a harness-only injection of a 3 ms player-only shift in `U` must fail the check; if it passes, the harness is broken and the gate is not valid. Consider the same injection for the other two classes as additional controls, recorded as such.
- Capture and analysis: T3 scenario captures timestamps in server time on the capturing side; the analysis tool in `tools/ci/stats/` emits observations (d, variance ratio, KS D, sample counts) and the gate script decides pass or fail (ADR-0008). Pass/fail never depends on wall-clock time, which is only a failure ceiling.
- Under `StealthLeak1` the samples include network jitter on both kinds; the measurement is taken on the third (non-owning) client so the player-target and NPC-target samples are subject to the same path. Spike S13 (story-007) supplies the cadence tolerance basis; story-043 (transport classifier) is the neighbouring statistical gate.
- Wiring: nightly and release/milestone gates (ADR-0008 section 8). Record the active replication system in every result file.
- Prototype: Replaces: nothing. Promotes: nothing. Reference only: `Source/StealthGame/Private/Spikes/StealthMeasureSpike.cpp` for the measurement pattern.
- Cross-epic: needs input-interaction-framework's rejection replies and its pawn command interface for replaying recorded streams; assassination-defence-reversal's kill-confirmation delay; shared-character-locomotion's `AStealthPawn` and the `ActionPose` forced update `U`; tuning-data-config Exit A (harness, T3 and T4 folders).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-043: transport classifier AUC (F2, cadence and bunch-size features)
- story-044: raw bunch schema parity (F1)
- story-052: strike-id oracle parity (reply code and timing class per id class)
- story-007: spike S13, CMC cadence PC against AI parity
- input-interaction-framework, assassination-defence-reversal and shared-character-locomotion epics: the replies, kill confirmation and `ActionPose` update themselves

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthTestHarness/Private/Scenarios/TimingParityScenario.cpp` (T3 capture, `StealthGame.Visibility.MP.TimingParity`) and its Gauntlet node in `Build/Scripts/`
- `tools/ci/stats/` pre-registration file (sample sizes, thresholds, seeds) and the analysis script (observations only)
- the TimingParity run record under `production/qa/`

*Note: the `StealthTestHarness` module folder is created by tuning-data-config Exit A (it does not exist yet).*

---

## QA Test Cases

- **AC-1**: Timing parity (AC-IVP-26)
  - Given: >= 1236 samples per class for rejection reply delay, `ActionPose` delay `U` and kill-confirmation delay, from player-target and NPC-target runs.
  - When: the two classes are compared.
  - Then: `|d| <= 0.1`; variance ratio in [0.8, 1.25]; `KS D <= 0.1`; negative control: a 3 ms player-only shift in `U` must fail or the harness is broken.
  - Edge cases: all three thresholds for each of the three delay types; pinned command streams.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T3 scenario under `Source/StealthTestHarness/` with the T4 analysis run record OR a documented playtest or spike report (the pre-registration file and the run record under `production/qa/`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: input-interaction-framework epic (rejection replies and the pawn command interface for replay); assassination-defence-reversal epic (kill-confirmation delay); shared-character-locomotion epic (`AStealthPawn` and the `ActionPose` forced update `U`); tuning-data-config Exit A (harness, T3 and T4 folders)
- Unlocks: None
