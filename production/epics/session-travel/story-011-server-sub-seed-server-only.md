# Story 011: GetServerSubSeed is unreachable from clients and from generator scope

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

**Why this story**: This story delivers `server-sub-seed-server-only` as one buildable behaviour: getServerSubSeed is unreachable from clients and from generator scope. It is traced to the GDD criterion below and to TR-session-004. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 14. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Every draw comes from FSecretStream (hidden decisions, seeded from GetServerSubSeed) or FIntStream (public geometry and decoration only); ranged draws use UniformBelow/UniformInRange; tests inject entropy through IEntropySource; golden vectors are committed and run on every CI platform (ADR-0006)
- Forbidden: FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand, <random> engines or time-seeded randomness in gameplay or server code; logging, tracing, replicating or URL-carrying ServerSecret, MatchNonce, server sub-seeds or stream keys; reusing or renumbering an ESeedDomain value; % on a draw, floats in a ranged draw, or an unordered container as draw input (ADR-0006)
- Guardrail: Hidden draws well under 0.05 ms/frame (ADR-0006); Session silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 53, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **Rule 14, sub-seeds are server-only**: GIVEN the built client and server modules, WHEN reflected and statically checked, THEN no client-callable function or RPC reaches `GetServerSubSeed`, and calling it without authority returns an error and raises an `ensure`. `ServerSubSeedServerOnly` [S] + [U]

---

## Implementation Notes

*Derived from ADR-0006 Implementation Guidelines and the GDD rules named above:*

- Misuse checks are plain if branches in every build configuration (no authority, not game thread, or inside FPublicOnlyScope): return 0 and call the fault sink directly; non-Shipping builds additionally fire ensureMsgf. check and ensure compile to nothing or report nothing in Shipping.
- FPublicOnlyScope is RAII with a thread_local depth counter so it also covers a generator stage on a worker thread. ADR-0006 validation also names StealthGame.Random.GeneratorCannotReadSecrets: calling GetServerSubSeed inside the scope raises the expected assert through an expected-error hook.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): The prototype AStealthCityGenerator draws from the public seed with FRandomStream and float draws, which ADR-0006 forbids. This story builds the replacement primitive; the generator itself migrates under the procedural-city-generator epic. Nothing is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 012 (`story-012-forbidden-rng-ci-grep-and-include-lint.md`): the include lint that bans Public/Server/Random from CityGenerator/
- the generator's use of the scope (procedural-city-generator epic)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Random/StealthServerSeeds.h`
- `Source/StealthFoundation/Private/Random/StealthServerSeeds.cpp`
- `Source/StealthFoundation/Private/Tests/Session/ServerSubSeedServerOnlyTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: **Rule 14, sub-seeds are server-only**: GIVEN the built client and server modules, WHEN reflected and statically checked, THEN no client-callable function or RPC reaches `GetServerSubSeed`, and calling it without authority returns an error and raises an `ensure`. `ServerSubSeedServerOnly` [S] + [U]
  - Given: The built client and server modules.
  - When: They are reflected and statically checked, and GetServerSubSeed is called without authority.
  - Then: No client-callable function or RPC reaches GetServerSubSeed. The call without authority returns an error and raises an ensure.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Session/ServerSubSeedServerOnlyTest.cpp` — automation test `StealthGame.Session.ServerSubSeedServerOnly` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [x] Created and passing (2026-10-26): Source/StealthFoundation/Private/Tests/Session/ServerSubSeedServerOnlyTest.cpp (StealthGame.Session.ServerSubSeedServerOnly, StealthGame.Random.GeneratorCannotReadSecrets, StealthGame.Session.ServerSubSeedStaticReach)

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.ServerSubSeedServerOnly`, T1 unit test, class `FSessionServerSubSeedServerOnlyTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-010-seed-derivation-golden-vectors.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: None

## Completion Notes
**Completed**: 2026-10-26
**Criteria**: 1/1 passing
**Deviations**: Misuse branch is a plain if in every configuration: authority (injected query; USessionService answers from the game instance net mode), game thread, FPublicOnlyScope (thread_local depth) -> 0 + EStealthFault::SeedMisuse (detail = misuse << 32 | domain) + ensureAlwaysMsgf in non-Shipping, consumed by FStealthSeedMisuseExpectScope in tests. Static check: no UFUNCTION names a sub-seed/secret, and no file naming GetServerSubSeed declares a UFUNCTION or sits in View/Presentation code.
**Test Evidence**: Source/StealthFoundation/Private/Tests/Session/ServerSubSeedServerOnlyTest.cpp (StealthGame.Session.ServerSubSeedServerOnly, StealthGame.Random.GeneratorCannotReadSecrets, StealthGame.Session.ServerSubSeedStaticReach)
**Run result**: N/A — a pure server-side primitive/CI check with nothing on screen (no player-visible behaviour changes).
**Build + tests**: cycle.ps1 -Tests -NoLaunch: Result: Succeeded; 153/153 StealthGame.* automation tests passed (tests exit=0), 2026-10-26
**Reviewed by**: focused self code review (dev-story Phase 5), unattended run 2026-10-26
