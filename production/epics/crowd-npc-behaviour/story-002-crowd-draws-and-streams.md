# Story 002: Crowd draws: counter-based streams, continuous mappings and kill independence

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-005 = Counter-based draws from the crowd server sub-seed.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0006 (seed domains, `FSecretStream` counter tuple, Lemire rejection; read only the sections on domains 5, 9, 13 and the unit-fraction note)
**ADR Decision Summary**: `FCrowdDraws` wraps one `FSecretStream(GetServerSubSeed(CrowdBehaviour, LayoutAttempt), Tag("CNB_BHVR"))` per round and exposes `Draw`, `DrawBelow`, `DrawWeighted` keyed by `(Ordinal, ECrowdDecision)` with per-ordinal decision counters held in the Director, so a pawn's k-th draw of a decision type is fixed no matter what else happened. Continuous samples map a 64-bit draw to a unit fraction and a closed-form inverse CDF (AS241 for the normal).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Pure C++ over integers and `double`; no post-cutoff API. Server-side only: cross-platform float identity is not required because no client recomputes these values, bit-identical reproduction is required for one build and seed.

**Why this story**: Every behavioural story in the epic draws from this layer, and AC-CNB-29 and AC-CNB-34 are properties of it alone. Doing it first fixes the draw tuple shape before any behaviour code can invent a second randomness path. Read GDD Core Rules CNB-R14 and the Acceptance Criteria AC-CNB-29 and AC-CNB-34.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: every behavioural draw is `FCrowdDraws::Draw`/`DrawBelow`/`DrawWeighted(Ordinal, Decision, ...)` on domain `CrowdBehaviour` (rejection retries extend the tuple, never the index); continuous samples use `UnitFraction53` and the named inverse CDFs with golden vectors; schedule draws use `AmbientSchedule` counter-as-Attempt; start placement uses `NpcRoutes`; decision counters are keyed by ordinal in the Director (ADR-0016)
- Forbidden: `FMath::Rand*` and `FRandomStream` in Crowd code (ADR-0016); never adapt crowd rates or profiles during a round
- Guardrail: draws are O(1) and allocation-free; counters live in a flat array keyed by ordinal

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-29 and AC-CNB-34, plus ADR-0016 section 9, scoped to this story:*

- [ ] GIVEN committed golden-vector fixtures, WHEN `UnitFraction53` and `InvNormalAS241` are evaluated, THEN outputs match bit-for-bit and u is never 0 or 1.
- [ ] (AC-CNB-29) GIVEN two fixed-timestep runs of one seed differing only by one extra NPC kill at t = 60 s, WHEN every other NPC's draws are compared by (ordinal, decision type, decision index), THEN for every non-involved NPC the k-th draw of each decision type is identical in both runs (decisions may differ in timing; draw values may not).
- [ ] (AC-CNB-34a) GIVEN two runs with the same seed and fixed timestep, WHEN all draws are logged, THEN they are bit-identical, and a takeover then release of one pawn leaves its later draws unchanged (counters keyed by ordinal in the Director).
- [ ] (AC-CNB-34b) GIVEN the Crowd source tree, WHEN the build check runs, THEN no `FMath::Rand*` or `FRandomStream` without a CNB key is found, and rejection retries extend the `Retry` field, never the `DecisionIndex`.

---

## Implementation Notes

*Derived from ADR-0016 section 9 and Implementation Guidelines:*

- `FCrowdDraws::Draw(Ordinal, Decision)` takes the next `DecisionIndex` for that `(Ordinal, Decision)` and returns `FSecretStream::Draw({Ordinal, uint64(Decision), DecisionIndex, Retry = 0})` (ADR-0006 section 5, mode 0x02). `DrawBelow(Ordinal, Decision, N)` applies Lemire rejection: each retry reads `{Ordinal, Decision, Index, Retry + 1}` with the same `DecisionIndex`.
- `ECrowdDecision : uint16` is append-only with fixed values (`TickPhase` = 1, `DeviceProfile` = 2, `LaneOffset` = 3, `Destination` = 4, then the list in ADR-0016 section 9). Never renumber.
- `UnitFraction53(Bits) = ((Bits >> 11) + 0.5) * 2^-53`; `InvNormalAS241` is Wichura PPND16. Exponential `-ln(1 - u)/lambda`, uniform `a + u(b - a)`, log-normal `exp(mu + sigma * PhiInv(u))`. Commit golden vectors under the test root as a fixture file (ADR-0006 rule 7).
- Counters belong to the Director, keyed by spawn ordinal, never to the controller (which is destroyed on possession). This story supplies the counter store type; story 004 owns it inside the Director.
- The build check is a source grep wired into the include lint (ADR-0008 mechanism); a string match in a comment must not trip it.
- Schedule draws (`AmbientSchedule`, counter-as-Attempt) and start placement (`NpcRoutes`) are used by stories 026 and 017; this story only guarantees `FCrowdDraws` does not touch those domains.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the prototype's `FMath::FRandRange` calls in `Source/StealthGame/Private/StealthNpcController.cpp` and `StealthGameMode.cpp` (`FindCrowdPoint`). Nothing is ported; the new code lives in `StealthGameplay/Features/Crowd/`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004 (`story-004-director-core-ledger-and-phase-gating.md`): the Director that owns the counter store
- Story 018 (`story-018-tell-sampler-and-calibration-counters.md`): compensated tell rates built on these draws
- Story 017 and 026: start placement and schedule draw domains

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdDraws.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdDraws.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdDrawsTest.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/Fixtures/CrowdGoldenVectors.json`

*Tests live in `Source/StealthGameplay/Private/Tests/Crowd/` once the ADR-0007 module skeleton exists; until then `Source/StealthGame/Private/Tests/Crowd/` (tests/README.md).*

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.GoldenVectors`
  - Given: committed golden-vector fixtures
  - When: `UnitFraction53` and `InvNormalAS241` are evaluated
  - Then: outputs are bit-identical and u is never 0 or 1
  - Edge cases: counter 0, max uint64, all-ones and all-zero hash words, tail inputs near 0 and 1
- **AC-2**: `StealthGame.Crowd.StreamsKillIndependent`
  - Given: two fixed-timestep runs of one seed, run B with one extra NPC kill at t = 60 s
  - When: draws are compared by (ordinal, decision type, decision index)
  - Then: for each non-involved NPC the k-th draw of each decision type is identical
  - Edge cases: victim and replacement excluded; compare only up to the shorter run's k; timing may differ
- **AC-3**: `StealthGame.Crowd.StreamDeterminism`
  - Given: two runs with the same seed and timestep, plus a variant with one takeover then release
  - When: all draws are logged
  - Then: logs are bit-identical and the released pawn's later draws are unchanged
  - Edge cases: takeover and draw in the same tick; counters keyed by ordinal in the Director
- **AC-4**: `StealthGame.Crowd.NoRawRandom`
  - Given: the Crowd source tree and a rejection-sampling fixture that forces retries
  - When: the build check runs and the fixture executes
  - Then: no `FMath::Rand*` or `FRandomStream` without a CNB key; retries extend Retry only and DecisionIndex stays unchanged
  - Edge cases: comments and strings excluded; several consecutive retries

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdDrawsTest.cpp` — must exist and pass (`FCrowdGoldenVectorsTest`, `FCrowdStreamsKillIndependentTest`, `FCrowdStreamDeterminismTest`, `FCrowdNoRawRandomTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None
- Unlocks: `production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md`, `production/epics/crowd-npc-behaviour/story-018-tell-sampler-and-calibration-counters.md`
