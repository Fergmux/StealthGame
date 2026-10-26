# Story 095: Server command log, outgoing-RPC spy and shared stub doubles exist in the test harness

> **Epic**: Input & Interaction Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`, `TR-iif-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: The command path has two observable server boundaries (the ingress/command stream and the owner reply flush) and consumer-owned seams (the queue, the Roster legality class, the shared pawn state view, Camera's aim ray, the feature predicates); ADR-0011's validation criteria (byte-identity of a human and an AI command after ingress, zero replies inside a slot) can only be asserted if the harness can record and stub them.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Test infrastructure; no post-cutoff API is introduced. Multi-client PIE and net emulation use ADR-0008's CQTest harness (`NETWORK_TEST_CLASS`, naming `StealthGame.Input.PIE.<Scenario>.<Method>`; PktLag 50 ms per direction and 1 % loss unless an AC states otherwise). The outgoing-RPC send hook must observe client-to-server RPCs without altering them. No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Every discrete command from any source enters through the entry points and the queue — source: ADR-0011 (the command log observes that single path)
- Required: A resolution-voided command produces exactly one reply, flushed once per drain per connection — source: ADR-0011 (the spy observes the flush)
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — source: ADR-0011 (the stub doubles must not introduce any)
- Forbidden: Never put owner-only data on the pawn — source: ADR-0011
- Guardrail: Command-interface tests live under the test roots of ADR-0007/0008; test seams are compiled only in test and development builds, never in Shipping — source: ADR-0011, ADR-0008

This story was added after the QA-lead pass to close QL-STORY-READY systemic gaps 2 to 4. The qa-lead found that many IIF tests need a server-side record of every command that reached the server (type, payload, `ClientCommandSeq`, arrival tick), a hook that records outgoing client-to-server RPCs (to prove "no RPC other than `ServerMove`" and "zero RPCs"), and a consistent set of stub doubles for the cross-epic interfaces the IIF consumes. Without them each story would invent its own, and the parity tests (a human and an AI command through identical paths) could not share fixtures.

It depends on story 002 (the core skeleton whose seams it implements stubs for) and 094 (the controller transport it observes). It unlocks stories that need these seams: 013 (flush), 015 (default bindings), 018 (walk-slower server clause), 067 (no event relay) and 083 (rejection timing identity-blind). Per the plan's note, story 002 declares only the interfaces of the cross-epic seams; the named doubles live here.

---

## Acceptance Criteria

*ADR-derived (no numbered GDD AC). Source: QL-STORY-READY systemic gaps 2 to 4 (command log, outgoing-RPC send hook, named stub doubles) under ADR-0011 Validation Criteria; criterion text from the plan:*

- [ ] **GIVEN** the `StealthTestHarness` module with a scripted command sequence **WHEN** a harness self-test runs it through the controller transport **THEN** the server command log records each command's type, payload, `ClientCommandSeq` and arrival tick in order, the outgoing-RPC hook records each client-to-server RPC sent and reports zero for a sequence that sends none, and the named stub doubles (Round queue, Roster legality class, SharedPawnStateView/Locomotion, Camera aim ray, Objectives interact-target owner, Assassination and Concealment predicates) each record the calls made on them, so every seam shows what the scripted sequence does.

---

## Implementation Notes

*Derived from ADR-0011 Validation Criteria and Key Interfaces; QL-STORY-READY systemic gaps:*

What to build (names are planned, confirm at implementation):
- **Server command log:** a recorder at the output of ingress (stage 1) and at the queue submit, capturing for each command: type, payload, `ClientCommandSeq`, arrival tick, and the opaque sink id; readable from the test body. It serves the "server command log" referred to by ACs 5, 30, 45 and the VS gate `ToggleStreamParity`. It must not alter ordering or timing.
- **Outgoing-RPC send hook:** a hook on the client controller recording every client-to-server RPC (name, args size) so a test can assert "zero RPCs" (AC 29) and "no RPC other than the `ServerMove` family" (AC 76); and, on the server, a spy on the `ClientCommandReplies` call (count and timing relative to the drain) for story 004 and AC 47.
- **Stub doubles**, named and shared by the IIF tests: `FStubResolutionQueue` (mirrors ADR-0002 `Submit`/`ScheduleAt`, records items and due times, controllable drain), `FStubRosterLegality` (returns a configured legality class, `Active`/`Informant`/`RoundEnded`/`Unbound` mapping to `Active`), `FStubSharedPawnStateView` (`IsRunning`, `IsInTraversal`, `CommandedDisplacementCm`), `FStubCamera` (`GetAimRay`), `FStubInteractTargetOwner` (Objectives; `GetInteractHoldSec`, `OnInteractExecuted/Ended`), `FStubCommandPredicate` (Assassination and Concealment; configurable Allow/AllowNoEffect/Reject(Code)). Each records calls made on it. They must carry no caller identity.

QA notes: tests that depend on rate-limit arrivals state dt = 1/64 s where a 0.125 s multiple matters (binary-exact boundaries, per the qa-lead). Net-emulation profile for N-form tests: PktLag 50 ms per direction, 1 % loss unless the AC states otherwise. The seams are compiled out of Shipping.

The criterion has no qa-lead verdict; its self-test is the story's own evidence. The intent of the self-test: script one human command and one AI command with the same content and show the log records identical entries after ingress (the ADR byte-identity criterion's observation point), without asserting identity here (that is story 026).

