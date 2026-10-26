# Story 018: Tell sampler: compensated clocks, chain-gap guard and calibration counters

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-010 = Baseline and Tell Matrix rates as data with calibration counters.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0023 (telemetry section of the Event Log store, probes at Feature call sites, server-only statistics), ADR-0006 (unit-fraction mapping)
**ADR Decision Summary**: Rates and profiles come from the frozen tuning snapshot; draws are counter-based; continuous clocks map a 64-bit draw to a unit fraction and the exponential inverse CDF at F2's compensated rate. Calibration counters are server-only statistics written to the telemetry section, never to clients or logs.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0016; ADR-0023 is LOW)
**Engine Notes**: Pure C++ math. The telemetry section is an Event Log store API from ADR-0023; counters must never reach `UE_LOG`/`UE_LOGF` or any replicated field.

**Why this story**: The red-herring system works only if accepted NPC tell rates equal the F2 targets. The chain-gap guard (no NPC shows two different tells within 20 s) discards some draws, so the sampler must compensate its draw rate, and the counters are how the calibration gate (story 023) and later playtests see the real rates. Read GDD CNB-R7 (evidence compounds), F2 and AC-CNB-10, AC-CNB-31 (the gates run in story 023).

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: every behavioural draw is `FCrowdDraws::Draw`...; continuous samples use `UnitFraction53` and named inverse CDFs with golden vectors; rates from the frozen tuning snapshot (ADR-0016); telemetry statistics are server-only and probes are on the path every pawn takes (ADR-0023)
- Forbidden: adapting rates at runtime; routing crowd Event Log or counter data through `UE_LOG`/`UE_LOGF`; making a probe conditional on controller kind (ADR-0016, ADR-0023)
- Guardrail: sampler O(1) per NPC per tell; counters are plain integers

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` CNB-R7, F2 and ADR-0016 section 9, scoped to this story:*

- [x] The sampler draws each tell's exponential clock at F2's compensated rate via `UnitFraction53` and the inverse CDF; golden-vector tests cover the compensation computed from the interval keys and `CrowdTellChainGapSec`.
- [x] A draw inside an NPC's chain-gap is discarded and that tell's clock restarts at the gap end; an R8 reaction that would be a tell inside the responder's gap becomes a glance.
- [x] Calibration counters (accepted runs, vaults, climbs, braces, decoy tends, bush entries, reversals, long head holds) are written only to the server telemetry section (ADR-0023), never to clients or `UE_LOG`.

---

## Implementation Notes

*Derived from GDD CNB-R7 and F2:*

- F2 gives each tell's accepted target rate; the sampler draws at a compensated rate so that discarded in-gap draws do not lower the accepted rate. Compute the compensation from `CrowdTellChainGapSec` and the interval keys; commit golden vectors for each tell (run 405 s, vault 1443 s, climb 3409 s, brace 3000 s, decoy 13125 s, bush 300 s, reversal 395 s, head hold 600 s at defaults).
- Chain-gap guard: an NPC never produces two different Matrix tells (T1-T5, T12, T13) within `CrowdTellChainGapSec` (20 s) of each other. The discarded draw's clock restarts at the gap's end.
- Assassination strike rate is 0 by design (the intended confession), asserted by a constant, not a key.
- Counters go to the ADR-0023 telemetry section so story 023 and the playtest tooling can read accepted-rate statistics; no per-person form, no client visibility.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype crowd had no red herrings. New code.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Stories 019, 020, 021, 022: the behaviours each tell triggers
- Story 023: the 600-minute calibration run and property tests
- The telemetry epoch: the store API (ADR-0023)

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdTellSampler.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdTellSampler.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdTellSamplerTest.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/Fixtures/CrowdTellCompensationVectors.h`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.TellSamplerCompensation`
  - Given: the interval keys, `CrowdTellChainGapSec` and golden-vector fixtures
  - When: the sampler computes the compensated rate and draws each tell's exponential clock via `UnitFraction53` and the inverse CDF
  - Then: rates and draws match the fixtures bit-for-bit for every tell
  - Edge cases: minimum and maximum interval keys; u near 1
- **AC-2**: `StealthGame.Crowd.TellChainGapDiscard`
  - Given: an NPC inside its chain-gap, a due draw, and an R8 reaction that would be a tell
  - When: they are processed
  - Then: the in-gap draw is discarded and that tell's clock restarts at the gap end; the R8 reaction becomes a glance
  - Edge cases: a draw on the exact gap-end tick
- **AC-3**: `StealthGame.Crowd.CalibrationCountersServerOnly`
  - Given: calibration events of every counter type
  - When: telemetry, replication and `UE_LOG` output are captured
  - Then: counters appear only in the server telemetry section; none are replicated or logged
  - Edge cases: listen versus dedicated server; verbose log level

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdTellSamplerTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Tells.CompensationGolden`, `StealthGame.Crowd.Tells.ChainGap`, `StealthGame.Crowd.Tells.AcceptedRates`, `StealthGame.Crowd.Tells.CountersServerOnly`; 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-002-crowd-draws-and-streams.md`, `production/epics/crowd-npc-behaviour/story-003-crowd-tuning-keys-and-profile-data.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-019-run-traversal-reversal-herrings.md`, `production/epics/crowd-npc-behaviour/story-020-decoy-tending-herring.md`, `production/epics/crowd-npc-behaviour/story-021-brace-and-startle-herring.md`, `production/epics/crowd-npc-behaviour/story-022-bush-herring.md`, `production/epics/crowd-npc-behaviour/story-024-strike-stimulus-and-reactions.md`

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 3/3 passing
**Deviations**: The running clocks use EffectiveRatePerMin (F2 a_j with Lambda over the ENABLED tells): with only G6 built, the all-tells a_j over-ran its rate by 37 %; with all eight enabled it equals the golden a_j. No ADR-0023 telemetry store yet: counters go to the ICrowdTelemetrySink seam.
**Test Evidence**: `StealthGame.Crowd.Tells.CompensationGolden`, `StealthGame.Crowd.Tells.ChainGap`, `StealthGame.Crowd.Tells.AcceptedRates`, `StealthGame.Crowd.Tells.CountersServerOnly`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
