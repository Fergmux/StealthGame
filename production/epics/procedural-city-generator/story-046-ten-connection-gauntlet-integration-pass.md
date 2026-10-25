# Story 046: Ten-connection Gauntlet integration pass

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: spikes R1 and R4 (generator-scale: stories 041-043; ADR-0022 R1, ADR-0014 R4) have not reported; without them a 10-connection run would measure an unproven budget. The pass is also only meaningful after the gameplay epics it exercises are Done (it is the epic's exit criterion, last in build order). (The net-emulation spikes have reported: production/qa/spikes/2026-10-25_SCL045-net-emulation-traversal.md and, for Session, production/qa/spikes/2026-10-25_session-net-emulation.md, 2026-10-25.)
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md` (the pass exercises AC-G24, AC-G36 and R-G9 at scale); epic exit criterion in `production/epics/procedural-city-generator/EPIC.md`; `production/milestones/M1-mvp-core.md` (10-connection round, Gauntlet run); `production/estimates/2026-10-25_mvp-rebaseline.md` (10-player integration pass, about 6 days with the real-network session, story 047)
**Requirement**: `TR-citygen-003`, `TR-citygen-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. The pass is a cross-cutting verification owned by this epic by decision (create-epics 2026-10-25): net-emulation pass to Shared Character & Locomotion, 50-character perf pass to Crowd, 10-player integration pass to the Generator, MVP exit evidence to HUD.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (ADR-0014 and ADR-0022 for what is measured)
**ADR Decision Summary**: Multi-process criteria are proved in Gauntlet T3 runs of a cooked Development build (listen host plus N clients on one machine), never in in-process PIE only; every T2/T3 test bounds by frames or server time and records the active replication system; the leak-parity diff over opponent-client captures must show no identity tell.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0008; Gauntlet topology on one machine is [U], ADR-0008 verification item 3)
**Engine Notes**: One cooked Development listen host plus 9 clients under Gauntlet (`StealthGame.CityGen.MP.TenPlayerRound`); node names follow `StealthGame.<System>.MP.<Scenario>`. The prototype's 10-player run (seed 13135) was in-process, not over a network: this pass replaces that evidence. Frames over 33 ms are only a failure when the hardware meets the ADR-0022 reference spec; otherwise the check reads NOT ASSESSED (never a pass).

**Control Manifest Rules (this layer)**:
- Required: Seamless-travel and multi-process evidence only from T3 Gauntlet runs of a cooked Development build; T2/T3 tests assert and record the active replication system — source: ADR-0008
- Forbidden: Never prove multi-process criteria in in-process PIE only; never report a performance check as passed on non-reference hardware — source: ADR-0008, ADR-0022
- Guardrail: 0 crashes in any retained 10-player round; listen-host game thread p95 <= 12.0 ms on 3060-class hardware (RC-1 input, non-binding elsewhere) — source: M1 milestone, ADR-0022

---

## Acceptance Criteria

*From the epic exit criterion, the M1 milestone success criteria and GDD AC-G24/AC-G36, scoped to this story:*

- [ ] **AC1** (epic exit criterion, M1 milestone, `MP.TenPlayerRound`): **GIVEN** a cooked Development listen host plus 9 clients (10 connections, 5v5) under Gauntlet on >= 3 generated seeds (one under a net-emulation profile) **THEN** every connection completes the round to `Ended`, all 10 report the same `LayoutHash`, and there are 0 crashes.
- [ ] **AC2** (AC-G36 at scale, `MP.TenPlayerFrozenNavAndFrames`): **GIVEN** the run **THEN** the navmesh tile build and dirty-area counters are unchanged after `OnLayoutReady` through `Ended` on every seed, the multi-client leak-parity diff shows 0 differences against the ADR-0008 allowlist, and the 33 ms frame check is recorded and reads NOT ASSESSED unless host and clients meet the ADR-0022 reference spec.
- [ ] **AC3** (RC-1 input, evidence): **GIVEN** the run **THEN** `production/qa/evidence/` holds one screenshot per connection at the briefing, the first `Active` frame and `Ended` (30 for 10 connections), plus the harness result JSON, which records the listen-host game-thread p95 against 12.0 ms (non-binding where no 3060-class host exists, with the sampling window stated).

---

## Implementation Notes

*Derived from the epic, M1 milestone and ADR-0008:*

