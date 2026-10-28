# Story 001: Objectives tuning rows and invariants I-OC1 to I-OC3

> **Epic**: Objectives Chain
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-28

## Context

**GDD**: `design/gdd/objectives-chain.md` (Tuning Knobs; OC-14; F4; AC-OC-11, AC-OC-22, AC-OC-32)
**Requirement**: `TR-oc-006` (rival-signal keys and the F4 bound), `TR-oc-004` (hold duration `D` and the tend cue). The ten Objectives-owned keys have no TR of their own: they are the data rows the seven TRs read, so this story is traced to the two TRs whose invariants it enforces.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision section 8)
**Other ADRs**: ADR-0004: Tuning data pipeline and validator (the schema-row and `Check`-row mechanism this story uses; Accepted, not a governing ADR of the epic)
**ADR Decision Summary**: Objectives tuning is two sub-structs: `FObjectivesTuning_Public` (audience All, client face, for owner ring prediction: `ObjectiveInteractSec`, `TendCueDelaySec`, `FixtureInteractSideCount`) and `FObjectivesTuning_Server` (the other eight keys). Ranges, defaults and latches are exactly the GDD Tuning Knobs table. Invariants I-OC1, I-OC2 and I-OC3 are ADR-0004 `Check` rows that warn or reject and never repair a value.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0020 rules and data; plain USTRUCT rows and C++ checks, pre-cutoff)
**Engine Notes**: No engine API beyond reflection `USTRUCT` rows. Integer-millisecond conversion of the rival-signal bounds happens once at tuning freeze (ADR-0004 / ADR-0020 section 7), which story 014 consumes; this story only guarantees the float rows and their invariants.

**Control Manifest Rules (this layer)**:
- Required: Objectives tuning rows are ADR-0004 rows with default, hard range, audience and latch exactly as the GDD table; invariants are `Check` rows that never repair a value — source: ADR-0020, ADR-0004
- Forbidden: Never set a per-court hold duration; `ObjectiveInteractSec` is one chain-wide value — source: ADR-0020
- Guardrail: Tuning validation runs at freeze only; no per-frame cost — source: ADR-0004

This is the data foundation the rest of the epic reads. It adds the ten keys the Objectives GDD owns and the three cross-field invariants the GDD makes build gates. `ObjectiveInteractSec` is one of the eight canonical keys registered by the Tuning Data and Config epic (story 040); this story references that row and only adds I-OC1, which reads it. I-OC2 is the arithmetic that keeps the rival signal from confirming a sighting (F4: the effective uncertainty window `W_eff = (RivalSignalDelayMaxSec - RivalSignalDelayMinSec) + RivalSignalReleaseQuantumSec` must be at least 163 s to warn-free and at least 100 s to be accepted). I-OC3 keeps a probe's earliest informed release inside the range of Crowd's early-release decoys (Pillar 1).

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-11, AC-OC-22, AC-OC-32 invariant clauses, Tuning Knobs table), scoped to this story. QL-STORY-READY revised AC4 (mechanism) and fixed the W_eff definition in AC2:*

