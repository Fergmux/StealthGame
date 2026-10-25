# Story 003: Crowd tuning keys, Baseline and Tell Matrix rates, and asset references as data

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Config/Data
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-010 = Baseline and Tell Matrix rates as data with calibration counters.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0004 (tuning pipeline, validator, frozen snapshot, hash per round, Crowd `_Server` sub-struct, soft-ref keys)
**ADR Decision Summary**: Rates and profiles come from the frozen tuning snapshot and `UCrowdMotionProfileData`, referenced by the registered key `CrowdMotionProfileAsset`; the two StateTrees by `CrowdFullStateTreeAsset` / `CrowdRestrictedStateTreeAsset`. Nothing adapts them during a round.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Soft object references and data assets are long-standing API. The three asset classes (`UCrowdMotionProfileData` and two StateTree assets) are created by stories 010 and 012; this story registers the keys and validates references, using placeholder assets if the real ones do not exist yet.

**Why this story**: GDD Open Question OQ13 requires every behaviour constant without a key to be registered as a `Crowd*` key or data-asset field before implementation, so no number hides in code (coding standard: gameplay values are data-driven). All later stories read their thresholds from here. Read the GDD Tuning Knobs section (`md-section.sh extract ... "Tuning Knobs"`, about 6 KB) and F2 in Formulas.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: rates and profiles come from the frozen tuning snapshot and `UCrowdMotionProfileData`, referenced by the registered key `CrowdMotionProfileAsset`; the two StateTrees by `CrowdFullStateTreeAsset` / `CrowdRestrictedStateTreeAsset` (ADR-0016, ADR-0004); gameplay numbers come from validated tuning, never literals, ini or CVars (Global rules)
- Forbidden: never adapt crowd rates or profiles during a round; never write a `Crowd*` tuning value after `OnTuningReady` (ADR-0016)
- Guardrail: Crowd keys are ServerOnly (not in the client tuning face); validation runs once at `OnTuningReady`

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` Tuning Knobs (CR-15), OQ13 and F2, scoped to this story:*

- [ ] Every `Crowd*` key in the GDD Tuning Knobs table plus the OQ13 unkeyed constants (R8 shares 40/30/12/7 %, glance, pause and panic durations, C1 leg length, C2 short-pause share) is registered in the Crowd ServerOnly sub-struct with default, hard range and latch (Round / Use / Action); the validator rejects an out-of-range value.
- [ ] The TDC invariants are enforced: `CrowdIdleMinSec <= CrowdIdleMaxSec <= CrowdMaxIdleSec`; `CrowdConversationMaxSec <= CrowdMaxIdleSec`; `CrowdIdleMinSec >= SessionSoftTimeoutSec + 1`.
- [ ] `CrowdMotionProfileAsset`, `CrowdFullStateTreeAsset` and `CrowdRestrictedStateTreeAsset` resolve to valid assets of the required class; a missing or wrong-class asset fails validation at `OnTuningReady`.
- [ ] The Baseline and Tell Matrix intervals (F2 defaults) are readable only from the frozen server snapshot after `OnTuningReady`, are part of the per-round tuning hash, and are absent from the client-facing tuning face.

---

## Implementation Notes

*Derived from ADR-0016 section 3, section 4 item 5, section 9 and ADR-0004 (the tuning pipeline story owns the validator mechanism):*

- Add one `FCrowdTuning` ServerOnly sub-struct in the tuning module. Defaults to carry: `NpcCount` 50 (TDC canonical row, not redefined here), `CrowdFullPacePct` 55, `CrowdYieldPct` 70, `CrowdLongPausePct` 45, `CrowdIdleMinSec`/`MaxSec` 4/16, `CrowdMaxIdleSec` 20, `CrowdRunIntervalSec` 405, `CrowdVaultIntervalSec` 1443, `CrowdClimbIntervalSec` 3409, `CrowdBraceIntervalSec` 3000, `CrowdDecoyTendIntervalSec` 13125, `CrowdBushIntervalSec` 300, `CrowdReversalIntervalSec` 395, `CrowdHeadHoldIntervalSec` 600, `CrowdTellChainGapSec` 20, `CrowdAmbientCycleIntervalSec` 80, `CrowdReplacementMinDelaySec`/`MaxDelaySec` 10/60, `CrowdDecisionPerSec` 4, `CrowdSteerPerSec` 30, `CrowdPathStartsPerFrameLimit` 2 (full table in the GDD; copy ranges and latches exactly).
- The three soft-ref keys default to `DA_CrowdMotionProfile_Default`, `ST_CrowdFull`, `ST_CrowdRestricted` under `Content/StealthGame/AI/`. Do not create `.uasset` files in this story if the owning stories have not; use the validator's "class and path" check against the registered default and mark the key's asset as pending in the evidence.
- Include the Crowd sub-struct in the per-round tuning hash (ADR-0004) so a changed rate is visible in the hash.
- Identity-tell review (GDD CR-15.8): replacement keys time a crowd-versus-player event and must apply to every victim kind; the validator needs no kind field and none may be added.
- Do not edit the GDD. If an OQ13 constant needs a different default, raise a TDC change request.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the prototype's `UStealthSettings` crowd values (`Source/StealthGame/Public/StealthSettings.h`, `Config` properties in `DefaultGame.ini`, which violate TDC CR-13) for everything Crowd reads. Not migrated: replaced.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010 (`story-010-virtual-input-layer.md`): the content of `UCrowdMotionProfileData`
- Story 012 (`story-012-statetree-schema-and-decision-grid.md`): the StateTree assets
- Story 018 (`story-018-tell-sampler-and-calibration-counters.md`): using the intervals (compensation, clocks)
- The tuning pipeline epic (`production/epics/tuning-data-config/EPIC.md`): the validator and snapshot mechanism itself

---

## Files

- `Source/StealthFoundation/Public/Server/Tuning/CrowdTuning.h` (or the Crowd ServerOnly sub-struct file named by the tuning epic)
- `Source/StealthFoundation/Private/Tuning/CrowdTuningValidation.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdTuningSmokeTest.cpp`
- `production/qa/smoke-<date>-crowd-tuning.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: key registry
  - Setup: GDD Tuning Knobs table plus the OQ13 constants list, and the Crowd ServerOnly sub-struct registry
  - Verify: each key has a default, hard range and latch; submit values just below and just above each range
  - Pass: no key missing or extra; the validator rejects out-of-range values and accepts the boundary values
