# Story 020: AC 6 IngressClampAndDrop

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story builds stage 1 of the two-stage server validation: `IngestCommand`, the ingress check that runs on RPC arrival and for the listen host's `HostLocal` source, never for AI. It implements GDD R5.1 and ADR-0011 Decision section 3.1 steps 4 and 5 (the `Attack` `WorldPoint` clamp, the self-`PawnRef` strip and the `Interact` target-id gate) together with the duplicate-`ClientCommandSeq` drop that belongs to step 2. Every failure at this stage is a silent drop: no reply, no replicated change, only a counter for the server-only abuse log.

It sits first in the ingress group of the epic because stories 021 (matched-End guard), 022 (transport bucket and abuse log), 023 (disconnect on impossible payloads), 024 (host parity, V6), 025 (possession-epoch gate) and 027 (stage 2 at resolution) all extend or consume the `IngestCommand` function this story creates. It depends on the `FPawnCommandCore` skeleton (story 002) and the reply-sink and drain-end flush (story 004), so that "no reply" can be asserted against a real sink rather than assumed.

Two AC 6 clauses are easy to misread. The clamp keeps the bearing and sets the distance to `AssassinationRangeCm + CommandRangeSlackCm` (240 cm at defaults); it never drops the Attack. An `Interact` that names a hidden or rival court fixture is accepted at ingress, not dropped (AC 71, story 060): only an id that is not a public Interact target is dropped.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; two-stage validation, where stage 1 (ingress) is cheap and silent and stage 2 (the command check) runs only at the command's queue slot; per-pawn and per-connection token buckets; one batched owner reply per drain.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path this story touches)
**Engine Notes**: No V1-V6 item gates this story (it is plain-C++ server logic). E10: server RPC bodies run during the NetDriver receive at the top of the world tick, before the tick groups, so a command received this frame is in its mailbox before the `TG_PostPhysics` drain. E7: `UPackageMapClient::GetObjectFromNetGUID` resolves any GUID the server assigned, even for a pawn culled for that connection, so resolving `PawnRefGuid` at ingress grants no visibility (V3, story 010, verifies it). `FNetPawnCommand` carries `WorldPoint` as `FVector_NetQuantize10`, so the clamp test must allow +/- 0.1 cm of quantisation.

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — source: ADR-0011
- Required: Discrete commands travel only in `AStealthPlayerController::ServerPawnCommand`; `PawnRef` travels as a NetGUID value — source: ADR-0011
- Required: The listen host's input goes through `IngestCommand(HostLocal)` with the same ingress and reply flush — source: ADR-0011
- Forbidden: Never validate everything at ingress (single stage) — breaks source parity and the total order — source: ADR-0011
- Forbidden: Never treat a resolved `PawnRef` as evidence of visibility — source: ADR-0011
- Forbidden: Never special-case `NM_ListenServer` in input ingress — source: ADR-0011
- Guardrail: Command interface: uplink at most 20 reliable RPCs/s per connection (transport bucket); at most one reply RPC per connection per drain — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 6 (R5.1, Logic, U `IngressClampAndDrop`): **GIVEN** four commands — an Attack with a `WorldPoint` 900 cm away, an Attack with a self `PawnRef`, a command with a duplicate `ClientCommandSeq`, and an Interact whose `TargetId` names no public Interact target (a nonexistent id) **WHEN** ingress runs **THEN**:
   - the point is clamped to `AssassinationRangeCm + CommandRangeSlackCm` (240 cm);
   - the self ref is stripped and counted, and that Attack proceeds as a `TargetSpot`;
   - the duplicate is ignored;
   - the non-target Interact is dropped (an Interact naming a hidden or rival court fixture is **not** dropped: it is accepted, AC 71);
   - the dropped commands get no reply.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 3.1 and Implementation Guidelines:*

**Ingress order (ADR-0011 3.1).** In `IngestCommand`, in this order, each failure a silent drop counted for the server-only abuse log (`AbuseLogDropCount` within `AbuseLogWindowSec`): (1) well-formed payload; (2) `PossessionEpoch` equals the controller's `ServerPossessionEpoch` and `Seq` is newer than the last accepted (stale or duplicate dropped); (3) per-connection transport bucket (stories 021, 022); (4) `Attack`: `WorldPoint` clamped (not dropped) to `AssassinationRangeCm + CommandRangeSlackCm` of the pawn; a `PawnRefGuid` naming the commanding pawn is stripped and counted; the `PawnRefGuid` is resolved through the NetDriver's GUID cache (E7) to a weak pawn handle, unresolvable meaning absent; (5) `InteractBegin`: the id must be a public Interact target, and every court fixture id is accepted whatever its role or the sender's knowledge. This story owns steps 4 and 5 and the duplicate-Seq half of step 2; step 1 is story 023's `WithValidation` territory, the epoch half of step 2 is story 025, step 3 is stories 021 and 022.

