# Story 038: AC 15 InteractEarlyReleaseResets

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

This story verifies the "early release resets progress to 0" rule of GDD R8: partial progress is never retained by the framework, so the tell stays one contiguous, readable interval (Pillars P2 and P4). With an Interact held to 2.0 s of 3.0 s, an End executes (after the 0.10 s release debounce) and a fresh Begin is then pressed; progress restarts at 0, a new `HoldTimer(Complete)` is scheduled at `t1 + D`, the new `FHoldStatus.ServerStartTime` is `t1`, and the original timer (due at `t0 + 3.0`) is voided by its `ActionEpoch` so no completion fires at the old due time.

It follows story 037 (the Complete timer and `ActionEpoch` it builds on). The cancel itself is `OnActionCancelled(Interact, Released)`, the first row of the R8 cancel table, which story 039 tests alongside the other causes; here the focus is that the next hold starts from zero.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; hold timers are queue items from server time carrying an `ActionEpoch`, and a timer whose epoch no longer matches is voided; the release debounce is server-side for every pawn.
**ADR Version**: 2026-10-24

**Other ADRs**: ADR-0002: Server resolution queue, server clock and round-end staging (due-time heap, `ScheduleAt`, epoch-token voiding).

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 HIGH for the input layer; ADR-0002 MEDIUM)
**Engine Notes**: No V1-V6 item gates this story. ADR-0002 section 4: every timed item carries an `FEpochToken` (and a hold's `ActionEpoch`); a stale token resolves `Rejected_StaleEpoch` and changes nothing. Time values are doubles (server world time).

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — source: ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — source: ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp; never debounce releases client-side — source: ADR-0011
- Forbidden: Never put owner-only data on the pawn — source: ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 15 (R8, Logic, U `InteractEarlyReleaseResets`): **GIVEN** an Interact at 2.0 s of 3.0 s **WHEN** End executes and Begin is re-pressed **THEN** progress restarts at 0.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 5 and GDD R8:*

"Early release resets progress to 0" (ADR-0011 section 5, Interact bullet; GDD R8). The mechanism: the End is pending for `ReleaseDebounceSec`; when `HoldTimer(DebounceExpiry)` executes the End the hold is cancelled `Released`, the hold's `ActionEpoch` advances and the original `HoldTimer(Complete)` is thereafter voided silently as stale. A fresh `InteractBegin` that executes afterwards is a new hold: a new `ActionStartServerTime = t1`, a new `HoldTimer(Complete)` at `t1 + D`, and an `FHoldStatus` with `ServerStartTime = t1`. A Begin that arrives inside the 0.10 s pending window merges with the pending End instead and the hold continues (AC 19, story 044); the test Begin must therefore follow the executed End.

**Rationale.** Retaining partial progress would let an observer's mental model of the tell depend on history; resetting to 0 keeps one contiguous, readable interval (GDD R8: "Partial progress is never retained by the framework, so the tell stays one contiguous, readable interval").

**Expected mechanics (QL-STORY-READY, ADEQUATE).** Given: Interact Begin executed at t0, D = 3.0, predicate `Allow`, still held at t0 + 2.0. When: End sent at t0 + 2.0 and executed (`DebounceExpiry` at +2.1), then a fresh Begin executed at t1 >= t0 + 2.1. Then: `OnActionCancelled(Interact, Released)` at t0 + 2.1; new `HoldTimer(Complete)` DueTime = t1 + 3.0 (progress 0 at t1); `FHoldStatus.ServerStartTime = t1`; the original timer (due t0 + 3.0) is voided by `ActionEpoch` and no completion fires at t0 + 3.0. Completion only at t1 + 3.0.

**GDD references.** Core Rules R8 (Interact, Cancels table first row "End (after the debounce) | Released"; "Early release resets progress to 0"; the 0.10 s grace consequence); F1. Tuning Knobs (defaults): `ReleaseDebounceSec` 0.10 (hard 0.05-0.2); `ObjectiveInteractSec` 3.0 (Objectives-owned).

**Prototype superseded.** Supersedes `OnInteractPressed/Released` and `ServerSetInteracting(bool)` in `StealthCharacter.cpp`, where the client set the hold state and any release timing was client-side. Not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-037-hold-completion-epsilon.md`: the Complete timer scheduling and epsilon
- `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`: every other cancel cause and its reason
- `production/epics/input-interaction-framework/story-044-debounce-merge.md`: Begin inside the pending window merging with the End (AC 19)
- `production/epics/input-interaction-framework/story-050-hold-timer-key-order.md`: Complete versus a same-frame Attack (AC 69)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (hold end resets progress; `ActionEpoch` advance)
- `Source/StealthGameplay/Private/Tests/Input/InteractEarlyReleaseResetsTest.cpp` (class `FInputInteractEarlyReleaseResetsTest`, name `StealthGame.Input.InteractEarlyReleaseResets`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Logic — automated test specs (QL-STORY-READY: ADEQUATE):**

- **AC-1**: AC 15 `InteractEarlyReleaseResets` (`StealthGame.Input.InteractEarlyReleaseResets`): early release resets Interact progress to 0
  - Given: Interact Begin executed at t0, D = 3.0, predicate Allow, still held at t0 + 2.0.
  - When: End sent at t0 + 2.0 and executed (`DebounceExpiry` at +2.1), then a fresh Begin executed at t1 >= t0 + 2.1.
  - Then: `OnActionCancelled(Interact, Released)` at t0 + 2.1; new `HoldTimer(Complete)` DueTime = t1 + 3.0 (progress 0 at t1); `FHoldStatus.ServerStartTime` = t1; the original timer (due t0 + 3.0) is voided by `ActionEpoch` and no completion fires at t0 + 3.0.
  - Edge cases: a Begin within the 0.10 s pending window merges instead (AC 19), so the test Begin must follow the executed End; completion at t1 + 3.0 only.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/InteractEarlyReleaseResetsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-037-hold-completion-epsilon.md`
  - cross-epic: objectives-chain (`IInteractTargetOwner` supplying D; stories not yet written)
- Unlocks:
  - None
