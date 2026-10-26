# Story 010: Round seed and sub-seed derivation match the F5 golden vectors

> **Epic**: Session & Travel
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-004 = Public seed vs server-secret keyed sub-seeds)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams
**Also relevant**: ADR-0005: Session, seamless travel and match-scoped state stores (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: A public Seed rebuilds the identical city on every machine (Mix32 public sub-seeds, integer PCG32 FIntStream); every hidden decision draws from SipHash-2-4 keyed server-secret streams (FSecretStream) derived from a 128-bit ServerSecret from the OS CSPRNG; never FRandomStream, FMath::Rand or floats in a draw.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API beyond plain integer arithmetic and one OS entropy call (BCryptGenRandom, bcrypt.lib linked under a Win64 guard). Golden vectors must pass on every CI platform and compiler (MSVC now, clang when a Linux dedicated server arrives).

**Why this story**: This story delivers `seed-derivation-golden-vectors` as one buildable behaviour: round seed and sub-seed derivation match the F5 golden vectors. It is traced to the GDD criterion below and to TR-session-004. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Formulas F5; Core Rules rule 14. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Every draw comes from FSecretStream (hidden decisions, seeded from GetServerSubSeed) or FIntStream (public geometry and decoration only); ranged draws use UniformBelow/UniformInRange; tests inject entropy through IEntropySource; golden vectors are committed and run on every CI platform (ADR-0006)
- Forbidden: FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand, <random> engines or time-seeded randomness in gameplay or server code; logging, tracing, replicating or URL-carrying ServerSecret, MatchNonce, server sub-seeds or stream keys; reusing or renumbering an ESeedDomain value; % on a draw, floats in a ranged draw, or an unordered container as draw input (ADR-0006)
- Guardrail: Hidden draws well under 0.05 ms/frame (ADR-0006); Session silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD AC 38, first part (the second part, SecretNeverExported, is story 013); ADR-0006 Validation Criteria also require redraw on a used seed and on a golden seed, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN Seed 123456789, THEN `Mix32` returns 2107646860 (domain 1, attempt 0), 523860287 (attempt 1) and 313478722 (domain 2) using `uint32` operands and logical shifts. Over a fake 99-round match, Seed stays in [1, 2^31-1] and never repeats. `ServerSubSeed` is unchanged when only Seed changes. `PublicSubSeed` refuses every domain except Layout. `SeedDerivationGolden` [U]

---

## Implementation Notes

*Derived from ADR-0006 Implementation Guidelines and the GDD rules named above:*

- Seed = (SipHash(ServerSecret, 0x01 || MatchNonce || RoundIndex || Draw) mod (2^31 - 1)) + 1, fixed-width little-endian; redraw Draw + 1 while Seed is in UsedSeeds or the committed GeneratorGoldenSeeds data file. The ServerSubSeed message is 0x02 || MatchNonce || RoundIndex || Domain || Attempt.
- Mix32 is constexpr and the three F5 vectors are static_asserted in its header so every compiler checks them at build time. ESeedDomain values are fixed and append-only (Layout = 1 ... BushSpotPick = 14); never renumber or reuse.
- Use a fixed test secret via the IEntropySource hook; the test uses no wall-clock time and no OS entropy.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Supersedes the prototype's in-place round restart (StealthGameMode.cpp StartRound/EndRound/auto-restart timer, lines 238-349) and the replicated StealthGameState::RoundSeed (StealthGameState.h:36-51). None of it is promoted. The prototype city seeds (29755, 8924, 25482, 17906, 13609, 19972, 13135) are smoke references only.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 011 (`story-011-server-sub-seed-server-only.md`): server-only guard and FPublicOnlyScope
- Story 013 (`story-013-secret-never-exported.md`): nothing secret leaves the server

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/Random/StealthMix32.h`
- `Source/StealthFoundation/Public/Server/Random/StealthServerSeeds.h`
- `Source/StealthFoundation/Private/Random/StealthServerSeeds.cpp`
- `Source/StealthFoundation/Public/Server/Session/StealthSessionService.h`
- `Source/StealthFoundation/Private/Session/StealthSessionService.cpp`
- `Source/StealthFoundation/Private/Tests/Session/SeedDerivationGoldenTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN Seed 123456789, THEN `Mix32` returns 2107646860 (domain 1, attempt 0), 523860287 (attempt 1) and 313478722 (domain 2) using `uint32` operands and logical shifts. Over a fake 99-round match, Seed stays in [1, 2^31-1] and never repeats. `ServerSubSeed` is unchanged when only Seed changes. `PublicSubSeed` refuses every domain except Layout. `SeedDerivationGolden` [U]
  - Given: Seed 123456789, and a fake 99-round match.
  - When: Mix32 is evaluated and seeds are derived each round.
  - Then: Mix32 returns 2107646860 (domain 1, attempt 0), 523860287 (attempt 1) and 313478722 (domain 2) using uint32 operands and logical shifts. Seed stays in [1, 2^31-1] and never repeats. ServerSubSeed is unchanged when only Seed changes. PublicSubSeed refuses every domain except Layout.
  - Edge cases: F5 redraws with Draw + 1 while Seed is in UsedSeeds or the generator golden set.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Session/SeedDerivationGoldenTest.cpp` — automation test `StealthGame.Session.SeedDerivationGolden` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [x] Created and passing (2026-10-26): Source/StealthFoundation/Private/Tests/Session/SeedDerivationGoldenTest.cpp (StealthGame.Session.SeedDerivationGolden)

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.SeedDerivationGolden`, T1 unit test, class `FSessionSeedDerivationGoldenTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-005-siphash24-reference-vectors.md`, `production/epics/session-travel/story-009-os-entropy-and-session-secret.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: `production/epics/session-travel/story-011-server-sub-seed-server-only.md`, `production/epics/session-travel/story-013-secret-never-exported.md`

## Completion Notes
**Completed**: 2026-10-26
**Criteria**: 1/1 passing
**Deviations**: Mix32 (Public/View/Random/StealthMix32.h, constexpr, three F5 vectors static_asserted). FStealthRoundSeedDrawer now draws F5 seeds through IServerSubSeedSource::GetRoundSeedCandidate (0x01 | Nonce | Round | Draw, mod 2^31-1, +1) with UsedSeeds and a golden-seed set (SetGoldenSeeds; the committed Generator golden list is procedural-city-generator story 031). Round's use (two consecutive starts draw different seeds, aborted starts never reuse) kept: StealthGame.Round.StartRollback passes. PublicSubSeed refuses non-Layout domains (fault + assert through the expected-error hook FStealthSeedMisuseExpectScope).
**Test Evidence**: Source/StealthFoundation/Private/Tests/Session/SeedDerivationGoldenTest.cpp (StealthGame.Session.SeedDerivationGolden)
**Run result**: N/A — a pure server-side primitive/CI check with nothing on screen (no player-visible behaviour changes).
**Build + tests**: cycle.ps1 -Tests -NoLaunch: Result: Succeeded; 153/153 StealthGame.* automation tests passed (tests exit=0), 2026-10-26
**Reviewed by**: focused self code review (dev-story Phase 5), unattended run 2026-10-26