A command that passes is submitted as `UResolutionQueue::Submit(Mailbox, FQueueItem{PawnCommand, FCommandPayload})`; the payload carries the pawn handle, the controller's recorded control epoch, the `ClientCommandSeq` and the opaque `FReplySinkId`. Ingress never touches the reply sink for a drop, which is how "no reply" holds (ADR-0011 section 8, ingress tier: silent).

**Rationale.** ADR-0011 rejected single-stage validation (Alternative 2) because checking legality at ingress makes net-receive and host-local commands see different live state and breaks the total order; stage 1 therefore does only what is cheap and state-free (shape, clamp, strip, id gate, rate). Clamping rather than dropping the `WorldPoint` keeps an honest high-ping client's Attack alive (GDD R5.1, `CommandRangeSlackCm` is the position-skew tolerance). Stripping a self `PawnRef` and proceeding as `TargetSpot` means a forged self-reference can neither crash resolution nor leak a distinct reply.

**GDD references.** Core Rules R5 (stage 1 list) and R5.1; R6 ingress tier; R10 for the `PawnRef` and `WorldPoint` meaning; Edge Cases "Attack and target payloads (anti-oracle)". Tuning Knobs (defaults): `CommandRangeSlackCm` 40 (hard 20-80), `AbuseLogDropCount` 20, `AbuseLogWindowSec` 10; `AssassinationRangeCm` 200 is owned by Assassination (consumed, not duplicated).

**Prototype superseded.** Supersedes the validate-nothing server RPC bodies `ServerAssassinate(Target, StrikePoint)` (the client names the victim object), `ServerSetInteracting` and `ServerSetDefence` in `StealthCharacter.cpp`, and the controller RPCs in `StealthPlayerController.*`. Nothing is ported. `StealthRulesTests.cpp` (`StealthGame.Rules.*`) is the style template for this pure-logic test.

**Test seams and gaps (from QL-STORY-READY).** The self-ref strip counter has no named counter in the ADR or GDD ("counted" is unnamed, and a stripped self-ref is not a drop); the author must name it (the qa-lead suggests `SelfRefStripCount`) and keep it distinct from the ingress-drop counter. The test needs a stub queue that records `Submit` calls (mirrors ADR-0002 Submit/ScheduleAt; owned cross-epic by round-match-framework, shared doubles by story 095) and readable counters. The four commands share state: the duplicate needs a prior accepted Seq. The non-finite-point case is `WithValidation` territory (story 023), not tested here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-021-ingress-never-drops-matched-end.md`: the matched-End guarantee and the unmatched-End limiter
- `production/epics/input-interaction-framework/story-022-transport-bucket-and-abuse-log.md`: the per-connection transport bucket and the abuse-log entry rule
- `production/epics/input-interaction-framework/story-023-impossible-payload-disconnects.md`: `WithValidation` and the disconnect on structurally impossible payloads
- `production/epics/input-interaction-framework/story-025-possession-epoch-gate.md`: the stale or ahead `PossessionEpoch` drop
- `production/epics/input-interaction-framework/story-060-hidden-site-probe.md`: acceptance of hidden or rival court ids through resolution (AC 71)
- `production/epics/input-interaction-framework/story-062-stale-pawnref-falls-back.md` and `story-063-pawnref-gate.md`: `PawnRef` fall-back and the `CanViewerKnow` and `PawnRefAgreementCm` gate

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/CommandIngress.h` (planned name: `IngestCommand`, `FCommandOrigin`, ingress counters)
- `Source/StealthGameplay/Private/Core/CommandIngress.cpp`
- `Source/StealthGameplay/Private/Tests/Input/IngressClampAndDropTest.cpp` (class `FInputIngressClampAndDropTest`, name `StealthGame.Input.IngressClampAndDrop`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "counted" has no named counter: the abuse-log drop counter counts drops, but the stripped self-ref is not a drop.
- The clamp geometry is unstated.
- The four commands share state (a duplicate needs a prior accepted Seq).

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Clamp keeps direction and sets distance to AssassinationRangeCm + CommandRangeSlackCm (240 at defaults) +/- 0.1 cm (quantisation); self-ref strip increments `SelfRefStripCount`; duplicate and non-target Interact each increment the ingress-drop counter (2); none of the four gets a reply."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/IngressClampAndDropTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-002-pawn-command-core-skeleton.md`
  - `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`
  - `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`
  - cross-epic: tuning-data-config (module skeleton and the knob registry; stories not yet written)
  - cross-epic: round-match-framework (`UResolutionQueue::Submit` and mailbox API; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-021-ingress-never-drops-matched-end.md`
  - `production/epics/input-interaction-framework/story-022-transport-bucket-and-abuse-log.md`
  - `production/epics/input-interaction-framework/story-023-impossible-payload-disconnects.md`
  - `production/epics/input-interaction-framework/story-024-host-ingress-parity.md`
  - `production/epics/input-interaction-framework/story-025-possession-epoch-gate.md`
  - `production/epics/input-interaction-framework/story-026-bound-unbound-same-events.md`
  - `production/epics/input-interaction-framework/story-027-stage-two-at-resolution.md`
  - `production/epics/input-interaction-framework/story-093-static-scan-and-prototype-retirement.md`