- [x] **AC1** (AC-OC-11 [U] `InteractSecFloor`): **GIVEN** a preset with `ObjectiveInteractSec` 1.0 **THEN** the effective value is 2.0 with a warning; **GIVEN** `CrowdAmbientTouchMaxSec` 1.8 and `ObjectiveInteractSec` 2.0 **THEN** I-OC1 (`ObjectiveInteractSec >= CrowdAmbientTouchMaxSec + 0.5`) warns.
- [x] **AC2** (AC-OC-22 [U] `RivalSignalBound`): **GIVEN** the defaults (Min 30, Max 165, Q 30) **THEN** `W_eff` = 165 s (`W_eff = (Max - Min) + Q`, GDD F4) and `LR_sig` <= 2.4 at lambda_c 0.0033; **GIVEN** Min 40, Max 150, Q 30 (`W_eff` 140) **THEN** I-OC2 warns; **GIVEN** Min 60, Max 100, Q 20 (`W_eff` 60) **THEN** I-OC2 rejects the preset (TDC CR-6 fallback applies, no value is repaired); **GIVEN** Min 120, Max 70 **THEN** I-OC2 rejects (`Max >= Min + 30` fails).
- [x] **AC3** (AC-OC-32 [U] `TendCueDelay`, invariant clauses): **GIVEN** `TendCueDelaySec` 0.5 with `CrowdDecoyEarlyReleaseMinSec` 0.8 **THEN** I-OC3 (`TendCueDelaySec + 0.25 >= CrowdDecoyEarlyReleaseMinSec` and `TendCueDelaySec <= ObjectiveInteractSec - 1.0`) rejects the preset; **GIVEN** `TendCueDelaySec` 2.5 with `ObjectiveInteractSec` 3.0 **THEN** the hard range clamps it to 2.0 with a warning and I-OC3 then passes (2.0 <= 3.0 - 1.0). The order is clamp, then invariant.
- [x] **AC4** (GDD Tuning Knobs, ADR-0020 section 8): the ten Objectives-owned keys exist as ADR-0004 rows equal to a **constant fixture copied from the GDD Tuning Knobs table** (never derived from the rows under test): public face `TendCueDelaySec`, `FixtureInteractSideCount`; server face `ObjectivesPerStageCount`, `bRivalSignalEnabled`, `RivalSignalDelayMinSec`, `RivalSignalDelayMaxSec`, `RivalSignalReleaseQuantumSec`, `ClueMatchMaxCount`, `ObjectiveStallGapDefectSec`, `ObjectiveFinalDwellDefectSec`; `ObjectiveInteractSec` resolves to the TDC row and is not redeclared; **AND** a grep of every client-module source and header for the eight server-face key names returns 0 hits, and a seeded client include of a server-face key header fails `ObjectivesIncludeLint` (story 022).

---

## Implementation Notes

*Derived from ADR-0020 Decision section 8 and the GDD Tuning Knobs table:*

Defaults, hard range, safe range, audience and latch (GDD table, re-read it at implementation time; it wins over this snapshot): `ObjectiveInteractSec` 3.0, hard 2.0-6.0, safe 2.9-4.5, All, latch Action (TDC canonical); `TendCueDelaySec` 1.0, 0.5-2.0, safe 0.8-1.5, All, Action; `FixtureInteractSideCount` 4, 1-4, safe 3-4, All, Round; `ObjectivesPerStageCount` 1, 1-3, safe 1-1, ServerOnly, Round; `bRivalSignalEnabled` true, ServerOnly, Round; `RivalSignalDelayMinSec` 30, 30-120, safe 30-60, ServerOnly, Round; `RivalSignalDelayMaxSec` 165, 70-300, safe 163-200; `RivalSignalReleaseQuantumSec` 30, 10-90, safe 28-45; `ClueMatchMaxCount` 3, 1-6, safe 2-3; `ObjectiveStallGapDefectSec` 300, 120-900, safe 240-420, latch Use; `ObjectiveFinalDwellDefectSec` 240, 90-600, safe 180-360, Use.

Invariants as `Check` rows (never repair a value, TDC CR-6): **I-OC1** `ObjectiveInteractSec >= CrowdAmbientTouchMaxSec + 0.5` warns. **I-OC2** on `W_eff`: below 163 s warns, below 100 s rejects, and `Max >= Min + 30` rejects when it fails. **I-OC3** rejects when `TendCueDelaySec + 0.25 < CrowdDecoyEarlyReleaseMinSec` or `TendCueDelaySec > ObjectiveInteractSec - 1.0`. The Crowd keys (`CrowdAmbientTouchMaxSec`, `CrowdDecoyEarlyReleaseMinSec`) are owned by Crowd: read them through the registry stub in the test and the real registry in the product; never copy their defaults. Read the I-OC1 boundary (`>=`) from the owner-invariant mechanism (TDC story 055) before writing the boundary test.

