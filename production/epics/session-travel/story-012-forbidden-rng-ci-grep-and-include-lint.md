# Story 012: CI grep bans engine RNG APIs and the CityGenerator include rule is enforced

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-004 = Public seed vs server-secret keyed sub-seeds)*

**ADR Governing Implementation**: ADR-0006: Seeds and deterministic/secret random streams
**Also relevant**: ADR-0007; ADR-0008 (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: A public Seed rebuilds the identical city on every machine (Mix32 public sub-seeds, integer PCG32 FIntStream); every hidden decision draws from SipHash-2-4 keyed server-secret streams (FSecretStream) derived from a 128-bit ServerSecret from the OS CSPRNG; never FRandomStream, FMath::Rand or floats in a draw.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API beyond plain integer arithmetic and one OS entropy call (BCryptGenRandom, bcrypt.lib linked under a Win64 guard). Golden vectors must pass on every CI platform and compiler (MSVC now, clang when a Linux dedicated server arrives).

**Why this story**: This story delivers `forbidden-rng-ci-grep-and-include-lint` as one buildable behaviour: cI grep bans engine RNG APIs and the CityGenerator include rule is enforced. It is traced to the GDD criterion below and to TR-session-004. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 14. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Every draw comes from FSecretStream (hidden decisions, seeded from GetServerSubSeed) or FIntStream (public geometry and decoration only); ranged draws use UniformBelow/UniformInRange; tests inject entropy through IEntropySource; golden vectors are committed and run on every CI platform (ADR-0006)
- Forbidden: FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand, <random> engines or time-seeded randomness in gameplay or server code; logging, tracing, replicating or URL-carrying ServerSecret, MatchNonce, server sub-seeds or stream keys; reusing or renumbering an ESeedDomain value; % on a draw, floats in a ranged draw, or an unordered container as draw input (ADR-0006)
- Guardrail: Hidden draws well under 0.05 ms/frame (ADR-0006); Session silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From ADR-0006 Validation Criteria ('CI grep: no FMath::Rand*, FRandomStream or <random> in StealthFoundation/StealthGameplay outside the allowlist') and Implementation Guideline 6; ADR-0007 amendment (rule (j)), scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN StealthFoundation and StealthGameplay sources, WHEN the CI grep runs, THEN no FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand or <random> appears outside the allowlist (none expected in the MVP), and the ADR-0007 include lint rule (j) fails any file under a CityGenerator/ folder that includes anything under Public/Server/Random/; both checks are watched failing against a planted violation before being trusted.

---

## Implementation Notes

*Derived from ADR-0006 Implementation Guidelines and the GDD rules named above:*

- The prototype module Source/StealthGame still contains FRandomStream (StealthCityGenerator.cpp); scope the grep to the ADR-0007 modules or carry an explicit prototype allowlist that expires when the generator migrates.
- Evidence is a smoke-check pass (production/qa/smoke-*.md) plus the recorded failing run against the planted violation (test-standards: watch a gate fail before trusting it).

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): The prototype AStealthCityGenerator draws from the public seed with FRandomStream and float draws, which ADR-0006 forbids. This story builds the replacement primitive; the generator itself migrates under the procedural-city-generator epic. Nothing is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- the include lint's other rules (a)-(h), (j) owner is the tuning-data-config bootstrap

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/check-forbidden-rng.sh`
- `tools/ci/include-lint (rule (j) addition)`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Config/Data smoke check:**

- **AC-1**: GIVEN StealthFoundation and StealthGameplay sources, WHEN the CI grep runs, THEN no FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand or <random> appears outside the allowlist (none expected in the MVP), and the ADR-0007 include lint rule (j) fails any file under a CityGenerator/ folder that includes anything under Public/Server/Random/; both checks are watched failing against a planted violation before being trusted.
  - Smoke: Run the CI grep over StealthFoundation and StealthGameplay sources. Pass when no FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand or <random> appears outside the allowlist (none expected in the MVP). Pass also requires ADR-0007 include-lint rule (j) to fail any file under a CityGenerator/ folder that includes anything under Public/Server/Random/. Plant one violation per check first and record each failing run before trusting the gate. Then remove the plants and record the passing smoke in production/qa/smoke-*.md.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass recorded in `production/qa/smoke-*.md` (the planted-violation failing run is recorded too)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None within this epic
- Unlocks: None