Build a Gauntlet node on the harness from story `tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: launch the cooked Development host and 9 clients on one machine, `LayoutProviderMode` `GeneratedWithFallback`, drive the round to `Ended` (bots for non-human slots through the StealthBot-style driver of the production architecture, not the prototype console commands), on >= 3 generated seeds chosen from fresh seeds (never golden seeds: golden seeds are test data and practice excludes them, R-G11.5) with one run under a net-emulation profile (`-PktEmulationProfile`, `StealthLeak2`/`StealthClean` profiles from TDC story 017). Each connection writes its `LayoutHash`; the JSON result records: per-seed result, the 10 hashes, tile build counter and dirty-area count at `OnLayoutReady`, `HandedOff` and `Ended`, per-frame game-thread maxima, host game-thread p95 over a stated window (the `Active` phase per seed), the replication system in use and the hardware spec check. The leak-parity diff is ADR-0008's multi-client diff over the opponent captures; its allowlist is the replication allowlist. A mid-round disconnect fails the run; per-seed results go in the JSON. Screenshots: 30 named by connection and moment (briefing, first Active frame, Ended). A recorded frame over 33 ms on reference hardware is a failure; on other hardware the entry reads NOT ASSESSED.

Prototype replaced: the prototype's 10-player functional run (5v5, seed 13135, in-process) is feasibility evidence only; this pass replaces it as the integration proof.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-047-ten-player-real-network-session-and-sign-off.md`: the real-network (separate machines) session and sign-off.
- `story-035-navmesh-frozen-after-hand-off.md`, `story-033-cross-client-manifest-parity-under-loss.md`: the smaller-scale PIE versions of the same checks.
- The PROCEED/PIVOT D38 measurement: M1 milestone and the HUD epic's exit evidence.
- The 50-character perf pass: `crowd-npc-behaviour/story-038-performance-pass-50-60-character.md`; the net-emulation pass: `shared-character-locomotion/story-055-net-emulation-pass-exit.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/CityGen/TenPlayerRoundMpTest.cpp` (Gauntlet node `StealthGame.CityGen.MP.TenPlayerRound`)
- `Build/Scripts/StealthGame.CityGen.TenPlayerRound.Automation.cs` (Gauntlet test node; note ADR-0009's `.gitignore` warning about `build/`)
- `production/qa/evidence/[date]_citygen-10conn-*.png` (30 screenshots) and `production/qa/evidence/[date]_citygen-10conn-result.json`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 ADEQUATE; AC2 and AC3 revised and ADEQUATE; DEFERRED until the gameplay epics and reference hardware exist).*

- **AC1** `StealthGame.CityGen.MP.TenPlayerRound`
  - Given: a cooked Development build, a listen host plus 9 clients (5v5), and >= 3 generated seeds. One seed runs under the net-emulation profile.
  - When: Gauntlet runs each seed.
  - Then: all 10 connections reach `Ended` within the failure-ceiling timeout. All 10 report the host's `LayoutHash`. 0 crashes (exit code and crash dumps).
  - Edge cases: a mid-round disconnect fails the run. Per-seed results go in the JSON.
- **AC2** `StealthGame.CityGen.MP.TenPlayerFrozenNavAndFrames`
  - Given: the 10-connection Gauntlet run on >= 3 seeds.
  - When: frame times, navmesh counters and the leak-parity diff are collected.
  - Then: the nav tile build and dirty-area counters are unchanged after `OnLayoutReady` through `Ended` on every seed. The multi-client leak-parity diff shows 0 differences against the ADR-0008 allowlist. The 33 ms check is recorded, and it reads NOT ASSESSED unless host and clients meet the ADR-0022 reference spec. It is never reported as a pass in that case.
  - Edge cases: the hardware spec check is recorded in the JSON. A recorded value over 33 ms on reference hardware is a failure.
- **AC3** evidence
  - Given: the same run.
  - When: evidence is collected.
  - Then: `production/qa/evidence/` holds 30 screenshots (10 connections x briefing, first Active frame, `Ended`) named by connection and moment, plus the harness result JSON. The JSON records the listen-host game-thread p95 against 12.0 ms. That figure is non-binding where no 3060-class host exists.
  - Edge cases: state the sampling window for the p95 in the JSON, for example the Active phase per seed. A missing screenshot makes the evidence incomplete.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the Gauntlet T3 test, its result JSON and the retained screenshots in `production/qa/evidence/` (the screenshots are not waived at any `qa.level`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-031-golden-seeds-and-bump-rule.md`, `story-033-cross-client-manifest-parity-under-loss.md`, `story-034-fallback-short-circuit.md`, `story-035-navmesh-frozen-after-hand-off.md`, `story-041-client-build-time-and-first-frame-completeness.md`, `story-042-attempt-time-budget-and-t-cand-measurement.md`, `story-043-kit-gpu-cost-and-server-spawn-nav-time.md`, `story-045-retire-the-prototype-generator.md`; cross-epic: the Foundation and Core epics' Done state, `crowd-npc-behaviour`, `assassination-defence-reversal`, `objectives-chain`, `tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`, `shared-character-locomotion/story-045-net-emulation-traversal-spike.md`
- Unlocks: `production/epics/procedural-city-generator/story-047-ten-player-real-network-session-and-sign-off.md`