Where the sub-structs live follows the TDC schema-row convention (story 038): the public face is client-visible, the server face lives in a guarded server header so a server-only key cannot even be named in client C++ (that is what AC4's grep proves). The rows are data, so tuning values are never hardcoded in Objectives code (coding standard: gameplay values are data-driven).

Prototype replaced: `UStealthSettings::ObjectiveInteractTime` (prototype settings) is superseded by the canonical `ObjectiveInteractSec`; its removal is TDC story 095's job, not this story's.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- TDC `story-040-canonical-keys-and-faces.md`: the `ObjectiveInteractSec` row and its client face.
- `story-014-rival-signal-draw-and-grid.md`: integer-millisecond conversion and the draw.
- `story-006-fixture-claims-and-interact-target-owner.md`: reads `ObjectiveInteractSec` at execution.

---

## Files

*Files changed by /dev-story (2026-10-28; the OC 001-009 batch shares one commit, decision D41). Feature code follows the built features' layout `Source/StealthGameplay/Features/Objectives/{Public/Server,Public/View,Private}` and the tuning rows the TDC convention in StealthFoundation (decisions logged 2026-10-28).*

- `Source/StealthFoundation/Public/View/StealthTuningPublicSubstructs.h` (`FObjectivesTuning_Public`: `TendCueDelaySec`, `FixtureInteractSideCount` beside the TDC canonical `ObjectiveInteractSec`)
- `Source/StealthFoundation/Public/Server/StealthTuningSubstructs.h` (`FObjectivesTuning_Server`, the eight ServerOnly keys), `StealthServerTuning.h`, `StealthTuningData.h` (`Objectives_Server` on both faces)
- `Source/StealthFoundation/Private/Tuning/StealthTuningSchema.generated.inl` (regenerated, `Stealth.Tuning.SchemaGen` WROTE), `Source/StealthFoundation/Public/Server/StealthTuningVersion.h` (TuningSchemaVersion 8)
- `Source/StealthFoundation/Public/Server/StealthObjectivesInvariants.h`, `Source/StealthFoundation/Private/Tuning/ObjectivesInvariants.cpp` (I-OC1..I-OC3 Check rows), `Source/StealthFoundation/Private/Tuning/StealthTuningInvariants.cpp` (registered, owner Objectives; I-OC2 mixed row)
- `Source/StealthFoundation/Private/Tuning/PenaltyInvariants.cpp` (named local namespace: the unity build merged it with AssassinationInvariants.cpp once the new file shifted the unity groups)
- `Source/StealthFoundation/Private/Tests/Tuning/InvariantBoundariesTest.cpp` (the real schema now has 19 range pairs: the 18 Crowd pairs and Objectives' RivalSignalDelayMinSec / MaxSec, which TDC's I-5 checks Min <= Max beside I-OC2)
- `Source/StealthFoundation/Public/Server/Testing/StealthObjectivesTuningKnobs.h` (the GDD table as a constant fixture), `Source/StealthGameEditor/Private/Tests/Tuning/SchemaCompleteTest.cpp` (pins)
- Tests: `Source/StealthGameplay/Private/Tests/Objectives/InteractSecFloorTest.cpp`, `RivalSignalBoundTest.cpp`, `TendCueDelayInvariantTest.cpp`, `ObjectivesTuningRowsTest.cpp`, helpers `ObjectivesTuningTestSupport.h`, `ObjectivesSourceScan.h`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS, AC4 mechanism and the W_eff definition were revised into the criteria above; the specs below cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`InteractSecFloor`): `ObjectiveInteractSec` floor 2.0 and I-OC1 warning
  - Given: stub ADR-0004 tuning registry; preset A `ObjectiveInteractSec` 1.0; preset B `CrowdAmbientTouchMaxSec` 1.8 + `ObjectiveInteractSec` 2.0
  - When: the preset is frozen (invariants run)
  - Then: A effective value 2.0 with exactly one warning; B raises exactly one I-OC1 warning, value stays 2.0
  - Edge cases: 2.0 exactly gives no floor warning; 1.99 floors to 2.0 with warning; read the I-OC1 comparison boundary (>= vs >) from TDC before writing the test.
