# Story 051: AC 64 WrongPhaseCommands

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`, `TR-iif-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story pins down what happens to discrete commands outside the Active phase. A command submitted while the round queue is not open (phases `Preparing` and `Ended`) is dropped by the queue as `WrongPhase`: no `OnActionStarted`, no IIF reply (a silent drop for every source alike, human and AI) and no token drawn. A command that was already pending in the queue when the terminal latch closes it returns `Rejected_Latched` at its slot: no `OnActionStarted`, and exactly one `NotNow` reply enqueued at drain end and flushed in the end-of-drain batch through the queue's held-reply buffer. A latched hold timer (`Complete`) returns `Rejected_Latched` without any reply, because holds are frozen.

It depends on story 029 (reply code set and the end-of-drain reply step). The governing ADR is ADR-0002 (phases, latch, held replies released at the round-end step); ADR-0011 section 3.1 (`WrongPhase`) and section 8 (`Rejected_Latched` replies `NotNow`) define the IIF side. GDD rules: R5 (two-stage validation), R6 (reply tiers), and the Round phase rule.

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: One resolution queue per round with the terminal latch storage and a two-step round end: commands submitted while the queue is not open are dropped (`WrongPhase`), commands pending at latch close return `Rejected_Latched`, and their `NotNow` replies are held and released at the round-end step.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 HIGH, ADR-0002 MEDIUM)
**Engine Notes**: Command path is MEDIUM in ADR-0011; input-layer HIGH does not apply. No V1-V6 gate. The stub queue must model both result codes (`WrongPhase` from `Submit`, `Rejected_Latched` from the resolver) as a shared fixture, as the qa-lead requires.

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot; the per-pawn token bucket is consumed only on execution — ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection; `Rejected_Latched` replies `NotNow` through the held-reply buffer — ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — ADR-0011
- Forbidden: Never let the condition sweep cancel a hold itself — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: at most one reply RPC per connection per drain — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 64 (Round phase, Logic, U `WrongPhaseCommands`): **GIVEN** phase `Preparing` or `Ended` **WHEN** any discrete command is sent **THEN** Round drops it (`WrongPhase`); **AND** no `OnActionStarted` fires and no IIF reply is sent; **AND GIVEN** a command still pending in the queue when the latch closes it **THEN** it returns `Rejected_Latched`, no `OnActionStarted` fires, and exactly one `NotNow` reply is enqueued at drain end.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.1, 5 and 8, and ADR-0002 section 8.1:*

- **`WrongPhase` is the queue's, not the IIF's.** "Commands submitted while the round queue is not open (phase not `Active`) are dropped by the queue (`WrongPhase`, no reply, every source alike)" (ADR-0011 section 3.1). The IIF must not add its own phase check or reply; it observes the `Submit` result and draws no token (tokens are consumed only on execution).
- **`Rejected_Latched` replies `NotNow`.** A command voided `Rejected_Latched` replies `NotNow` through ADR-0002's held-reply buffer, released at the round-end step; exactly one reply per command; the normal flush point is the end of the drain's resolution step, and the second flush point is the end of the staging pass when it runs a round-end step bound to a death write (ADR-0011 section 8). Test only the "enqueued at drain end and flushed in the end-of-drain batch" behaviour here; the held-buffer release timing belongs to the Round epic.
- **Holds are frozen at the latch.** Every hold's completion item returns `Rejected_Latched`; it carries no reply sink for timer items, so a latched `HoldTimer(Complete)` produces no reply; holds cancel through the normal release blend at the round-end step.
- **Continuous commands unaffected.** `Move` and `Look` are never queue items and are not covered (Round may freeze movement; Locomotion/Round-owned).
- **Commands covered.** One of each discrete command: Traverse, Interact Begin/End, Prepare Begin/End, Attack, Hide Enter/Exit.
- **Test seam.** A `RoundQueue` stub whose `Submit` returns `WrongPhase` in phases `Preparing` and `Ended` and whose resolver path returns `Rejected_Latched`; sink capture of replies. Named doubles in story 095.
- **Prototype.** New code, no prototype counterpart (the prototype RPCs had no phase gate; they are replaced, not ported).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 029 (`story-029-rejection-code-set.md`): reply codes per stage and drain-end enqueue.
- Story 004 (`story-004-reply-sink-and-drain-end-flush.md`): the batched `ClientCommandReplies` flush.
- Story 025 (`story-025-possession-epoch-gate.md`): stale possession epoch drops at ingress.
- round-match-framework epic (cross-epic): the latch itself, phase machine, held-reply buffer release.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (`Rejected_Latched` handling, no-token-on-`WrongPhase`)
- Source/StealthGameplay/Private/Tests/Input/WrongPhaseCommandsTest.cpp (`FInputWrongPhaseCommandsTest`, `StealthGame.Input.WrongPhaseCommands`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Logic story — automated test spec (`StealthGame.Input.WrongPhaseCommands`):**

- **AC-1**: Wrong-phase commands
  - Given: `RoundQueue` stub whose `Submit` returns `WrongPhase` in phases `Preparing` and `Ended`; one of each discrete command (Traverse, Interact B/E, Prepare B/E, Attack, Hide Enter/Exit).
  - When: each is sent.
  - Then: no `OnActionStarted`, no reply from the IIF (silent drop), no token drawn.
  - Given: a command pending in the queue when the latch closes.
  - When: the drain runs.
  - Then: it returns `Rejected_Latched`, no `OnActionStarted`, exactly one `NotNow` enqueued at drain end, flushed in the end-of-drain batch.
  - Edge cases: continuous Move/Look not affected; a latched hold timer (`Complete`) returns `Rejected_Latched` without a reply; the stub must model both result codes (shared fixture).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/WrongPhaseCommandsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-029-rejection-code-set.md`; cross-epic: round-match-framework (stories not yet written) for the phase machine, `Submit` results and latch
- Unlocks: None
