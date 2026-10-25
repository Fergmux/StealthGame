# Story 048: A playtest session record must name a non-competing host or exclude the host team's rows

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

On a listen-server build the host sees its own server memory (leak row L21, accepted), so a playtest hosted by a competing player produces tainted data. Playtest data feeds three gates: the F5 hide-event posterior, the F8 human strip test and AC-IVP-25 (hide posterior parity). This story is the check that runs before such data enters a gate: the session record must name a non-competing host, or mark the host team's rows excluded and those rows must really be absent from the computed rates. A record with neither is rejected.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (sections 1 T4, 3, 8)
**ADR Decision Summary**: ADR-0008 runs T4 statistical analyses over session records and T3 captures with fixed checked-in seed sets and pre-registered thresholds, ships a negative control with every parity, statistical and lint gate, and keeps pass or fail in the gate script while analysis scripts only emit observations.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API is used. The gate is a script over session records, wrapped in a test whose system name is `Build` (ADR-0008 section 2 lint/probe wrapper family).

**Control Manifest Rules (this layer)**:
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Required: **Lints and analysis scripts emit observations only; pass/fail lives in the gate script** — source: ADR-0008
- Required: **Every Logic story ships T1 tests; every Integration story ships a T2 or T3 test or a documented playtest** — source: ADR-0008
- Forbidden: **Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail** — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-36, scoped to this story (retyped Logic: a gate-script test):*

- [ ] **AC-IVP-36** [Doc] `StealthGame.Visibility.EvidencePlaytestHost`: GIVEN any playtest whose data feeds F5, F8 or AC-IVP-25, WHEN its session record is checked before the data enters a gate, THEN the record names a non-competing host, or marks the host team's rows excluded and they are absent from the computed rates; a record with neither is rejected. (R10, OQ8)

---

## Implementation Notes

*Derived from ADR-0008 sections 1, 3, 8 and the GDD R10 / OQ8 text:*

- Define the session-record schema (the story owns it; keep it a small checked-in format): session id, host participant and whether the host is competing, host-team rows with an `excluded` flag, and the list of rows actually present in the computed rates. Keep it free of gameplay secrets; it is a process record.
- The gate script (location `tools/ci/gate/`, new folder; observations out, verdict in the gate script) accepts exactly two shapes: (1) the record names a non-competing host; (2) the record marks the host team's rows excluded and those rows are absent from the computed rates. A record with neither is rejected. A record that marks rows excluded but still includes them in the computed rates is also rejected (the QA edge case).
- Wrap the script in a fixture-driven test `StealthGame.Build.EvidencePlaytestHost` (class `FBuildEvidencePlaytestHostTest`) with fixtures for each shape, the rejected cases and the negative control.
- Negative control (ADR-0008 section 3, the GDD criterion's control): a record with neither a non-competing host nor excluded host rows must be rejected; the suite passes only if the rejection is detected.
- Fixtures and test data: ADR-0008 amendment 2026-10-24 says T1 tests do no file I/O. Keep fixtures as `constexpr` records for the logic and run the script against files only in a `Build`-system wrapper agreed with the technical-director; record that decision in the story.
- Consumers: stories 040 (hide posterior parity) and 045 (human strip test) feed this data; they must call this gate before data enters. They are not intra-epic dependency edges of this story, so do not add hard links; add the call when those stories are implemented.
- Prototype: not applicable. Replaces: none (new).
- Cross-epic needs: tuning-data-config Exit A (module skeleton, CI gate wiring).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-040: the AC-IVP-25 hide posterior parity run itself
- story-045: the human strip test itself
- story-047: leak catalogue status table (L21 host memory is accepted there)
- story-035: listen-host hardening of the Shipping build (R10 technical measures)

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `tools/ci/gate/` (session-record check script, record schema, fixtures)
- `Source/StealthFoundation/Private/Tests/Build/EvidencePlaytestHostTest.cpp` (test `StealthGame.Build.EvidencePlaytestHost`)

---

## QA Test Cases

- **AC-1**: AC-IVP-36 `StealthGame.Build.EvidencePlaytestHost` (gate-script test)
  - Given: session records feeding F5, F8 or AC-IVP-25.
  - When: the record is checked before the data enters a gate.
  - Then: a record naming a non-competing host is accepted; a record that marks the host team's rows excluded, with those rows absent from the computed rates, is accepted; a record with neither is rejected (the control).
  - Edge cases: a record that marks rows excluded but still includes them in the computed rates.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Build/EvidencePlaytestHostTest.cpp` (the studio docs' "tests/unit/build/") must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: tuning-data-config Exit A (module skeleton, CI gate wiring)
- Unlocks: None
