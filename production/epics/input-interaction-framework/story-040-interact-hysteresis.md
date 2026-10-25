# Story 040: AC 37 InteractHysteresis

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

This story implements formula F6, the Interact reach and cancel hysteresis: a Begin is accepted only within `InteractRangeCm + CommandRangeSlackCm` (190 cm by default) and within `InteractFacingHalfAngleDeg` (60 degrees) of the site; once accepted, only distance is re-checked, against a larger cancel range (240 cm by default), so crowd capsule pushes and Look turns never end a hold; commanded displacement beyond `InteractMoveCancelDistCm` after the grace cancels `Moved`. The cancel range is derived (accept range plus `InteractCancelHysteresisCm`), so it can never fall inside the accept range.

It follows story 039, which provides the condition sweep and the cancel plumbing; this story supplies the numeric thresholds and the Begin-time reach check that replies `OutOfReach`. In ADR-0011 it is section 5 (Interact: Begin check, cancel range, commanded displacement via `ISharedPawnStateView::CommandedDisplacementCm`). GDD rules: R8 (Interact, After Begin, Commanded displacement) and F6. The grace behaviour on arrival is isolated in story 041 and the NPC-contact replay in story 059.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Interact accepts a Begin when horizontal distance is within `InteractRangeCm + CommandRangeSlackCm` and control-rotation yaw is within `InteractFacingHalfAngleDeg` (else `OutOfReach`); after Begin only the cancel range `InteractRangeCm + CommandRangeSlackCm + InteractCancelHysteresisCm` is re-checked, so Look never cancels.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility (pre-cutoff mechanisms); the HIGH rating is the input layer, which this story does not use. No V1-V6 item gates it. The reach check reads the owner's control-rotation yaw as carried by `ServerMove` (E8) and the pawn position the server holds; `CommandRangeSlackCm` exists precisely for the skew between `ServerMove` and the reliable RPC ordering. Tests keep geometry axis-aligned so 60.0 degrees and 190.0 cm are exact in float.

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — ADR-0011
- Required: Hold ends are `ScheduleAt` queue items — ADR-0011
- Forbidden: Never let the condition sweep cancel a hold itself (it schedules `HoldTimer(ConditionCancel)` items) — ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — ADR-0011
- Forbidden: Never replicate `IsRunning`, `IsInTraversal` or `CommandedDisplacementCm` — ADR-0010
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case, inside the 0.5 ms queue line — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 37 (F6, Logic, U `InteractHysteresis`): **GIVEN** a site with default keys **WHEN** Begins are issued at 190.0 cm and at 190.1 cm **THEN** the first is accepted and the second rejected `OutOfReach`; **AND WHEN** issued at 60° and at 61° off facing **THEN** the first is accepted and the second rejected `OutOfReach`; **AND GIVEN** an accepted hold at 185 cm **WHEN** pushed externally to 200 cm, then turned 180° by Look **THEN** it continues; **AND WHEN** held at 240.0 cm **THEN** it continues; **AND WHEN** at 240.1 cm **THEN** it cancels `OutOfRange`; **AND WHEN** 20 cm of commanded displacement occurs after the grace **THEN** it cancels `Moved`.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 5 and the Implementation Guidelines:*

