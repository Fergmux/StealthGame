# Story 024: A culled concealed player pawn has no actor on any non-ally client and stays relevant to its owner and allies (provisional on S9)

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: spike story-005 (S10 re-open and NetGUID) has not reported. S9 (story-004) reported PASS 2026-10-25 (production/qa/spikes/2026-10-25_S9.md): the cull tick and explicit per-connection close are specified from it Update 2026-10-25 (S10 resolution): the S10 FAIL is resolved by ADR-0003 amendment "S10 resolution 2026-10-25" (re-open guard with bounded timeout and fresh channel, lost-pawn repair, warm-up, ADR-0002 clock budget); the S10 gate is now **Blocked: story-056 (S10 fix + re-run)** (`production/epics/information-visibility-policy/story-056-s10-fix-implementation-and-rerun.md`); other gates unchanged.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The concealment hybrid hides a concealed pawn from non-allies in two layers: an immediate client-local hide, and a quantised server cull that stops the pawn's data reaching non-ally connections at all. This story builds the server half in the Visibility Policy: the per-pawn cull record, the `IConcealmentCullSource::SetCullState` entry Concealment calls, the `IsNetRelevantFor` wiring of story-023's predicate, and the cull tick that closes the channel explicitly. It then proves the audience: a culled player pawn has no actor on any non-ally client, active or informant, while staying relevant to its owner and every ally including informant allies, and a culled NPC has no actor on any client. It is the first cull story gated on S9.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.1-5.5); also ADR-0001: Crowd identity rules (section 5); ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 3)
**ADR Decision Summary**: Concealment pushes state through the consumer-owned `IConcealmentCullSource`; the Policy owns the per-pawn cull record `{State, CommitServerTime, CullDeadline, Reason}`; `IsNetRelevantFor` replaces `Super`; a game-thread `FConcealmentCullTickFunction` in `TG_PostUpdateWork` moves due records to `Culled` and closes every denied connection's channel (`FindActorChannelRef` then `Close(EChannelCloseReason::Relevancy)`) in the same frame; the override then keeps returning false.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 E1 (`IsNetRelevantFor` clauses) [V]; E2/E3 (legacy relevancy timeout and channel creation) [V]; E5 (`FindActorChannelRef` + `UActorChannel::Close`) [V]; E10 (a closed channel stays closed while the override returns false) [V], in-flight cull/un-cull race [U, S10]; E15 (listen-host client throttle) [V]. S9 (story-004) owns whether the explicit close lands at the deadline and stays closed; S10 (story-005) owns re-open behaviour.

**Control Manifest Rules (this layer)**:
- Required: **The concealment cull lands on the global boundary grid and closes the channel explicitly per denied connection in the cull tick**, with no client acknowledgement wait; un-cull happens in the same server step as the state change — source: ADR-0003
- Required: **`AStealthPawn::IsNetRelevantFor` does not call `Super`**; its only inputs are the Policy's cull record and the viewer's relation — source: ADR-0003
- Forbidden: **Never rely on the relevancy override alone to cull** — the channel closes 1-6 s late with jitter (a timing channel) — source: ADR-0003
- Forbidden: **Never hide concealment client-side only or replicate a concealed flag** (internal development switch only); never use dormancy or distance culling for concealment — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-10** [PIE-3][S9] `StealthGame.Visibility.ConcealmentCullAudience`: GIVEN a concealed player pawn and a concealed NPC pawn, WHEN `Culled` is reached, THEN the player pawn has no actor on any non-ally client (Active or Informant) and stays relevant to its owner and every ally including informant allies; the NPC pawn has no actor on any client. (R7.1, R9a)

---

## Implementation Notes

*Derived from ADR-0003 sections 5.1-5.5 and ADR-0008 sections 1 and 3:*

