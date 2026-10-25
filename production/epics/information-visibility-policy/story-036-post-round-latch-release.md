# Story 036: Every viewer becomes PostRound in the round-end step before any Ended data is sent

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs Round's round-end step and the death-write staging (round-match-framework, ADR-0002) built.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

When a round ends, the Visibility Policy is allowed to release one new thing to everyone: the round result. It must not release it a step early, or the end of the round (or the identity of the deciding strike) would be readable before the contact lands. ADR-0002 therefore splits the end into a silent latch step and a single observable round-end step, and the Policy flips every viewer to `PostRound` inside that step, before any `Ended` data leaves. This story proves the ordering with visible and culled pawns present, including a strike win (the deciding strike's death write is bound to the round-end step), and that in MVP only `FRoundResult` is newly released while culled pawns stay culled through `Ended`.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-001` (`CanViewerKnow` server query, default deny, seven viewer classes)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Resolution queue and round-end staging (section 8, terminal latch and the two-step round end); ADR-0001: Crowd identity rules (section 5, viewer classes and carriers)
**ADR Decision Summary**: The latch step replicates and presents nothing. The round-end step runs in one server step bound to the deciding slot's latest death write by `CauseKey`: phase `Ended` and `FRoundResult` published with a forced GameState net update, `Roster.EndRoundAll()` with forced carrier updates, held `NotNow` replies released, `OnRoundEnded` fires, and the Visibility Policy treats every viewer as `PostRound` from there (MVP releases only `FRoundResult`).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: E1 (network dispatch before tick groups, replication flush at the end of the frame) and E2 (custom `FTickFunction` registered per level) [V]; the drain, staging pass and round-end step run inside the same tick function, and the replication flush follows it. Single-process PIE is sufficient (no travel).

**Control Manifest Rules (this layer)**:
- Required: **The latch step replicates and presents nothing; the round-end step runs in one server step, bound to the deciding slot's latest death write by `CauseKey` (or inline when the slot staged none),** and calls `ICrowdControl::Halt()` plus the kind-blind stop-and-blend for every live pawn — source: ADR-0002
- Required: **Only Round's `TryTerminate` calls `Latch`**; after the latch the queue rejects every item with `Rejected_Latched` except parked `HostCommand`s, the post-latch lane and the `ConcealmentTransition` exemption — source: ADR-0002
- Forbidden: **Never end the round in one step at the latch, or through an `OnRoundEnded` subscriber chain** — reveals the end before contact; subscriber order is unspecified — source: ADR-0002
- Guardrail: **Queue drain + staging + teardown + Roster flush**: ≤ 0.5 ms listen-host game thread — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-20), scoped to this story:*

- [ ] **AC-IVP-20** [PIE-3] `StealthGame.Visibility.PostRoundLatchRelease`: GIVEN visible and culled pawns when the round ends, WHEN Round's round-end step runs (R9; for a strike win the deciding strike's death write), THEN every viewer becomes `PostRound` in that server step before any `Ended` data is sent; in MVP only `FRoundResult` is newly released, and culled pawns stay culled through `Ended`. (R2, Round R19)

---

## Implementation Notes

*Derived from ADR-0002 sections 6 and 8 (8.1-8.4) and ADR-0001 section 5, with ADR-0008 sections 2-3:*

- The Policy consumes the round-end step through the Foundation-declared terminal interface: Round implements `ITerminalDecision::RunRoundEndStep(const FRoundEndPayload&)`; step (e) fires `OnRoundEnded(Result)` and the Policy treats every viewer as `PostRound` from there. The viewer class is computed by story-009's core (R2 order: `Ended` to `PostRound` first); this story proves the flip is observable only inside the round-end step and before any `Ended` datum is replicated (the `FRoundPublicState` write with `Phase = Ended` and the Roster carriers' `RoundEnded` records).
- Assertions by server step, not wall time: record the server step at which each viewer's class becomes `PostRound`, at which `FRoundPublicState` is replicated with `Ended`, and at which the first `RoundEnded` carrier record is sent; the class flip must be at or before the earliest of those, in the same step. For a strike win include the deciding strike's death write: the round-end step runs immediately after that death write in the same server step (ADR-0002 8.3, TypeRank DeathWrite < RoundEndStep). Other round ends (objective win, forfeit by grace expiry, debug win) consume the staged `RoundEndStep` inline at slot end.
- In MVP only `FRoundResult` is newly released; culled pawns stay culled through `Ended` (their cull record and channel state do not change at the flip), asserted against the non-ally view.
- Negative control (added by the story, ADR-0008 section 3): a harness-only variant that sends `Ended` data one server step early (before the viewers are flipped) must be detected by the suite; the suite passes only if the control is detected. The control type lives only in `StealthTestHarness`.
- T2 test: `NETWORK_TEST_CLASS(PostRoundLatchRelease, "StealthGame.Visibility.PIE")` with `WithGameMode(AStealthGameMode)`, at least 3 clients, a fixture map, bound by server time and frames; record the active replication system. Cases: visible pawns, culled pawns, a strike win (death write bound), an objective win (inline).
- Client arrival rule (ADR-0002 8.5): a client may receive `RoundEnded` records a net frame before `Phase = Ended`; consumers treat that as no change. The test asserts on what is sent per server step, not on client arrival order.
- Prototype: Replaces: nothing directly. Promotes: nothing.
- Cross-epic: needs round-match-framework's round-end step, latch and death-write staging (`ITerminalDecision`, `RunRoundEndStep`, `FRoundResult`) built (ADR-0002 Exit A); assassination-defence-reversal's `DeathWrite` applier for the strike-win case (a harness staged write may stand in for the death write payload).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-009: the viewer-class computation (R2 order) this step relies on
- story-011: the R3 matrix cells for `PostRound` release classes
- story-024: the cull this story observes staying culled
- story-038: kill-confirmation parity
- round-match-framework epic: the latch, staging pass and round-end step themselves; assassination-defence-reversal epic: the death write

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthFoundation/Public/Server/StealthVisibilityPolicy.h` (`PostRound` flip on `OnRoundEnded`, extends story-009)
- `Source/StealthFoundation/Private/Visibility/StealthVisibilityPolicy.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/PostRoundLatchReleaseTest.cpp` (T2, `NETWORK_TEST_CLASS(PostRoundLatchRelease, "StealthGame.Visibility.PIE")`, plus the harness-only early-`Ended` control)

*Note: the `StealthFoundation` and `StealthTestHarness` module folders are created by tuning-data-config Exit A (they do not exist yet).*

---

## QA Test Cases

- **AC-1**: Post-round latch release (AC-IVP-20)
  - Given: visible and culled pawns when the round ends, including a strike win.
  - When: Round's round-end step runs.
  - Then: every viewer becomes `PostRound` in that server step before any `Ended` data is sent; only `FRoundResult` is newly released in MVP; culled pawns stay culled through `Ended`.
  - Edge cases: for a strike win include the deciding strike's death write; assert ordering by server step; negative control (added): a variant that sends `Ended` data one step early must be detected.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/PostRoundLatchReleaseTest.cpp` (T2 test, must exist and pass) OR a documented playtest or spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-009-viewer-class-invariants.md`, `production/epics/information-visibility-policy/story-011-matrix-per-fact.md`; round-match-framework epic (the round-end step, latch and death-write staging, ADR-0002 Exit A)
- Unlocks: None