- **Begin check (section 5, GDD F6).** Accept `Begin` iff `d_xy <= InteractRangeCm + CommandRangeSlackCm` and `|yaw_ctrl - yaw_to_point| <= InteractFacingHalfAngleDeg`; a failure of either replies `OutOfReach` (the stage-2 interact distance and facing steps, reply code mapping in story 029). `d_xy` is the horizontal distance from capsule centre to the site's interact point.
- **After Begin.** Only the cancel range `InteractRangeCm + CommandRangeSlackCm + InteractCancelHysteresisCm` (240 cm default) is re-checked, in the condition sweep, which only reads state and schedules `HoldTimer(ConditionCancel)` (story 039 supplies this). Look never cancels. An external push on zero-intent ticks never counts toward displacement; it cancels only through the range check.
- **Commanded displacement** comes from `ISharedPawnStateView::CommandedDisplacementCm(SinceServerTime)`; the IIF reads it with `SinceServerTime = t_start + InteractMoveGraceSec` (ADR-0011 resolves the accumulation definition). It accumulates only on ticks where Move intent is non-zero. Do not recompute displacement inside the IIF.
- **Rejected Begin consumes no token.** The distance and facing failures happen at the interact checks before the per-pawn bucket (step 5), and a rejected Begin gets exactly one `OutOfReach` reply and consumes no token.
- **Derive, never hard-code.** The qa-lead requires deriving 190 and 240 from the tuning keys rather than literals. Keys and defaults: `InteractRangeCm` 150 (hard 80-250), `CommandRangeSlackCm` 40 (20-80), `InteractCancelHysteresisCm` 50 (30-150), `InteractFacingHalfAngleDeg` 60 (30-90), `InteractMoveCancelDistCm` 15 (5-40), `InteractMoveGraceSec` 0.25 (0-0.5).
- **Test seams.** One fresh pawn per sub-case; site on the +X axis so the yaw error is exact; a `CommandedDisplacementCm` stub returning 20 for the `Moved` sub-case. Named doubles come from story 095; until then use test-local stubs.
- **Prototype.** `OnInteractPressed/Released` and `ServerSetInteracting` in `StealthCharacter.cpp` have no reach check or hysteresis; replaced, not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 039 (`story-039-interact-cancel-matrix.md`): the full cancel-reason matrix and the sweep plumbing.
- Story 041 (`story-041-interact-grace-on-arrival.md`): isolating the grace period on arrival.
- Story 059 (`story-059-crowd-brush-does-not-cancel-tend.md`): replay of 20 NPC paths passing within touching distance.
- Story 029 (`story-029-rejection-code-set.md`): the reply code set and end-of-drain reply timing.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (reach check, cancel-range test)
- Source/StealthGameplay/Private/Core/PawnCommandConditionSweep.cpp (cancel-range and displacement conditions)
- Source/StealthGameplay/Private/Tests/Input/InteractHysteresisTest.cpp (`FInputInteractHysteresisTest`, `StealthGame.Input.InteractHysteresis`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Logic story — automated test spec (`StealthGame.Input.InteractHysteresis`):**

- **AC-1**: Interact reach and cancel hysteresis
  - Given: defaults (150 + 40 = 190 accept, 240 cancel, 60 deg, 15 cm); site on the +X axis so yaw error is exact.
  - When: Begin at 190.0 cm and at 190.1 cm; Begin at 60.0 deg and at 61.0 deg.
  - Then: 190.0 cm accepted, 190.1 rejected `OutOfReach`; 60.0 deg accepted, 61.0 rejected `OutOfReach`.
  - When: hold accepted at 185 cm, pushed to 200 cm (zero Move intent), then Look turned 180 deg; held at 240.0 cm; then 240.1 cm; after the grace the `CommandedDisplacementCm` stub returns 20.
  - Then: continues after the push and the Look turn; continues at 240.0 cm; cancels `OutOfRange` at 240.1 cm; cancels `Moved` on the 20 cm displacement.
  - Edge cases: one fresh pawn per sub-case; derive 190/240 from tuning keys (not literals); 60.0 deg and 190.0 cm are exact in float, keep geometry axis-aligned; a rejected Begin gets exactly one `OutOfReach` reply and consumes no token.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/InteractHysteresisTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`; cross-epic: shared-character-locomotion (stories not yet written) for `ISharedPawnStateView::CommandedDisplacementCm`; cross-epic: tuning-data-config (stories not yet written) for the IIF knobs
- Unlocks: `production/epics/input-interaction-framework/story-041-interact-grace-on-arrival.md`, `production/epics/input-interaction-framework/story-059-crowd-brush-does-not-cancel-tend.md`
