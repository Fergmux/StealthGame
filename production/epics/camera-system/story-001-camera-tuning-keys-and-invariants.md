# Story 001: Camera tuning keys and invariants

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-???` (no registry entry: the eight camera TR-IDs are architecture-level constraints; the Tuning Knobs table and AC-CS-22 have none. Register one with `/architecture-review` if wanted.)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (keys come from ADR-0004)
**ADR Decision Summary**: Every camera value (envelope limits, boom, mask, FOV, stick) is a client-audience tuning key from the frozen client snapshot (ADR-0004), identical for every client; mode and stick settings are local user settings, never tuning data. The rig math lives in `FStealthCameraCore` and `FEyeMaskCore`, which read these keys through plain parameter structs.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; this story is data-only and pre-cutoff)
**Engine Notes**: No post-cutoff API. The schema and lint machinery belong to the tuning-data-config epic (ADR-0004); this story adds the Camera rows and the five camera invariants to it. No ADR-0015 verification item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Camera values are Client-audience tuning keys with one value for every client; no key has an NPC or player variant — source: ADR-0015, ADR-0004
- Forbidden: Never hardcode a gameplay value that has a key; never repair a violating tuning set at runtime (TDC CR-6: reject at load) — source: ADR-0004
- Guardrail: Invariant checks run at load, never per frame

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-22, I-CS-1 to I-CS-5, AC-CS-16 last bullet), scoped to this story:*

- [ ] **AC1** (AC-CS-22, [U] `KeysRegistered`): **GIVEN** the TDC schema **WHEN** the CR-15 lint runs **THEN** every Camera key in the GDD Tuning Knobs table exists with owner Camera, audience Client, its latch, default and hard range.
- [ ] **AC2** (AC-CS-22 invariants, AC-CS-16 last bullet): **GIVEN** tuning sets that violate each of I-CS-1 (`CameraToggleBlendSec` > `CameraToggleLockoutSec` − 1/60 s), I-CS-2 (`BoomHeightOffsetCm` > 25), I-CS-3 (eye-cone half-angles below half of `FovHMaxDeg` / `FovVMaxDeg`), I-CS-4 (`ConcealedBoomCm` > 70 or > `BoomLengthCm`) and I-CS-5 (`CameraEdgePushDeg` < 2) **WHEN** loaded **THEN** each is rejected at load with no runtime clamp or repair and the defaults load; blend 0.13 s with lockout 0.15 s loads, blend 0.20 s with lockout 0.15 s is rejected.

---

## Implementation Notes

*Derived from ADR-0015 Decision §2/§7 and GDD Tuning Knobs:*

The keys are listed in the GDD "Tuning Knobs" table (about 45 keys across the envelope, pivot, boom, concealed boom, mask, toggle, FOV and stick groups). Rows marked `*` in the GDD (`FpFovVDeg`, `TpFovVDeg`, `StickDeadZoneFrac`, `StickCurveExp`, `StickYawRateDegPerSec`, `StickPitchRateDegPerSec`) are user settings clamped to the hard range, not tuning keys: they are stored in local settings (TDC CR-14, story `tuning-data-config/story-093-local-settings-not-tuning.md`), and this story registers only their hard ranges and defaults. Consumed-not-owned keys (`LookYawLimitDeg`, `TurnInPlaceDelaySec`, `PawnSmoothSnapThresholdCm`, `CameraToggleLockoutSec`, `DeathFallSec`, `StrikeWarpYawLimitDeg`, `ExileBeatSec`, `ExileCardTimeoutSec`) belong to SCL, IIF, Assassination and Penalty; only the invariants that read them are registered here.

I-CS-1 has no satisfiable runtime clamp by design: the hard maximum 0.28 s of `CameraToggleBlendSec` makes every blend value legal at the lockout default 0.30 s, and the set is rejected whole at load if a designer lowers the lockout under it (TDC CR-6). Expose the keys to the camera code as a plain `FStealthCameraParams` struct (no `UObject`) so the cores in stories 002, 006, 010 and 012 take it by argument.

Prototype replaced: none directly. `UStealthSettings` camera-adjacent fields in `StealthSettings.h`, if any, are retired by `tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-002-view-envelope-pure-function.md`, `story-006-eye-pivot-boom-collision.md`, `story-010-field-of-view.md`, `story-012-eye-mask-core.md`: the formulas that consume these keys.
- `story-008-toggle-blend-reduce-motion-remembered-mode.md`: the toggle behaviour that AC-CS-16's other bullets cover.
- Schema machinery, generator and fingerprint: tuning-data-config epic.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/Tuning/StealthCameraParams.h` (parameter structs; final home set by the TDC schema story)
- `Source/StealthFoundation/Private/Tuning/StealthCameraKeys.cpp` (key rows and invariants I-CS-1 to I-CS-5)
- `Source/StealthGameplay/Private/Tests/Camera/KeysRegisteredTest.cpp` (class `FCameraKeysRegisteredTest`, name `StealthGame.Camera.KeysRegistered`)
- `Source/StealthGameplay/Private/Tests/Camera/InvariantsRejectedTest.cpp` (class `FCameraInvariantsRejectedTest`, name `StealthGame.Camera.InvariantsRejected`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: ADEQUATE

Test: AC1 `KeysRegistered` (AC-CS-22)
  Given: the TDC schema and a checked-in expected-key fixture of Camera keys (owner Camera, audience Client, latch, default, hard range). The test must not parse the GDD markdown at run time.
  When: the CR-15 lint runs.
  Then: no key is missing and every field matches; an extra unlisted Camera key is reported.
  Edge cases: a key with audience Server fails; a default outside its hard range fails.

Test: AC2 `InvariantsRejected` (AC-CS-22 invariants, AC-CS-16 last bullet)
  Given: the defaults plus one violating set per invariant: I-CS-1 blend 0.20 with lockout 0.15; I-CS-2 height offset 25.01; I-CS-3 cone half-angle 53.9 H or 37.4 V; I-CS-4 concealed boom 70.1; I-CS-5 push 1.9.
  When: each set is loaded.
  Then: it is rejected naming the invariant, no value is altered, and the active set equals the defaults.
  Edge cases: boundaries that must load: blend 0.13 with lockout 0.15; height offset 25; cone 54.0 and 37.5; concealed boom 70; push 2.0. The I-CS-1 limit is 0.15 − 1/60 = 0.13333, so test 0.1333 against 0.1334. I-CS-2, I-CS-5 and the 70 leg of I-CS-4 coincide with hard-range edges, so assert that the rejection reason names the invariant. The "> BoomLengthCm" leg of I-CS-4 cannot occur within the hard ranges (min 100, max 70): call that invariant directly.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Camera/KeysRegisteredTest.cpp` and `InvariantsRejectedTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic: `production/epics/tuning-data-config/story-038-schema-row-types.md`, `story-040-canonical-keys-and-faces.md` (row types and key registry), `story-002-gameplay-presentation-module-skeletons.md` (the `StealthGameplay` module exists for the tests)
- Unlocks: `production/epics/camera-system/story-002-view-envelope-pure-function.md`, `story-006-eye-pivot-boom-collision.md`, `story-010-field-of-view.md`, `story-012-eye-mask-core.md`
