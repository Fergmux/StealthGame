# Story 081: AC-23 TravelEpoch (multi-process)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-011` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-23 (Edge Cases: seamless travel)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 7 and Implementation Guideline 12); the multi-process tier is ADR-0008 section 1 (T3)
**ADR Decision Summary**: Seamless travel recreates the GameState, so the previous snapshot is discarded and reads invalid from travel start; the new GameState's snapshot is invalid until the freeze and carries the new `RoundIndex`. PlayerController and PlayerState persist, so no tuning state is stored on them. A `ClientHash` for the old epoch is ignored and never marks a participant Ready, and server tuning-ready bookkeeping, keyed by `ParticipantId`, is cleared at every epoch.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Seamless travel is disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`; travel results come from multi-process standalone (ADR-0005), so this story is T3 and is DEFERRED: multi-process. It needs the Gauntlet topology of story 020 (ADR-0008 Verification 3 [U]) and the seam availability in cooked Development processes (Verification 7 [U]). The seamless-travel spikes of ADR-0005 (S3, S4, S5) own the [U] claims about travel itself; this story assumes the travel mechanism the harness uses to `ServerTravel` between rounds and does not settle those claims. Seamless travel replaces the PlayerController and PlayerState in UE 5.8.4 (ADR-0005 E5).

**Control Manifest Rules (this layer)**:
- Required: server tuning-ready bookkeeping is keyed by `ParticipantId`, never replicated, cleared at every epoch — source: ADR-0004
- Required: T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map; travel scenarios are T3 — source: ADR-0008
- Forbidden: never prove multi-process criteria in in-process PIE only — source: ADR-0008
- Engine API constraint: seamless travel is disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`; travel results come from multi-process standalone — source: ADR-0005

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-23, scoped to this story:*

- [ ] **AC-23 (Edge, seamless travel)**: GIVEN a multi-process `ServerTravel` between rounds, WHEN the new GameState replicates, THEN all of the following hold:
  - the old snapshot reads as *invalid* from travel start;
  - the new snapshot carries the new `RoundIndex`;
  - a `ClientHash` for the old epoch never marks a participant `Ready`;
  - server tuning-ready bookkeeping is empty at the start of the new epoch.
  (`StealthGame.Tuning.MP.TravelEpoch`)

---

## Implementation Notes

*Derived from ADR-0004 sections 6-7, GDD CR-8, CR-9 and Edge Cases (seamless travel), ADR-0008 sections 1, 2, 5, and global amendment G1:*

- **Scenario.** `Source/StealthTestHarness/Private/Scenarios/TravelEpochScenario.cpp`, named scenario `TravelEpoch`, test name `StealthGame.Tuning.MP.TravelEpoch`, result JSON per story 018 with `replicationSystem=legacy` asserted (story 020). The plan lists no Gauntlet node script for this story: whether the generic node of story 020 drives it or a node is added is open (as in story 078); record at implementation. Build/ must not be ignored by `.gitignore` (story 024).
- **Observations required.** On a `ServerTravel` between two rounds (the stub Round, story 052, drives the round change): (1) the client's `GetClientTuning()` reads invalid from the moment travel starts, not only after the new GameState arrives (QA edge case: observe invalid at travel start); sample every frame across the travel; (2) the new GameState's snapshot carries the new `RoundIndex` once the freeze has run; (3) submit a `ClientHash` computed for the old epoch to the server in the new epoch; it must report not matching and the participant must not become Ready (the comparison is story 075's mechanism; here it is checked across a real travel); (4) at the start of the new epoch the server's tuning-ready bookkeeping is empty (read through a seam-guarded accessor on the store; whether a test accessor exists is open and is added under `WITH_STEALTH_TEST_SEAMS` if not).
- **Store survives travel.** The pipeline state and the preset name live in the game-instance store because `ServerTravel` recreates the GameMode and world subsystems (ADR-0004 section 6); do not move bookkeeping onto the GameMode, GameState, PlayerController or PlayerState. The travel URL's `?Preset=` versus the store is story 063, not this story.
- **No new production file.** The plan lists only the scenario file: the behaviours under test are implemented by stories 058, 066, 073 and 075; this story adds the multi-process observation. If an observation needs a production change, record it and add the file to this story's Files at `/dev-story`.
- **Deferral.** DEFERRED: multi-process (nightly) per the QA spec, runnable once stories 020, 073 and 052 are Done.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-075-stale-epoch-ignored.md`: stale-epoch comparison and bookkeeping in a single process
- `production/epics/tuning-data-config/story-063-preset-survives-travel.md`: the preset name surviving two `ServerTravel` round changes and the `?Preset=` precedence
- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the replicated property
- `production/epics/tuning-data-config/story-080-rejoin-snapshot.md`: a rejoin within one round
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: the Gauntlet host plus client smoke
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: the stubs that drive the round change

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Scenarios/TravelEpochScenario.cpp`
- `Build/Scripts/` node script, only if the story-020 node cannot drive this scenario (open)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: across a real `ServerTravel` the old snapshot reads invalid from travel start, the new one carries the new `RoundIndex`, an old-epoch hash never marks Ready, and bookkeeping starts empty
  - Given: a multi-process `ServerTravel` between rounds (DEFERRED: multi-process; test `StealthGame.Tuning.MP.TravelEpoch`)
  - When: the new GameState replicates
  - Then: the old snapshot is invalid from travel start; the new snapshot carries the new `RoundIndex`; a `ClientHash` for the old epoch never marks Ready; server tuning-ready bookkeeping is empty at the start of the new epoch
  - Edge cases: observe invalid at travel start, not only after

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the T3 scenario `Source/StealthTestHarness/Private/Scenarios/TravelEpochScenario.cpp` (`StealthGame.Tuning.MP.TravelEpoch`) run by the Gauntlet node must exist and pass (nightly, deferred multi-process), with its result JSON retained

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`
- Unlocks: None
- Open verification claims: ADR-0008 Verification 3 (Gauntlet topology) [U] and 7 [U]; ADR-0005's seamless-travel spikes (S3, S4, S5) own travel's own [U] claims and are not settled here. Open: node reuse and the seam accessor for the bookkeeping read.
