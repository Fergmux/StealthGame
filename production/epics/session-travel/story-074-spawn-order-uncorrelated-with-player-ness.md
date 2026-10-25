# Story 074: Spawn order is uncorrelated with player-ness over the 1,000-seed fixture

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-010 = Leak-surface checklist (PlayerState fields, ping, join timing) enforced via the allowlist)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams
**Also relevant**: ADR-0005: Session, seamless travel and match-scoped state stores (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: A public Seed rebuilds the identical city on every machine (Mix32 public sub-seeds, integer PCG32 FIntStream); every hidden decision draws from SipHash-2-4 keyed server-secret streams (FSecretStream) derived from a 128-bit ServerSecret from the OS CSPRNG; never FRandomStream, FMath::Rand or floats in a draw.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API beyond plain integer arithmetic and one OS entropy call (BCryptGenRandom, bcrypt.lib linked under a Win64 guard). Golden vectors must pass on every CI platform and compiler (MSVC now, clang when a Linux dedicated server arrives).

**Why this story**: This story delivers `spawn-order-uncorrelated-with-player-ness` as one buildable behaviour: spawn order is uncorrelated with player-ness over the 1,000-seed fixture. It is traced to the GDD criterion below and to TR-session-010. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 25(e); Core Rules rule 14. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Every draw comes from FSecretStream (hidden decisions, seeded from GetServerSubSeed) or FIntStream (public geometry and decoration only); ranged draws use UniformBelow/UniformInRange; tests inject entropy through IEntropySource; golden vectors are committed and run on every CI platform (ADR-0006)
- Forbidden: FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand, <random> engines or time-seeded randomness in gameplay or server code; logging, tracing, replicating or URL-carrying ServerSecret, MatchNonce, server sub-seeds or stream keys; reusing or renumbering an ESeedDomain value; % on a draw, floats in a ranged draw, or an unordered container as draw input (ADR-0006)
- Guardrail: Hidden draws well under 0.05 ms/frame (ADR-0006); Session silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 32, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN the 1,000 fixed seeds of the `SpawnOrderSeeds` fixture, each shuffling 10 players among 50 NPCs (`NpcCount` default since 2026-10-25), WHEN the player ranks are bucketed into deciles, THEN the chi-square statistic is < 21.67 (df 9, p = 0.01) and neither the first nor the last spawned actor is a player in more than 25% of shuffles (expected 10/60 = 16.7%; the earlier "20%" was the 40-NPC value). Deterministic because the seeds are fixed. `SpawnOrderUncorrelated` [U]

---

## Implementation Notes

*Derived from ADR-0006 Implementation Guidelines and the GDD rules named above:*

- Fixture: 1,000 fixed seeds each shuffling 10 players among 50 NPCs (NpcCount default since 2026-10-25); decile chi-square < 21.67 (df 9, p = 0.01); neither the first nor the last spawned actor is a player in more than 25 percent of shuffles (expected 16.7). Domain SpawnOrder (6) via FSecretStream + Fisher-Yates over a canonically sorted array. The production shuffle is Round's P6; this test owns the statistical contract.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): The prototype AStealthCityGenerator draws from the public seed with FRandomStream and float draws, which ADR-0006 forbids. This story builds the replacement primitive; the generator itself migrates under the procedural-city-generator epic. Nothing is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Round P6 spawn implementation

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Session/SpawnOrderUncorrelatedTest.cpp`
- `Source/StealthFoundation/Private/Tests/Data/SpawnOrderSeeds.inl`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN the 1,000 fixed seeds of the `SpawnOrderSeeds` fixture, each shuffling 10 players among 50 NPCs (`NpcCount` default since 2026-10-25), WHEN the player ranks are bucketed into deciles, THEN the chi-square statistic is < 21.67 (df 9, p = 0.01) and neither the first nor the last spawned actor is a player in more than 25% of shuffles (expected 10/60 = 16.7%; the earlier "20%" was the 40-NPC value). Deterministic because the seeds are fixed. `SpawnOrderUncorrelated` [U]
  - Given: The 1,000 fixed seeds of the SpawnOrderSeeds fixture, each shuffling 10 players among 50 NPCs.
  - When: Player ranks are bucketed into deciles.
  - Then: The chi-square statistic is < 21.67 (df 9, p = 0.01), and neither the first nor the last spawned actor is a player in more than 25% of shuffles (expected 10/60 = 16.7%).
  - Edge cases: Deterministic because the seeds are fixed.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Session/SpawnOrderUncorrelatedTest.cpp` — automation test `StealthGame.Session.SpawnOrderUncorrelated` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.SpawnOrderUncorrelated`, T1 unit test, class `FSessionSpawnOrderUncorrelatedTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-007-ranged-draw-helpers-unbiased.md`, `production/epics/session-travel/story-008-secret-stream-counter-independence.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Cross-epic: Epic `round-match-framework` (`production/epics/round-match-framework/EPIC.md`) consumes Session's events and pushes the admission mode; use a fake Round behind the consumer-owned interfaces here.
- Unlocks: None
