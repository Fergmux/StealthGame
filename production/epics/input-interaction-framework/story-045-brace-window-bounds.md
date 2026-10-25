# Story 045: AC 33 BraceWindowBounds

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

This story verifies formula F2, the effective brace window: `W = min(t_endEff, t_start + D_cap) - t_start` with `t_endEff = t_endRes + ReleaseDebounceSec` for every pawn. Both ends are `HoldTimer` items (`DebounceExpiry`, `CapEnd`), so the realised window is quantised to server drains: `W_real` lies in `[W, W + dt)`. The three canonical holds are 0 s (the minimum brace), 1.2 s and 9 s (which ends by the cap with `OnActionCompleted`), identical on the AI and human paths.

It builds on story 044 (the debounce and the pending-End machinery) and is the base for stories 047, 049, 053 and 058. The governing ADR is ADR-0002 because the bounds come from the due-time heap and its drain quantisation; ADR-0011 section 5 defines the window and the cap. GDD rules: R8 (Prepare, Release debounce) and F2.

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: A single server-side resolution queue with a due-time heap drained once per frame: hold ends (`Complete`, `CapEnd`, `DebounceExpiry`) are due-time items stamped on the first drain with `DueTime <= t_now + epsilon`, so outcomes are quantised to server frames and never to client timestamps.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 HIGH, ADR-0002 MEDIUM)
**Engine Notes**: Command path is MEDIUM in ADR-0011; the input-layer HIGH does not apply. No V1-V6 gate. The drain tick is custom `TG_PostPhysics`; observers see a Brace from `t_start + L_down + U` where U is the one-frame `FActionState` replication delay (F2), a presentation concern outside this story.

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — ADR-0011
- Required: Action-hub events fire only inside queue slots — ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp; never debounce releases client-side — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case, inside the 0.5 ms queue line — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 33 (F2, Logic, U `BraceWindowBounds`): **GIVEN** holds of 0, 1.2 and 9 s at injected dt **WHEN** W_real is measured **THEN** it lies in [0.10, 0.10+dt), [1.30, 1.30+dt) and [4.0, 4.0+dt) respectively, identically on the AI and human paths.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 5, ADR-0002 section 4 and GDD F2:*

- **Both ends are queue items.** `CapEnd` is scheduled at the Begin with `DueTime = t_start + DefenceDurationSec`; `DebounceExpiry` is scheduled when the End resolves with `DueTime = t_endRes + ReleaseDebounceSec`. The earlier one executes; the other is voided by the `ActionEpoch`. A 9 s hold ends by `CapEnd` with `OnActionCompleted(Prepare)`; the shorter ones end by `DebounceExpiry` with `OnActionCancelled(Released)`. An End after the cap is a no-op.
- **Measurement.** `W_real` is measured from the `OnActionStarted` time to the end event, or to `FHoldStatus.ServerEndTime`. "Hold of X s" means the End resolves at `t_start + X`.
- **Known GDD defect, not fixed here.** AC 33 and F2 bound `W_real` at `[W, W + dt)` but the heap's due test admits Round R12's epsilon of 1 ms early (an item is due when `DueTime <= t_now + epsilon`), so a drain landing 0.999 ms early breaks the lower bound; dt is also unspecified ("injected dt"). The qa-lead proposes `W_real` in `[W - eps, W + dt)` with eps = 1 ms, for dt in {1/60, 1/30, the 0.0083 / 0.05 / 0.02 table}. Implement and test against that tolerance and flag the GDD F2 output range for the same correction; the AC text above stays the authority.
- **Defaults.** `ReleaseDebounceSec` 0.10, `DefenceDurationSec` 4.0 (Assassination-owned, range 3-5), so `W_real` in [0.10, 4.05) by default. Read from tuning, not literals.
- **Test seam.** Injected clock plus stub queue with a due-time heap; the AI path uses direct entry-point calls with a null sink, the human path a stub sink. Named doubles from story 095.
- **Prototype.** Replaces `OnDefencePressed/Released` and `ServerSetDefence` (client-timed brace, no cap item); not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 044 (`story-044-debounce-merge.md`): the merge rule itself.
- Story 046 (`story-046-same-frame-tap-is-short-brace.md`): same-frame Begin and End.
- Story 047 (`story-047-forced-run-cancels-brace.md`): the forced-run cancel and `FHoldStatus.ServerEndTime` update.
- Story 037 (`story-037-hold-completion-epsilon.md`): completion epsilon for Interact.
- Story 087 (`story-087-action-pose-bandwidth.md`) and story 082 (`story-082-action-pose-parity-observed.md`): observer-side onset and offset (`U`).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (`CapEnd` and `DebounceExpiry` scheduling)
- Source/StealthGameplay/Private/Tests/Input/BraceWindowBoundsTest.cpp (`FInputBraceWindowBoundsTest`, `StealthGame.Input.BraceWindowBounds`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC 33 bounds `W_real` at `[W, W+dt)` but the due test admits eps = 1 ms early (F1), so a drain landing 0.999 ms early breaks the lower bound.
- dt is unspecified ("injected dt").
- "Hold of X s" does not say whether X is End send time or End resolution time (send time adds up to one more dt).
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "...W_real lies in [W - eps, W + dt), eps = Round R12's 1 ms, for dt in {1/60, 1/30, 0.0083/0.05/0.02 table}; a hold of X s means End resolves at t_start + X." GDD F2 output range needs the same eps.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/BraceWindowBoundsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-044-debounce-merge.md`; cross-epic: round-match-framework (stories not yet written) for the due-time heap and epsilon; cross-epic: tuning-data-config (stories not yet written) for the knobs; cross-epic: assassination (stories not yet written) for `DefenceDurationSec`
- Unlocks: `production/epics/input-interaction-framework/story-047-forced-run-cancels-brace.md`, `production/epics/input-interaction-framework/story-049-reversal-consumes-brace.md`, `production/epics/input-interaction-framework/story-053-brace-sampled-at-execution.md`, `production/epics/input-interaction-framework/story-058-round-queue-order.md`
