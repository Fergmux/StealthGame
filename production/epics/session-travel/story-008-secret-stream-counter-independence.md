# Story 008: FSecretStream counter draws are independent of interleaved draws

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-004 = Public seed vs server-secret keyed sub-seeds)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams
**ADR Decision Summary**: A public Seed rebuilds the identical city on every machine (Mix32 public sub-seeds, integer PCG32 FIntStream); every hidden decision draws from SipHash-2-4 keyed server-secret streams (FSecretStream) derived from a 128-bit ServerSecret from the OS CSPRNG; never FRandomStream, FMath::Rand or floats in a draw.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API beyond plain integer arithmetic and one OS entropy call (BCryptGenRandom, bcrypt.lib linked under a Win64 guard). Golden vectors must pass on every CI platform and compiler (MSVC now, clang when a Linux dedicated server arrives).

**Why this story**: This story delivers `secret-stream-counter-independence` as one buildable behaviour: fSecretStream counter draws are independent of interleaved draws. It is traced to the GDD criterion below and to TR-session-004. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Formulas F5; Core Rules rule 14. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Every draw comes from FSecretStream (hidden decisions, seeded from GetServerSubSeed) or FIntStream (public geometry and decoration only); ranged draws use UniformBelow/UniformInRange; tests inject entropy through IEntropySource; golden vectors are committed and run on every CI platform (ADR-0006)
- Forbidden: FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand, <random> engines or time-seeded randomness in gameplay or server code; logging, tracing, replicating or URL-carrying ServerSecret, MatchNonce, server sub-seeds or stream keys; reusing or renumbering an ESeedDomain value; % on a draw, floats in a ranged draw, or an unordered container as draw input (ADR-0006)
- Guardrail: Hidden draws well under 0.05 ms/frame (ADR-0006); Session silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From ADR-0006 Validation Criteria (StealthGame.Random.SecretStreamIndependence) and Decision section 5, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN FSecretStream built on a fixed sub-seed and stream tag, WHEN counter-based Draw(ordinal, decisionType, decisionIndex) values for one pawn ordinal are taken with and without interleaved draws for other ordinals and decision types, THEN they are identical (StealthGame.Random.SecretStreamIndependence), the sequential mode (0x01) and counter-tuple mode (0x02) never collide, and the stream tag enters the message with a mode byte while the key's second word is one fixed public constant.

---

## Implementation Notes

*Derived from ADR-0006 Implementation Guidelines and the GDD rules named above:*

- Block(i) = SipHash(K, 0x01 || u64le(StreamTag) || u64le(i)); Draw(i, j, k...) = SipHash(K, 0x02 || u64le(StreamTag) || ...). StealthRandom::Tag("XXXXXXXX") packs 8 ASCII bytes little-endian as a constexpr uint64 (multicharacter literals are implementation-defined, so never used).
- Never use PCG32 or any invertible PRNG for secret draws (control manifest Forbidden).

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): The prototype AStealthCityGenerator draws from the public seed with FRandomStream and float draws, which ADR-0006 forbids. This story builds the replacement primitive; the generator itself migrates under the procedural-city-generator epic. Nothing is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 006 (`story-006-pcg32-int-stream-reference-vectors.md`): public PCG32
- consumer stream tags (each consumer epic records its own)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Random/StealthSecretStream.h`
- `Source/StealthFoundation/Private/Random/StealthSecretStream.cpp`
- `Source/StealthFoundation/Public/View/Random/StealthRandomTag.h`
- `Source/StealthFoundation/Private/Tests/Random/SecretStreamIndependenceTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN FSecretStream built on a fixed sub-seed and stream tag, WHEN counter-based Draw(ordinal, decisionType, decisionIndex) values for one pawn ordinal are taken with and without interleaved draws for other ordinals and decision types, THEN they are identical (StealthGame.Random.SecretStreamIndependence), the sequential mode (0x01) and counter-tuple mode (0x02) never collide, and the stream tag enters the message with a mode byte while the key's second word is one fixed public constant.
  - Given: FSecretStream built on a fixed sub-seed and stream tag.
  - When: Counter-based Draw(ordinal, decisionType, decisionIndex) values for one pawn ordinal are taken with and without interleaved draws for other ordinals and decision types.
  - Then: They are identical (StealthGame.Random.SecretStreamIndependence). Sequential mode (0x01) and counter-tuple mode (0x02) never collide. The stream tag enters the message with a mode byte, and the key's second word is one fixed public constant.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Random/SecretStreamIndependenceTest.cpp` — automation test `StealthGame.Random.SecretStreamIndependence` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Random.SecretStreamIndependence`, T1 unit test, class `FRandomSecretStreamIndependenceTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-005-siphash24-reference-vectors.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: `production/epics/session-travel/story-074-spawn-order-uncorrelated-with-player-ness.md`
