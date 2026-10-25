# Story 033: AC 13 LegalityMatrix

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story verifies the R7 legality table: which commands are legal for each pawn class (Active, Informant, RoundEnded) and locomotion state (running, in traversal), checked at stage 2 steps 1 and 2 (legality class from Roster's `GetLegalityClass`, locomotion from `ISharedPawnStateView`). The table is data-driven in the test (5 columns by 8 rows) and each cell is asserted: a cross is one `NotNow` reply and no event, a tick means the command executes subject to slot and predicate, and Run is clamped to Walk for an Informant.

It follows story 027 (the stage 2 resolver) and is the base of story 034 (brace clamps), 035 (slot priority) and 052 (no brace or tend in a bush). Unbound pawns resolve to Active, so the component never learns the binding; this story is where the class column is exercised, while the unbound-resolves-to-Active mapping itself is Roster's and is covered in story 057 (AC 44).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; stage 2 checks the Roster legality class and the locomotion state published through `ISharedPawnStateView` before slot arbitration and the feature predicate.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story. `ISharedPawnStateView` exposes only `IsRunning`, `IsInTraversal` and `CommandedDisplacementCm(SinceServerTime)` (ADR-0010 section 8, ADR-0011 Key Interfaces); position, body yaw, concealed flag, slot occupant and cooldown are not in it, so stories asserting them need a named stub. The implementing adapter is cross-epic (shared-character-locomotion).

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step — source: ADR-0011
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; every discrete command from any source enters through its entry points and the queue — source: ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener; the command component never calls a Roster binding query — source: ADR-0011
- Forbidden: Never replicate `IsRunning`, `IsInTraversal` or `CommandedDisplacementCm` (owner-local copies for prompts only) — source: ADR-0010
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 13 (R7, Logic, U `LegalityMatrix`): **GIVEN** each class × locomotion state of the R7 table **WHEN** each command is issued **THEN** the outcome matches the table cell, including Run clamped to Walk for an Informant.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 3.2 steps 1-2 and GDD R7:*

Stage 2 step 1 is the legality class `Roster.GetLegalityClass(Pawn)` in {Active, Informant, RoundEnded}; step 2 is locomotion from `ISharedPawnStateView` (`IsRunning`, `IsInTraversal`; server state, not key state). The GDD R7 table (a tick is legal subject to slot and feature predicate; a cross is `NotNow`): Move (Walk) legal except RoundEnded; Move (Run) clamped to Walk for Informant and rejected for RoundEnded; Look legal for every class; Traverse legal for Active (including running), rejected for in-traversal, Informant and RoundEnded; Interact Begin, Prepare Begin and Attack legal only for Active and not while running or in traversal; Hide Enter rejected while running (Exit legal while running), both rejected in traversal, for Informant and RoundEnded. Camera toggle is local, never a command (R14), and is excluded.

**Rationale.** "Informant and brace legality must gate running and parkour at the same boundary for every pawn" (GDD R1 note): the table is enforced once, here, for every source, so informant restrictions apply to any pawn in that class whatever controls it.

**Gaps and rewrite (QL-STORY-READY).** Two R7 cells have no assertable outcome: "Move (Walk) while in traversal = Locomotion-owned" and "Move while RoundEnded = x (Round may freeze)"; and Move/Look are continuous (no queue slot, no reply per R5/R6), so "rejected `NotNow`" has no observable for those rows. The qa-lead proposes asserting discrete rows (Traverse, Interact, Prepare, Attack, Hide) by the R7 cell (cross: one `NotNow` reply and no event; tick: executes), and the Move/Look rows on the Gait/intent forwarded to the Locomotion stub: Informant Run forwarded as Walk; RoundEnded Move forwarded as zero; Walk Move during traversal forwarded unchanged. The GDD AC text stays the authority.

**Test seam.** A Roster stub returning Active / Informant / RoundEnded; an `ISharedPawnStateView` stub (IsRunning / IsInTraversal per column); predicates `Allow`, slot empty, buckets full so that "ok" cells are not interfered with.

**GDD references.** Core Rules R7 (table and the paragraph after it), R1; States and Transitions; Edge Cases "Input-layer". No tuning keys apply.

**Prototype superseded.** New code, no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-034-brace-clamps-run-traverse.md`: while Bracing, Run clamped and Traverse rejected (AC 14)
- `production/epics/input-interaction-framework/story-035-slot-priority.md`: the action-slot pair rules at step 3 (AC 20)
- `production/epics/input-interaction-framework/story-052-no-brace-or-tend-in-bush.md`: Prepare and Interact rejected for a concealed pawn (AC 39)
- `production/epics/input-interaction-framework/story-057-roster-legality-class.md`: unbound resolves to Active; mid-hold class change cancels `Illegal` (AC 44)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h` (legality table data at stage 2 steps 1-2)
- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp`
- `Source/StealthGameplay/Private/Tests/Input/LegalityMatrixTest.cpp` (class `FInputLegalityMatrixTest`, name `StealthGame.Input.LegalityMatrix`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Two R7 cells have no assertable outcome ("Move (Walk) while in traversal = Locomotion-owned"; "Move while RoundEnded = x (Round may freeze)").
- Move/Look are continuous (no queue slot, no reply per R5/R6), so "rejected NotNow" has no observable for those rows.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Discrete rows (Traverse, Interact, Prepare, Attack, Hide) match the R7 cell: x = one NotNow reply, no event; ok = executes. Move/Look rows are asserted on the Gait/intent forwarded to the Locomotion stub: Informant Run forwarded as Walk; RoundEnded Move forwarded as zero; Walk Move during traversal forwarded unchanged (Locomotion-owned, no IIF reply)."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/LegalityMatrixTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-027-stage-two-at-resolution.md`
  - cross-epic: participant-state-roster (`GetLegalityClass`; stories not yet written)
  - cross-epic: shared-character-locomotion (`IsRunning`, `IsInTraversal` adapter; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-034-brace-clamps-run-traverse.md`
  - `production/epics/input-interaction-framework/story-035-slot-priority.md`
  - `production/epics/input-interaction-framework/story-052-no-brace-or-tend-in-bush.md`
