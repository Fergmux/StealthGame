# Story 004: One batched ClientCommandReplies per connection per drain, null sink discards

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: Replies (rejections, `FHoldStatus`, owner results) are appended to the producing sink's per-pass buffer and flushed once at the end of the pass that produced them, as one reliable owner-only `ClientCommandReplies` RPC per connection, ordered by the queue key of the slot that produced them; a null sink (AI) discards, and that difference is decided by the sink, never by a caller check.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Highest of ADR-0011 (HIGH) and ADR-0002 (MEDIUM). The flush relies on ADR-0002 E1/E8: server RPC bodies run in the NetDriver dispatch before the tick groups, the drain runs in `TG_PostPhysics`, and replies leave at the end-of-frame flush, so no reply can be sent inline in a slot. Reliable owner-only client RPCs on one controller channel arrive in order. No V1-V6 item gates this story directly (V4, stories 080/081, measures the resulting latency).

**Control Manifest Rules (this layer)**:
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection; `Rejected_Latched` replies `NotNow` through the held-reply buffer — source: ADR-0011
- Required: Ingress drops are silent — source: ADR-0011
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state; never deliver replies as replicated owner-only properties — source: ADR-0011
- Forbidden: Never put owner-only data on the pawn — source: ADR-0011
- Guardrail: At most one reply RPC per connection per drain — source: ADR-0011
- Guardrail: Resolution queue line 0.5 ms; reply buffering must stay O(1) per reply — source: ADR-0011, ADR-0002

This story builds the reply step of the command interface: the per-sink buffer, the opaque `FReplySinkId` table, and the drain-end flush that turns buffered replies into at most one batched `ClientCommandReplies` per connection. The timing rule is the privacy rule: because every reply leaves at the end of the pass that produced it, arrival time never encodes where in the drain a failure occurred (GDD R6, "Rejections reveal nothing about anyone else"). The null sink used by AI discards its replies, which is how the IIF avoids any `IsPlayerControlled` check while still replying only to humans.

It sits fourth because it needs the types (001) and the core skeleton (002), and it is a prerequisite for the ingress, rejection-code, ring and UI stories (020, 026, 029, 068, 074, 076-078, 080, 088). The real round queue and its drain are cross-epic (round-match-framework, stories not yet written); this story uses the stub queue from story 002. It maps to GDD R6 (two reply tiers, line 90) and ADR-0011 section 8. The second flush point (ADR-0002 staging pass) is covered by story 051 / AC 64, not here.

---

## Acceptance Criteria

*ADR-derived (no numbered GDD AC). Source: ADR-0011 Decision §8 (replies appended per sink and flushed once at drain end in one reliable owner-only RPC; null sink discards; ordered by queue key), with the zero-reply rule proposed by the qa-lead:*

- [ ] **GIVEN** a drain in which sink A has 3 voided commands (submitted with queue keys 5, 2, 9), sink B has 1 voided command and 1 `FHoldStatus`, and a null sink has 2 voided commands **WHEN** the drain resolves **THEN** the transport spy sees exactly one `ClientCommandReplies` call for A (entries ordered 2, 5, 9) and one for B (entries ordered by key), each issued after the drain's last slot and none inside any slot, the null sink produces 0 calls and 0 buffered entries without error, and a connection whose buffer is empty at drain end receives no call.

---

## Implementation Notes

*Derived from ADR-0011 Decision §8, Key Interfaces and Implementation Guidelines; ADR-0002 Decision §5 (the drain):*

"Ingress tier: silent. No reply for anything dropped at stage 1. Resolution tier: exactly one reply per command voided at stage 2 for any reason (including 'pawn removed earlier in this drain') ... **Timing**: every reply (rejection or `FHoldStatus`, and Assassination's owner results) is appended to its sink's per-pass buffer and flushed **once at the end of the pass that produced it**, as one reliable owner-only client RPC per connection, `ClientCommandReplies(const TArray<FCommandReply>&)` ... so arrival time never encodes the failure point. The normal flush point is the end of the drain's resolution step. ... Inside a batch, entries are ordered by the queue key of the slot that produced them (held replies keep their original keys); within one slot, the IIF's reply precedes the feature's owner result. A null sink (AI) discards; that is the only difference, decided by the sink, never by a caller check. The host's sink obeys the same flush."

