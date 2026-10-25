# Story 008: Completion gate and stage effects

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-10 the completion gate, OC-4, OC-11, OC-17; AC-OC-06 stage rows)
**Requirement**: `TR-oc-005` (completion gate inside the hold-timer slot), `TR-oc-003` (a counted stage pushes the team view)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 3 and 5.4)
**Other ADRs**: ADR-0002: Server resolution queue, server clock and round-end staging (inline follow-ons inherit the slot key)
**ADR Decision Summary**: Completion is judged only in `OnInteractEnded(..., Completed, Key)`, inline in the `HoldTimer(Complete)` slot, against live state. It counts only if (a) phase `Active` and the latch not set; (b) the pawn is bound to a participant with `GetState == Active`; (c) the Session connection state is `Connected` (not `Unresponsive` or `Disconnected`) and the pawn's control epoch equals the one recorded with the claim and no hand-over is pending; (d) the court is a `Live` site of a definition eligible for the participant's team; (e) that definition is not `Completed`. Otherwise nothing changes beyond the IIF's normal `OnActionCompleted`. A counted stage marks `Completed`, applies the OC-4 unlock, logs `ObjectiveStage`, pushes the team view and schedules the rival release.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0020 overall; ADR-0002 MEDIUM for drain and slot behaviour; the gate itself is plain C++ over injected interfaces)
**Engine Notes**: No engine API in the gate. Gate (c) reads Session's own state table (`Session.GetConnectionState(P)`), not the Roster's collapsed value that also covers `Unresponsive` (cross-review S-I4, 2026-10-24), and the two ADR-0010 queries `GetControlEpoch` and `IsHandOverPending`, injected through `IPawnControlView`.

**Control Manifest Rules (this layer)**:
- Required: Completion is judged only in `OnInteractEnded(Completed)` inside the `HoldTimer(Complete)` slot, against live state, with gate (a)-(e) — source: ADR-0020
- Required: Gate (c) compares the control epoch recorded at Begin to the current one and requires `!IsHandOverPending` — source: ADR-0020
- Required: Objectives Event Log entries go through the Round Event Log `Append` only — source: ADR-0020
- Forbidden: Never judge completion from an `OnActionCompleted` subscription; never let a completion cancel another pawn's hold — source: ADR-0020
- Forbidden: Never branch on player vs NPC in Objectives except the gate's server-only queries and the bound-only `TendCompleted` entry — source: ADR-0020
- Guardrail: completion O(team size) view writes; about 5 counted completions per round — source: ADR-0020

This is the gate of OC-10 for the two preliminary stages (the final's win declaration is story 009, sharing this gate). A tend that fails the gate is exactly the IIF's `AllowNoEffect` outcome: the same pose, timer, events and replies, with no state change and no extra replication to anyone. The six rows of the GDD table (a human on an eligible team; an AI-driven hand-over; an Unresponsive tender; a Disconnected one; an Informant; an Active pawn of a team for which the site is not Live) are the contract, and the control-epoch rule closes a hole: `HoldTimer` items carry only the `ActionEpoch`, so without gate (c) a hold begun under one controller could complete under another after a takeover, `PawnRecovery` or a Player-to-AI hand-over committed mid-hold.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-06 stage rows, OC-10) and ADR-0020 sections 3 and 5.4, scoped to this story. QL-STORY-READY revised AC4 (Shipping evidence) and asked for the TendCompleted reading in AC3, resolved in Implementation Notes:*

