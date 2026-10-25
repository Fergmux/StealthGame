# Story 025: Each cull lands at the first global grid point at or after t0 + 250 ms, with withheld acks shifting nothing and early exit cancelling it (provisional on S9, S12)

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: spike story-004 (S9) and story-006 (S12) have not reported
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

When a pawn is culled is itself information: a cull that lands at a pawn-specific instant, or that waits on a client acknowledgement, gives an observer a fine-grained timing signal about when concealment began. The design therefore quantises every cull to a global boundary grid shared by all pawns, so all culls committed in one server frame land at the same grid point and no client's acks move them. This story proves the timing rule end to end under a lossy profile: the cull lands at the first grid point at or after `t0 + 250 ms`, same-frame commits share a grid point, withheld acks shift nothing by more than one server tick, and exit, break or kill during `CullPending` cancels the cull. It feeds the delay-and-un-cull invariant between this epic and Concealment (F3 and F4).

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.3, 5.5, 5.6); also ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 3, 5)
**ADR Decision Summary**: `CullDeadline` is the first point of a global boundary grid (multiples of `1 / PawnNetUpdateHz` from the round's `Active` entry, shared by every pawn) at or after `CommitServerTime + ConcealCullDelayMs`; every cull committed in one server frame lands at the same boundary, never waits on a client acknowledgement; the cull tick closes denied channels in the same frame. Un-cull and cancellation happen in the same server step as the state change.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 E14 [V]: each actor's `NextUpdateTime` gets a random delay of up to one server tick, so per-pawn boundaries are not a common grid; hence the global grid replaces the per-pawn boundary. E15 [V]: the listen-host client throttle (`NetClientTicksPerSecond`) must be `>= MaxConnections x MaxFps` or skipped clients wait for the next regular update, which would break same-step timing. E2/E5/E10 as in story-024. Open spikes: S9 (story-004) confirms the close lands at the deadline within one server frame; S12 (story-006) recomputes F3 from measured L99 and may change the default delay pair (250 ms) for TDC and Concealment.

**Control Manifest Rules (this layer)**:
- Required: **The concealment cull lands on the global boundary grid and closes the channel explicitly per denied connection in the cull tick**, with no client acknowledgement wait; un-cull happens in the same server step as the state change — source: ADR-0003
- Forbidden: **Never rely on the relevancy override alone to cull** — the channel closes 1-6 s late with jitter (a timing channel) — source: ADR-0003
- Required: **T2/T3 tests bound by frames or server time and assert and record the active replication system** — source: ADR-0008
- Guardrail: **Server frame**: the drain runs once per server frame after CMC and AI ticks; listen-host frame time ≤ 1/60 s (p99 under the profiled load) — source: ADR-0002, ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-11** [PIE-3][Net][S9][S12] `StealthGame.Visibility.ConcealmentCullTiming`: GIVEN `ConcealCullDelayMs = 250`, WHEN pawns commit concealment, including several in one server frame and one viewer whose acks are withheld, THEN each cull lands at the first global pawn-update grid point at or after `t0 + 250 ms` (R7.2; the same grid point for all commits of one server frame, ±1 server tick), withheld acks shift no cull by more than one server tick, and exit, break or kill during `CullPending` cancels the cull. (R7.2, F3)

---

## Implementation Notes

*Derived from ADR-0003 sections 5.3, 5.5, 5.6, 1 (client-throttle row) and ADR-0008 sections 1, 3 and 5:*

- **What to build.** The deadline computation and the cancellation path inside the Policy's cull record (story-024): `CullDeadline` = first grid point (multiples of `1 / PawnNetUpdateHz` from the round's `Active` entry) at or after `CommitServerTime + ConcealCullDelayMs`; the cull tick (story-024) acts on records whose deadline has passed; a cancel (exit through `HideExit`, `OnConcealmentBroken` by attacking from the bush, or a kill while `CullPending`) returns the record to `Visible` in the same server step, so the override returns true in that frame's flush (ADR-0003 section 5.6).
- **Grid anchoring.** The grid is global and shared by every pawn; commits in one server frame must map to one grid point. Read `PawnNetUpdateHz` and `ConcealCullDelayMs` from tuning, never as literals.
- **Test shape.** T2 under a lossy profile (packet loss is not seedable, so assert bounds, never exact counts): `NETWORK_TEST_CLASS(ConcealmentCullTiming, "StealthGame.Visibility.PIE")`, `WithPacketSimulationSettings` loaded from the `StealthLeak2` profile (ADR-0008 section 5). Several commits in one server frame; one viewer with acks withheld (a harness-level ack drop for that connection); assert each cull's landing against the grid in server time, never wall-clock. Test each of the three cancel causes (exit, break, kill) separately.
- **Startup assertion.** The harness asserts `NetClientTicksPerSecond >= MaxConnections x MaxFps` (story-004 adds it first; this test relies on it), because without it same-step timing can be broken by per-client throttle.
- **Dependence on S12.** `ConcealCullDelayMs = 250` and the matching F4 `T_uncull` are provisional until story-006 recomputes F3 with measured L99; if S12 records a larger pair, update the test's input from the new default (tuning), not the rule.
- **Prototype disposition.** Replaced, never promoted: `UpdateConcealmentVisibility` and `bConcealed` replication (`Source/StealthGame/Private/StealthCharacter.cpp:63-64`); the prototype has no cull deadline.
- **Spike gates.** story-004 (S9), story-006 (S12).
- **Cross-epic.** concealment-bushes supplies the commit, exit and break events in production (harness callers until then); round-match-framework provides the `Active` entry time anchoring the grid and the server clock.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-024: cull record, cull tick and audience (AC-IVP-10)
- story-026: the `ConcealCullDelayMs` knob validation (AC-IVP-28)
- story-029: un-cull lead bound (AC-IVP-24)
- story-006: measured L99 and the F3 recomputation (S12)
- story-004: S9 cull latency spike
- concealment-bushes epic: the exit transition and the client-side hide at the swap

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Visibility/` (deadline computation on the global grid, cancellation path)
- `Source/StealthTestHarness/Private/Tests/PIE/ConcealmentCullTimingTest.cpp` (`NETWORK_TEST_CLASS(ConcealmentCullTiming, "StealthGame.Visibility.PIE")`, ack-withholding harness hook)
- `Config/DefaultEngine.ini` net profile entries `StealthLeak1`/`StealthLeak2` (created by the harness story if absent)

---

## QA Test Cases

- **AC-1**: Concealment cull timing (AC-IVP-11)
  - Given: `ConcealCullDelayMs = 250` under a lossy profile, several commits in one server frame, one viewer whose acks are withheld.
  - When: pawns commit concealment.
  - Then: each cull lands at the first global grid point at or after `t0 + 250 ms`; all commits of one server frame share the same grid point (±1 server tick); withheld acks shift no cull by more than 1 tick; exit, break or kill during `CullPending` cancels the cull.
  - Edge cases: assert as bounds (packet loss is not seedable); test each of the three cancel causes separately. (Tier: T2, bound assertions.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T2 test at `Source/StealthTestHarness/Private/Tests/PIE/ConcealmentCullTimingTest.cpp` OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `production/epics/information-visibility-policy/story-006-spike-s12-cull-delay-vs-measured-l99.md`, `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`; round-match-framework epic (the round `Active` entry time and server clock); tuning-data-config Exit A (harness, net profiles)
- Unlocks: None