Why one batch: a reliable RPC per voided command would put the failure point on the wire (each reply would arrive at its own time) and would let a probe flood the reliable buffer; one batch per connection per drain bounds it to "at most one reply RPC per connection per drain" (Guardrail). The `FReplySinkId` is "an index into the server's reply-sink table; null for AI" (§3.1); it is opaque, so the command component can hand it to the reply step without ever inspecting it.

Not-yet-defined behaviour resolved by the QL-STORY-READY rewrite: a drain with zero replies for a connection sends no RPC (an empty RPC every drain would be bandwidth and a presence signal): "At most one `ClientCommandReplies` per connection per drain-end flush; none when the connection's buffer is empty." Edge cases to cover: host sink obeys the same flush (never inline); a sink whose connection closed mid-drain discards without crash; ordering ties inside one slot; 1,000 voided commands of mixed codes produce 0 inline sends (ADR-0011 Validation Criteria reply-timing test); two consecutive drains each producing a reply for A give two calls each with only its own drain's entries.

Prototype: new code, no prototype counterpart (the prototype's `StealthPlayerController` controller RPCs are replaced by story 094; `ServerPing` is gone). Tuning keys: none. The RPC declaration itself belongs to story 094; this story uses a transport spy interface so the flush logic is testable before the controller exists.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-001-command-reply-types.md`: `FCommandReply`, `FHoldStatus` types.
- `story-002-pawn-command-core-skeleton.md`: the core that produces voided results.
- `story-020-ingress-clamp-and-drop.md`: stage-1 silent drops (no reply).
- `story-029-rejection-code-set.md`: which `ECommandRejectCode` a given failure maps to.
- `story-051-wrong-phase-commands.md`: the staging-pass second flush point and post-latch `NotNow`.
- `story-094-controller-transport-skeleton.md`: the `ClientCommandReplies` RPC declaration on `AStealthPlayerController`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/ReplySink.h` (`FReplySinkId`, sink table, per-pass buffer, `IReplyTransport` spy seam)
- `Source/StealthGameplay/Private/Core/ReplySinkTable.cpp`
- `Source/StealthTestHarness/Private/ReplySinkDrainFlushTest.cpp` (class `FInputReplySinkDrainFlushTest`, name `StealthGame.Input.ReplySinkDrainFlush`; stub queue and transport spy, no PIE)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "Once per connection per drain" does not say what a drain with zero replies for a connection does (empty RPC versus no RPC). An empty RPC every drain is bandwidth and a presence signal.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "At most one ClientCommandReplies per connection per drain-end flush; none when the connection's buffer is empty."
- Scenario sketch from the qa file (`ReplySinkDrainFlush`, derived from ADR 8): stub queue, one drain; sink A (3 voided commands, keys 5, 2, 9), sink B (1 voided plus 1 `FHoldStatus`), null sink (2 voided). Transport spy sees exactly 1 call for A and 1 for B, each after the drain's last slot, 0 calls inside any slot; A's entries ordered 2, 5, 9; null sink 0 calls, 0 buffered entries, no error. A drain with no reply: 0 calls. Two consecutive drains each producing a reply for A: 2 calls. Edge cases as in Implementation Notes.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/ReplySinkDrainFlushTest.cpp` — must exist and pass (stub queue plus transport spy)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-001-command-reply-types.md`, `production/epics/input-interaction-framework/story-002-pawn-command-core-skeleton.md`; cross-epic: round-match-framework (stories not yet written; real queue drain and staging pass); cross-epic: tuning-data-config (stories not yet written; StealthTestHarness module)
- Unlocks: `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`, `production/epics/input-interaction-framework/story-026-bound-unbound-same-events.md`, `production/epics/input-interaction-framework/story-029-rejection-code-set.md`, `production/epics/input-interaction-framework/story-068-no-owner-data-on-pawn.md`, `production/epics/input-interaction-framework/story-074-brace-protected-only-on-confirm.md`, `production/epics/input-interaction-framework/story-076-rejection-plate.md`, `production/epics/input-interaction-framework/story-077-latency-icon.md`, `production/epics/input-interaction-framework/story-078-ring-monotonic-clamp.md`, `production/epics/input-interaction-framework/story-080-latency-budget.md`, `production/epics/input-interaction-framework/story-088-owner-status-bandwidth.md`
