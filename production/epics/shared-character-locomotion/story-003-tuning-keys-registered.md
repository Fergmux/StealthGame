# Story 003: Every SCL tuning key registered with its audience and latch, invariants enforced

> **Epic**: Shared Character & Locomotion
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (Tuning Knobs, invariants I-SCL-1 to I-SCL-16; AC-SCL-35)
**Requirement**: `TR-scl-014` (engine CMC network settings pinned in project config and boot-tested against SCL tuning keys, I-SCL-15); the keys also carry every SCL requirement's values
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (with ADR-0010 section 6 for I-SCL-15)
**ADR Decision Summary**: Every gameplay number is one cooked data-asset value declared once in C++ schema rows with audience (`Client` / `ServerOnly`) and latch, resolved, validated and frozen server-side; no config or CVar back door. ADR-0010 pins the `AGameNetworkManager` forced-update values in `Config/DefaultGame.ini` and boot-tests them against the SCL keys.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0004); HIGH for the I-SCL-15 boot test (ADR-0010)
**Engine Notes**: `AGameNetworkManager` forced-update and movement-time-discrepancy values are read from the class default object (`GlobalConfig`), process-wide and before any tuning applies, so they are pinned in `Config/DefaultGame.ini` `[/Script/Engine.GameNetworkManager]` and a boot test asserts the CDO equals the SCL keys. That `.ini` edit was applied on 2026-10-25 (ADR-0010 "Config applied").

**Control Manifest Rules (this layer, plus Foundation tuning)**:
- Required: tuning is one data asset of per-owner sub-structs, schema rows compiled into a checked-in table, server-only resolve/validate/freeze with TuningHash and ClientHash (ADR-0004); `AGameNetworkManager` forced-update and discrepancy values come from `Config/DefaultGame.ini` and equal the SCL tuning keys (boot test) (ADR-0010).
- Forbidden: no per-kind variant of any SCL key; no config or CVar back door; `InformantMaxSpeedCmPerSec` and `FootstepStrideWalkCm` must not exist (retired).
- Guardrail: validator work is boot-time only.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-35** [U] `KeysRegistered`: GIVEN the TDC schema, WHEN the CR-15 lint runs, THEN every key in Tuning Knobs exists with the stated audience and latch, invariants I-SCL-1 to I-SCL-16 (Tuning Knobs; I-SCL-10 and I-SCL-15 by the boot and harness checks stated there) are enforced, and `InformantMaxSpeedCmPerSec` and `FootstepStrideWalkCm` do not exist.

*QA clarification (create-stories 2026-10-25, not a GDD change):* reclassified from Config/Data to Logic because the GDD tags AC-SCL-35 [U] and it asserts invariant enforcement (BLOCKING gate); the fixture is the Tuning Knobs table as parsed data; each invariant has one passing and one rejecting case, except I-SCL-10 and I-SCL-15 which are harness and boot checks.

---

## Implementation Notes

*Derived from ADR-0004 and GDD Tuning Knobs:*

- Add one schema row per Tuning Knobs entry (about 55 keys: `WalkSpeedCmPerSec` through `HandoverHoldSec`, including `StopMinSec`, `IdleGazeDelaySec` and `IdleGazeLookDeadzoneDegPerSec`) with the table's Owner, Default, Hard range, Safe range, Audience and Latch. Audience `Client` keys travel with the tuning snapshot; `ServerOnly` keys (`RunSpeedThresholdCmPerSec`, `RunExitHysteresisCmPerSec`, `IdleGazeDelaySec`, `IdleGazeLookDeadzoneDegPerSec`, `PawnNetUpdateHz`, `InputStarvationSec`, `StarvationCatchUpMaxMult`, `HandoverHoldSec`) are never replicated. Keys marked † in the GDD stay provisional pending the traversal, net-emulation or R3 spikes; their defaults may change through `/propagate-design-change`, never by editing the registry in a story.
- Enforce I-SCL-1 to I-SCL-9, I-SCL-11 to I-SCL-14 and I-SCL-16 as validator invariants (reject versus warn as the GDD states: I-SCL-4, I-SCL-5 and I-SCL-16 reject). I-SCL-10 (listen-host frame time at p99) is checked by the harness frame-time capture, not at startup. I-SCL-15 is a boot test comparing the `AGameNetworkManager` CDO (`MAXCLIENTUPDATEINTERVAL`, `MaxClientForcedUpdateDuration` 1.40 s, `bMovementTimeDiscrepancyDetection`, `bMovementTimeDiscrepancyResolution`, `MaxMoveDeltaTime`) with `InputStarvationSec`, `T_stop,max` and the other SCL keys; it fails the boot on a mismatch.
- The pace set {0.8, 0.9, 1.0} is a code constant held by no key (decision D2, Crowd owns none); `StopMinSec` is the single stop-floor key Crowd also reads.
- `IdleGazeLookDeadzoneDegPerSec` is `ServerOnly`, latch `Use`; out-of-range values (below 0 or above 3) are rejected by the validator (also asserted by story 025).
- **Prototype replaced**: `UStealthSettings` (`Source/StealthGame/Public/StealthSettings.h`, `Config` properties in `DefaultGame.ini`) violates TDC CR-13 and is replaced, not migrated, for every SCL value it holds.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The TDC pipeline itself (schema table generator, validator, snapshot): `production/epics/tuning-data-config/EPIC.md`.
- The behaviour each key drives: stories 009 onward.

