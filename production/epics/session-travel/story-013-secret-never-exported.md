# Story 013: ServerSecret and RejoinToken never appear in logs, URLs or replicated properties

> **Epic**: Session & Travel
> **Status**: Blocked — BLOCKED (readiness check 2026-10-26): Depends on `story-027-rejoin-token-gate.md` (Ready, not implemented): the criterion searches for the `RejoinToken` byte sequence, and no RejoinToken exists yet. The ServerSecret half is implementable now (stories 009-011 landed: `-StealthServerSecret` override, USessionService); unblock when 027 is Complete, then re-run `/story-readiness`.
> **Layer**: Foundation
> **Type**: Integration
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

**Why this story**: This story delivers `secret-never-exported` as one buildable behaviour: serverSecret and RejoinToken never appear in logs, URLs or replicated properties. It is traced to the GDD criterion below and to TR-session-004. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rules 6, 14; Formulas F5. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Every draw comes from FSecretStream (hidden decisions, seeded from GetServerSubSeed) or FIntStream (public geometry and decoration only); ranged draws use UniformBelow/UniformInRange; tests inject entropy through IEntropySource; golden vectors are committed and run on every CI platform (ADR-0006)
- Forbidden: FMath::Rand, FMath::FRand, FMath::RandRange, FRandomStream, std::rand, <random> engines or time-seeded randomness in gameplay or server code; logging, tracing, replicating or URL-carrying ServerSecret, MatchNonce, server sub-seeds or stream keys; reusing or renumbering an ESeedDomain value; % on a draw, floats in a ranged draw, or an unordered container as draw input (ADR-0006)
- Guardrail: Hidden draws well under 0.05 ms/frame (ADR-0006); Session silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD AC 38, second part (the first part is story 010); control manifest Forbidden (ADR-0006), scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN a PIE-2 run, WHEN the captured server log, client logs, travel URLs and a reflection of every replicated property are searched, THEN none contains a byte sequence of `ServerSecret` or `RejoinToken` (`SecretNeverExported` [PIE-2] + [S]); a static check additionally finds no Blueprint-exposed function, UE_LOGF, TRACE or CSV name that carries ServerSecret, MatchNonce, a server sub-seed or a stream key.

---

## Implementation Notes

*Derived from ADR-0006 Implementation Guidelines and the GDD rules named above:*

- Logs may carry only Seed, LayoutAttempt, (Domain, Attempt, StreamTag, counter) tuples and digests (ADR-0006 section 7). IdentityKey logs carry at most a 4-byte prefix (GDD rule 6).
- Searching for the secret needs the test-injected secret (-StealthServerSecret) so the byte sequence is known.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no session handling, identity key, admission, liveness or travel, so there is nothing to replace or promote.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- 026/027: producing the key and token

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/Session/SecretNeverExportedPieTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN a PIE-2 run, WHEN the captured server log, client logs, travel URLs and a reflection of every replicated property are searched, THEN none contains a byte sequence of `ServerSecret` or `RejoinToken` (`SecretNeverExported` [PIE-2] + [S]); a static check additionally finds no Blueprint-exposed function, UE_LOGF, TRACE or CSV name that carries ServerSecret, MatchNonce, a server sub-seed or a stream key.
  - Given: A PIE-2 run with a test-injected ServerSecret so its bytes are known.
  - When: The captured server log, client logs, travel URLs and a reflection of every replicated property are searched.
  - Then: None contains a byte sequence of ServerSecret or RejoinToken. A static check finds no Blueprint-exposed function, UE_LOGF, TRACE or CSV name that carries ServerSecret, MatchNonce, a server sub-seed or a stream key.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/Session/SecretNeverExportedPieTest.cpp` — `StealthGame.Session.PIE.SecretNeverExported` (CQTest PIE network test), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.PIE.SecretNeverExported`, T2 CQTest PIE network test declared with `NETWORK_TEST_CLASS(SecretNeverExported, "StealthGame.Session.PIE")` (exempt from the F[System][Scenario]Test class-name rule, ADR-0008); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-010-seed-derivation-golden-vectors.md`, `production/epics/session-travel/story-027-rejoin-token-gate.md`, `production/epics/session-travel/story-001-harness-connection-event-hook.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: None
