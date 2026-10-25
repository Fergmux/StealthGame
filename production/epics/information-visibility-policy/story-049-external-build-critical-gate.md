# Story 049: No external build ships while a CRITICAL leak row is Spk or Req

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Any build that leaves the development team (invited, evidence or public) must be blocked while a CRITICAL leak row is unresolved, while the cull is off, or while the possession canary has not passed on that commit. This story is the release-checklist gate script that enforces it. Today it correctly fails, because L1, L2, L4, L13, L30 and L31 are still `Spk` or `Req`; that failing run is the negative control, and a second fixture where every condition holds proves the gate can pass.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (section 8, release / milestone row); ADR-0003: Replication, relevancy and the concealment cull hybrid (section 5, R7.6 `bConcealServerCull`)
**ADR Decision Summary**: ADR-0008 section 8 lists AC-IVP-33 and AC-IVP-44 as doc gates at the release and milestone gate, blocking invited, evidence and public builds while any CRITICAL row is `Spk` or `Req`. ADR-0003 forbids hiding concealment client-side only, except as an internal development switch.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API is used by the gate itself. It reads the `bConcealServerCull` value of the build configuration and the per-commit result record that the possession canary (story-019) writes; the canary's engine facts are in ADR-0001 and ADR-0008.

**Control Manifest Rules (this layer)**:
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Required: **Lints and analysis scripts emit observations only; pass/fail lives in the gate script** — source: ADR-0008
- Forbidden: **Never hide concealment client-side only or replicate a concealed flag** (internal development switch only) — source: ADR-0003
- Forbidden: **Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail** — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-44, scoped to this story (retyped Logic: a fixture-driven gate-script test):*

- [ ] **AC-IVP-44** [Doc] `StealthGame.Visibility.ExternalBuildCriticalGate`: GIVEN any build leaving the development team (invited, evidence or public), WHEN the release checklist runs, THEN no CRITICAL row (L1, L2, L4, L13, L29, L30, L31) has status `Spk` or `Req`, `bConcealServerCull` is true, and AC-IVP-37 passed on that commit. (R12 ship gate, R7.6)

---

## Implementation Notes

*Derived from ADR-0008 section 8 and the GDD R12 severity and ship-gate text:*

- Build the gate script (location `tools/ci/gate/`, new folder). Inputs: the status table from story-047 (`production/qa/leak-catalogue-status.md`, the CRITICAL rows L1, L2, L4, L13, L29, L30, L31 and their single-token status), the build's `bConcealServerCull` value, and the result record of the possession canary AC-IVP-37 (story-019) for the commit being released. Output: pass or block, with the failing condition named. The script reads; it never edits the table.
- Three conditions, all required: no CRITICAL row is `Spk` or `Req`; `bConcealServerCull` is true; AC-IVP-37 passed on that commit (a missing or stale record counts as not passed).
- Wrap it in a fixture-driven test `StealthGame.Build.ExternalBuildCriticalGate` (class `FBuildExternalBuildCriticalGateTest`) with fixtures: (1) every condition holds (proves the gate can pass); (2) each condition failing singly; (3) the current repository state. **At creation the live run correctly fails** because L1, L2, L4, L13, L30 and L31 are `Spk` or `Req`; that failing run is the negative control and must be recorded as detected (ADR-0008 section 3, a gate never watched to fail is not a gate). Do not edit the table or the fixtures to make the live run pass.
- Fixtures and test data: ADR-0008 amendment 2026-10-24 says T1 tests do no file I/O. Embed the fixtures as `constexpr` records for the logic and run the script against the real files only through a `Build`-system wrapper agreed with the technical-director; record that decision in the story.
- Depends on story-047 for the status table format and story-019 for the canary result record. The `bConcealServerCull` source (Concealment and Policy own it, R7.6, L29) may not exist yet: read it from the build configuration the harness exposes, and use a fixture value until the concealment-bushes epic provides the real key.
- Prototype: not applicable. Replaces: none (new).
- Cross-epic needs: tuning-data-config Exit A (CI and release-checklist wiring), concealment-bushes epic (the `bConcealServerCull` setting, only for the live read).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-047: the leak catalogue status table and its review
- story-019: the possession canary itself
- story-031: the player-only-cull-never-ships rule (AC-IVP-30, which also needs the AC-IVP-25 run record)
- story-046: the Iris rerun gate
- story-048: the playtest session-record gate

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `tools/ci/gate/` (release-checklist gate script and fixtures)
- `Source/StealthFoundation/Private/Tests/Build/ExternalBuildCriticalGateTest.cpp` (test `StealthGame.Build.ExternalBuildCriticalGate`)

---

## QA Test Cases

- **AC-1**: AC-IVP-44 `StealthGame.Build.ExternalBuildCriticalGate` (gate-script test)
  - Given: the story-047 status table, the `bConcealServerCull` value and the AC-IVP-37 result record.
  - When: the release checklist runs for a build leaving the team.
  - Then: no CRITICAL row (L1, L2, L4, L13, L29, L30, L31) has status `Spk` or `Req`; `bConcealServerCull` is true; AC-IVP-37 passed on that commit.
  - Edge cases: at creation the run correctly fails, and that failing run is the negative control; also keep a fixture where every condition holds, to prove the gate can pass.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Build/ExternalBuildCriticalGateTest.cpp` (the studio docs' "tests/unit/build/") must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-047-leak-catalogue-review.md`, `production/epics/information-visibility-policy/story-019-possession-canary.md`; tuning-data-config Exit A (CI and release-checklist wiring)
- Unlocks: None