Prototype: new code, no prototype counterpart (the prototype's `StealthRulesTests.cpp` pure-logic style, names `StealthGame.Rules.*`, is only a style template). Tuning keys: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-002-pawn-command-core-skeleton.md`: the core and the seam interface declarations.
- `story-094-controller-transport-skeleton.md`: the controller transport being observed.
- `story-026-bound-unbound-same-events.md`: the parity assertion (byte-identity of human and AI commands).
- `story-004-reply-sink-and-drain-end-flush.md`: reply flush behaviour (uses the spy).
- The real Round queue, Roster, Locomotion, Camera, Objectives, Assassination and Concealment implementations (cross-epic, stories not yet written).

---

## Files

*The code and test files this story changed (written by /dev-story; /story-done checks scope against it).*

- `Source/StealthTestHarness/Private/Input/CommandLog.h`
- `Source/StealthTestHarness/Private/Input/RpcSpy.h`
- `Source/StealthTestHarness/Private/Input/StubDoubles.h`
- `Source/StealthGameplay/Public/Server/Testing/StealthCommandSeams.h`
- `Source/StealthGameplay/Private/Core/StealthCommandSeams.cpp`
- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp`
- `Source/StealthTestHarness/Private/Tests/Input/CommandTestSeamsSelfTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: NOT ASSESSED — story added after the review

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CommandTestSeamsSelfTest.cpp` — the harness self-test must exist and pass

**Status**: [x] Created and passing (`StealthGame.Input.CommandTestSeams`, `StealthGame.Input.PIE.CommandTestSeams.ScriptedSequenceIsLoggedAndSpied`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-002-pawn-command-core-skeleton.md`, `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md`; cross-epic: tuning-data-config (stories not yet written; StealthTestHarness module, ADR-0008)
- Unlocks: `production/epics/input-interaction-framework/story-013-flush-sends-end.md`, `production/epics/input-interaction-framework/story-015-default-bindings.md`, `production/epics/input-interaction-framework/story-018-walk-slower-two-client.md`, `production/epics/input-interaction-framework/story-067-no-event-relay.md`, `production/epics/input-interaction-framework/story-083-rejection-timing-identity-blind.md`

---

## Completion Notes
**Completed**: 2026-10-26 (IIF batch 13: command/reply types, core, hub, reply step, transport, seams, spikes V2/V3/V5, input ACs; D53 integration of the production input path)
**Criteria**: 1/1 passing (through the controller transport the server log records kind, seq, epoch, sink, mailbox and arrival tick in order at ingress output and at the queue submit; the outgoing-RPC hook records each ServerPawnCommand and reports zero beyond the engine move/camera background for a sequence that sends none; each stub double records its calls; an AI command is logged at the submit point with the server-local mailbox and the null sink)
**Deviations**: The harness headers are in Source/StealthTestHarness/Private/Input/ (not Public/Input/, which ADR-0007 rule (a) would reject); the seam itself (StealthCommandSeams) is in StealthGameplay under WITH_STEALTH_TEST_SEAMS. The RPC spy uses UNetDriver::SendRPCDel (non-Shipping); 'sends none' excludes the engine's ServerMove family and ServerUpdateCamera (ADR-0015 disables client camera updates later). FStubCamera and FStubInteractTargetOwner are plain recorders (the Camera and Objectives interfaces do not exist yet).
**Test Evidence**: `Source/StealthTestHarness/Private/Tests/Input/CommandTestSeamsSelfTest.cpp`; full suite `production/qa/evidence/iif-command-path-2026-10-26/full-suite.txt`
Run result: N/A — test infrastructure; nothing is drawn
**Reviewed by**: lead-programmer focused review (sonnet; 3 medium + 6 low findings, all fixed except the documented limits) + self-review, IIF batch 13
