# Story 014: Rival signal delay draw and release grid

> **Epic**: Objectives Chain
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-28

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-12 the rival race signal, F4 correlation bound; AC-OC-09 grid clause, AC-OC-10)
**Requirement**: `TR-oc-006` (`RivalPrelimCount` with a fresh per-publication delay and one global release grid)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision section 7 steps 1-2)
**Other ADRs**: ADR-0006: Seeds and deterministic/secret random streams (`ESeedDomain::StageSignal = 10`, `FSecretStream`, tag `OC_STAGE`, integer-only arithmetic)
**ADR Decision Summary**: On each counted preliminary completion the core increments a round-global ordinal `k` (both teams together, 1-based) and draws `Delta_ms = UniformInRange(FSecretStream(GetServerSubSeed(StageSignal, k), Tag("OC_STAGE")), MinMs, MaxMs)`, keyed by `k` only, never by team, step, completer or court. The release time is `t_rel_ms = t0_ms + QMs * ceil((t_c_ms + Delta_ms - t0_ms) / QMs)` in integer milliseconds (`int64`), anchored at Round's `ActivePhaseStart`. The grid is shared by both teams, so a release time carries no team-specific timing.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0006: integer SipHash/PCG primitives in project code, no engine API beyond arithmetic)
**Engine Notes**: ADR-0006 requires golden vectors on every CI platform and compiler; Objectives adds its own pinned vectors for `k = 1..3` at a fixed test secret. Bounds are converted to whole milliseconds once at tuning freeze (ADR-0004 / story 001), so no float enters the draw. `FRandomStream` and `FMath::Rand*` are never used.

**Control Manifest Rules (this layer)**:
- Required: The rival delay is drawn only from `FSecretStream(GetServerSubSeed(StageSignal, Ordinal), Tag("OC_STAGE"))` in integer milliseconds; release times lie on the grid `t0 + k·Q` anchored at `ActivePhaseStart` — source: ADR-0020
- Forbidden: Never use per-team timers, a fixed delay or the engine timer manager for the rival signal — source: ADR-0020
- Forbidden: Never use `FRandomStream`/`FMath::Rand` for any hidden draw — source: ADR-0006
- Guardrail: draw and grid arithmetic are O(1) integer operations — source: ADR-0020

The rival signal is the only pressure a stalled team feels ("they are ahead") without any reveal, and it must not confirm a sighting (Pillar 1): a player who watched a rival tend a court must not be able to match that tend to a release. Three design choices achieve that and this story implements them as a pure function: a fresh delay per event (so the delay is not a per-team tell), keyed only by the round-global completion ordinal (so who completed, and where, never changes the draw: AC-OC-10), and one global release grid shared by both teams (so the tick a release lands on says nothing about which visit caused it). F4 states the bound the three choices must meet: `LR_sig <= 2.4`, which needs `W_eff >= 163 s` at the slow-round `lambda_c` of 0.0033; story 001 owns that invariant, this story consumes the validated bounds.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-09 grid clause, AC-OC-10) and ADR-0020 section 7 steps 1-2, scoped to this story:*

- [x] **AC1** (AC-OC-09 [U] `RivalSignalReleaseGrid`, math): **GIVEN** a pinned `StageSignal` sub-seed and team 1 completing `Prelim1` at `t_c` = 100 s (`Active` at t = 0) **THEN** team 0's `RivalPrelimCount` becomes 1 exactly at `t_rel` = 30 * ceil((100 + Delta_1) / 30) s with `Delta_1` in [30, 165], never earlier, and only at a multiple of 30 s from `Active`.
- [x] **AC2** (AC-OC-10 [U] `RivalSignalDistribution`): **GIVEN** 1,000 pinned sub-seeds and scripted rounds that vary which team, completer and court produce the k-th preliminary completion **THEN** `Delta` is uniform on [30, 165] (Kolmogorov-Smirnov p > 0.01, per team), `Delta` for ordinal k is identical whichever team, completer or court produced it (the draw is keyed by ordinal only), and every release time of both teams lies on the one 30 s grid from `Active`. (Satisfies AC-IVP-40.)
- [x] **AC3** (ADR section 7 steps 1-2 [U] `RivalSignalGolden`): all arithmetic is integer milliseconds (bounds converted once at tuning freeze); the draw is `UniformInRange(FSecretStream(GetServerSubSeed(StageSignal, k), Tag("OC_STAGE")), MinMs, MaxMs)`; the ceiling-division numerator is asserted positive (`t_c >= t0` and `Delta_ms >= 30,000`); results for `k` = 1..3 equal pinned golden vectors.

---

## Implementation Notes

*Derived from ADR-0020 Decision section 7 and ADR-0006:*

Pure functions in `FObjectivesCore` (no clock, no queue): `DrawRivalDelayMs(Ordinal k, ISecretDraw&, MinMs, MaxMs)` and `ComputeReleaseMs(t0_ms, t_c_ms, Delta_ms, QMs)`. `t_c_ms = llround(ServerNow() * 1000)` at the completion slot and `t0_ms = llround(ActivePhaseStart * 1000)`; the numerator `(t_c + Delta - t0)` is always positive (asserted), and the integer ceiling division is done in `int64`. Equal integer-ms release times give identical double due times, so releases landing on one grid tick resolve in one drain (story 015). The unbiased ranged draw is ADR-0006's `UniformInRange` (multiply-shift rejection), never `mod`. The `ISecretDraw` interface is the Objectives-local seam over Session's `ServerSubSeed(StageSignal, k)`; the real implementation comes from the Session epoch's secret-stream stories, a stub supplies fixed streams in tests.

