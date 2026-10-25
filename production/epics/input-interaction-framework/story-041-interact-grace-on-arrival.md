# Story 041: AC 74 InteractGraceOnArrival

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

This story pins down the arrival grace of the Interact hold: for `InteractMoveGraceSec` (0.25 s) after an Interact executes, commanded displacement is not accumulated, so a player who presses Tend just as the movement key is released (a Move still in progress when the press lands) does not cancel their own hold. The same rule applies to AI, because Move intent means the same thing for both (R17).

It follows story 040, which owns the displacement threshold and the cancel range; this story isolates the grace boundary. In ADR-0011 it is the "commanded displacement ... after `InteractMoveGraceSec`" clause of section 5, together with the resolution that the IIF reads `CommandedDisplacementCm(SinceServerTime = t_start + grace)`, so the accumulation start is a parameter of the read, not state the IIF keeps. GDD rules: R8 (Commanded displacement) and F6.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Commanded displacement (`ISharedPawnStateView::CommandedDisplacementCm`) beyond `InteractMoveCancelDistCm` after `InteractMoveGraceSec` cancels an Interact with `Moved`; the re-check runs in the read-only condition sweep, which schedules `HoldTimer(ConditionCancel)`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility; HIGH is the input layer, which is not touched here. No V1-V6 gate. `CommandedDisplacementCm` is an owner-local or server-local read of locomotion state and is never replicated (ADR-0010). Time arithmetic uses `double` server time (`ActionStartServerTime`).

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items — ADR-0011
- Forbidden: Never let the condition sweep cancel a hold itself (it schedules `HoldTimer(ConditionCancel)` items) — ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp — ADR-0011
- Forbidden: Never replicate `IsRunning`, `IsInTraversal` or `CommandedDisplacementCm` — ADR-0010
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case, inside the 0.5 ms queue line — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 74 (F6, Logic, U `InteractGraceOnArrival`): **GIVEN** a pawn decelerating from walk speed when Interact executes **WHEN** it travels 12 cm within the first 0.25 s and then stops **THEN** the hold continues; **AND GIVEN** 20 cm of commanded travel after 0.25 s **THEN** it cancels `Moved`.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 5 and GDD R8 / F6:*

- **Grace definition.** Commanded displacement is accumulated only on ticks where the pawn's Move intent is non-zero and not during the first `InteractMoveGraceSec` after execution. The IIF reads `ISharedPawnStateView::CommandedDisplacementCm(SinceServerTime = t_start + InteractMoveGraceSec)`; the cancel fires when that value exceeds `InteractMoveCancelDistCm`. External pushes (zero intent) never count.
- **Known weakness of the GDD criterion (QL-STORY-READY GAPS).** The AC's 12 cm is below `InteractMoveCancelDistCm` (15), so an implementation with no grace period would also pass the first clause. The gap analysis proposes a stricter scenario in the QA section (20 cm inside the grace continues; 14 cm after the grace continues; 16 cm after cancels). The GDD AC text stays the authority; implement the grace as specified and use the stricter scenario as the discriminating check where the QA-lead rewrite is confirmed.
- **Numbers to remember.** A pawn already at 140 cm/s covers 15 cm in about 0.11 s; one from rest needs about 0.29 s (SCL walk start ramp), so the `Moved` cancel lands about 0.04 s after the grace opens (F6 Output Range). The cancel is evaluated only after the grace.
- **Test seam.** A displacement source stub (Locomotion adapter) keyed by `SinceServerTime`, driven by the injected clock; the stub must model `Move intent carried over` at the time Interact executes. Declared against the story 002 interface; named doubles in story 095.
- **Tuning keys (defaults).** `InteractMoveGraceSec` 0.25 (hard 0-0.5, latch Action: captured when the action executes, so a mid-action edit never retimes it), `InteractMoveCancelDistCm` 15 (5-40, latch Use).
- **Prototype.** New code, no prototype counterpart (the prototype `OnInteractPressed/Released` has no displacement rule).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 040 (`story-040-interact-hysteresis.md`): reach, facing, cancel range, and the `Moved` threshold after the grace.
- Story 039 (`story-039-interact-cancel-matrix.md`): the cancel-reason matrix and sweep plumbing.
- Story 059 (`story-059-crowd-brush-does-not-cancel-tend.md`): external pushes and NPC contact replay.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandConditionSweep.cpp (grace start passed to the displacement read)
- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (records `ActionStartServerTime`)
- Source/StealthGameplay/Private/Tests/Input/InteractGraceOnArrivalTest.cpp (`FInputInteractGraceOnArrivalTest`, `StealthGame.Input.InteractGraceOnArrival`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- The criterion does not isolate the grace period: 12 cm is below `InteractMoveCancelDistCm` (15), so an implementation with no grace also passes. (ADR-0011 resolves the accumulation definition: the IIF reads `CommandedDisplacementCm(SinceServerTime = t_start + grace)`.)
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "GIVEN a pawn whose Move intent is still non-zero when Interact executes WHEN it travels 20 cm (above the 15 cm threshold) inside the first 0.25 s THEN the hold continues; AND GIVEN 20 cm of commanded travel after 0.25 s THEN it cancels Moved; AND 14 cm after the grace does not cancel."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/InteractGraceOnArrivalTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-040-interact-hysteresis.md`; cross-epic: shared-character-locomotion (stories not yet written) for `ISharedPawnStateView::CommandedDisplacementCm`; cross-epic: tuning-data-config (stories not yet written) for the IIF knobs
- Unlocks: None
