# Story 037: Animation significance tier formula with hysteresis and hold

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R11 Significance-only animation LOD, F7 Animation significance tier; AC-SCL-26)
**Requirement**: `TR-scl-007` (significance-only tiers by screen height; caps; client-local tie key; server always-ticks pose)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0013: Animation: motion matching and significance budget (section 2, `FAnimTierCore`)
**ADR Decision Summary**: Tiering is a thin `UAnimSignificanceSubsystem` facade over `FAnimTierCore` (plain C++, no `UWorld`), which takes `(h, rendered, current tier, held time, TierTieKey)` per pawn plus the frozen client tuning and returns tiers. `h = H_fig / (2 · z · tan(FOV_v / 2))` with `H_fig` = 175 cm, `h = 1` if `z < 30 cm`, `h = 0` if not eye-mask-visible within the last 0.2 s. Promotion is immediate at `h ≥ T(1 + η)`; demotion only when `h < T(1 − η)` and the tier has been held `AnimTierMinHoldSec`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `FAnimTierCore` has no engine dependency, so this story runs headless. `H_fig` of 175 cm is nominal: the spike measured the in-use mesh at 165.6 cm and art bible open item OI-1 is open, so the character height is a fixture parameter. The "rendered" input is Camera's eye-mask flag (`IsEyeMaskVisible`, ADR-0015), never the engine's `WasRecentlyRendered` (development-only fallback while ADR-0015 is unbuilt; never shipped).

**Control Manifest Rules (this layer)**:
- Required: tier inputs are only `h` (view depth and FOV) and Camera's eye-mask flag; ties break on the client-local `TierTieKey` (ADR-0013).
- Forbidden: never let anything kind-varying enter a tier decision; never tier by world distance alone; never ship the `WasRecentlyRendered` fallback.
- Guardrail: tier subsystem at most 0.05 ms game thread.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-26** [U] `TierFormula`: GIVEN FOV 60°, each depth a separate trial starting in tier B with ≥ 0.5 s held, WHEN tiers are computed, THEN: 7.5 m h = 20.2 % stays B (promotion needs ≥ 20.52); 8.3 m 18.3 % B; 18 m 8.42 % B; 20 m 7.57 % stays B (demotion only below 7.36); 49 m 3.09 % C; 52 m starting in C 2.91 % stays C (demotion below 2.76); a demotion crossing at 0.4 s of hold causes no demotion; a promotion crossing at 0.1 s promotes at once; unrendered for 0.2 s gives h = 0 (D-culled); z = 20 cm gives h = 1 (tier A), no division by zero.

*QA clarification (create-stories 2026-10-25, not a GDD change):* character height is a fixture parameter, 175 cm nominal (art bible OI-1 open).

---

## Implementation Notes

*Derived from ADR-0013 section 2 and GDD F7:*

- Implement `FAnimTierCore` in `StealthGameplay/Core/Animation` (plain C++): threshold bands A ≥ 19 %, B 8-19 %, C 3-8 %, D below, with relative hysteresis `η = AnimTierHysteresisPct / 100` (8 %), demotion hold `AnimTierMinHoldSec` (0.5 s), promotion immediate, D-culled when unrendered for 0.2 s. Frozen client tuning is passed in; I-SCL-3 keeps the bands separated.
- Each AC depth is a separate trial starting in the stated tier with the stated hold; inject the character height as 175 cm.
- Caps and the tie key are story 038; applying tiers to meshes is story 039.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 038: caps and `TierTieKey`.
- Story 039: the world subsystem and external tick-rate application.
- Story 040: frozen-pawn guard.

---

## Files

- `Source/StealthGameplay/Private/Core/Animation/AnimTierCore.cpp` (plain C++, planned)
- `Source/StealthGameplay/Private/Tests/Locomotion/TierFormulaTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.TierFormula`.*

Test: tier formula (AC-SCL-26)
  Given: FOV 60°, character height as a fixture parameter (175 cm nominal), each depth a separate trial starting in tier B with ≥ 0.5 s held
  When: tiers are computed
  Then: 7.5 m h = 20.2 % stays B (promotion needs ≥ 20.52); 8.3 m 18.3 % B; 18 m 8.42 % B; 20 m 7.57 % stays B (demotion only below 7.36); 49 m 3.09 % C; 52 m starting in C 2.91 % stays C (demotion below 2.76); a demotion crossing at 0.4 s of hold causes no demotion; a promotion crossing at 0.1 s promotes at once; unrendered for 0.2 s gives h = 0 (D-culled); z = 20 cm gives h = 1 (tier A) with no division by zero
  Edge cases: the AC percentages back-solve to 175 cm; the spike measured the in-use mesh at 165.6 cm and art bible OI-1 is open, so the height is injected

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/TierFormulaTest.cpp`, test name `StealthGame.Locomotion.TierFormula` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-003-tuning-keys-registered.md`
- Unlocks: `production/epics/shared-character-locomotion/story-038-tier-caps-tie-key.md`, `story-039-tier-parity-ab.md`
