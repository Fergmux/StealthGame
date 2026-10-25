# Story 010: CanViewerKnow denies by default and reports every fault once

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

`CanViewerKnow(Viewer, Subject, Fact)` is a default-deny query. An unlisted fact, an unknown viewer, a call on a client and a call outside the game thread all return false and report to the injected fault sink; a legitimate "N" or "not applicable" cell also returns false but reports nothing, so the sink stays a signal of real defects and not noise. This story builds that gate on top of story-009's classes. The fault sink and clock seams come from the ADR-0008 test strategy, so the test can inject a counting stub and assert exactly one report per faulty call.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-001` (`CanViewerKnow` server query, default deny, seven viewer classes)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules; also ADR-0008: Test and automation strategy for leak and parity tests (section 4, test seams)
**ADR Decision Summary**: Every invariant violation, dead registration or unknown viewer reports to `IStealthFaultSink` (`Report(EStealthFault, FName Source, int64 Detail)`); every T1-T3 test asserts zero unexpected faults at teardown and declares expected ones explicitly. The Policy's answers are compiled into the carriers; the query exists for server systems and for the audit.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure C++ with a game-thread check; reflective or Blueprint requests for a server-only value on a client return invalid and use `ensure`, never `check` (ADR-0007 section 6). Calls come from the plain core, so the thread and "client side" contexts need injectable seams.

**Control Manifest Rules (this layer)**:
- Required: **A reflective or Blueprint request for a server-only value on a client returns invalid and uses `ensure`, never `check`** — source: ADR-0007
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Forbidden: **Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail** — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-05):*

- [ ] **AC-IVP-05** [Unit] `StealthGame.Visibility.QueryDefaultDeny`: GIVEN an unlisted fact, an unknown viewer, a client-side call and a non-game-thread call, WHEN `CanViewerKnow` runs, THEN it returns `false` and the counting fault-sink stub records exactly one report per call; a "—" (not applicable) cell returns `false` with no report. (R1, R3)

---

## Implementation Notes

*Derived from the GDD rule R1 and ADR-0008 section 4:*

- `CanViewerKnow` returns `false` for: an unlisted fact, an unknown viewer, a call on a client, a call outside the game thread. Each faulty call reports exactly once to the injected `IStealthFaultSink` (an `ensure` in development builds plus a counted `LogStealthVisibility` error; use `UE_LOGF`). An "N" cell returns `false` with no report and no log line; a "—" cell returns `false` with no report.
- The "client-side" and "non-game-thread" contexts are injected through small seams (a context provider interface; an async task or injected thread-id check in the test) so the core stays headless.
- The query is not called per frame by gameplay; the carriers are the enforcement. Do not add a convenience overload that bypasses the fact table.
- Files live in `StealthFoundation` (`Public/Server/` for the interface with the guard include first; `Private/Visibility/` for the core). The fault sink and clock interfaces (`IStealthFaultSink`, `IStealthClock`) come from tuning-data-config Exit A.
- Negative control: the criterion has none beyond the stub counts; the test must also show a deliberately wrong implementation (returns true for an unlisted fact) is caught by the same assertions.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-009: class computation; story-011: the fact matrix; story-012: the NPC row
- story-034: the build-time check that presentation never calls `CanViewerKnow`

---

## Files

- `Source/StealthFoundation/Public/Server/StealthVisibilityPolicy.h` (query declaration; extends story-009)
- `Source/StealthFoundation/Private/Visibility/StealthVisibilityQueryCore.h` / `.cpp`
- `Source/StealthFoundation/Private/Tests/Visibility/QueryDefaultDenyTest.cpp` (test `StealthGame.Visibility.QueryDefaultDeny`, class `FVisibilityQueryDefaultDenyTest`)

*(The module folders do not exist yet; tuning-data-config Exit A creates them.)*

---

## QA Test Cases

- **AC-1**: QueryDefaultDeny (AC-IVP-05)
  - Given: an unlisted fact, an unknown viewer, a client-side call and a non-game-thread call; a counting IStealthFaultSink stub.
  - When: CanViewerKnow runs for each.
  - Then: it returns false every time; the stub records exactly 1 report per call; a "-" cell returns false with 0 reports.
  - Edge cases: the "client-side" and "non-game-thread" contexts need injectable seams (async task for the thread case). (Tier: T1.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/QueryDefaultDenyTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-009-viewer-class-invariants.md`; tuning-data-config Exit A (fault sink and clock seams)
- Unlocks: `production/epics/information-visibility-policy/story-011-matrix-per-fact.md`, `production/epics/information-visibility-policy/story-023-relevancy-truth-table.md`
