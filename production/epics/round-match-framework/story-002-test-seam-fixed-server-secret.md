# Story 002: Tests inject a fixed ServerSecret and therefore fixed sub-seeds

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-011` (single shuffled spawn list in one frame, server-secret order; nearest TR, approximate: the seam serves every secret-driven Round draw, i.e. the `SeqOrder` and `TieRank` permutations of R12 and the spawn order of R6/F5, not only spawn order). GDD AC: Test seams (binding on implementation), item 2. Rules: R12, R23 (via F1, F5).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams
**ADR Decision Summary**: 128-bit `ServerSecret`, in-project SipHash-2-4 as `H64`, counter-mode secret streams for hidden draws, PCG32 for the public layout stream; domain registry append-only. This story applies ADR-0006 Implementation Guideline 8: tests inject entropy through `IEntropySource`, so no test depends on OS entropy or wall-clock time, and the sub-seeds a test sees are derived from a fixed fixture secret.
**ADR Version**: ADR-0006 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: None touched. ADR-0006 depends on no engine API beyond integer arithmetic and one platform entropy call; this story never calls the entropy source for real (the fake `IEntropySource` returns the fixture secret). Its verification item (golden vectors for SipHash, PCG32, Mix32 and the seed derivation fixture on every CI platform) belongs to the session-travel epic stories listed under Dependencies.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Tests inject entropy through `IEntropySource` (ADR-0006)
- Forbidden: Never log, trace, replicate, put in a URL or expose to Blueprint `ServerSecret`, `MatchNonce`, server sub-seeds or stream keys (only digests and `(Domain, Attempt, StreamTag, counter)` tuples) (ADR-0006)
- Guardrail: Random streams: hidden draws well under 0.05 ms/frame (ADR-0006); the seam adds no per-draw cost
- Manifest 2026-10-25g re-read at implementation: it changes no Foundation queue or Event Log rule (its changes are the S10 replication rules)

**Why this story**: Round's order-sensitive logic (the per-tick intake permutation, the `TieRank` shuffle, spawn order) is drawn from server-secret streams, and the GDD wants statistical tests that are deterministic (fixed fixture secrets in `RoundTestSeeds`, fixed bounds, no flake rate). This story proves the injection point exists and that the same secret always gives the same sub-seeds while another secret gives different ones. Exit A seam story; stories 006, 036 and 043 consume it.

**Prototype disposition**: none (new Foundation code). The prototype draws from `FRandomStream`, which ADR-0006 forbids; the replacement primitives belong to the session-travel epic.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md`, "Test seams (binding on implementation)" item 2, scoped to this story (one criterion, `story_granularity: fine`):*

- [x] GIVEN a [U] test with a fixed ServerSecret, THEN the sub-seeds derived from it are identical across runs and a different secret gives different sub-seeds. `StealthGame.Round.FixedSecretSeam` [U]

---

## Implementation Notes

*Derived from ADR-0006 Key Interfaces and Implementation Guidelines 3 and 8, and GDD R12:*

