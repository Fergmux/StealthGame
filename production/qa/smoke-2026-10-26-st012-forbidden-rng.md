# Smoke check — st-012 forbidden-RNG CI grep and the Generator include rule (2026-10-26)

**Story:** `production/epics/session-travel/story-012-forbidden-rng-ci-grep-and-include-lint.md` (Config/Data evidence)
**Verdict:** PASS — the gate was watched failing on the planted violations before the real-tree pass was trusted.

| Check | Command | Result |
|---|---|---|
| Planted violations (watched failing) | `bash tools/ci/lint/selftest/run-forbidden-rng-selftest.sh` case `rng-seeded` | GATE: FAIL, 12 findings: FMath::Rand, FRand, RandRange, RandInit, SRandInit, RandBool, VRand, FRandomStream, std::rand, std::mt19937, std::uniform_int_distribution, #include <random> — each as file:line rng |
| Clean controls | cases `rng-clean`, `rng-allowlist` | names in comments/strings, longer identifiers and the out-of-scope prototype module give 0 findings; an allowlisted file is skipped, the unlisted one in the same tree is reported |
| Real tree (StealthFoundation, StealthGameplay) | `bash tools/ci/lint/gate.sh tools/ci/lint/forbidden-rng.sh tools/ci/lint/include-lint-ij.sh` | GATE: PASS (332 files scanned, 0 findings) |
| Rule (j): no CityGenerator file includes Server/Random/ | `StealthGame.Build.IncludeLintIj` (tdc-015 selftest: j-seeded FAIL, j-clean PASS) | PASS (in the 153/153 automation run) |
| Wired into CI | `tools/ci/gate-per-commit.ps1` `$LintRules` and `tools/ci/hooks/pre-commit.sh` (lists must match: `StealthGame.Build.PreCommitHook`) | PASS |
| Automation wrapper | `StealthGame.Build.ForbiddenRngLint` | PASS |

Allowlist `tools/ci/lint/rng-allowlist.txt`: empty (MVP). The prototype's `AStealthCityGenerator` (`Source/StealthGame`, FRandomStream) is outside the scanned modules until the procedural-city-generator epic migrates it.

## Selftest output

```
SELFTEST rng-seeded ok
    Source/StealthFoundation/Private/StealthRngLeaks2.h:3 rng uses FMath::RandBool (engine RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:3 rng uses FMath::Rand (engine RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:4 rng uses FMath::FRand (engine RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:5 rng uses FMath::RandRange (engine RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:6 rng uses FMath::RandInit (engine RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:6 rng uses FMath::SRandInit (engine RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:7 rng uses FRandomStream (draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:8 rng uses std::rand (C++ standard RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:9 rng uses std::mt19937 (C++ standard RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:10 rng uses std::uniform_int_distribution (C++ standard RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:11 rng uses FMath::VRand (engine RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    Source/StealthGameplay/Private/StealthRngLeaks.cpp:2 rng includes <random> (C++ standard RNG; ADR-0006)
    GATE: FAIL (12 finding(s); 2 file(s) scanned)
SELFTEST rng-clean ok
    GATE: PASS (1 file(s) scanned, 0 findings)
SELFTEST rng-allowlist ok
    Source/StealthGameplay/Private/StealthNotListed.cpp:2 rng uses FMath::FRand (engine RNG; draws come from FSecretStream or FIntStream, ADR-0006)
    GATE: FAIL (1 finding(s); 2 file(s) scanned)
SELFTEST real-tree-zero-findings ok
    GATE: PASS (258 file(s) scanned, 0 findings)
SELFTEST RESULT: PASS (4 cases)
```
