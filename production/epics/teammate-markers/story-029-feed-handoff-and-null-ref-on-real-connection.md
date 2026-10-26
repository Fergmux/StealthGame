# Story 029: Feed handoff and null-reference semantics on a real connection

> **Epic**: Teammate Markers
> **Status**: Ready — Update 2026-10-26: the S10 gate is cleared — story-056 (S10 fix + re-run) reported **PASS** (`production/qa/spikes/2026-10-26_S10-rerun.md`: re-open guard with `ReopenAckTimeoutMs` 350, lost-pawn repair, warm-up and the ADR-0002 clock budget all met; S9 PASS 2026-10-25). Other dependencies are ordinary story order (see Dependencies).
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

On a UE client a replicated reference to an actor that is not yet mapped reads as null exactly like one the server cleared, so the marker module cannot tell "no pawn" from "pawn not replicated to me" from the `PawnRef` alone. The design uses the fallback feed as the server's explicit signal and relies on a stated invariant: the feed entry and the `PawnRef` are fields of the same C-TeamView entry, the server adds the feed entry in the same update in which the pawn leaves that connection's scope while still existing, and removes it only one feed update after that connection acknowledges the pawn's channel open, so a client never sees "null, no feed" for a pawn that still exists for longer than one update. This story verifies the whole contract on a real connection: the fed marker's position and handoff to tracking, staleness over a real feed, and the three null-reference cases.

**GDD**: `design/gdd/teammate-markers.md` (TM-R4, TM-R4a, F5; AC-TMK-15, AC-TMK-30)
**Requirement**: `TR-tmk-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.4 and 7)
**Also governed by**: ADR-0021 (section 5), ADR-0001
**ADR Decision Summary**: The feed carries the teammate's feet quantised to a 10 cm grid, added in the same frame the channel goes missing and removed one feed update after the connection acknowledges the pawn's channel open (so the switch from `Fed` back to `Tracking` never passes through "null, no feed"); entries carry a per-entry `Seq`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E5/E10 (explicit close and a channel that stays closed) are [V]; `UChannel::OpenAcked` is [U] and is verified by IVP story 032; the unmapped-GUID case needs a test hook that holds a GUID unmapped for 0.10 s with the channel open (net-emulation and the cull test hook of IVP stories 004 and 005). Spikes S9 and S10 are the gate.

**Control Manifest Rules (this layer)**:
- Required: The teammate fallback feed is added in the same frame a teammate's channel goes missing, to teammate connections only; every entry carries a `uint8 Seq` — source: ADR-0003
- Forbidden: Never extrapolate or interpolate a fed position; never retire a marker while a feed entry is present — source: ADR-0021 (TM-R4, TM-R4a)
- Guardrail: Feed budget at most 13 B per entry and 104 B/s per client (IVP F6, story 032)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-15, AC-TMK-30), scoped to this story:*

- [ ] **AC1** (AC-TMK-15, handoff, [PIE-2][Net][U] `FeedHandoffAndStale`): **GIVEN** an ally whose pawn is forced out of the viewer's connection scope while the server feeds positions (grid `TeamPositionFeedGridCm` 10) **WHEN** the ref later resolves **THEN** the fed marker sat within one grid cell (10 cm) of the server position (AC-IVP-15), the handoff to tracking moves the anchor by at most 1.5 grid cells (15 cm) with no fade (requires feet `Pos`, CR-2), and fed positions were stepped, never interpolated or extrapolated.
- [ ] **AC2** (AC-TMK-15, stale): **GIVEN** the feed sequence stops advancing on the real connection **THEN** the marker hides 2.0 s (+/-1 frame) after the last advance while `IsMarkedAlly` is unaffected, and a stationary fed ally whose sequence keeps advancing never hides.
- [ ] **AC3** (AC-TMK-30, [PIE-2][Net][U] `NullRefInterpretation`): **GIVEN** a tracked ally **WHEN** (a) the test forces the ally pawn's GUID to stay unmapped on the viewer for 0.10 s while its channel is open (no feed), (b) the server takes the pawn out of the viewer's scope (feed entry added), (c) the server clears the binding (no feed) **THEN** (a) the marker never fades or moves and `OnAllyMarkerRetired` does not fire, (b) the marker goes `Fed` with no hold and no fade in the same update the ref reads null, and a server log asserts the feed entry and the null ref were sent in one update, (c) the marker holds and exits per F6.

---

## Implementation Notes

*Derived from TM-R4, TM-R4a and the stated invariant:*

The pure logic is in stories 004, 005, 006 and 007; this story proves the real connection behaves as the logic assumes. For AC1 force a pawn out of the viewer's scope with the Policy's cull test hook (the concealment cull is the usual trigger; a plain relevancy exclusion works for the non-concealed case). Measure the fed marker against the server's quantised position and the handoff displacement against feet `Pos` (the feed `Pos` is the pawn's feet, the same point as the visual root, CR-2). For AC3 use a server-side log assertion that the feed entry and the null ref were sent in one update (the Policy feed story 032 provides the counters).

If a case fails on the real connection (a "null, no feed" window longer than one update), the fix is in the Policy or Roster carriers, not in this module: file it against ADR-0003, not a marker workaround; the declined `bPawnBound` flag (OQ13 closed) stays declined unless the user reopens it.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-anchor-fed-position-and-null-ref.md` and `story-005-fallback-feed-staleness.md`: the unit logic.
- `story-030-informant-return-timing.md`: the kill case timing.
- `information-visibility-policy/story-032-team-fallback-feed-budget.md` and `story-033-teammate-marker-concealed-ally.md`: the Policy-side feed proofs.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Markers/FeedHandoffAndStaleTest.cpp` (CQTest, `StealthGame.Markers.PIE.FeedHandoffAndStale.*`)
- `Source/StealthTestHarness/Private/Markers/NullRefInterpretationTest.cpp` (CQTest, `StealthGame.Markers.PIE.NullRefInterpretation.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: an ally forced out of the viewer's scope, server feeding on a 10 cm grid. When: the ref later resolves. Then: the fed marker is within one grid cell (10 cm) of the server position; the handoff moves the anchor by 15 cm or less with no fade; positions are stepped. Edge cases: the ally moves during the handoff.
- **AC2**: Given: sequence advances stopping on the real connection, then a stationary ally with advancing `Seq`. When: time runs. Then: the marker hides 2.0 s (+/-1 frame) after the last advance and `IsMarkedAlly` is unaffected; the stationary ally never hides. Edge cases: loss at the 2.0 s boundary.
- **AC3**: Given: a tracked ally. When: (a) its GUID stays unmapped 0.10 s with the channel open and no feed; (b) the server takes the pawn out of scope (feed added); (c) the server clears the binding with no feed. Then: (a) no fade or move and no `OnAllyMarkerRetired`; (b) `Fed` with no hold or fade in the same update, with a server log asserting the feed entry and null ref were sent in one update; (c) holds and exits per F6. Edge cases: (a) at 0.10 s versus longer.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest files above OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-004-anchor-fed-position-and-null-ref.md`, `story-005-fallback-feed-staleness.md`, `story-006-lifecycle-retire-hold-takeover-direct-switch.md`, `story-016-presenter-local-controller-only.md`; cross-epic `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `story-005-spike-s10-reopen-behaviour-and-netguid.md`, `story-032-team-fallback-feed-budget.md`
- Unlocks: None
