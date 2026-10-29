# Story 001: Concealment tuning keys are registered and the F3 timing chain is validated at load

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (Tuning Knobs, F3, AC-CON-11 data part, AC-CON-12 whitelist part)
**Requirement**: `TR-con-005` (nearest registry entry for the transition-timing keys; no TR is specific to the tuning keys, so none is invented)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (key set and F3 chain), with ADR-0004 (tuning pipeline and validator; Accepted, not named in the epic)
**ADR Decision Summary**: Concealment owns eleven tuning keys registered under owner `Concealment`; the timing keys replicate as `All` because every client plays the clips, and the F3 chain (enter duration vs swap plus cull delay, exit crossing vs un-cull time, warp speed, strike crossing) is a pass/fail constraint set that the validator enforces, not independent safe ranges. No key may name dwell, capacity, stay, reveal or timeout (P4).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; this story is plain C++ data validation)
**Engine Notes**: No engine API is involved beyond the ADR-0004 pipeline. F3(d) reads the provisional SCL walk speed (140 cm/s) and capsule radius (35 cm); both are owned by Shared Character & Locomotion and must be injected, never hardcoded in the validator.

**Control Manifest Rules (this layer)**:
- Required: Gameplay values are data-driven through the tuning pipeline; Concealment reads referenced keys it does not own (`ConcealCullDelayMs`, `StrikeContactSec`, `ConcealedBoomCm`) and registers only its own — source: ADR-0004, ADR-0017
- Forbidden: Never add a dwell, capacity, reveal or timeout key — source: ADR-0017
- Guardrail: Validation runs at load only; zero per-frame cost

This story makes the timing chain a load-time gate. Every later story reads these keys; none reads a literal. The GDD's AC-CON-11 fixtures `BushExitCrossSec 0.2` and `BushEnterSwapSec 0.9` lie outside the per-key hard ranges, so the story tests the F3 validator directly with the range gate bypassed (or reported as a separate result); the GDD wording should be flagged to its owner but is not edited here.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (Tuning Knobs, AC-CON-11, AC-CON-12), scoped to this story:*