- **AC-2**: invariants
  - Setup: tuning fixtures that each violate one invariant by one unit
  - Verify: run the validator on each
  - Pass: all three invariants rejected with the offending key named; equality cases (Min == Max, Idle == SoftTimeout + 1) accepted
- **AC-3**: asset references
  - Setup: three asset slots, each with a valid, a null and a wrong-class fixture
  - Verify: run `OnTuningReady` validation
  - Pass: valid assets pass; a null or wrong-class asset fails validation with the slot named
- **AC-4**: snapshot and hash
  - Setup: frozen server snapshot, the client tuning face and the per-round hash
  - Verify: read baseline and Tell Matrix intervals before and after `OnTuningReady`; mutate one interval and recompute the hash; inspect the client face
  - Pass: reads refused before the freeze, the hash changes, the keys are absent from the client face

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass in `production/qa/smoke-<date>-crowd-tuning.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (needs the tuning pipeline from `production/epics/tuning-data-config/EPIC.md` to exist when implemented)
- Unlocks: `production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md`, `production/epics/crowd-npc-behaviour/story-005-command-sinks-epoch-and-halt-filter.md`, `production/epics/crowd-npc-behaviour/story-010-virtual-input-layer.md`, `production/epics/crowd-npc-behaviour/story-018-tell-sampler-and-calibration-counters.md`
