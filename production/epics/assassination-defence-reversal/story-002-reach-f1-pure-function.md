# Story 002: Reach and warp (F1) as a pure function, standard and bush strike

> **Epic**: Assassination & Defence Reversal
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (Formulas F1; ASN-R8, ASN-R12; AC-ASN-02, AC-ASN-38)
**Requirement**: `TR-asn-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§2.1 step 1, §2.2 clamp)
**ADR Decision Summary**: A strike's victim and outcome are decided from live server capsules inside the Attack's queue slot; the reach test is the GDD's F1, applied to server positions with no rewind, so reach never exceeds what the lunge visibly covers; hit tests use the capsule, never animated bones.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0018 overall; this story is pure arithmetic over injected positions and velocities)
**Engine Notes**: No engine API is used by the core: it takes `FVector2D`/`double` inputs and returns a boolean plus the numbers behind it, so AC-ASN-02 and AC-ASN-38 run headless with no world, which is the ADR-0007 plain-C++-core rule. The capsule radius is SCL's provisional 35 cm (OQ-ASN-5); the test injects it so the numbers do not move when the pool capsule is measured.

**Control Manifest Rules (this layer)**:
- Required: Plain C++ `F…Core` with injected inputs, behind a subsystem facade; hit tests use capsules — source: ADR-0007, ADR-0018
- Required: `WarpPoint` is derived only from an honoured `PawnRef`'s extrapolation or the submitted `WorldPoint` (clamped) — source: ADR-0018
- Forbidden: Never apply lag compensation or rewind (dropped `AssassinationServerTolerance`); never read animated bones for hit tests — source: ADR-0018, GDD Tuning Knobs
- Guardrail: Resolver cost is microseconds per Attack inside the 0.5 ms queue line (≤ a handful of capsule tests)

This story is the reach-versus-animation check: `Reach(A, V) = [d0 − r_c ≤ R] ∧ [|Δz| ≤ Z] ∧ [X_eff ≤ X_max] ∧ [|θ_T| ≤ Θ]`, with the victim's extrapolated position `P_V(T_v)` = its position plus its velocity held for `t_d = T_v − λ` and baseline braking after. It also returns the warp translation `X_eff` and bearing `θ_T` that the strike write (story 012) and the clip variant choice (story 018) reuse, so the clip's lunge and the server's verdict can never disagree.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-02, AC-ASN-38), scoped to this story:*

- [ ] **AC1** (AC-ASN-02 stationary and bearing, [Logic/BLOCKING] `StealthGame.Assassination.ReachF1`): **GIVEN** r_c 35, R 200, Z 75, T_v 0.40, λ 0.15, `StrikeWarpLimitCm` 240, gap 15, Θ 90° **WHEN** `Reach` is evaluated **THEN** a standing victim at d0 = 235.4 is in reach and at 235.6 is not (0.5 cm epsilon); Δz = 75 is in reach and Δz = 76 is not; a standing victim at d0 = 150 with θ_T = 89° is in reach and with θ_T = 91° is not; a victim walking toward the attacker at d0 = 100 is in reach with warp translation `X_eff` = 0 (never negative).
- [ ] **AC2** (AC-ASN-02 moving victims, [Logic/BLOCKING]): **GIVEN** the same defaults **WHEN** moving victims are evaluated **THEN** a victim walking straight away (140 cm/s) at d0 = 235 is in reach with D = 53.9 ± 0.1 and X = 203.9; one running straight away (450 cm/s) at d0 = 235 is not (D = 173.25, X = 323.25); one running across at 235 is in reach (X ≈ 207.0); a runner moving straight away is in reach at d0 = 151.5 and not at 152.0 (X_max 240).
- [ ] **AC3** (AC-ASN-38, [Logic/BLOCKING] `StealthGame.Assassination.ReachBushStrike`): **GIVEN** `BushStrike` with T_v = 0.75 s and λ 0.15 (t_d 0.60) **WHEN** a victim walks straight away at 140 cm/s **THEN** D = 102.9 ± 0.1 and it is in reach at d0 = 222.0 and not at 222.3; **AND** a victim running straight away at 450 cm/s has D = 330.75 and is out of reach at every d0 ≥ 70.

---

## Implementation Notes

*Derived from ADR-0018 §2.1-§2.2 and GDD Formulas F1:*

`d0 = |P_V − P_A|_xy` at the slot. `B(v) = v·λ − ½·a_b·λ²` if `λ < v/a_b`, else `v²/(2·a_b)`; `D(v) = v·t_d + B(v)`; `P_V(T_v) = P_V + v̂·D(|v|)`; `X = |P_V(T_v) − P_A|_xy − 2·r_c − g`; `X_eff = max(X, 0)`; `θ_T` is the bearing of `P_V(T_v)` relative to the attacker's body facing at the slot. The in-reach inequality `d0 − r_c ≤ R` is inclusive with a 0.5 cm epsilon. Inputs: `AssassinationRangeCm`, `StrikeVerticalReachCm`, `StrikeContactSec` (plus `BushStrikeEmergeSec` for the `BushStrike` variant, giving T_v = 0.75 s), `StrikeNetLeadSec`, `StrikeWarpLimitCm`, `StrikeContactGapCm`, `StrikeBearingLimitDeg` (all from story 001), and per victim its server velocity and its gait's baseline braking from SCL (walk 140 cm/s, braking 187 cm/s²; run 450 cm/s, braking 600 cm/s²). Output also carries `X_eff` and `θ_T`.

For a spot-rule strike the same test is applied to each candidate; `WarpPoint` is still derived from the `WorldPoint` only (story 012). A named pawn that fails reach is a whiff (story 005). Hit tests use the capsule, never bones. The vertical limit applies to the server position, so a climber with Δz above `StrikeVerticalReachCm` is out of reach (resolved in story 008's AC for moving victims).

**Prototype replaced:** the prototype's cone test and lag-tolerance multiplier in `AStealthGameMode::HandleAssassinationRequest` (`AssassinationConeHalfAngle`, `AssassinationServerTolerance`, `BlindStrikeRadius`) are dropped by design (GDD Tuning Knobs, "Prototype keys retired"); nothing from them is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-005-victim-resolution-order.md`: which rule picks the candidate, the dead-body exclusion, terminal whiffs.
- `story-012-slot-write-outcome-blind-and-warp-point.md`: deriving and quantising `WarpPoint`.
- `story-018-strike-clip-variants-warp-and-contact-scheduling.md`: choosing `Strike_F`/`L60`/`R60` from the bearing and the client warp.
- `story-008-attacker-lockout-and-attack-lifecycle.md`: AC-ASN-11 (runners and vaulters killed at their server positions, a climber out of reach) at the resolver level.

