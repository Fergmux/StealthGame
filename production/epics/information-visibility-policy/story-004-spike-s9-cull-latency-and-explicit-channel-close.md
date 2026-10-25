# Story 004: Spike S9, the explicit channel close lands at the deadline and the channel stays closed

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The concealment cull is the one permitted exception to pawn parity, and on the legacy driver it only works if an `IsNetRelevantFor` override is paired with an explicit per-connection channel close: a relevancy override alone would leave the channel open for `min(RelevantTimeout, 1 s)` plus `RelevantTimeout` with jitter, which is itself a timing channel (ADR-0003 E2). Whether the close really lands at the deadline, whether the channel stays closed (no re-creation through the relevancy timeout or a forced-relevant frame), and whether a pawn standing on a culled pawn keeps it relevant (L27) are all unverified engine claims owned by S9. Every cull story is gated on this report.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide); also `TR-ivp-005` (Relevancy varies by concealment and relation only)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.3-5.5)
**ADR Decision Summary**: The cull deadline is the first point of a global boundary grid at or after `CommitServerTime + ConcealCullDelayMs`; `AStealthPawn::IsNetRelevantFor` replaces `Super` entirely; at the deadline a game-thread cull tick closes each denied connection's channel (`FindActorChannelRef` then `Close(EChannelCloseReason::Relevancy)`); the channel then stays closed because the override keeps returning false.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E1 (`APawn::IsNetRelevantFor` clauses) [V]; E2/E3 (legacy relevancy timeout and channel creation) [V]; E5 (`FindActorChannelRef` + `UActorChannel::Close`) [V]; E10 (closed channel stays closed while the override returns false) [V], in-flight cull/un-cull race [U, S10]; E15 (listen-host client throttle, `NetClientTicksPerSecond`) [V]. This spike owns S9.

**Control Manifest Rules (this layer)**:
- Required: **The concealment cull lands on the global boundary grid and closes the channel explicitly per denied connection in the cull tick**, with no client acknowledgement wait; un-cull happens in the same server step as the state change — source: ADR-0003
- Forbidden: **Never rely on the relevancy override alone to cull** — the channel closes 1-6 s late with jitter (a timing channel) — source: ADR-0003
- Forbidden: **Never call `ForceNetRelevant()` or `ForceActorRelevantNextUpdate()` on a pawn** — source: ADR-0003
- Guardrail: **Replication**: relevancy override O(1) per pawn-connection pair, inside the 1.5 ms replication line — source: ADR-0003

---

## Acceptance Criteria

*From the story's spike definition (ADR-0003 Verification Required, S9):*

- [ ] GIVEN the legacy NetDriver, a throwaway IsNetRelevantFor override that replaces Super and an explicit per-connection FindActorChannelRef->Close(EChannelCloseReason::Relevancy) at a deadline on the global boundary grid, under StealthLeak1 and StealthLeak2 with at least 3 clients, WHEN a pawn is culled for the non-ally connections (including several pawns in one server frame, and a pawn standing on a culled pawn per L27), THEN every non-ally connection loses the pawn at the first grid boundary at or after t0 + ConcealCullDelayMs (+- one server frame), all same-frame culls land at one boundary, no channel for the culled pawn re-appears on any denied connection for at least 60 s (no relevancy-timeout or forced-relevant-frame re-creation; E10 and the ForceNetRelevant ban), the E15 NetClientTicksPerSecond >= MaxConnections x MaxFps startup assertion holds, and the verdict is written to production/qa/spikes/.

---

## Implementation Notes

*Derived from ADR-0003 sections 1, 5 and Validation Criteria (S9):*

- Throwaway code on the prototype map `L_Prototype` is acceptable; it must still follow the rule shape the production code will take: a per-pawn cull record `{State (Visible, CullPending, Culled), CommitServerTime, CullDeadline, Reason}`, a deadline on a global grid of multiples of `1 / PawnNetUpdateHz` (15 Hz) counted from the round's `Active` entry, and a tick function in `TG_PostUpdateWork` (after the drain/staging pass, before the replication flush) that moves due records to `Culled` and closes every denied connection's channel in the same frame.
- The override must not call `Super`, must resolve the viewer from `RealViewer` (the connection's PlayerController) through a server-side cache, and must deny an unknown viewer for a culled subject. Test the L27 case explicitly (a pawn based on a concealed pawn; Super's `IsBasedOnActor` clause both directions).
- Verify that the E15 client-throttle config (`[/Script/Engine.Engine] NetClientTicksPerSecond=2400`, already applied 2026-10-25) holds and add the startup assertion `NetClientTicksPerSecond >= MaxConnections x MaxFps` (MaxFps = `t.MaxFPS` if set, else 240).
- Observe for at least 60 s after the cull that no denied connection holds a channel (server side `FindActorChannelRef == null` each frame) and that no `ForceRelevantFrame` caller exists for the pawn.
- Net profiles: `StealthLeak1`/`StealthLeak2` (ADR-0008 section 5). Packet loss is not seedable: assert bounds.
- Record the result in `production/qa/spikes/<date>_S9.md`. A failure is handled by a superseding ADR.
- Prototype: reuse only for the map and pawn; `UpdateConcealmentVisibility`/`bConcealed` replication are replaced, never promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-005: re-open behaviour and NetGUID (S10)
- story-006: cull delay against measured L99 (S12)
- story-023, story-024, story-025: the production predicate, cull audience and timing

---

## Files

- `production/qa/spikes/<date>_S9.md` (the verdict)
- Throwaway spike code (prototype or harness): `Source/StealthGame/Private/Spikes/` or `Source/StealthTestHarness/Private/Scenarios/`

---

## QA Test Cases

- **AC-1**: S9 cull latency and explicit channel close
  - Given: legacy NetDriver, a throwaway IsNetRelevantFor that replaces Super, and an explicit FindActorChannelRef->Close(Relevancy) at a deadline on the global boundary grid; StealthLeak1 and StealthLeak2, at least 3 clients.
  - When: a pawn is culled for non-ally connections, including several pawns in one frame and a pawn standing on a culled pawn (L27).
  - Then: every non-ally connection loses the pawn at the first grid boundary at or after t0 + ConcealCullDelayMs (+- one server frame); all same-frame culls land on one boundary; no channel for the culled pawn re-appears on a denied connection for at least 60 s; the E15 startup assertion NetClientTicksPerSecond >= MaxConnections x MaxFps holds; verdict written to production/qa/spikes/.
  - Edge cases: the same-frame multi-cull case and the L27 case are both required. (Tier: T3.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a written spike report `production/qa/spikes/<date>_S9.md` with capture artefacts (a documented measurement).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None
- Unlocks: `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`, `production/epics/information-visibility-policy/story-006-spike-s12-cull-delay-vs-measured-l99.md`, `production/epics/information-visibility-policy/story-023-relevancy-truth-table.md`, `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`, `production/epics/information-visibility-policy/story-025-concealment-cull-timing.md`, `production/epics/information-visibility-policy/story-028-blind-spot-attack-on-culled.md`, `production/epics/information-visibility-policy/story-030-release-to-crowd-while-concealed.md`, `production/epics/information-visibility-policy/story-044-raw-bunch-schema-parity.md`, `production/epics/information-visibility-policy/story-052-strike-id-oracle-parity.md`
