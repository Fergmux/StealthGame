# Story 009: OpenSession draws ServerSecret and MatchNonce from OS entropy and fails closed

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
**Also relevant**: ADR-0005: Session, seamless travel and match-scoped state stores (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: A public Seed rebuilds the identical city on every machine (Mix32 public sub-seeds, integer PCG32 FIntStream); every hidden decision draws from SipHash-2-4 keyed server-secret streams (FSecretStream) derived from a 128-bit ServerSecret from the OS CSPRNG; never FRandomStream, FMath::Rand or floats in a draw.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API beyond plain integer arithmetic and one OS entropy call (BCryptGenRandom, bcrypt.lib linked under a Win64 guard). Golden vectors must pass on every CI platform and compiler (MSVC now, clang when a Linux dedicated server arrives).

**Why this story**: This story delivers `os-entropy-and-session-secret` as one buildable behaviour: openSession draws ServerSecret and MatchNonce from OS entropy and fails closed. It is traced to the GDD criterion below and to TR-session-004. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 14. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Every draw comes from FSecretStream (hidden decisions, seeded from GetServerSubSeed) or FIntStream (public geometry and decoration only); ranged draws use UniformBelow/UniformInRange; tests inject entropy through IEntropySource; golden vectors are committed and run on every CI platform (ADR-0006)
- Forbidden: FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand, <random> engines or time-seeded randomness in gameplay or server code; logging, tracing, replicating or URL-carrying ServerSecret, MatchNonce, server sub-seeds or stream keys; reusing or renumbering an ESeedDomain value; % on a draw, floats in a ranged draw, or an unordered container as draw input (ADR-0006)
- Guardrail: Hidden draws well under 0.05 ms/frame (ADR-0006); Session silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From ADR-0006 Decision section 2 (entropy and the session secret) and Implementation Guideline 8; the injected test source is the IEntropySource hook Character Pool CP-9 also uses, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN an IEntropySource whose Fill fails, WHEN OpenSession runs, THEN the session does not open, the failure reaches the fault sink and a user-facing 'could not host' result, and no weaker source is substituted; GIVEN a deterministic entropy source (non-Shipping only), THEN ServerSecret (16 bytes) and MatchNonce (8 bytes) are exactly the injected bytes, and the production source is FOsEntropySource over BCryptGenRandom with BCRYPT_USE_SYSTEM_PREFERRED_RNG checked with BCRYPT_SUCCESS.

---

## Implementation Notes

*Derived from ADR-0006 Implementation Guidelines and the GDD rules named above:*

- Include the Windows header the engine's way: Windows/WindowsHWrapper.h, then Windows/AllowWindowsPlatformTypes.h, <bcrypt.h>, Windows/HideWindowsPlatformTypes.h; link with PublicSystemLibraries.Add("bcrypt.lib") under a Win64 guard.
- FDeterministicEntropySource and -StealthServerSecret=<32 hex> exist only under #if !UE_BUILD_SHIPPING with the sentinel literal for the ADR-0004 AC-22 binary scan. Shipping has no logging, so use the fault sink directly, never ensure alone.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): The prototype AStealthCityGenerator draws from the public seed with FRandomStream and float draws, which ADR-0006 forbids. This story builds the replacement primitive; the generator itself migrates under the procedural-city-generator epic. Nothing is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010 (`story-010-seed-derivation-golden-vectors.md`): DrawRoundSeed and sub-seed derivation
- Story 014 (`story-014-session-service-lifecycle-and-empty-stores.md`): the OpenSession lifecycle that calls this

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Random/StealthEntropySource.h`
- `Source/StealthFoundation/Private/Random/StealthEntropySource.cpp`
- `Source/StealthFoundation/StealthFoundation.Build.cs (Win64-guarded bcrypt.lib)`
- `Source/StealthFoundation/Private/Tests/Random/EntropyFailsClosedTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN an IEntropySource whose Fill fails, WHEN OpenSession runs, THEN the session does not open, the failure reaches the fault sink and a user-facing 'could not host' result, and no weaker source is substituted; GIVEN a deterministic entropy source (non-Shipping only), THEN ServerSecret (16 bytes) and MatchNonce (8 bytes) are exactly the injected bytes, and the production source is FOsEntropySource over BCryptGenRandom with BCRYPT_USE_SYSTEM_PREFERRED_RNG checked with BCRYPT_SUCCESS.
  - Given: An IEntropySource whose Fill fails; separately, a deterministic entropy source (non-Shipping only).
  - When: OpenSession runs.
  - Then: With the failing source, the session does not open, the failure reaches the fault sink and a user-facing 'could not host' result, and no weaker source is substituted. With the deterministic source, ServerSecret (16 bytes) and MatchNonce (8 bytes) are exactly the injected bytes. The production source is FOsEntropySource over BCryptGenRandom with BCRYPT_USE_SYSTEM_PREFERRED_RNG, checked with BCRYPT_SUCCESS.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Random/EntropyFailsClosedTest.cpp` — automation test `StealthGame.Random.EntropyFailsClosed` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Random.EntropyFailsClosed`, T1 unit test, class `FRandomEntropyFailsClosedTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-014-session-service-lifecycle-and-empty-stores.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: `production/epics/session-travel/story-010-seed-derivation-golden-vectors.md`
