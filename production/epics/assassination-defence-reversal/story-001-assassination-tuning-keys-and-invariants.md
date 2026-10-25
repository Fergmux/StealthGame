# Story 001: Assassination tuning keys and cross-field invariants I-ASN-1 to I-ASN-8

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (Tuning Knobs; Edge Cases "Tuning extremes"; AC-ASN-31)
**Requirement**: `TR-asn-???` — no registry entry covers tuning registration (the TR registry has no data-driven-values row for this system). Traced instead to the Tuning Knobs table and ADR-0004; request a `TR-asn-014` at the next `/architecture-review`.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (the keys are Assassination's; ADR-0018 §2-§3 are their consumers)
**ADR Decision Summary**: Every gameplay number is a designer-editable, server-validated, frozen and hashed tuning value declared once in UPROPERTY metadata; the validator runs default-for-non-finite, clamp to the hard range, then cross-field invariants that reject or warn and never clamp or repair; a CI safe-range lint proves every owner safe range satisfies every invariant with the other keys at defaults.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0004's tuning types are project code; no post-cutoff engine API)
**Engine Notes**: Data assets (`UPrimaryDataAsset`) and a generated schema table are pre-cutoff. The Tuning Data & Config epic owns the schema generator, the hasher and invariants I-1 to I-5; this story adds Assassination's rows and invariants to that machinery and writes no validator code of its own.

**Control Manifest Rules (this layer)**:
- Required: Gameplay numbers come from validated tuning, never literals, ini or CVars; `Action`-latched values are captured into the action record at start on server and owning client — source: ADR-0004
- Required: Cross-field invariants reject or warn, never clamp or repair — source: ADR-0004
- Forbidden: Never read a tuning literal in the resolver, the defence component or the presentation (the prototype's `BlindStrikeRadius`, `AttackerRecoveryTime`, `BodyLifetime`, `AssassinationConeHalfAngle`, `AssassinationServerTolerance` leave `UStealthSettings` before any external playtest, TDC CR-13)
- Guardrail: Validation is load/preset-change time only, never per frame

This story registers the 16 Assassination-owned keys (names, types, defaults, hard and safe ranges, audience, latch, unit: exactly the GDD table) and the Assassination invariants as change requests to TDC (OQ-ASN-8): I-ASN-1 to I-ASN-5 and I-ASN-8 reject the preset, and I-ASN-5w, I-ASN-6, I-ASN-7, W-ASN-10 and W-ASN-11 warn. It also lowers the canonical hard maximum of `AssassinationRangeCm` to 250 and registers `StrikeBearingLimitDeg`. Every later story reads these keys through the frozen tuning face and never a literal, which is why this is story 001.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-31, the Tuning Knobs table), scoped to this story:*

- [ ] **AC1** (Tuning Knobs table, [Logic/BLOCKING]): **GIVEN** the generated tuning schema **WHEN** it is read for owner `Assassination` **THEN** it holds exactly the keys `AssassinationRangeCm`, `StrikeBearingLimitDeg`, `DefenceDurationSec`, `DefenceCooldownSec`, `StrikeVerticalReachCm`, `StrikeContactSec`, `StrikeWarpLimitCm`, `StrikeContactGapCm`, `StrikeWarpYawLimitDeg`, `AttackAttemptLockoutSec`, `OpenSpotToleranceCm`, `CounterDurationSec`, `DeathFallSec`, `BodyLingerSec`, `BodyRemovalGraceSec`, `StrikeNetLeadSec` with the GDD defaults, hard ranges, audience (`All` or `ServerOnly`) and latch (`Use`, `Action` or `Round`), and `AssassinationRangeCm` has hard range 100-250.
- [ ] **AC2** (AC-ASN-31 rejections, [Logic/BLOCKING] `StealthGame.Assassination.TuningInvariants`): **GIVEN** presets (a) range 250 with `StrikeWarpLimitCm` 150 and gap 5, (b) `CounterDurationSec` 2.0 with lockout 1.75, (c) `StrikeContactSec` 0.8 with `BushStrikeEmergeSec` 0.5, (d) `StrikeNetLeadSec` 0.3 with contact 0.4, (f) `DeathFallSec` 1.0, and (i) `StrikeBearingLimitDeg` 100 with `StrikeWarpYawLimitDeg` 30, each otherwise at defaults **THEN** each is rejected and the rejection names exactly I-ASN-1, I-ASN-2, I-ASN-3, I-ASN-4, I-ASN-5 and I-ASN-8 respectively; preset (c) additionally reports one I-ASN-6 warning, listed separately from the rejection.
- [ ] **AC3** (AC-ASN-31 acceptances, [Logic/BLOCKING]): **GIVEN** (e) `DeathFallSec` 1.6, `BodyLingerSec` 8, `BodyRemovalGraceSec` 2.5 and (g) fall 1.5, linger 6.5, grace 2.0 (sum exactly 10.0) against a 10 s `CrowdReplacementMinDelaySec`, lockout 1.2 with contact 0.4 and counter 1.2, and (h) `StrikeContactSec` 0.30 with `StrikeWarpLimitCm` 240 **THEN** (e) and (g) are accepted with one I-ASN-5w warning each (strict `<`), the lockout preset is accepted with one I-ASN-6 warning, (h) is accepted with one I-ASN-7 warning, and the default preset reports no rejection and no warning.

---

## Implementation Notes

*Derived from ADR-0004 (Decision, Implementation Guidelines) and the GDD Tuning Knobs cross-field invariants:*

Declare the keys once in the `Assassination` `_Public` / `_Server` sub-structs of the tuning preset class, with the GDD table's metadata; the schema generator commandlet (TDC story 041) regenerates the checked-in table. `StrikeVerticalReachCm`, `OpenSpotToleranceCm` and `StrikeNetLeadSec` are `ServerOnly`; every other key is `All` (clients need them to play the clips and show owner feedback) and is uniform for every pawn.

Invariants are evaluated on clamped values:
- Reject: I-ASN-1 `AssassinationRangeCm − r_c − StrikeContactGapCm ≤ StrikeWarpLimitCm` (r_c is SCL's provisional capsule radius, 35 cm, read from the published body constants); I-ASN-2 `StrikeContactSec + 0.4 ≤ CounterDurationSec ≤ AttackAttemptLockoutSec`; I-ASN-3 `StrikeContactSec + BushStrikeEmergeSec ≤ 1.2`; I-ASN-4 `StrikeNetLeadSec ≤ StrikeContactSec / 2`; I-ASN-5 `DeathFallSec ≥ 1.1`; I-ASN-8 `StrikeBearingLimitDeg ≤ 60 + StrikeWarpYawLimitDeg`.
- Warn only: I-ASN-5w `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < CrowdReplacementMinDelaySec` (strict); I-ASN-6 `AttackAttemptLockoutSec ≥ StrikeContactSec + 1.1`; I-ASN-7 `StrikeWarpLimitCm / StrikeContactSec ≤ 600`; W-ASN-10 `AttackAttemptLockoutSec > StrikeContactSec + BushStrikeEmergeSec` (strict); W-ASN-11 `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < InformantRespawnDelaySec`; and `OpenSpotToleranceCm ≠ PawnRefAgreementCm`.
- The default preset's three equal values (`OpenSpotToleranceCm`, `BushSpotToleranceCm`, `PawnRefAgreementCm` = 75) give forged-ref radius parity; keep that warning.
- The safe-range lint must pass: every safe range satisfies every invariant with the others at defaults, except the documented I-ASN-7 band (a safe pair inside the band is accepted with the warning, AC-ASN-31 (h)).

Keys owned elsewhere (`BushStrikeEmergeSec`, `BushSpotToleranceCm`, `NpcKillStrikeLimit`, `CrowdReplacementMinDelaySec`, `InformantRespawnDelaySec`, `ReleaseDebounceSec`, `PawnRefAgreementCm`) are read, never re-registered; if one is not yet in the schema, the invariant that reads it reports `NotEvaluated` and the test asserts against an injected value.

**Prototype replaced:** `UStealthSettings` assassination keys (the GDD and TDC call them `AttackerRecoverySec`, `BodyLifetimeSec`, `BlindStrikeRadiusCm`, `AssassinationConeHalfAngleDeg`, `AssassinationLagToleranceMult`; the prototype members are `AttackerRecoveryTime`, `BodyLifetime`, `BlindStrikeRadius`, `AssassinationConeHalfAngle`, `AssassinationServerTolerance`) are retired by story 027, not here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-002-reach-f1-pure-function.md`: using `AssassinationRangeCm` and the warp keys in the reach test.
- `story-027-port-rules-reference-tests-retire-prototype.md`: removing the five prototype keys.
- TDC epic: the schema generator, hasher and invariants I-1 to I-5 themselves.

---

## Files

*The code and test files this story creates or changes. `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/StealthTuningAssassination.h` (the `_Public` sub-struct)
- `Source/StealthFoundation/Public/Server/StealthTuningAssassinationServer.h` (the `_Server` sub-struct; guard header first)
- `Source/StealthFoundation/Private/Tuning/AssassinationInvariants.cpp` (I-ASN-1 to I-ASN-8, W-ASN-10, W-ASN-11)
- `Source/StealthGameplay/Private/Tests/Assassination/TuningInvariantsTest.cpp` (class `FAssassinationTuningInvariantsTest`, name `StealthGame.Assassination.TuningInvariants`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: the expected key table is cited by reference; proposed rewrite (unconfirmed): compare the generated schema to a checked-in fixture `AssassinationKeyTable` transcribed from the GDD Tuning Knobs table (name, type, default, hard and safe range, audience, latch, unit) with no extra, missing or differing key or field; exactly 3 keys are `ServerOnly` (`StrikeVerticalReachCm`, `OpenSpotToleranceCm`, `StrikeNetLeadSec`) and 13 are `All`; `AssassinationRangeCm` hard range 100-250.
- AC2: does not say the other presets must produce no warnings, nor the injected values of externally owned keys; proposed rewrite (unconfirmed): each rejection list equals exactly {I-ASN-n}; the warning list is empty for (a), (b), (d), (f), (i) and equals [I-ASN-6] for (c); `BushStrikeEmergeSec`, `CrowdReplacementMinDelaySec` and `InformantRespawnDelaySec` are injected at their GDD defaults.
- AC3: "one I-ASN-5w warning each" is unmeasurable while `InformantRespawnDelaySec` and `BushStrikeEmergeSec` are unfixed, and the lockout preset is unnamed; proposed rewrite (unconfirmed): inject those two at GDD defaults; name the lockout preset (j) (lockout 1.2, contact 0.4, counter 1.2); the full warning list for (e), (g), (h), (j) is exactly I-ASN-5w, I-ASN-5w, I-ASN-7 (800 cm/s over 600) and I-ASN-6 respectively; the default preset has empty reject and warn lists.

- **AC-1**: Schema holds exactly the 16 Assassination keys
  - Given: generated tuning schema and the `AssassinationKeyTable` fixture
  - When: rows for owner `Assassination` are read
  - Then: row set and every field equal the fixture; `AssassinationRangeCm` hard range is 100-250; audience split is 3 `ServerOnly` and 13 `All`
  - Edge cases: an extra or missing key fails; a default off by 1 ulp fails; a key owned elsewhere (`BushStrikeEmergeSec`) must not appear
- **AC-2**: Six rejection presets name the right invariant
  - Given: default preset with one override each for (a), (b), (c), (d), (f), (i)
  - When: the validator runs (clamp, then invariants)
  - Then: reject list is exactly I-ASN-1, 2, 3, 4, 5 and 8 respectively; (c) also has one I-ASN-6 warning in a separate list
  - Edge cases: (a) is 210 > 150 (250 - 35 - 5); (i) is 100 > 90; a rejected preset is never clamped or repaired
- **AC-3**: Acceptance presets warn without rejecting
  - Given: presets (e), (g), (h), (j) and the default preset, `CrowdReplacementMinDelaySec` injected at 10
  - When: the validator runs
  - Then: all accepted; warnings are I-ASN-5w, I-ASN-5w, I-ASN-7, I-ASN-6 (one each); default preset has none
  - Edge cases: (g) sum exactly 10.0 must warn (strict `<`); (h) is 800 cm/s against 600; reject list empty in all five

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Assassination/TuningInvariantsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`, `story-041-schema-generator-commandlet.md`, `story-048-invariants-i1-i5.md`, `story-097-invariants-reject-whole-preset.md`; `production/epics/shared-character-locomotion/story-002-body-constants-published.md` (capsule radius for I-ASN-1)
- Unlocks: every other story in this epic (all read these keys); notably `story-002-reach-f1-pure-function.md`, `story-003-defence-state-component-and-cooldown.md`, `story-008-attacker-lockout-and-attack-lifecycle.md`
