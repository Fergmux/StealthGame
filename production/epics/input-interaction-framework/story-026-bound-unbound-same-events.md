# Story 026: AC 3 BoundUnboundSameEvents

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This is the parity test for the whole command boundary (GDD R2 and R17, ADR-0011 sections 3 and 10): a human command arriving by the remote-client path, the same command arriving by the host-local path, and an AI command made by calling the component directly must produce the same hub events at the same tick offsets, and the two human paths must produce the same reply codes. The AI pawn's null sink discards everything, which is the only permitted difference, decided by the sink and never by a caller check.

It is the integration point of stories 004 (reply sink and flush), 020 (ingress), and 024 (host parity), and so it depends on all three. It does not introduce new production behaviour; it asserts that the pieces built by those stories compose without any identity-dependent branch, which is the Pillar 1 guarantee ("below the boundary no system can tell who issued a command"). ADR-0011 Validation Criteria adds a byte-identity requirement (a human command and an AI command with the same content produce identical `FCommandPayload` bytes after ingress, excluding the sink), which this test should also check on the payloads it captures.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; human, host-local and AI commands converge on the same entry points, queue slot check and events, differing only by an opaque reply sink (null for AI).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story, but V6 (story 024) establishes the host half of the comparison. The test is pure logic over `FPawnCommandCore`, a stub queue and a stub PlayerController RPC path (no `UWorld`), per ADR-0008 headless specs.

**Control Manifest Rules (this layer)**:
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; every discrete command from any source enters through its entry points and the queue — source: ADR-0011
- Required: The listen host's input goes through `IngestCommand(HostLocal)` with the same ingress and reply flush — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step; a null sink (AI) discards — source: ADR-0011
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener; the command component never calls `IsPlayerControlled`, `IsLocallyControlled`, `IsBotControlled` or a Roster binding query — source: ADR-0011
- Forbidden: Never special-case `NM_ListenServer` in input ingress — source: ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case; at most one reply RPC per connection per drain — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 3 (R2/R17, Logic, U `BoundUnboundSameEvents`): **GIVEN** three pawns in the same state — one driven by the remote-client path (stub PC RPC), one by the host-local path (`IngestCommand(Source=HostLocal)`), and one unbound pawn driven by direct AI calls **WHEN** each receives the same scripted command sequence **THEN** the recorded hub events (event, ActionType, tick offset) are identical for all three; **AND** the reply codes for the two human paths are identical; **AND** the AI pawn's null sink records zero replies.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3, 8 and 10 and Implementation Guidelines:*

Remote path: stub PC RPC to `IngestCommand` (stage 1) to `UResolutionQueue::Submit` to the stage 2 resolver. Host-local path: `IngestCommand(HostLocal)` with a synthesised Seq (story 024). AI path: the component's entry points called directly on the server through the server-local mailbox with a null sink, skipping stage 1 but not the per-pawn buckets or any stage 2 step (R17: no exemptions). Replies are appended to the sink's per-pass buffer and flushed once at drain end (section 8), never inline.

**Rationale.** ADR-0011 section 10: AI "calls the same entry points on the server"; "neither rule is enforced by a kind check here". The only legitimate difference is ingress (invisible to other clients) and the sink. Any divergence in hub events would be a tell observable to every client.

**Script (QL-STORY-READY proposal; author to confirm).** The AC says "the same scripted command sequence" without defining it. Proposed: one command per drain at dt 1/64 s: `InteractBegin(own fixture)` at tick 0; `InteractEnd` at tick 4; `PrepareBegin` at tick 20; `PrepareEnd` at tick 24; `PrepareBegin` at tick 30 (cooldown reject, so reply parity is non-empty); `HideEnter` near own bush at tick 40; `Attack` at tick 60. Each pawn has its own fixture and bush, because three pawns acting on one fixture would contend (full-fixture refusal). Tick offsets are measured from the first command's tick. The AI path skips ingress but not the per-pawn buckets, so keep the script under them.

**GDD references.** Core Rules R2 and R17; R6 (reply tiers); Edge Cases "If an AI bug floods commands". Tuning Knobs (defaults, from tuning data): `BucketInteractSize` 3, `BucketPrepareSize` 2, `BucketHideSize` 4, `ReleaseDebounceSec` 0.10.

**Prototype superseded.** Supersedes `StealthNpcController.*` (AI movement and actions that did not go through the player path) and the per-feature `ServerAssassinate`, `ServerSetDefence` and `ServerSetInteracting` RPC paths in `StealthCharacter.cpp`; nothing ported. `StealthRulesTests.cpp` is the unit-test style template.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-024-host-ingress-parity.md`: stage-1 byte parity of host versus remote (V6) for clamp, duplicate and mash inputs
- `production/epics/input-interaction-framework/story-032-ai-no-exemption.md`: AI and human bucket parity at the Prepare bucket
- `production/epics/input-interaction-framework/story-006-no-caller-identity-parameter.md`: the reflection and grep check that no identity name or type exists (AC 2)
- `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`: reply timing independent of nearby pawn identity

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Private/Tests/Input/BoundUnboundSameEventsTest.cpp` (class `FInputBoundUnboundSameEventsTest`, name `StealthGame.Input.BoundUnboundSameEvents`)
- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (only if a parity defect is found; this story otherwise adds no production code)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "The same scripted command sequence" is not defined.
- Three pawns acting on one fixture would contend (full-fixture refusal), so identical events need separate fixtures.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Script (one command per drain, dt 1/64): InteractBegin(own fixture) at tick 0; InteractEnd at tick 4; PrepareBegin at tick 20; PrepareEnd at tick 24; PrepareBegin at tick 30 (cooldown reject); HideEnter near own bush at tick 40; Attack at tick 60. Each pawn has its own fixture and bush."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/BoundUnboundSameEventsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`
  - `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`
  - `production/epics/input-interaction-framework/story-024-host-ingress-parity.md`
  - cross-epic: round-match-framework (stub or real queue; stories not yet written)
  - cross-epic: crowd-npc-behaviour (AI direct-call path; stories not yet written)
- Unlocks:
  - None
