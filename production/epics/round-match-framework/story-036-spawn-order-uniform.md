# Story 036: Per-slot spawn rank is uniform over 1..D across 1,000 fixed-secret round starts

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-011` (Single shuffled spawn list in one frame, server-secret order); GDD AC 29 (made concrete by the revised AC); formula F5 (`sigma`), rules R6 P6, R23 (a), (b).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams
**ADR Decision Summary**: 128-bit `ServerSecret`, in-project SipHash-2-4 as `H64`, counter-mode secret streams for hidden draws, PCG32 for the public layout stream; domain registry append-only. This story applies the `SpawnOrder` domain (value 6, secret, Round P6 spawn shuffle, `Attempt` = layout attempt): the spawn list is shuffled by Fisher-Yates over a canonically sorted array, drawn from `FSecretStream(GetServerSubSeed(SpawnOrder, LayoutAttempt), tag)` with `UniformBelow`.
**ADR Version**: ADR-0006 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (ADR-0006 knowledge risk)
**Engine Notes**: No engine API and no post-cutoff API is touched: the draw is plain integer arithmetic over in-project SipHash-2-4. The ADR-0006 verification item that applies is the committed golden-vector fixtures (SipHash reference vectors, seed derivation with a fixed test secret) running on CI; this story uses a fixed fixture secret through `IEntropySource`/the fixed-secret seam (story 002), never OS entropy or wall-clock time.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules, section Seeds and random streams):
- Required: Ranged draws use `UniformBelow`/`UniformInRange`; shuffles are Fisher-Yates over canonically sorted arrays (ADR-0006).
- Forbidden: Never use `FMath::Rand`, `FMath::FRand`, `FMath::RandRange`, `FRandomStream`, `std::rand`, `<random>` engines or time-seeded randomness in gameplay or server code (ADR-0006).
- Guardrail: Random streams: hidden draws well under 0.05 ms/frame (ADR-0006).

**Prototype disposition**: none specific. The prototype's `FRandomStream`-based randomness (ADR-0006 Alternative 1) is replaced, not ported; the spawn-order shuffle is new Round Core code.

---

## Acceptance Criteria

*From the revised ACs (AC 29 made concrete), scoped to this story:*

- [ ] GIVEN D = 10 (5v5 fixture), fixed fixture secrets from `RoundTestSeeds` and 1,000 round starts, THEN the spawn rank of each player slot is uniform on 1..10: chi-square with 9 degrees of freedom is below the fixed critical value 27.88 (alpha 0.001) for every slot (secrets and threshold fixed before the first run).

---

## Implementation Notes

*Derived from ADR-0006 sections 4, 5 and Implementation Guidelines, GDD F5 and R6 P6:*

- **Spawn order (F5).** `sigma = FisherYates(D, SubSeed(SpawnOrder, LayoutAttempt))` with `D = players + NpcCount`. The list handed to the shuffle is in a canonical order that does not depend on player-ness (sorted array of entries), the shuffle draws `UniformBelow` from an `FSecretStream` keyed by `GetServerSubSeed(SpawnOrder, LayoutAttempt)`, and `E[rank of a player pawn] = (D + 1) / 2` (30.5 at D = 60). The sub-seed is `SipHash(ServerSecret, 0x02 || MatchNonce || RoundIndex || Domain || Attempt)` (ADR-0006 section 3), so it varies with the secret, match nonce, round index and attempt.
- **Never the public seed.** The seed is public by design; every hidden decision draws from a server sub-seed (ADR-0006 guideline 2; GDD R23 (b)). No sub-seed, stream state or key is ever logged, traced or replicated (only `(Domain, Attempt, StreamTag, counter)` tuples may be logged).
- **Stream tag.** Stream tags are fixed `uint64` constants per consumer, built with `StealthRandom::Tag("...")` and documented in the consumer ADR. Open: confirm against ADR-0006 section 5 and ADR-0002 section 8.6 the spawn-order stream tag (neither names one); a new tag is recorded by ADR amendment.
- **Test shape.** With D = 10 (5v5 fixture), for each of 1,000 round starts compute `sigma` from the fixture secrets in `RoundTestSeeds`, tally the rank of each player slot per slot (expected count 100 per rank per slot), and compute chi-square with 9 degrees of freedom per slot; assert each is below 27.88. Secrets and threshold are fixture constants fixed before the first run (test constants file, story 005; no inline literals). Open: confirm against GDD F5 and ADR-0006 section 3 how the seed varies per start (the AC does not say; the derivation includes `RoundIndex` and `MatchNonce` as well as `Attempt`). The test is deterministic: no random seeds and no wall-clock reads.
- **Failure mode.** A slot with a missing rank still fails because the chi-square is large.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002 (`production/epics/round-match-framework/story-002-test-seam-fixed-server-secret.md`) and 005 (`production/epics/round-match-framework/story-005-test-fixture-constants.md`): the fixed-secret seam and the constants file (this story's dependencies).
- Story 037 (`production/epics/round-match-framework/story-037-spawn-order-ignores-playerness.md`): sigma identical when player flags are flipped and different under a different secret.
- Story 038 (`production/epics/round-match-framework/story-038-placement-invariant.md`): the F5 placement share invariant and `GlobalShare`.
- Story 039 (`production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md`): spawning all D pawns in one frame in sigma order.
- Session internals (`ServerSecret`, `GetServerSubSeed` implementation, `ESeedDomain` registry) and Character Pool's deal.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Start/SpawnOrder.h`
- `Source/StealthGameplay/Private/Round/Start/SpawnOrder.cpp`
- `Source/StealthGameplay/Private/Tests/Round/SpawnOrderUniformTest.cpp` (class `FRoundSpawnOrderUniformTest`, test name `StealthGame.Round.SpawnOrderUniform`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: The spawn rank of each player slot is uniform on 1..10, by chi-square below 27.88.
  - Given: D = 10 (5v5 fixture), fixed `RoundTestSeeds` secrets and 1,000 round starts.
  - When: the spawn rank of each player slot is tallied per slot over the 1,000 starts.
  - Then: for every slot, chi-square with 9 degrees of freedom is below 27.88 (alpha 0.001).
  - Edge cases: the expected count is 100 per rank per slot; the 1,000 starts differ by start index (Open: confirm against GDD F5 and ADR-0006 section 3 how the seed varies per start, the AC does not say); fix the secrets before the first run; a slot with a missing rank still fails because the chi-square is large.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/SpawnOrderUniformTest.cpp` (automation test `StealthGame.Round.SpawnOrderUniform`, class `FRoundSpawnOrderUniformTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.SpawnOrderUniform`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-002-test-seam-fixed-server-secret.md`, `production/epics/round-match-framework/story-005-test-fixture-constants.md`
- Unlocks: `production/epics/round-match-framework/story-037-spawn-order-ignores-playerness.md`, `production/epics/round-match-framework/story-038-placement-invariant.md`, `production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md`
- Cross-epic: `production/epics/session-travel` (`GetServerSubSeed`, `ServerSecret`, `ESeedDomain` registry; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (chi-square per slot below 27.88 for D = 10 over 1,000 starts on fixture secret S1)
**Deviations**: Open items resolved: starts differ by RoundIndex (folded into every sub-seed by ADR-0006 section 3); the stream tag is StealthRandom::Tag("RND_SPWN") (ADR-0006 section 5 amendment, decision log).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/SpawnOrderUniformTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: N/A - a statistical property of the server spawn order (no visible difference by design)
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