The golden vectors for `k = 1..3` must be derived once, at the fixed test secret, and independently reviewed (a second person or a re-derivation by a separate tool) before they are pinned; a self-generated vector that only ever agrees with itself tests nothing. For AC2, use a constant list of 1,000 pinned sub-seeds so the Kolmogorov-Smirnov test is deterministic, and check ordinals 1..4. Keep the draw free of any team, participant or court input in its signature so the "keyed by ordinal only" property is structural, not just tested.

Defaults: `RivalSignalDelayMinSec` 30, `RivalSignalDelayMaxSec` 165, `RivalSignalReleaseQuantumSec` 30 (story 001). Mean delay is Min + W_eff/2 (about 112 s) and the latest release is Max + Q = 195 s.

Prototype replaced: none (the prototype had no rival signal; the GDD notes the earlier `RivalStageStep` keyed by team and step was denied by IVP R3 and withdrawn).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-015-rival-signal-release-scheduling.md`: scheduling the `TimerFired(StageSignal)` item, the resolver, the count, the latch and idle behaviour.
- `story-001-objectives-tuning-rows-and-invariants.md`: the I-OC2 invariant on `W_eff`.
- Session epic: `ServerSubSeed` and `FSecretStream` (`story-011-server-sub-seed-server-only.md`, `story-008-secret-stream-counter-independence.md`).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Features/Objectives/Private/ObjectivesRivalSignal.h / .cpp` (DrawRivalDelayMs, ComputeReleaseMs, ToMs)
- `Source/StealthGameplay/Features/Objectives/Public/Server/ObjectivesSeams.h` (IObjectivesSecretDraw keyed by ordinal only, FObjectivesRivalRelease)
- `Source/StealthGameplay/Features/Objectives/Private/ObjectivesCore.cpp` (bounds to ms at SetTuning, ScheduleRivalSignal)
- `Source/StealthGame/Private/Composition/StealthObjectivesWiring.cpp` (GetServerSubSeed(StageSignal, k) over Session)
- `tools/analysis/oc_rival_signal_golden.py` (independent re-derivation of the golden vectors)
- `Source/StealthGameplay/Private/Tests/Objectives/RivalSignalReleaseGridTest.cpp, RivalSignalDistributionTest.cpp, RivalSignalGoldenTest.cpp, ObjectivesRivalRig.h`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; golden vectors should be derived and reviewed independently). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`RivalSignalReleaseGrid`): release lands on the grid
  - Given: pinned `StageSignal` sub-seed; fake clock; Active at t = 0; team 1 completes Prelim1 at t_c = 100 s; stub release observer
  - When: the release is scheduled and the clock advances
  - Then: team 0 `RivalPrelimCount` becomes 1 exactly at t_rel = 30 x ceil((100 + Delta_1)/30) s with Delta_1 in [30, 165]; never earlier; only at a multiple of 30 s
  - Edge cases: 100 + Delta exactly on a multiple of 30 (no extra step); Delta = 30 and Delta = 165 bounds; assert t_rel > t_c + 29.999.
- **AC-2** (`RivalSignalDistribution`): uniform and ordinal-keyed
  - Given: 1,000 pinned sub-seeds from a constant list; scripted rounds varying team, completer and court for the k-th completion
  - When: Delta is drawn for each
  - Then: Delta uniform on [30, 165] (KS p > 0.01 per team); Delta for ordinal k identical whichever team, completer or court; all release times of both teams on the one 30 s grid from Active
  - Edge cases: pinned seeds make the KS test deterministic; k = 1..4 all checked; satisfies AC-IVP-40.
- **AC-3** (`RivalSignalGolden`): integer math and golden vectors
  - Given: tuning frozen to integer milliseconds; `FSecretStream(GetServerSubSeed(StageSignal, k), Tag("OC_STAGE"))`
  - When: the draw runs for k = 1..3
  - Then: draw is `UniformInRange(stream, MinMs, MaxMs)` in integer ms; ceiling-division numerator asserted positive; results equal pinned golden vectors for k = 1..3
  - Edge cases: golden values should be derived once and independently reviewed; negative numerator triggers the assertion; no floating point in the path.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [x] Created and passing (2026-10-28): StealthGame.Objectives.RivalSignalReleaseGrid, RivalSignalDistribution, RivalSignalGolden; evidence production/qa/evidence/objectives-chain-013-023-2026-10-28/

Run result: N/A - pure draw and grid arithmetic (the released count is observed in story 015 / the run-and-observe of this batch)

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md`; cross-epic: `production/epics/session-travel/story-011-server-sub-seed-server-only.md`, `production/epics/session-travel/story-008-secret-stream-counter-independence.md`, `production/epics/session-travel/story-005-siphash24-reference-vectors.md`; `production/epics/information-visibility-policy/story-050-rival-signal-shape.md`
- Unlocks: `production/epics/objectives-chain/story-015-rival-signal-release-scheduling.md`

## Completion Notes
**Completed**: 2026-10-28
**Criteria**: AC1-AC3 passing. Golden vectors k = 1..3 at the fixed test secret (0x10..0x27) re-derived by tools/analysis/oc_rival_signal_golden.py (own SipHash-2-4 with the reference vector check, own sub-seed derivation and Lemire draw): 44 172 / 66 957 / 112 399 ms. KS p = 0.117 per team over 4 000 draws; 0 ordinal mismatches between mirrored scripts; 0 off-grid releases.
**Deviations**: the AC1 boundary case pins a sub-seed whose t_c + Delta lands 1-100 ms past a tick (the exact-tick case is pinned arithmetically in RivalSignalGolden).
**Test Evidence**: production/qa/evidence/objectives-chain-013-023-2026-10-28/
**Reviewed by**: self code review 2026-10-28 (notes in the evidence)
