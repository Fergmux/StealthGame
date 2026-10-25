# Story 067: AC 26 NoEventRelay

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity, two-stage validation, Enhanced Input edges and batched owner replies. Action-hub events are server-only and native; no listener may relay an event to a client except through its own Visibility Policy audience.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 input layer HIGH, command path MEDIUM)
**Engine Notes**: No post-cutoff API is used. The test needs a way to observe "a client RPC sent from inside a handler's call stack"; the engine offers no such hook (the qa-lead flags that an "RPC-send hook" is not an engine facility), so the story introduces an injectable client-RPC sender seam on the controller or Visibility Policy layer. Native (non-dynamic) multicast delegates let the test enumerate the hub's registered listeners at test time.

**Control Manifest Rules (this layer)**:
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Forbidden: Never relay action-hub events to a client except through the Visibility Policy — source: ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — source: ADR-0011
- Forbidden: Never put owner-only data on the pawn — source: ADR-0011

This story is the enforcement test for the "no relay" half of GDD R11. Hub events (`Started`, `Cancelled`, `Completed`) are rich in information: a `Cancelled` event with its reason tells a listener exactly why a tender stopped. If any hub subscriber (Objectives, Concealment, Assassination, Crowd, the HUD presenter, a debug tool) forwarded such an event to a client in the same call stack, an opponent could learn it, and the timing of the RPC would itself encode a server-side decision point. The permitted route to a client is each system's own Visibility Policy audience, which applies the leak catalogue to what is sent.

It depends on story 066 (the hub's order and the `CancelReason` scan) and on story 095 (the shared test seams: the outgoing-RPC spy and the stub doubles), and it sits at the end of the hub chain. It relates to GDD R11 and ADR-0011 section 6. Because each hub subscriber lives in another epic, the test enumerates the hub's registered-listener list at run time so later subscribers are covered automatically.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 26 (R11, Integration, U `NoEventRelay`): **GIVEN** each hub subscriber, with an RPC-send hook installed **WHEN** it handles an event **THEN** zero client RPCs are sent within the handler's call stack, except through a Visibility Policy audience.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 6 and the Implementation Guidelines:*

- "Action-hub events must fire only inside queue slots, must be native (not Blueprint-assignable) and must never be relayed to a client except through the Visibility Policy." No dynamic or Blueprint-assignable delegate and no client copy exist, so a listener can only be native C++ code on the server.
- The seam the test needs (qa-lead gap 1): an injectable client-RPC sender interface on the controller or Visibility Policy layer that records each outgoing RPC with a call-stack depth marker. The shared outgoing-RPC send hook of story 095 is the intended home; if it does not give a depth marker this story extends it. Do not patch engine code and do not add a production-only RPC.
- Machine-testable form of the exception (qa-lead gap 2, proposed rewrite for the author to confirm): "zero client RPCs are sent within the handler's call stack, except calls made through `FVisibilityAudienceSender` (the single Visibility Policy send function), which the hook records separately". The Visibility Policy epic owns that function; until it exists use the stub from story 095.
- Subscriber enumeration (qa-lead gap 3): "each hub subscriber" depends on other epics' subscribers; enumerate by the hub's registered-listener list at test time so later subscribers are covered automatically. A test with zero registered subscribers must fail (denominator guard).
- Test per the qa-lead: a world with every registered hub subscriber and a sender seam recording RPCs with a call-stack depth marker; raise each of Started, Cancelled and Completed for a test pawn; per subscriber, the count of non-VP client RPCs sent inside the handler is 0. Edge cases: a subscriber that routes via a Visibility Policy audience (allowed, recorded); a subscriber that defers an RPC to a later tick (outside this check, note for review).
- Prototype disposition: new code, no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-066-event-hub-order.md`: key order of events and the `CancelReason` reflection scan.
- `production/epics/input-interaction-framework/story-095-command-test-seams.md`: the shared harness seams and stub doubles this story consumes.
- `production/epics/input-interaction-framework/story-068-no-owner-data-on-pawn.md`: owner-only data kept off the pawn.
- `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`: reply timing and identity blindness.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/Server/ActionHub.h` (registered-listener enumeration for the test, if not already exposed)
- `Source/StealthTestHarness/Private/` outgoing-RPC sender seam with call-stack depth marker (extends story 095's hook)
- `Source/StealthGameplay/Private/Tests/Input/NoEventRelayTest.cpp` (class `FInputNoEventRelayTest`, name `StealthGame.Input.NoEventRelay`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Gap 1: "RPC-send hook" is not an engine facility; the story must name the test seam (for example an injectable client-RPC sender interface on the PlayerController or Visibility Policy layer).
- Gap 2: "except through a Visibility Policy audience" has no machine test.
- Gap 3: "each hub subscriber" depends on other epics' subscribers; enumerate by the hub's registered-listener list at test time so later subscribers are covered automatically.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "...zero client RPCs are sent within the handler's call stack, except calls made through `FVisibilityAudienceSender` (the single Visibility Policy send function), which the hook records separately".

QA-lead test sketch (unconfirmed): `StealthGame.Input.NoEventRelay`; Given a world with every registered hub subscriber and a sender seam recording RPCs with a call-stack depth marker; When each of Started, Cancelled, Completed is raised for a test pawn; Then per subscriber the count of non-VP client RPCs sent inside the handler is 0. Edge cases: a subscriber that routes via a VP audience (allowed, recorded); a subscriber that defers an RPC to a later tick (outside this check, note for review); zero subscribers registered fails the test (denominator guard).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Input/NoEventRelayTest.cpp` (automation test `StealthGame.Input.NoEventRelay`, U form) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-066-event-hub-order.md`, `production/epics/input-interaction-framework/story-095-command-test-seams.md`; cross-epic: information-visibility-policy (stories not yet written; the Visibility Policy audience send function)
- Unlocks: None