- [ ] **AC1** (AC-OC-06 [U, table-driven] `CompletionGate`, stage rows): **GIVEN** a stage `HoldTimer(Complete)` slot and, one row each, a completer that is (1) Active+Connected+human on an eligible team, (2) AI-driven (hand-over), (3) Unresponsive, (4) Disconnected, (5) an Informant at the slot, (6) Active on a team for which the site is not `Live`, **THEN** exactly one `ObjectiveStage` log entry for row 1 and 0 for rows 2-6, and site state and team views are unchanged for rows 2-6.
- [ ] **AC2** (ADR-0020 5.4 (c)-(e) [U] `CompletionGateControlEpoch`): **GIVEN** a pawn whose control epoch changed since Begin, or with a hand-over pending, **THEN** the completion has no effect; **GIVEN** a second completion at an already `Completed` site **THEN** no effect and no log entry; a rejected completion emits nothing beyond the IIF's normal `OnActionCompleted`.
- [ ] **AC3** (OC-10 stage effect [U] `StageCompletionEffects`): a counted stage completion marks the site `Completed`, unlocks the next stage for that team only in the same slot, logs `ObjectiveStage(team, stage, ObjectiveId, ParticipantId, Key)`, pushes the team view and schedules the rival release (a stub observes the call); every completion by a pawn bound to a participant (counted or not) appends server-only `TendCompleted(ParticipantId, Court, Counted|NoEffect)`; an unbound NPC decoy completion appends nothing.
- [ ] **AC4** (ADR-0020 section 3 [U] `DebugCompleteStage`): (a) non-Shipping `DebugCompleteStage(Team)` enqueues its own Round `DebugItem` slot and calls the same `ApplyCountedCompletion` as AC3; (b) a source scan asserts every definition and declaration of `DebugCompleteStage` sits inside `#if !UE_BUILD_SHIPPING`; (c) a Shipping build/link of the module succeeds with no unresolved reference, with the build log retained in `production/qa/evidence/oc-shipping-build.md`.

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 3 and 5.4:*

The gate is `FObjectivesCore::OnInteractEnded(Pawn, Court, End, Key)` with `End == Completed` and a valid key. Read live at the slot: (a) `IRoundObjectiveHooks::IsActive()` and `!UResolutionQueue::IsLatched()` (the queue already voids items after the latch, so (a) is a defensive re-check); (b) `IObjectivesRosterView::GetParticipant(Pawn)` (server-only binding lookup) yields P with `GetState(P) == Active`; (c) `ISessionConnectionView::GetConnectionState(P) == Connected` and `IPawnControlView::GetControlEpoch(Pawn)` equals the epoch recorded with the claim (story 006) and `!IsHandOverPending(Pawn)`; (d) the court is bound to a row eligible for `GetTeam(P)` whose state is `Live`; (e) the row is not `Completed`. Any failure: nothing changes. The completer's pose ends through the IIF's normal completion blend, identical for counted and no-effect completions.

If it counts (stage): `ApplyCountedCompletion` (story 004) marks `Completed` and applies the OC-4 unlock; append `ObjectiveStage(team, stage, ObjectiveId, ParticipantId, Key)` through the Round Event Log `Append` (never `UE_LOG`); `PushTeamView(team)` (story 010 builds it; the writer is story 011); then schedule the rival release for that completion (story 015 owns the schedule, story 014 the draw). Opponent clients receive nothing from a counted stage.

**TendCompleted reading (QL-STORY-READY question).** ADR-0020 5.4 says the server-only `TendCompleted(ParticipantId, Court, Counted|NoEffect)` entry is appended "for every completion by a bound pawn (counted or not)". A bound pawn here means the Roster's server-only binding lookup returns a participant for the pawn at the slot; so rows 2-6 of AC1 (the AI-driven hand-over pawn, an Unresponsive or Disconnected tender, an Informant) are bound-pawn completions and each appends `TendCompleted(..., NoEffect)` while appending no `ObjectiveStage`; only a crowd NPC with no binding appends nothing. This is the named bound-versus-unbound branch the Guidelines allow because its sole effect is a server-only log entry. If the Roster contract returns no participant for a hand-over pawn, confirm and record the ruling in this story at implementation; the test asserts the two entries separately. The `TendCompleted` entries feed F3's measurements (story 017).

`DebugCompleteStage(ETeam Team)` is non-Shipping only (`#if !UE_BUILD_SHIPPING`) and runs in a Round `DebugItem` slot (Round R13, ADR-0002), calling the same `ApplyCountedCompletion`, so fast automated tests never need a short hold (OC-14: tests use `DebugObjectiveCompletion`, not a short `ObjectiveInteractSec`).