- [ ] **AC1** (derived, Tuning Knobs): **GIVEN** the tuning schema **WHEN** loaded **THEN** the 11 keys are registered with default, hard range, Audience and Latch equal to a checked-in fixture copied from the GDD Tuning Knobs table (Audience All: `BushEnterDurSec` 0.9, `BushEnterSwapSec` 0.5, `BushExitDurSec` 0.9, `BushExitCrossSec` 0.45, `BushStrikeEmergeSec` 0.35, `BushEntryReachCm` 60, `BushRustleAmplitudeCm` 10; ServerOnly: `BushSpotToleranceCm` 75, `BushReenterCooldownSec` 6, `BushExitDepenetrationCapCm` 30, `BushVictimMode` UniformHash; Latch Round except `BushRustleAmplitudeCm` = Use). For every numeric key the per-key range gate accepts min and max and rejects min-epsilon and max+epsilon (epsilon = 0.001 in the key's unit; the range gate only, F3 is checked in AC2). `BushVictimMode` accepts exactly UniformHash, EarliestCommitted and All and rejects any other value.
- [ ] **AC2** (AC-CON-11 data part, [U] `StealthGame.Concealment.TimingChain`): **GIVEN** the defaults **THEN** F3 (a) 0.9 ≥ 0.75, (b) 0.45 ≥ 0.25, (c) 0.35 ≥ 0.25, (d) 190 ≤ 210 cm/s, (e) 0.5 < 0.9 and 0.45 < 0.9, (f) 0.75 ≤ 1.2 all pass; **GIVEN** data sets `BushExitCrossSec` 0.2; `BushEnterSwapSec` 0.9 with `BushEnterDurSec` 0.8; `BushEntryReachCm` 100; `BushStrikeEmergeSec` 0.5 with `StrikeContactSec` 0.8, passed directly to the F3 validator (range gate bypassed, or range and F3 results reported separately), **THEN** the F3 output names exactly {b}, {a,e}, {d}, {f} respectively. Injected constants: `ConcealCullDelayMs` 250, v_walk 140 cm/s, r_c 35, `StrikeContactSec` 0.4, L99 150 ms, f_net 15 Hz, T_reopen 33 ms (T_uncull 0.25 s).
- [ ] **AC3** (AC-CON-12 whitelist part, [U] `StealthGame.Concealment.NoDwellKey`): **GIVEN** the Concealment key set **THEN** a case-insensitive whitelist check finds no key name containing dwell, capacity, stay, reveal or timeout; a planted fixture key named `BushDwellSec` is flagged (negative control).

---

## Implementation Notes

*Derived from ADR-0017 Decision §3 and GDD F3 / Tuning Knobs:*

Register the keys in the ADR-0004 schema under owner `Concealment` with TDC CR-15 columns (type, default, hard range, safe range, Audience, Latch, unit). Client-visible values go through `FConcealmentTuning_Public` (the ADR-0004 client face); the ServerOnly keys live in a server-only struct under `Public/Server/`. The F3 validator is a pure function over a key snapshot plus injected constants (`ConcealCullDelayMs`, v_walk, r_c, `StrikeContactSec`, L99, f_net, T_reopen) returning the set of violated constraints; the load path calls it after the per-key range gate and reports both. The safe ranges are not independent: (d) is the binding test (Reach 70 needs Swap ≥ 0.5; practical floor at Reach 60 is 0.46 s). `BushVictimMode` exists for playtest A/B only; `All` breaks P3 and `EarliestCommitted` makes NPC entrants cover, so the default must stay UniformHash.

No prototype code is replaced: the prototype has no concealment tuning (constants are inline in `StealthCharacter.cpp`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-hide-enter-execution-and-commit.md`: the swap-notify-equals-key check on the clip asset (AC-CON-11 clip part).
- `story-007-hide-legality-predicate.md`: use of the reach and cooldown keys.
- TDC epic: the schema generator and validator pipeline themselves.

---

## Files

*The code and test files this story creates or changes. `/dev-story` replaces the list with the files it changed.*

- `Source/StealthGameplay/Features/Concealment/Public/View/ConcealmentTuning.h` (`FConcealmentTuning_Public`)
- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentTuningServer.h`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentTimingValidator.cpp`
- tuning data rows per the TDC schema (location set by the TDC epic)
- `Source/StealthGameplay/Private/Tests/Concealment/TimingChainTest.cpp` (class `FConcealmentTimingChainTest`, name `StealthGame.Concealment.TimingChain`)
- `Source/StealthGameplay/Private/Tests/Concealment/NoDwellKeyTest.cpp` (class `FConcealmentNoDwellKeyTest`, name `StealthGame.Concealment.NoDwellKey`)

---

## QA Test Cases

*Written by qa-lead at story creation. QL-STORY-READY returned GAPS on the first draft; the rewrites above are the qa-lead's proposals applied unchanged (autonomous run, not re-gated), and the specs below are derived from them.*

- **AC1**: key registration and range gate
  - Given: the schema and a checked-in fixture copied from the GDD Tuning Knobs table.
  - When: the schema loads and each numeric key is probed at min, max, min-0.001 and max+0.001.
  - Then: defaults, ranges, Audience and Latch equal the fixture; min and max load, the out-of-range values are rejected; `BushVictimMode` accepts exactly its three enum values.
  - Edge cases: `BushEnterSwapSec` 0.3 passes the range gate (F3 is separate); a missing key is a load error.
- **AC2**: F3 chain
  - Given: the defaults, then each of the four failing data sets, with the injected constants listed in the AC.
  - When: the F3 validator runs with the range gate bypassed.
  - Then: defaults pass (a)-(f); the failing sets name exactly {b}, {a,e}, {d}, {f}.
  - Edge cases: Reach 70 with Swap 0.5 passes (d); Swap 0.45 at Reach 60 gives 211 cm/s and fails (d).
- **AC3**: no dwell key
  - Given: the registered Concealment key names plus a planted `BushDwellSec`.
  - When: the case-insensitive whitelist check runs.
  - Then: the real set is clean; the planted key is flagged.
  - Edge cases: mixed-case names (`bushStayMax`) are flagged.

---

## Test Evidence

*Governed by `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Concealment/TimingChainTest.cpp` and `NoDwellKeyTest.cpp` — must exist and pass

**Status**: [x] Created and passing (2026-10-29): StealthGame.Concealment.TuningRows, StealthGame.Concealment.TimingChain, StealthGame.Concealment.NoDwellKey; StealthGame.Tuning.SchemaComplete (Concealment pins); evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: N/A - headless core rule (no player-observable surface of its own); the integrated behaviour is observed in the batch's 3-player PIE run (con-*.png)

---

## Dependencies

- Depends on: TDC epic `production/epics/tuning-data-config/story-041-schema-generator-commandlet.md` (schema registration) and the TDC validator pipeline stories
- Unlocks: `story-004-hide-enter-execution-and-commit.md`, `story-005-hide-exit-execution-and-blocked-side.md`, `story-007-hide-legality-predicate.md`, `story-011-server-warp-to-anchor-and-exit-motion.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1-AC3 passing. The eleven keys (Public: BushEnterDurSec 0.9, BushEnterSwapSec 0.5, BushExitDurSec 0.9, BushExitCrossSec 0.45, BushStrikeEmergeSec 0.35, BushEntryReachCm 60, BushRustleAmplitudeCm 10; Server: BushSpotToleranceCm 75, BushReenterCooldownSec 6, BushExitDepenetrationCapCm 30, BushVictimMode) are schema rows equal to the GDD fixture; the F3 chain is six TDC invariant rows I-CON-F3a..f (EvaluateF3 / DescribeF3); no key names dwell/capacity/stay/reveal/timeout (planted names are flagged).
**Deviations**: Sub-structs live in StealthFoundation (FConcealmentTuning_Public / _Server; the TDC schema convention every feature followed). BushVictimMode is the schema's first enum key: SchemaGen, the fingerprint and ApplyF1 gained enum support (an undeclared enum value falls back to the default with DefaultedUndeclaredEnum). TuningSchemaVersion 8 -> 9 (fingerprint 0x21e2825b8dc4aec5). ConcealCullDelayMs is IVP-owned and not registered yet: a pending-owner constant (250 ms) read by I-CON-F3 and the cull, the BushStrikeEmergeSec pending constant removed (Assassination and Penalty invariants read the real row).
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