- Build the seam from existing ADR-0006 primitives; do not add a second derivation path. Inject a fake `IEntropySource` (`virtual bool Fill(TArrayView<uint8> Out)`) that fills the 128-bit secret from a fixture. Derive sub-seeds through the same `GetServerSubSeed(ESeedDomain, Attempt)` route production uses (backed by `FSessionCore` on `USessionService`, ADR-0005), so the test exercises the real derivation, not a stand-in. Open: confirm against ADR-0006 Key Interfaces and ADR-0005 whether a test constructs `FSessionCore` directly (plain core) or a thin test helper wraps it; either way no `UWorld`.
- The fixture secrets are the `RoundTestSeeds` constants (story 005 owns the constant file and the scan that keeps the literals out of tests). Until story 005 lands, keep the two fixture secrets in the single test constant file named in the Files list so story 005 can adopt it; never inline them in the test body.
- Compare each derived sub-seed per purpose separately (domains used by Round: `SeqOrder` = 11, `TieRank` = 12, `SpawnOrder` = 6; the domain registry is append-only, ADR-0006 section 4). Assert on the sub-seed values themselves inside the test only; never write them to `UE_LOG`, the automation info output, a file or any trace (a sub-seed is secret even in a fixture, and a failing assertion message must not print one).
- Never use `FRandomStream`, `FMath::Rand*`, `<random>` or time-seeded randomness (ADR-0006 Guideline 1).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005 (`production/epics/round-match-framework/story-005-test-fixture-constants.md`): the scan that keeps `RoundTestSeeds` literals out of test bodies.
- Story 006 (`production/epics/round-match-framework/story-006-intake-fairness.md`) and story 036 (`production/epics/round-match-framework/story-036-spawn-order-uniform.md`): statistical tests that consume the fixed secret.
- Session-travel epic: the SipHash-2-4, PCG32, `FSecretStream`, seed-derivation golden vectors, the OS entropy call and the server-only access rule for `GetServerSubSeed`. This story only injects a fake entropy source into them.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Random/StealthServerSeeds.h` (`ESeedDomain` (ADR-0006 registry 1-17), `IEntropySource`, `IServerSubSeedSource`, `FStealthServerSeeds` (F5 sub-seed derivation core))
- `Source/StealthFoundation/Private/Random/StealthServerSeeds.cpp`
- `Source/StealthFoundation/Public/Server/Random/StealthSipHash.h` (`StealthRandom::SipHash24`)
- `Source/StealthFoundation/Private/Random/StealthSipHash.cpp`
- `Source/StealthFoundation/Public/Server/Random/StealthSecretStream.h` (`FSecretStream` (SipHash counter mode, `KStreamConst`))
- `Source/StealthFoundation/Private/Random/StealthSecretStream.cpp`
- `Source/StealthFoundation/Public/View/Random/StealthRandomTag.h` (`StealthRandom::Tag`)
- `Source/StealthFoundation/Public/View/Random/StealthRangedDraw.h` (`UniformBelow`, `UniformInRange`, `FisherYates`)
- `Source/StealthFoundation/Public/Server/StealthFaultSink.h` (appended `EntropyFailed`=9 and the queue/log faults 10-15)
- `Source/StealthFoundation/Public/Server/Testing/StealthQueueTestRig.h` (`FStealthFakeEntropySource`)
- `Source/StealthFoundation/Public/Server/Testing/StealthRoundTestConstants.h` (`RoundTestSeeds` S1, S2, S3)
- `Source/StealthFoundation/Private/Tests/Round/FixedSecretSeamTest.cpp` (test `StealthGame.Round.FixedSecretSeam`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN a [U] test with a fixed ServerSecret, THEN the sub-seeds derived from it are identical across runs and a different secret gives different sub-seeds.
  - Given: fixture secret S1 from `RoundTestSeeds`.
  - When: sub-seeds are derived twice, then once from fixture secret S2.
  - Then: the S1 sub-seeds are identical across both derivations.
  - Then: the S2 sub-seeds differ from S1's for every derived sub-seed.
  - Edge cases: both secrets are fixed fixtures, so there is no chance of a coincidental match; each sub-seed purpose is compared separately.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Round/FixedSecretSeamTest.cpp` — automation test `StealthGame.Round.FixedSecretSeam` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundFixedSecretSeamTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [x] Created and passing (`StealthGame.Round.FixedSecretSeam`, 2026-10-26)

---

## Dependencies

- Depends on: None (within this epic)
- Cross-epic: `production/epics/session-travel` (the seed primitives this seam injects into: `story-008-secret-stream-counter-independence.md`, `story-009-os-entropy-and-session-secret.md`, `story-010-seed-derivation-golden-vectors.md`, `story-011-server-sub-seed-server-only.md`; if those are not Done, the test may use the plain derivation core as soon as it exists)
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: `production/epics/round-match-framework/story-006-intake-fairness.md`, `production/epics/round-match-framework/story-036-spawn-order-uniform.md`, `production/epics/round-match-framework/story-043-no-draw-property.md`

---

## Completion Notes
**Completed**: 2026-10-26 (round-match-framework Exit A batch 001-017)
**Criteria**: 1/1 passing (`StealthGame.Round.FixedSecretSeam`)
**Notes**: The session-travel seed primitives (st-005..011) were not built yet, so the ADR-0006 primitives were implemented minimally in their ADR-named headers and checked against two SipHash-2-4 reference vectors; their golden-vector suites, the OS entropy source and the authority/thread/`FPublicOnlyScope` misuse checks stay with session-travel (decision log). `FStealthServerSeeds` is the plain core `FSessionCore` will own, so the test exercises the real derivation. Every domain 1-17 at four attempts: S1 identical across derivations, S2 different; values are compared in the test and never printed.
**Build/test**: see `production/qa/evidence/story-002-test-seam-fixed-server-secret/full-suite.txt`