Prototype replaced: `AStealthGameMode::OnObjectiveCompleted` and `FStealthRules::CanCompleteObjective` (prototype test D, full chain 1 to 2 to final to objective win, is the behavioural reference); the prototype let the client-set interact flag complete a hold and did not re-validate connection state or control epoch.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-009-final-completion-and-win-declaration.md`: the final branch, `DeclareObjectiveWin`, no-stack/no-cancel and same-slot ordering.
- `story-010-team-view-struct-and-builder.md` and `story-011-team-view-component-and-replication.md`: building and writing the view.
- `story-014-rival-signal-draw-and-grid.md` and `story-015-rival-signal-release-scheduling.md`: the draw and the release timer.
- IIF `story-055-objectives-completion.md`: the IIF-side slot/key behaviour this gate relies on.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesCore.h` and `ObjectivesCore.cpp` (`OnInteractEnded`, gate (a)-(e), `ApplyCountedCompletion` wiring)
- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesService.cpp` (`DebugCompleteStage`, guarded)
- `Source/StealthGameplay/Private/Tests/Objectives/CompletionGateTest.cpp` (class `FObjectivesCompletionGateTest`, name `StealthGame.Objectives.CompletionGate`)
- `Source/StealthGameplay/Private/Tests/Objectives/CompletionGateControlEpochTest.cpp` (`StealthGame.Objectives.CompletionGateControlEpoch`)
- `Source/StealthGameplay/Private/Tests/Objectives/StageCompletionEffectsTest.cpp` (`StealthGame.Objectives.StageCompletionEffects`)
- `Source/StealthGameplay/Private/Tests/Objectives/DebugCompleteStageTest.cpp` (`StealthGame.Objectives.DebugCompleteStage`)
- `production/qa/evidence/oc-shipping-build.md`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS; the AC4 rewrite and the TendCompleted reading were applied above and the specs cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`CompletionGate`): six-row stage gate table
  - Given: stub Round queue; fake clock; team 0 Stage1 Live; rows (1) Active+Connected+human eligible; (2) AI-driven hand-over; (3) Unresponsive; (4) Disconnected; (5) Informant; (6) Active on a team whose site is not Live
  - When: a stage `HoldTimer(Complete)` slot executes once per row (fresh fixture each row)
  - Then: row 1 exactly one `ObjectiveStage` log entry; rows 2-6 zero; for rows 2-6 site state and team views identical to before
  - Edge cases: table-driven, one fixture rebuilt per row; row order must not matter; row 1 repeated in the same slot still logs once.
- **AC-2** (`CompletionGateControlEpoch`): stale and duplicate completions
  - Given: pawn with control epoch E at Begin; hand-over pending case; already Completed site
  - When: epoch changes to E+1 before Complete; Complete with hand-over pending; second Complete at a Completed site
  - Then: no effect in each case; second completion produces no log entry; a rejected completion emits nothing beyond the IIF `OnActionCompleted`
  - Edge cases: epoch change in the same tick as Complete; two completions at one site in the same drain.
- **AC-3** (`StageCompletionEffects`): counted completion effects
  - Given: bound human pawn at a Live P1; stub queue, stub team view sink, stub rival-release observer; unbound NPC decoy
  - When: the pawn completes; a bound pawn completes a NoEffect tend; the decoy completes
  - Then: site Completed; next stage Live for that team only in the same slot; `ObjectiveStage(team, stage, ObjectiveId, ParticipantId, Key)` logged; team view pushed; rival release scheduled; every bound-pawn completion appends `TendCompleted(ParticipantId, Court, Counted|NoEffect)`; decoy completion appends nothing
  - Edge cases: opponent team state untouched; log entry order within the slot is stable.
- **AC-4** (`DebugCompleteStage`): debug path shares the effect code
  - Given: non-Shipping build; stub Round queue
  - When: `DebugCompleteStage(0)` is called
  - Then: it enqueues its own Round `DebugItem` slot and calls the same `ApplyCountedCompletion`; effects equal AC-3 row 1; source scan shows the symbol is guarded out of Shipping; the Shipping build log is retained
  - Edge cases: calling at a Locked or Completed stage has no effect; Shipping build evidence file retained.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the four test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)
- AC4(c): `production/qa/evidence/oc-shipping-build.md` (Shipping build/link log)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-004-core-site-state-and-progress.md`, `production/epics/objectives-chain/story-006-fixture-claims-and-interact-target-owner.md`, `production/epics/objectives-chain/story-007-interact-predicate-and-feint-parity.md`; cross-epic: `production/epics/input-interaction-framework/story-055-objectives-completion.md`, `production/epics/input-interaction-framework/story-050-hold-timer-key-order.md`; round-match-framework (queue, latch, Event Log, `DebugItem`; stories being written in parallel); session-travel (connection state: `story-015-connection-state-machine-illegal-transitions.md`); shared-character-locomotion/pawn factory (`GetControlEpoch`, `IsHandOverPending`)
- Unlocks: `production/epics/objectives-chain/story-009-final-completion-and-win-declaration.md`, `production/epics/objectives-chain/story-012-rejoin-rebind-and-reset.md`, `production/epics/objectives-chain/story-015-rival-signal-release-scheduling.md`