- **AC-2** (`RivalSignalBound`): I-OC2 on `W_eff` and `Max >= Min + 30`
  - Given: stub registry; presets (30/165/Q30), (40/150/Q30), (60/100/Q20), (120/70/Q30); lambda_c 0.0033
  - When: invariants run on each preset
  - Then: defaults `W_eff` 165 and `LR_sig` <= 2.4, no diagnostics; 40/150 `W_eff` 140 warns; 60/100 `W_eff` 60 rejects; 120/70 rejects; on reject no key is repaired and the TDC CR-6 fallback is reported
  - Edge cases: `W_eff` exactly 100 (accept/warn boundary); `Max = Min + 30` exactly passes; Q of 0 or negative is clamped by the hard range before the invariant sees it.
- **AC-3** (`TendCueDelayInvariant`): I-OC3 reject and range clamp
  - Given: stub registry; case 1 `TendCueDelaySec` 0.5 + `CrowdDecoyEarlyReleaseMinSec` 0.8; case 2 `TendCueDelaySec` 2.5 + `ObjectiveInteractSec` 3.0
  - When: hard ranges then invariants run
  - Then: case 1 rejects (0.5 + 0.25 = 0.75 < 0.8), no repair; case 2 clamps to 2.0 with one warning and I-OC3 passes (2.0 <= 3.0 - 1.0)
  - Edge cases: `TendCueDelaySec` 0.55 is the exact 0.80 boundary; the order clamp-then-invariant is asserted.
- **AC-4** (`ObjectivesTuningRows`): ten rows exist as specified
  - Given: constant fixture of the ten expected rows (key, default, hard range, audience, latch) copied from the GDD Tuning Knobs table
  - When: the ADR-0004 registry is enumerated for the Objectives owner
  - Then: exactly ten keys, each equal to the fixture; `ObjectiveInteractSec` resolves to the TDC row and is not redeclared; no server-face key name appears in client-module sources
  - Edge cases: duplicate registration of `ObjectiveInteractSec` fails; audience flag flipped on one row fails; an eleventh Objectives-owned key fails.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the four test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [x] Created and passing (2026-10-28): StealthGame.Objectives.InteractSecFloor, RivalSignalBound, TendCueDelayInvariant, ObjectivesTuningRows; evidence production/qa/evidence/objectives-chain-001-009-2026-10-28/

Run result: N/A - tuning rows and preset-load invariants have no in-game surface (warnings and rejects are log lines and report findings, captured by the tests).

---

## Dependencies

- Depends on: cross-epic: `production/epics/tuning-data-config/story-038-schema-row-types.md`, `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`, `production/epics/tuning-data-config/story-055-owner-invariant-mechanism.md`; cross-epic: `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md` (the `StealthGameplay` module); cross-epic: crowd-npc-behaviour (owns `CrowdAmbientTouchMaxSec`, `CrowdDecoyEarlyReleaseMinSec`; read through the registry, stories written in parallel)
- Unlocks: `production/epics/objectives-chain/story-006-fixture-claims-and-interact-target-owner.md`, `production/epics/objectives-chain/story-013-clue-function-and-too-broad-log.md`, `production/epics/objectives-chain/story-014-rival-signal-draw-and-grid.md`, `production/epics/objectives-chain/story-016-presentation-view-helpers-and-ring-rule.md`

## Completion Notes
**Completed**: 2026-10-28
**Criteria**: AC1-AC4 passing. AC1 cases floor 1.0 / 1.99 to 2.0 with one clamp warning; the floored presets lower Crowd's CrowdDecoyEarlyReleaseMaxSec to 1.9 so Crowd's own I-CNB-6 (< ObjectiveInteractSec) does not reject first. AC4's seeded client INCLUDE of the server-face header is ObjectivesIncludeLint's (story 022, as the AC says); the name grep of every client-module source (StealthPresentation and every View/ folder) and its seeded control are here.
**Deviations**: sub-structs live in StealthFoundation (the TDC schema convention every feature followed; FStealthServerTuning is a Foundation type) instead of the story's StealthGameplay paths; `bRivalSignalEnabled` carries Unit meta "Bool" (the generator requires a Unit; bools skip the suffix rule).
**Test Evidence**: production/qa/evidence/objectives-chain-001-009-2026-10-28/
**Reviewed by**: self code review 2026-10-28 (APPROVED; notes in the evidence)
