# Story 043: Transport classifier AUC stays at chance for the player and AI paths

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: spike story-007 (S13) has not reported; needs AStealthPawn and the pawn command interface for replaying recorded command streams (shared-character-locomotion, input-interaction-framework)
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Identical replicated properties are not enough for Pillar 1: the transport texture of a pawn (update cadence, bunch sizes, movement correction counts, speed and heading statistics, pitch distribution) must not let a non-owning client tell a player-driven pawn from an AI-driven one. This story is the final judge of that claim: the same recorded command streams are replayed through the player path and the AI path over at least 60 matches, a classifier (F2) is scored, and the result must stay at chance with a bootstrap bound. It is a release-gate input (ADR-0008 section 8, release / milestone row) and it re-proves, on production classes, what spike story-007 (S13) only measures on the prototype.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (section 1 T3/T4, section 3, section 9); ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 1, 3, Validation Criteria)
**ADR Decision Summary**: ADR-0008 places the transport-classifier data capture in the T3 multi-process tier (cooked Development listen host plus client processes, harness scenario, JSON result) and the AUC analysis in T4 (`tools/ci/stats/`, nightly or milestone, fixed checked-in seed sets and pre-registered thresholds); every statistical gate ships a negative control that the suite must detect.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0008: Gauntlet launches one cooked Development listen host plus N clients [V ships in 5.8; U topology, Verification 3]; packet simulation through `-PktEmulationProfile=<Name>` [V]. ADR-0003 E13 (CMC simulated-proxy cadence and corrections identical for PC- and AI-possessed pawns under the fixed frequency) is [U] and owned by S13 (story-007); E14 (random per-actor update offset, identical treatment for every pawn) [V]. The capture mechanism is whatever story-001 (S1) records as an ADR-0008 amendment.

**Control Manifest Rules (this layer)**:
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Required: **AI issues every command through `UPawnCommandComponent` entry points and moves only through the CMC input and control-rotation consumers a human uses; nothing writes a pawn's velocity or rotation directly for AI** — source: ADR-0001
- Forbidden: **Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail** — source: ADR-0008
- Guardrail: **Replication**: pawns at a fixed 15 Hz within 36 KB/s worst case (60 pawns) — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-03, scoped to this story:*

- [ ] **AC-IVP-03** [Stat][Net][S13] `StealthGame.Visibility.TransportClassifierAuc`: GIVEN ≥ 60 matches (n1 ≥ 300, n2 ≥ 3000 pawn-rounds, F2) replaying the same recorded command streams through the player and AI paths, split by match, WHEN the F2 classifier is scored with a 2,000-resample cluster bootstrap, THEN `AUC_hat ≤ 0.55` and `AUC_hat + 1.645·SE_boot ≤ 0.58`. Negative control: an injected player-only cadence offset fails.

---

## Implementation Notes

*Derived from ADR-0008 sections 1, 3, 9 and ADR-0003 sections 1, 3 / Validation Criteria:*

- Two parts, as ADR-0008 section 1 splits them. **T3 capture**: a harness scenario under `Source/StealthTestHarness/Private/Scenarios/` launched by a Gauntlet node in `Build/Scripts/` records, on a non-owning third client under `StealthLeak1` or `StealthLeak2`, the F2 feature set for every pawn-round while the same recorded command stream drives a pawn through the PlayerController path and through the AI path (AI through `UPawnCommandComponent` entry points only, ADR-0001). Each result JSON carries `replicationSystem` and `netProfile` (ADR-0008 Key Interfaces) and the run asserts the legacy driver. **T4 analysis**: `tools/ci/stats/` scores the F2 classifier split by match with a 2,000-resample cluster bootstrap and emits `AUC_hat`, `SE_boot` and the bootstrap seeds as observations; the gate script decides pass or fail (ADR-0008 section 8, lints and analysis scripts emit observations only).
- Pre-register before collecting data: ADR-0008 section 9 says "pre-registered priors and thresholds for T4 analyses are checked in beside the tools before the data is collected". Check in the thresholds (0.55, 0.58, 1.645, 2,000 resamples), the minimum counts (60 matches, n1 >= 300, n2 >= 3000) and the fixed seed set first. Record the bootstrap seeds in the result (the QA edge case).
- Both thresholds must hold; the `AUC_hat + 1.645*SE_boot <= 0.58` bound is the stricter one when the sample is small.
- Negative control (ADR-0008 section 3): a harness-only injected player-only cadence offset (compiled only in `StealthTestHarness`, never in Shipping) must drive the classifier above the threshold; the suite passes only if the control is detected. Run it in the same suite as the real run.
- Spike gate: story-007 (S13) must have reported. If S13 recorded differences and a cause for ADR-0003 section 1/3 follow-up, the follow-up must be resolved (or its ADR amendment accepted) before this story can pass for real; this story proves the production result, S13 only measures the prototype.
- The capture mechanism comes from story-001 (S1 records it as an ADR-0008 amendment); it must pass its own negative control (`COND_None` `APawn::PlayerState` detected) before any result here is trusted (ADR-0008 section 6).
- Prototype: reference only `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*` and `StealthMeasureSpike.cpp`; nothing is promoted. Replaces: none (new).
- Cross-epic needs: shared-character-locomotion epic (`AStealthPawn` with the ADR-0003 section 1 net config, `UStealthPawnFactory`), input-interaction-framework epic (the pawn command interface that lets a recorded command stream be replayed through both paths), crowd-npc-behaviour epic (AI command source), tuning-data-config Exit A (module skeleton, harness, CI).
- Unblocks when: story-007 reports, `AStealthPawn` exists and a recorded command stream can be replayed through both controller paths.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-007: the prototype-pawn S13 measurement (this story re-proves it on production classes)
- story-044: raw-bunch schema parity (F1, structural leak count)
- story-042: timing parity of rejection replies, `ActionPose` delay and kill-confirmation delay
- story-001: choosing the capture mechanism
- crowd-npc-behaviour and shared-character-locomotion epics: the AI command path and the pawn net config themselves

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `Source/StealthTestHarness/Private/Scenarios/` (T3 capture scenario for `StealthGame.Visibility.MP.TransportClassifierAuc`)
- `Build/Scripts/` (Gauntlet node for the scenario)
- `tools/ci/stats/` (F2 classifier, cluster bootstrap, pre-registration file with thresholds and seed set, gate script)
- `production/qa/` (checked-in result JSON and the run record)

---

## QA Test Cases

- **AC-1**: AC-IVP-03 `StealthGame.Visibility.MP.TransportClassifierAuc` (T3 capture plus T4 analysis)
  - Given: at least 60 matches (n1 >= 300, n2 >= 3000 pawn-rounds), the same recorded command streams replayed through the player and AI paths, split by match.
  - When: the F2 classifier is scored with a 2,000-resample cluster bootstrap.
  - Then: `AUC_hat <= 0.55` and `AUC_hat + 1.645*SE_boot <= 0.58`; the negative control (an injected player-only cadence offset) fails the inner check and is reported as detected.
  - Edge cases: both thresholds must hold; record the bootstrap seeds.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: T3 capture scenario under `Source/StealthTestHarness/` plus the T4 analysis result and run record (a documented measurement), OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`, `production/epics/information-visibility-policy/story-007-spike-s13-cmc-cadence-pc-vs-ai-parity.md`; shared-character-locomotion epic (`AStealthPawn` and the ADR-0003 section 1 net config); input-interaction-framework epic (pawn command interface for replaying recorded command streams); tuning-data-config Exit A (harness, Gauntlet and CI)
- Unlocks: None