- **What to build.** In the Visibility Policy: the `ECullState {Visible, CullPending, Culled}` per-pawn record `{State, CommitServerTime, CullDeadline, Reason}` (server-only, `Reason` is `Concealment`); the consumer-owned `IConcealmentCullSource::SetCullState(Pawn, bConcealed, CommitServerTime)` that Concealment (a feature) calls, so the Policy never names a feature type; `AStealthPawn::IsNetRelevantFor` delegating to story-023's predicate; and `FConcealmentCullTickFunction` (game thread, `TG_PostUpdateWork`, after the drain and staging pass and before the replication flush) that moves records whose `CullDeadline <= ServerNow` to `Culled` and closes each newly denied connection's channel in the same frame. The deadline arithmetic and the grid are story-025; here the audience result is the criterion.
- **Test harness caller.** Concealment is not built: this story tests with a harness caller of `SetCullState`.
- **Test shape.** T2: `NETWORK_TEST_CLASS(ConcealmentCullAudience, "StealthGame.Visibility.PIE")`, `WithGameMode(AStealthGameMode)`, 3 clients. Test the informant ally and the informant non-ally separately; the NPC pawn must be absent on every client, including the owner-less case.
- **Negative control (added by the story, ADR-0008 section 3).** A harness-only variant that leaves the channel open to one non-ally connection must be detected by the same audience check; the suite passes only when the control is flagged. The criterion text stays the GDD's.
- **Relation changes while culled.** `ReleaseToCrowd`/`ClearBinding` re-evaluation of a culled pawn's denied connections (ADR-0003 section 5.5) is story-030, not this story.
- **Prototype disposition.** Replaced, never promoted: `bConcealed`/`bRevealedUntilExit` replication to all clients (`Source/StealthGame/Private/StealthCharacter.cpp:63-64`) and `UpdateConcealmentVisibility`, the client-side toggle. The production cull is written new once S9 reports.
- **Spike gates.** story-004 (S9) and story-005 (S10, in-flight cull/un-cull race and re-open behaviour).
- **Cross-epic.** concealment-bushes calls `SetCullState` in production; shared-character-locomotion provides `AStealthPawn` and the relevancy hook; participant-state-roster provides ally and informant relations through the Policy's viewer cache.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-023: the pure cull predicate and viewer cache
- story-025: cull timing on the global grid, `CullPending` cancellation, withheld acks (AC-IVP-11)
- story-030: release to crowd while concealed (AC-IVP-32)
- story-031: player-only cull never ships (AC-IVP-30)
- story-029: un-cull lead bound (AC-IVP-24)
- story-005: re-open behaviour spike (S10)
- concealment-bushes epic: concealment decisions and the production `SetCullState` caller

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/StealthVisibilityPolicy.h` (cull record and `IConcealmentCullSource` declarations; related `Stealth*Visibility*.h`)
- `Source/StealthFoundation/Private/Visibility/` (cull record, cull tick `FConcealmentCullTickFunction`, relevancy wiring)
- `Source/StealthTestHarness/Private/Tests/PIE/ConcealmentCullAudienceTest.cpp` (`NETWORK_TEST_CLASS(ConcealmentCullAudience, "StealthGame.Visibility.PIE")`, harness `SetCullState` caller and negative-control variant)

---

## QA Test Cases

- **AC-1**: Concealment cull audience (AC-IVP-10)
  - Given: a concealed player pawn and a concealed NPC pawn, 3 clients.
  - When: `Culled` is reached.
  - Then: the player pawn has no actor on any non-ally client (Active or Informant); it stays relevant to its owner and every ally including informant allies; the NPC pawn has no actor on any client.
  - Edge cases: test the informant ally and the informant non-ally separately; negative control (added): a variant that leaves the channel open to one non-ally must be detected. (Tier: T2.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T2 test at `Source/StealthTestHarness/Private/Tests/PIE/ConcealmentCullAudienceTest.cpp` OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`, `production/epics/information-visibility-policy/story-023-relevancy-truth-table.md`; shared-character-locomotion epic story delivering `AStealthPawn`; tuning-data-config Exit A (harness)
- Unlocks: `production/epics/information-visibility-policy/story-025-concealment-cull-timing.md`, `production/epics/information-visibility-policy/story-027-concealment-local-hide-at-swap.md`, `production/epics/information-visibility-policy/story-028-blind-spot-attack-on-culled.md`, `production/epics/information-visibility-policy/story-029-uncull-lead-bound.md`, `production/epics/information-visibility-policy/story-030-release-to-crowd-while-concealed.md`, `production/epics/information-visibility-policy/story-031-player-only-cull-never-ships.md`, `production/epics/information-visibility-policy/story-033-teammate-marker-concealed-ally.md`; concealment-bushes epic (production caller of `SetCullState`)
