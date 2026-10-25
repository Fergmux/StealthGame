# Story 089: AC 75 KnobRegistryConformance

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-???` — no entry in `docs/architecture/tr-registry.yaml` matches. TR-iif-001 to TR-iif-013 cover the command boundary, transport, validation, holds, slot, replies, hub, replication, targeting, contexts, latency and possession epoch; none covers tuning-knob registry conformance (the Knob Registry Convention is Tuning Data & Config's CR-15, not an IIF requirement). The criterion is traced to the GDD Tuning Knobs section and AC 75 directly.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: ADR-0011 puts every IIF threshold behind data: bucket sizes and rates, ranges, slack, debounce and plate cooldowns come from the frozen tuning snapshot (never literals), while bindings, the hold/toggle preference and sensitivity are client-local and never tuning (TDC CR-14).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No engine API. The check runs TDC's schema test (AC-3a) and documentation lint (AC-3b), which parse the GDD Tuning Knobs table. Engine-risk is inherited from ADR-0011 (input layer HIGH); this story touches only data.

**Control Manifest Rules (this layer)**:
- Required: Bindings, the hold/toggle preference and sensitivity are client-local, never tuning (TDC CR-14) — source: ADR-0011
- Required: Gameplay values are data-driven (external config), never hardcoded — source: `.claude/docs/coding-standards.md`

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 75 (Tuning, Logic, U `KnobRegistryConformance`): **GIVEN** the IIF tuning data asset **WHEN** TDC's schema test (AC-3a) and doc lint (AC-3b) run **THEN** every key in the Tuning Knobs table is present with its default, hard and safe ranges, Audience and Latch; **AND** every key ends with a CR-15 suffix.

---

## Implementation Notes

*Derived from GDD Tuning Knobs (lines 506-554, "Registered per the TDC Knob Registry Convention (CR-15 ...)") and ADR-0011 Decision section 9 (rebinding and settings are client-local):*

**What this story builds.** The IIF's slice of the knob registry and its conformance test. The IIF tuning data asset must contain every key of the GDD Tuning Knobs table, with default, hard range, safe range, Audience (`All` or `ServerOnly`) and Latch (`Action` or `Use`) matching the table, and every key must end with a CR-15 suffix. The keys: `ReleaseDebounceSec`, `InteractRangeCm`, `InteractCancelHysteresisCm`, `InteractFacingHalfAngleDeg`, `InteractMoveCancelDistCm`, `InteractMoveGraceSec`, `CommandRangeSlackCm`, `PawnRefAgreementCm`, `AckSlackSec`, `BucketInteractSize`/`PerSec`, `BucketPrepareSize`/`PerSec`, `BucketAttackSize`/`PerSec`, `BucketHideSize`/`PerSec`, `TransportBucketSize`/`PerSec`, `UnmatchedEndPerSec`, `AbuseLogDropCount`, `AbuseLogWindowSec`, `CameraToggleLockoutSec`, `InformantPlateCooldownSec`, `RejectPlateCooldownSec`.

**Retired and not-IIF keys.** The retired key `LatencyWarnRttSec` must be absent (replaced by HUD's `HudLatencyWarnRttSec`, user decision 2026-10-25, TDC CR-15.3). Keys consumed but owned elsewhere (`ObjectiveInteractSec`, `DefenceDurationSec`, `DefenceCooldownSec`, `AssassinationRangeCm`, `AttackAttemptLockoutSec`, `CounterDurationSec`, `BushReenterCooldownSec`, `TendCueDelaySec`) must not be duplicated in the IIF asset. Key bindings, the hold/toggle preference and Look sensitivity are client-local (TDC CR-14) and must not appear. The GDD records cross-field rules (`TransportBucketSize` at least 2 x the sum of per-pawn bucket sizes, 22 by default; `TransportBucketPerSec` at least 2 x the sum of per-pawn refill rates, 12 by default) as a change request to TDC: this story does not enforce them.

**Test notes (qa-lead).** Edge cases: the retired key is absent; zero keys parsed fails the test (denominator check, so a broken parser cannot pass vacuously). The test calls TDC's schema test (AC-3a) and doc lint (AC-3b); it does not reimplement them.

**Prototype.** New code, no prototype counterpart (prototype values were literals in `StealthCharacter`).

**Tuning keys (defaults).** All keys of the table above; defaults are read from the table at test time, for example `ReleaseDebounceSec` 0.10, `InteractRangeCm` 150, `CommandRangeSlackCm` 40, `TransportBucketSize` 30.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-031-token-bucket-defaults.md`: the bucket arithmetic using the bucket keys (F3).
- `production/epics/input-interaction-framework/story-037-hold-completion-epsilon.md`: use of the debounce and hold keys at runtime.
- `production/epics/input-interaction-framework/story-077-latency-icon.md`: the HUD-owned `HudLatencyWarnRttSec`.
- `production/epics/input-interaction-framework/story-093-static-scan-and-prototype-retirement.md`: static scans of the project source.

---

## Files

- `Source/StealthGameplay/Private/Tests/Input/KnobRegistryConformanceTest.cpp` — class `FInputKnobRegistryConformanceTest`, name `StealthGame.Input.KnobRegistryConformance`
- The IIF tuning data asset / knob rows: file locations are owned by the tuning-data-config epic (cross-epic; not in this story's file list until that skeleton exists)

---

## QA Test Cases

**Automated test specs:**

- **AC-1**: IIF knob registry conforms to the GDD table (`StealthGame.Input.KnobRegistryConformance`).
  - Given: the IIF tuning data asset and the IIF GDD Tuning Knobs table (parsed by TDC's doc lint).
  - When: TDC's schema test (AC-3a) and doc lint (AC-3b) run.
  - Then: every table key exists in the asset with default, hard range, safe range, Audience and Latch matching the table; every key ends with a CR-15 suffix.
  - Edge cases: the retired key `LatencyWarnRttSec` must be absent; zero keys parsed fails the test.

*QL-STORY-READY: ADEQUATE (qa-lead). Cross-epic: tuning-data-config AC-3a/AC-3b tooling.*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/KnobRegistryConformanceTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-031-token-bucket-defaults.md`; cross-epic: tuning-data-config (stories not yet written; AC-3a schema test, AC-3b doc lint, knob registry skeleton)
- Unlocks: None
