# Story 044: AC 19 DebounceMerge

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story builds the server-side release debounce, identical for every pawn: an `End` becomes pending for `ReleaseDebounceSec` (0.10 s); a same-type `Begin` from the same pawn inside the window cancels the pending End and the action simply continues (no token, event, cap restart or cooldown change); otherwise `HoldTimer(DebounceExpiry)` executes the End. A tap is therefore a 0.10 s minimum brace for humans and AI alike. The debounce is server-side on purpose: a client-side version was rejected because it would give humans a longer minimum brace than AI, a Pillar 1 tell.

It follows story 037 (the hold timer items and `ActionEpoch` voiding) and is the base for stories 045 (brace window bounds) and 046 (same-frame tap). ADR-0002 is the governing ADR for this story because the debounce is a due-time heap item whose ordering (equal `DueTime` by the key of the hold's Begin; `Complete` before `DebounceExpiry` at an exact tie) is ADR-0002 section 4; ADR-0011 section 5 defines how the IIF uses it. GDD rule: R8 (Release debounce) and F2.

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: One total-order resolution queue keyed `(ServerTickIndex, Seq)` drained in `TG_PostPhysics` with a due-time heap: every time-driven end of a hold is a `ScheduleAt` item, never a client timestamp or the IIF's own tick.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 HIGH, ADR-0002 MEDIUM)
**Engine Notes**: ADR-0011 rates the command path MEDIUM; the HIGH value is the input layer, not exercised here. No V1-V6 gate. The drain is a custom `TG_PostPhysics` tick function (ADR-0002); the debounce quantises to server drains, so `W_real` lies in `[W, W + dt)` (F2). Tests inject the clock and a stub queue that models the due-time heap.

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — ADR-0011
- Required: Action-hub events fire only inside queue slots and are native — ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp; never debounce releases client-side — ADR-0011
- Forbidden: Never rate-limit `End` per pawn or drop it at the transport bucket when it matches an outstanding Begin — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case, inside the 0.5 ms queue line — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 19 (R8, Logic, U `DebounceMerge`): **GIVEN** End resolved at t and Begin at t+0.05 **THEN** the action is continuous, with no new token or event; **GIVEN** Begin at t+0.15 instead **THEN** the End executed in the first drain at or after t+0.10 (within one dt).

---

## Implementation Notes

*Derived from ADR-0011 Decision section 5 and ADR-0002 section 4:*

- **Mechanism (ADR-0011 section 5).** "Release debounce, server-side, for every pawn: an `End` becomes *pending* for `ReleaseDebounceSec`; a same-type `Begin` from the same pawn inside the window cancels the pending End (no token, event, cap restart or cooldown change); otherwise `HoldTimer(DebounceExpiry)` executes the End. A tap is a 0.10 s brace for humans and AI alike."
- **Merge case (Begin at t+0.05).** The pending End is cancelled and its `DebounceExpiry` item is voided through the `ActionEpoch` (the item belongs to an ended or superseded pending state and is voided silently). No new token is drawn, no `OnActionStarted` or `OnActionCancelled` fires, `FHoldStatus` is unchanged, cap `DueTime` and cooldown are unchanged. Applies to Prepare and to Interact.
- **Expiry case (Begin at t+0.15).** The End executes in the first drain at or after `t + ReleaseDebounceSec` (within one dt; the heap treats an item as due when `DueTime <= t_now + epsilon`, epsilon = Round R12's 1 ms, cited and not redefined here). `OnActionCancelled(Released)` fires, then the later Begin starts a new action with a new token and a new event.
- **Tie rule.** `HoldTimer(Complete)` and `HoldTimer(DebounceExpiry)` of the same hold with the same `DueTime` resolve `Complete` first (Objectives change request 1b). A Begin landing exactly at `t + 0.10` (key order against `DebounceExpiry`) is optional per the qa-lead; do not rely on it.
- **Release before D still completes (Interact).** A release less than `ReleaseDebounceSec` before `D` still completes because the End is still pending; the same 0.10 s for every pawn, not partial credit (R8). Story 055 asserts the completion side.
- **Test seam.** Stub queue with a due-time heap and an injected clock; run dt in {1/60, 1/30}; same assertions for the AI path (direct calls, null sink) and the human path.
- **Tuning key.** `ReleaseDebounceSec` 0.10 (hard 0.05-0.2, safe 0.05-0.12, latch Action). The 0.05 minimum guarantees a Brace write and its end land in different frames.
- **Prototype.** Replaces `OnDefencePressed/Released` and `OnInteractPressed/Released` (immediate ends driven by the client key state); not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 037 (`story-037-hold-completion-epsilon.md`): `HoldTimer(Complete)` schedule and completion epsilon.
- Story 045 (`story-045-brace-window-bounds.md`): brace window bounds `W_real` over 0 / 1.2 / 9 s holds.
- Story 046 (`story-046-same-frame-tap-is-short-brace.md`): Begin and End in one mailbox in one frame.
- Story 043 (`story-043-prepare-mash-no-reset.md`): repeated Begins during an active or cooling brace.
- Client-side release safety (`UPawnCommandClient` End flush): stories 013 and 094.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h (pending-End state)
- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (debounce merge, `DebounceExpiry` resolver)
- Source/StealthGameplay/Private/Tests/Input/DebounceMergeTest.cpp (`FInputDebounceMergeTest`, `StealthGame.Input.DebounceMerge`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Logic story — automated test spec (`StealthGame.Input.DebounceMerge`):**

- **AC-1**: Release debounce merges a quick re-press
  - Given: active Prepare (repeat for Interact), End resolved at t, `ReleaseDebounceSec` 0.10.
  - When: Begin resolves at t+0.05.
  - Then: action continuous: no new token drawn, no `OnActionStarted`/`OnActionCancelled`, `DebounceExpiry` voided, cap/cooldown unchanged.
  - Given: same, Begin at t+0.15 instead.
  - Then: End executes in the first drain at or after t+0.10 (within one dt), `OnActionCancelled(Released)`, then the Begin starts a new action (new token, new event).
  - Edge cases: dt 1/60, 1/30; same for AI and human; Begin exactly at t+0.10 (key order vs `DebounceExpiry`) not required, optional.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/DebounceMergeTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-037-hold-completion-epsilon.md`; cross-epic: round-match-framework (stories not yet written) for the resolution queue and due-time heap; cross-epic: tuning-data-config (stories not yet written) for `ReleaseDebounceSec`
- Unlocks: `production/epics/input-interaction-framework/story-045-brace-window-bounds.md`, `production/epics/input-interaction-framework/story-046-same-frame-tap-is-short-brace.md`