---

## Files

- `Source/StealthGameplay/StealthGameplay.Build.cs` (Features/Assassination include paths)
- `Source/StealthGameplay/Features/Assassination/Public/Server/StrikeReachCore.h` (new)
- `Source/StealthGameplay/Features/Assassination/Private/StrikeReachCore.cpp` (new)
- `Source/StealthGameplay/Private/Tests/Assassination/ReachF1Test.cpp` (FAssassinationReachF1Test)
- `Source/StealthGameplay/Private/Tests/Assassination/ReachBushStrikeTest.cpp` (FAssassinationReachBushStrikeTest)

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: the 0.5 cm epsilon applies to the range clause only by implication, the "walking toward" case lacks speed and direction, and the attacker facing for θ_T is undefined; proposed rewrite (unconfirmed): the epsilon applies only to `d0 - r_c <= R`, not to the Z, X or Θ clauses; the attacker faces +X; the toward-walker moves at 140 cm/s at d0 = 100 and is in reach with `X_eff` = 0 and θ_T = 0.
- AC2: the in-reach boundary (151.5 in, 152.0 out) only holds if no epsilon applies to `X_eff <= X_max`; proposed rewrite (unconfirmed): `X_eff <= 240` is evaluated without epsilon (X = 239.75 at d0 151.5, 240.25 at d0 152.0); the across-runner bearing is atan(173.25/235) = 36.4 degrees.
- AC3: "out of reach at every d0 >= 70" has no sweep definition; proposed rewrite (unconfirmed): asserted for every integer d0 in [70, 235] at 1 cm step, with X >= 315.75 at d0 = 70.

