# Story 006: PCG32 integer stream reproduces the reference outputs

> **Epic**: Session & Travel
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
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

**Why this story**: This story delivers `pcg32-int-stream-reference-vectors` as one buildable behaviour: pCG32 integer stream reproduces the reference outputs. It is traced to the GDD criterion below and to TR-session-004. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Formulas F5. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Every draw comes from FSecretStream (hidden decisions, seeded from GetServerSubSeed) or FIntStream (public geometry and decoration only); ranged draws use UniformBelow/UniformInRange; tests inject entropy through IEntropySource; golden vectors are committed and run on every CI platform (ADR-0006)
- Forbidden: FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand, <random> engines or time-seeded randomness in gameplay or server code; logging, tracing, replicating or URL-carrying ServerSecret, MatchNonce, server sub-seeds or stream keys; reusing or renumbering an ESeedDomain value; % on a draw, floats in a ranged draw, or an unordered container as draw input (ADR-0006)
- Guardrail: Hidden draws well under 0.05 ms/frame (ADR-0006); Session silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From ADR-0006 Validation Criteria (StealthGame.Random.Pcg32Vectors) and Decision section 6, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN FIntStream seeded as pcg32_srandom_r(initstate 42, initseq 54), WHEN its first outputs are drawn, THEN they equal the reference PCG32 outputs on every CI platform, using only uint64/uint32 arithmetic with defined wrap, logical shifts, the rotation written (32u - rot) and 31u, and no float (StealthGame.Random.Pcg32Vectors).

---

## Implementation Notes

*Derived from ADR-0006 Implementation Guidelines and the GDD rules named above:*

- Public/View/Random is client-safe: it carries no server-only guard and may not include Public/Server/ headers (ADR-0007 include lint).
- Stage streams use StageSeed(s, j) = Mix32(AttemptSeed, s, j); that belongs to the generator epic, not here.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): The prototype AStealthCityGenerator draws from the public seed with FRandomStream and float draws, which ADR-0006 forbids. This story builds the replacement primitive; the generator itself migrates under the procedural-city-generator epic. Nothing is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 007 (`story-007-ranged-draw-helpers-unbiased.md`): ranged draws
- Story 010 (`story-010-seed-derivation-golden-vectors.md`): Mix32 and the F5 goldens

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/Random/StealthIntStream.h`
- `Source/StealthFoundation/Private/Random/StealthIntStream.cpp`
- `Source/StealthFoundation/Private/Tests/Random/Pcg32VectorsTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS — 'Its first outputs' gives no count and no expected values; only the seed pair (initstate 42, initseq 54) is fixed. (accepted and written as is in the unattended run; revise the criterion or run `/qa-plan` before `/dev-story`).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Random/Pcg32VectorsTest.cpp` — automation test `StealthGame.Random.Pcg32Vectors` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [x] Created and passing (2026-10-26): Source/StealthFoundation/Private/Tests/Random/Pcg32VectorsTest.cpp (StealthGame.Random.Pcg32Vectors)

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Random.Pcg32Vectors`, T1 unit test, class `FRandomPcg32VectorsTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: None within this epic
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: `production/epics/session-travel/story-007-ranged-draw-helpers-unbiased.md`

## Completion Notes
**Completed**: 2026-10-26
**Criteria**: 1/1 passing
**Deviations**: FIntStream (Public/View/Random/StealthIntStream.h, constexpr, integer only) reproduces pcg32-demo round 1 for (42, 54); the first output is also static_asserted through a constexpr evaluation.
**Test Evidence**: Source/StealthFoundation/Private/Tests/Random/Pcg32VectorsTest.cpp (StealthGame.Random.Pcg32Vectors)
**Run result**: N/A — a pure server-side primitive/CI check with nothing on screen (no player-visible behaviour changes).
**Build + tests**: cycle.ps1 -Tests -NoLaunch: Result: Succeeded; 153/153 StealthGame.* automation tests passed (tests exit=0), 2026-10-26
**Reviewed by**: focused self code review (dev-story Phase 5), unattended run 2026-10-26
