# Story 034: AC 14 BraceClampsRunTraverse

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`, `TR-iif-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story verifies the "no run or parkour while prepared" rule (GDD R7, the paragraph after the table, and R8 Prepare): while a pawn is Bracing, a requested `Gait = Run` is clamped to Walk and a `Traverse` is rejected `NotNow`. The clamp never cancels the brace (an externally forced run or traversal reported by Locomotion does, but a requested Run does not), and the rule is identical on the AI path (null sink, so no reply).

It follows story 033 (the legality matrix and its locomotion-state inputs). It straddles two ADRs: the IIF decides the clamp of the requested gait and the Traverse rejection at its boundary (ADR-0011), while the speed effect of the clamped gait and the `IGaitLegalitySource` input live in the movement model (ADR-0010 section 6: "Run to Walk if `IGaitLegalitySource` reports `Informant` or `FActionState.Pose == Brace`"). This story asserts what the IIF forwards, using a Locomotion stub.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; one action slot per pawn; while Bracing, requested Run is clamped (never cancels) and Traverse is rejected.
**ADR Version**: 2026-10-24

**Other ADRs**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (one AStealthPawn on CMC; effective-gait clamps in the movement model; publishes `ISharedPawnStateView`).

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 HIGH for the input layer; ADR-0010 HIGH)
**Engine Notes**: No V1-V6 item gates this story. ADR-0010: movement is the CharacterMovementComponent (never Mover/ChaosMover); the gait travels in the saved move as `FLAG_Custom_0` (`WantsRun`); the planted-brace rule (Move ignored for translation and yaw while `Brace`) is Locomotion's and is covered by story 048. This story uses a Locomotion stub and does not touch the CMC.

**Control Manifest Rules (this layer)**:
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; only `UPawnCommandComponent::WriteActionState` writes `FActionState` — source: ADR-0011
- Required: Movement shaping is one function per rule for every pawn; the gait clamp's participant-state input comes only from `IGaitLegalitySource` — source: ADR-0010
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step — source: ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — source: ADR-0011
- Forbidden: Never read controller kind, PlayerState or team in a movement rule — source: ADR-0010
- Guardrail: Movement shaping well under 5 % of the 3.0 ms CMC game-thread line — source: ADR-0010

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 14 (R7, Logic, U `BraceClampsRunTraverse`): **GIVEN** Bracing **WHEN** Gait=Run then Traverse are issued **THEN** Run is clamped to Walk; **AND** Traverse is rejected `NotNow`.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.2 and 5, and ADR-0010 Decision section 6:*

ADR-0011 section 5, Prepare: "While `Bracing`, requested Run is clamped (never cancels) and `Traverse` is rejected; an externally forced run or traversal cancels `Moved`." The Traverse rejection is a stage 2 locomotion-or-slot rule giving `NotNow`; it is a locomotion restriction, not slot `Busy`. The clamp is applied to the gait forwarded to Locomotion: ADR-0010 section 6, effective-gait clamps: Run becomes Walk if `IGaitLegalitySource` reports Informant or `FActionState.Pose == Brace`. `Move`/`Look` are continuous and never queue items (ADR-0002 section 3), so the clamp is on the forwarded intent, not on a reply.

**Rationale.** "No run/parkour while prepared" (GDD R7) stops a braced pawn from stacking a defence with mobility; keeping it at the shared boundary means the same rule applies to AI pawns (same result on the AI path, null sink, no reply).

**Test seam (QL-STORY-READY, ADEQUATE).** Pawn with an active Prepare (predicate `Allow`, Active class, not running, not traversing). When `Move(Gait=Run)` then `Traverse` are issued: the Locomotion stub receives `Gait=Walk`; `Traverse` yields exactly one `NotNow` and no event; the brace is not cancelled (no `OnActionCancelled`). Edge cases: a clamped Run must not cancel the brace (R8); same result on the AI path (null sink, no reply); Traverse before Prepare is legal (control case).

**GDD references.** Core Rules R7 (paragraph after the table), R8 Prepare (Cancels), R9; AC 63 for the externally forced run (story 047). No tuning keys apply.

**Prototype superseded.** New code, no prototype counterpart (the prototype had a Ctrl jog toggle and no brace gating; the sample's gait handling is removed by D37, story 019).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-048-planted-brace-ignores-move.md`: the planted brace (Move ignored for translation and yaw; AC 78)
- `production/epics/input-interaction-framework/story-047-forced-run-cancels-brace.md`: an externally forced run cancelling the brace `Moved` (AC 63)
- `production/epics/input-interaction-framework/story-033-legality-matrix.md`: the full R7 table
- `production/epics/input-interaction-framework/story-035-slot-priority.md`: slot-pair priority

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (gait clamp while Bracing; Traverse rejection)
- `Source/StealthGameplay/Private/Tests/Input/BraceClampsRunTraverseTest.cpp` (class `FInputBraceClampsRunTraverseTest`, name `StealthGame.Input.BraceClampsRunTraverse`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Logic — automated test specs (QL-STORY-READY: ADEQUATE):**

- **AC-1**: AC 14 `BraceClampsRunTraverse` (`StealthGame.Input.BraceClampsRunTraverse`): while Bracing, Run is clamped to Walk and Traverse is rejected `NotNow`
  - Given: a pawn with an active Prepare (predicate Allow, Active class, not running, not traversing).
  - When: `Move(Gait=Run)` then `Traverse` are issued.
  - Then: the Locomotion stub receives `Gait=Walk`; `Traverse` yields exactly one `NotNow` and no event; the brace is not cancelled (no `OnActionCancelled`).
  - Edge cases: a clamped Run must not cancel the brace (R8); same result on the AI path (null sink: no reply); Traverse before Prepare is legal (control case).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/BraceClampsRunTraverseTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-033-legality-matrix.md`
  - cross-epic: shared-character-locomotion (gait clamp and `IGaitLegalitySource`; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-048-planted-brace-ignores-move.md`