- **AC-1**: Stationary range, vertical, bearing, toward-walker
  - Given: r_c 35, R 200, Z 75, T_v 0.40, λ 0.15, warp limit 240, gap 15, Θ 90, injected positions, standing velocity 0
  - When: `Reach` runs
  - Then: d0 235.4 in and 235.6 out; Δz 75 in and 76 out; (d0 150, θ_T 89) in and θ_T 91 out; toward-walker at d0 100 in with `X_eff` = 0
  - Edge cases: epsilon applies to range only; X never negative; Δz is absolute (test -75 and -76)
- **AC-2**: Moving victims use F1 extrapolation
  - Given: same defaults, walk 140 cm/s braking 187, run 450 cm/s braking 600
  - When: moving victims are evaluated at the slot
  - Then: walk away at d0 235: D 53.9 ± 0.1, X 203.9, in; run away at 235: D 173.25, X 323.25, out; run across at 235: X ≈ 207.0 (± 0.1), in; runner away in at 151.5 and out at 152.0
  - Edge cases: both gaits take the `λ < v/a_b` branch; no epsilon on the X clause
- **AC-3**: Bush strike uses T_v = 0.75
  - Given: `BushStrike`, T_v 0.75, λ 0.15 (t_d 0.60), same defaults
  - When: victims walk or run straight away
  - Then: walker D 102.9 ± 0.1, in at d0 222.0 and out at 222.3; runner D 330.75, out for every d0 in [70, 235]
  - Edge cases: T_v comes from contact + `BushStrikeEmergeSec`, not a literal

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `ReachF1Test.cpp` and `ReachBushStrikeTest.cpp` under `Source/StealthGameplay/Private/Tests/Assassination/` — must exist and pass

**Status**: [x] Created and passing (2026-10-27)

---

## Dependencies

- Depends on: `production/epics/assassination-defence-reversal/story-001-assassination-tuning-keys-and-invariants.md`; cross-epic `production/epics/shared-character-locomotion/story-002-body-constants-published.md` (capsule radius, gait speeds and braking)
- Unlocks: `story-005-victim-resolution-order.md`, `story-006-bush-spot-rule-and-victim-pick.md`, `story-007-attack-outcomes-resolved-in-slot.md`, `story-012-slot-write-outcome-blind-and-warp-point.md`

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: 3/3 passing (AC1 range 235.4 in / 235.6 out with the epsilon on the range clause only, dz +-75 in / +-76 out, bearing +-89 in / +-91 out, toward-walker X_eff 0; AC2 walk-away D 53.9 X 203.9 in, run-away D 173.25 X 323.25 out, run-across X 207.0 bearing 36.4 in, runner 151.5 in / 152.0 out; AC3 bush T_v 0.75 from tuning, walker D 102.9 in at 222.0 / out at 222.3, runner D 330.75 out for every integer d0 in [70, 235]) — `StealthGame.Assassination.ReachF1`, `StealthGame.Assassination.ReachBushStrike`
**Deviations**: ADVISORY — header placed at `Source/StealthGameplay/Features/Assassination/Public/Server/StrikeReachCore.h` (include "Server/StrikeReachCore.h") and the .cpp under `Features/Assassination/Private/`, mirroring Features/Crowd (ADR-0007, batch brief), instead of `Public/Server/Features/Assassination/`. The capsule radius and BushStrikeEmergeSec are injected parameters (SCL constant / Concealment key). Cross-epic SCL-002 proceeded with risk accepted.
**Test Evidence**: `ReachF1Test.cpp`, `ReachBushStrikeTest.cpp` under `Source/StealthGameplay/Private/Tests/Assassination/`; run in `production/qa/evidence/assassination-defence-reversal-001-004-2026-10-27/full-suite.txt`
**Run result**: N/A — a pure function with no caller in play yet (the strike resolver, stories 005-007, calls it); the prototype strike path still decides strikes
**Reviewed by**: self, no independent review (focused /code-review pass by the implementing agent; unattended batch)
