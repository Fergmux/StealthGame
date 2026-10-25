# Story 046: AC 73 SameFrameTapIsShortBrace

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story covers the degenerate tap: `Prepare(Begin)` and `Prepare(End)` arriving in the same mailbox in one frame (a fast key tap or two direct AI calls in one tick). The mailbox is FIFO, so the Begin resolves first, the brace executes (and burns its cooldown), the End becomes pending, and `DebounceExpiry` ends it after the 0.10 s debounce. It must never run to the cap and the End must never resolve before the Begin.

It builds on story 044 (the debounce) and sits beside story 045 (the bounds for longer holds). The governing ADR is ADR-0002 (FIFO mailboxes, key order, due-time heap); ADR-0011 sections 3.1 and 5 apply the same rule to every source. GDD rules: R5 (ordering inside a mailbox), R8 (Release debounce) and F2.

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: One total-order resolution queue keyed `(ServerTickIndex, Seq)` with permuted per-source mailboxes (FIFO within a mailbox) and a due-time heap; hold ends are `ScheduleAt` items.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 HIGH, ADR-0002 MEDIUM)
**Engine Notes**: Command path is MEDIUM in ADR-0011; the input-layer HIGH does not apply. No V1-V6 gate. Reliable RPCs on one controller channel arrive in order, and E10 puts the received command in its mailbox before that frame's `TG_PostPhysics` drain, so a same-frame Begin and End share one drain.

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — ADR-0011
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot; the per-pawn token bucket is consumed only on execution — ADR-0011
- Forbidden: Never rate-limit `End` per pawn or drop it at the transport bucket when it matches an outstanding Begin — ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: at most 20 reliable RPCs/s per connection; stage 2 a few microseconds per frame — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 73 (R5/F2, Logic, U `SameFrameTapIsShortBrace`): **GIVEN** `Prepare(Begin)` and `Prepare(End)` in the same mailbox in one frame **WHEN** the drain resolves **THEN** the brace executes and ends via `DebounceExpiry` within [0.10, 0.10 + dt) s, never at the cap.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3, 5 and ADR-0002 mailbox ordering:*

- **FIFO inside a mailbox.** The Begin is keyed before the End; End-before-Begin must never happen. The End matches an outstanding Begin, so ingress never drops it (ADR-0011 section 3.1 step 3). The AI path (two direct calls in one tick, null sink) takes the same route through the server-local mailbox.
- **Result.** `OnActionStarted(Prepare)` once; cooldown stamped once at `t_start` (by the Assassination subscriber); the End is pending; `HoldTimer(DebounceExpiry)` fires and ends via `OnActionCancelled(Released)`; no `CapEnd` item fires (it is voided by the `ActionEpoch`).
- **Known GDD defect, not fixed here.** The bound `[0.10, 0.10 + dt)` ignores Round R12's 1 ms due tolerance (six ticks of 1/60 accumulate to about 0.0999999), and dt is not stated. The qa-lead's proposed bound is `[0.10 - eps, 0.10 + dt)` with eps = 1 ms for dt = 1/60 and 1/30. Implement and test against that and flag GDD F2 / AC 73 for the same correction; the AC text above stays the authority.
- **Test seam.** Stub queue honouring FIFO within a mailbox and the due-time heap; injected dt; predicate stub = Allow. Named doubles from story 095.
- **Tuning keys.** `ReleaseDebounceSec` 0.10, `DefenceDurationSec` 4.0 (Assassination-owned), `BucketPrepareSize` 2.
- **Prototype.** Replaces `OnDefencePressed/Released` and `ServerSetDefence`; not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 044 (`story-044-debounce-merge.md`): a re-press inside the pending window.
- Story 045 (`story-045-brace-window-bounds.md`): bounds for 0 / 1.2 / 9 s holds with separate End arrival.
- Story 042 (`story-042-prepare-cooldown-stamped.md`): the cooldown stamp assertion.
- Story 078 and 079 (ring monotonic clamp and resync): owner-side presentation of a short brace.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (End-after-Begin in one drain)
- Source/StealthGameplay/Private/Tests/Input/SameFrameTapIsShortBraceTest.cpp (`FInputSameFrameTapIsShortBraceTest`, `StealthGame.Input.SameFrameTapIsShortBrace`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Same strict-lower-bound defect as story 045: `[0.10, 0.10 + dt)` ignores the 1 ms due tolerance (6 x 1/60 accumulates to about 0.0999999), and dt is not given.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "...within [0.10 - eps, 0.10 + dt) s, eps = Round R12's 1 ms, for dt = 1/60 and 1/30, never at the cap."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/SameFrameTapIsShortBraceTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-044-debounce-merge.md`; cross-epic: round-match-framework (stories not yet written) for the mailbox and due-time heap; cross-epic: tuning-data-config (stories not yet written) for the knobs
- Unlocks: None
