# Story 007: UniformBelow, UniformInRange, WeightedPick and Fisher-Yates are unbiased and edge-safe

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
**ADR Decision Summary**: A public Seed rebuilds the identical city on every machine (Mix32 public sub-seeds, integer PCG32 FIntStream); every hidden decision draws from SipHash-2-4 keyed server-secret streams (FSecretStream) derived from a 128-bit ServerSecret from the OS CSPRNG; never FRandomStream, FMath::Rand or floats in a draw.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API beyond plain integer arithmetic and one OS entropy call (BCryptGenRandom, bcrypt.lib linked under a Win64 guard). Golden vectors must pass on every CI platform and compiler (MSVC now, clang when a Linux dedicated server arrives).

**Why this story**: This story delivers `ranged-draw-helpers-unbiased` as one buildable behaviour: uniformBelow, UniformInRange, WeightedPick and Fisher-Yates are unbiased and edge-safe. It is traced to the GDD criterion below and to TR-session-004. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Formulas F5. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Every draw comes from FSecretStream (hidden decisions, seeded from GetServerSubSeed) or FIntStream (public geometry and decoration only); ranged draws use UniformBelow/UniformInRange; tests inject entropy through IEntropySource; golden vectors are committed and run on every CI platform (ADR-0006)
- Forbidden: FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand, <random> engines or time-seeded randomness in gameplay or server code; logging, tracing, replicating or URL-carrying ServerSecret, MatchNonce, server sub-seeds or stream keys; reusing or renumbering an ESeedDomain value; % on a draw, floats in a ranged draw, or an unordered container as draw input (ADR-0006)
- Guardrail: Hidden draws well under 0.05 ms/frame (ADR-0006); Session silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From ADR-0006 Validation Criteria (StealthGame.Random.UniformBelowUnbiased) and Decision section 6 (Lemire multiply-shift with rejection, edge cases), scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN a fixed stream, WHEN UniformBelow(n) is exhaustively checked for n up to 64 over a full 2^16-output window and chi-square tested at a fixed seed for n in {3, 24, 1000, 2^31-1}, THEN the draws are unbiased, UniformBelow(0) is refused (returns 0 through the misuse branch), UniformInRange handles a span that wraps to 0 by returning NextU32(), and Fisher-Yates over a canonically sorted array is deterministic (StealthGame.Random.UniformBelowUnbiased).

---

## Implementation Notes

*Derived from ADR-0006 Implementation Guidelines and the GDD rules named above:*

- Lemire multiply-shift: m = uint64(x) * n; the single % computes the rejection threshold (0u - n) % n in uint32, never the result. No % on a draw, no float, no unordered container as draw input (control manifest Forbidden).
- Helpers are templated on the stream type so FIntStream (public) and FSecretStream (server) share them.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): The prototype AStealthCityGenerator draws from the public seed with FRandomStream and float draws, which ADR-0006 forbids. This story builds the replacement primitive; the generator itself migrates under the procedural-city-generator epic. Nothing is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 008 (`story-008-secret-stream-counter-independence.md`): the secret stream
- consumer epics' own deals and shuffles

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/Random/StealthRangedDraw.h`
- `Source/StealthFoundation/Private/Tests/Random/UniformBelowUnbiasedTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS — 'Unbiased' has no numeric pass threshold (chi-square critical value or p); the fixed seed is not named and the stream behind the 2^16-output window is not fixed. (accepted and written as is in the unattended run; revise the criterion or run `/qa-plan` before `/dev-story`).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Random/UniformBelowUnbiasedTest.cpp` — automation test `StealthGame.Random.UniformBelowUnbiased` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [x] Created and passing (2026-10-26): Source/StealthFoundation/Private/Tests/Random/UniformBelowUnbiasedTest.cpp (StealthGame.Random.UniformBelowUnbiased)

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Random.UniformBelowUnbiased`, T1 unit test, class `FRandomUniformBelowUnbiasedTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-006-pcg32-int-stream-reference-vectors.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: `production/epics/session-travel/story-074-spawn-order-uncorrelated-with-player-ness.md`

## Completion Notes
**Completed**: 2026-10-26
**Criteria**: 1/1 passing
**Deviations**: Exhaustive window read as: a sweep stream whose 2^16 outputs span the 32-bit range evenly, bucket counts within 1 for every n <= 64 (decision logged). Chi-square at fixed seeds for n in {3, 24, 1000, 2^31-1 (64 bins)} below p=0.001 critical values (values precomputed: 0.11, 18.9, 989, 77.9). WeightedPick added to StealthRangedDraw.h (integer cumulative weights, binary search, refuses empty/zero/decreasing input).
**Test Evidence**: Source/StealthFoundation/Private/Tests/Random/UniformBelowUnbiasedTest.cpp (StealthGame.Random.UniformBelowUnbiased)
**Run result**: N/A — a pure server-side primitive/CI check with nothing on screen (no player-visible behaviour changes).
**Build + tests**: cycle.ps1 -Tests -NoLaunch: Result: Succeeded; 153/153 StealthGame.* automation tests passed (tests exit=0), 2026-10-26
**Reviewed by**: focused self code review (dev-story Phase 5), unattended run 2026-10-26