---

## Files

- `Source/StealthFoundation/Public/View/StealthTuningPublicSubstructs.h`
- `Source/StealthFoundation/Public/Server/StealthTuningSubstructs.h`
- `Source/StealthFoundation/Public/Server/StealthServerTuning.h`
- `Source/StealthFoundation/Public/View/StealthClientTuning.h`
- `Source/StealthFoundation/Public/Server/StealthTuningData.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningSchema.generated.inl`
- `Source/StealthFoundation/Public/Server/StealthTuningVersion.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningInvariants.cpp`
- `Source/StealthFoundation/Public/Server/StealthTuningInvariants.h`
- `Source/StealthFoundation/Public/Server/Testing/StealthSclTuningKnobs.h`
- `Source/StealthGameEditor/Private/Tests/Tuning/SchemaCompleteTest.cpp`
- `Source/StealthGame/Public/StealthSettings.h`
- `design/gdd/shared-character-locomotion.md`
- `design/registry/entities.yaml`
- `Source/StealthGameplay/Private/Tests/Locomotion/KeysRegisteredTest.cpp`
- `Source/StealthGameplay/Private/Tests/Locomotion/GameNetworkManagerPinTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.KeysRegistered`.*

Test: SCL tuning keys registered, invariants enforced (AC-SCL-35)
  Given: the TDC schema and the Tuning Knobs table as parsed fixture data
  When: the CR-15 lint runs
  Then: every Tuning Knobs key exists with the stated audience and latch; each invariant I-SCL-1 to I-SCL-16 has one passing and one rejecting case (I-SCL-10 and I-SCL-15 are covered by the harness frame-time check and the boot test instead); `InformantMaxSpeedCmPerSec` and `FootstepStrideWalkCm` are absent from the schema
  Edge cases: a key added to the table without schema support must fail the lint; a Client-audience key marked ServerOnly (or the reverse) must fail

---

## Test Evidence

**Story Type**: Logic (reclassified from Config/Data, see the QA clarification)
**Required evidence**:
- Logic (BLOCKING): `Source/StealthGameplay/Private/Tests/Locomotion/KeysRegisteredTest.cpp`, test name `StealthGame.Locomotion.KeysRegistered` (CR-15 lint) — must exist and pass; plus the I-SCL-15 boot test `GameNetworkManagerPinTest.cpp`; a smoke check line in `production/qa/smoke-*.md` for the shipped data asset

**Status**: [x] Created and passing (`StealthGame.Locomotion.KeysRegistered`, `StealthGame.Locomotion.GameNetworkManagerPin`; `StealthGame.Tuning.SchemaComplete` (now pins the 55 SCL defaults) and `StealthGame.Tuning.SchemaVersionBumped` (TuningSchemaVersion 2), 2026-10-26)

---

## Dependencies

- Depends on: TDC Exit B pipeline (`production/epics/tuning-data-config/EPIC.md`; stories not yet written, path TBD)
- Unlocks: every other story in this epic that reads a key, starting with `story-002-body-constants-published.md`, `story-009-stop-floor.md`

---

## Completion Notes
**Completed**: 2026-10-26 (SCL batch: 003, 007, 009-018, 046, 047)
**Criteria**: 1/1: 55 SCL keys registered (47 Client, 8 ServerOnly) with type, default, hard/safe range, audience, latch, owner SCL and unit; retired InformantMaxSpeedCmPerSec and FootstepStrideWalkCm absent; I-SCL-1..9, 12, 13, 15, 16 each pass the defaults and reject a violating case; I-SCL-10/11/14 registered offline; I-SCL-15 boot check against the AGameNetworkManager CDO; negative controls (unregistered key, flipped audience) fail the lint
**Deviations**: TraversalHurdleHeightCm (1.2 m, spike SCL-045 final) registered with this story and added to the GDD Tuning Knobs table, I-SCL-2 and entities.yaml (decision log). Three Unit cells map to the CR-15.2 suffix the key ends with (PerSec, Hz, Frac). Every SCL row rejects (decision log). I-SCL-12 reads SessionSoftTimeoutSec 3 as a pending owner constant until Session registers it. UStealthSettings RunSpeedThreshold / InformantMaxSpeed removed (CR-13). The smoke-check line for the shipped data asset is not applicable yet: no Default preset asset exists (tdc-056). Carried AC-SCL-01 key clause from SCL-004: SchemaComplete asserts one key per name (no per-kind variant).
**Test Evidence**: `StealthGame.Locomotion.KeysRegistered`, `StealthGame.Locomotion.GameNetworkManagerPin`; `StealthGame.Tuning.SchemaComplete` (now pins the 55 SCL defaults) and `StealthGame.Tuning.SchemaVersionBumped` (TuningSchemaVersion 2); full suite 171/171 StealthGame.* Success (`production/qa/evidence/story-010-speed-tiers/full-suite.txt`)
**Run result**: N/A - tuning schema rows and validator invariants; nothing renders
**Reviewed by**: lead-programmer focused review (sonnet) + self-review
