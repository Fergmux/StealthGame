# Story 004: Core site state and per-team progress

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-4 progress is per team and one-way, OC-11, States and Transitions, SP-3, SP-4; AC-OC-02, AC-OC-17)
**Requirement**: `TR-oc-002` (per-team state over the bound courts), `TR-oc-007` (stall-pressure rules SP-3 and SP-4: waiting earns nothing, camping the final cannot win)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision section 3)
**ADR Decision Summary**: The round-scoped store holds per (team, definition) site state `Locked | Live | Completed` and per team `StageIndex ∈ {Stage1, Stage2, Final}` and `CompletedDefMask`. Progress changes only inside the completion follow-on (`ApplyCountedCompletion`), in one slot: mark `Completed`, make the next stage's rows `Live` for that team only. No other code path writes site state. At `Active` entry both teams are `Stage1` with their `Prelim1` sites `Live`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (plain C++ state machine, no engine API)
**Engine Notes**: None. The core takes time only from an injected clock stub; nothing here reads engine time, so tests advance a fake clock.

**Control Manifest Rules (this layer)**:
- Required: Completion is judged only in `OnInteractEnded(Completed)` inside the `HoldTimer(Complete)` slot, against live state — source: ADR-0020 (this story supplies the state it judges against)
- Forbidden: Never give Objectives a tick function, `FTimerManager` timer or latent action — source: ADR-0020
- Forbidden: Never put a time-triggered unlock, dwell timer or reveal in Objectives (SP-3, SP-7; Pillar 4) — source: ADR-0020, GDD
- Guardrail: Store under 2 KB; all transitions O(1) — source: ADR-0020

This is the one-way state machine of OC-4. A court's state is never stored on the court or the fixture; it lives only in the server-side store keyed by (team, definition). The transitions have exactly three triggers: `Active` entry (Prelim1 goes Live), a team's own counted completion of every definition of a stage (the next stage goes Live for that team only), and `OnRosterReset` (cleared, story 012). There is no time trigger, no host command (outside the non-Shipping debug completion, story 008) and no opponent action, which is what makes waiting and camping earn nothing (SP-3, SP-4).

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-02, AC-OC-17, OC-4, OC-17, States and Transitions), scoped to this story:*

- [ ] **AC1** (AC-OC-02 [U] `ProgressOnlyByCompletion`): **GIVEN** team 0 at `Stage1` **WHEN** its `Prelim1` completes **THEN** `Prelim2(0)` is `Live` in the same slot and team 1's states are unchanged; **WHEN** the fake clock advances 60 min with no completion **THEN** no state changes and no event is emitted.
- [ ] **AC2** (AC-OC-17 [U] `FinalNeedsOwnUnlock`): **GIVEN** team 1 before its unlock **WHEN** a team 1 pawn tends the true final to `D` **THEN** no effect and no state change (a team 0 unlock never unlocks team 1; team 1 with only `Prelim1` done is still locked).
- [ ] **AC3** (OC-4, OC-17 [U] `StageLifecycle`): at `Active` entry both teams are `Stage1` with `Prelim1` `Live` and all else `Locked`; completing every `Prelim2` makes `Final` `Live` for that team only; progress never regresses when a completer later dies, disconnects or is removed (roster events from a stub); after `Ended` all states are frozen and a completion arriving afterwards is ignored.

---

## Implementation Notes

*Derived from ADR-0020 Decision section 3 and the GDD States and Transitions tables:*

Site state per (team, definition) is `Locked -> Live -> Completed` with no backward transition: `Locked -> Live` on `Active` entry (for `Prelim1`), on the team's own completion of every `Prelim1` definition (`Prelim2`) and on completion of every `Prelim2` (`Final`, for that team only); `Live -> Completed` on a counted completion. Team `StageIndex` goes `Stage1 -> Stage2 -> Final`; the round then ends by `ObjectiveWin` or by the other team's win. `CompletedDefMask` records the team's own completed prelim rows (never the final, story 010).

`ApplyCountedCompletion(Team, DefIndex, ...)` is the only writer of site state (the debug completion in story 008 calls it too). It is a pure function of the store, so the gate (story 008) and the debug path can share it. Keep the pawn, participant and Round dependencies out of it: those live in the gate. Nothing in this story schedules a timer or reads a clock except through the injected interface; AC1's 60-minute advance exists to prove that nothing keyed on elapsed time can change a site.

SP-4 deserves one sentence in the code: a team that has not unlocked the final cannot complete it because gate (d) (story 008) sees the court as not `Live` for that team; this story makes that true by never setting the final `Live` for the team early. The final is `Live` for team T only, so when team 0 unlocks first, team 1's final stays `Locked`.

Prototype replaced: `FStealthRules::CanCompleteObjective` and the stage counter in `AStealthGameState` (`GetTeamStage`); the prototype test `StealthGame.Rules.ObjectiveStages` is the reference for the stage-gating cases and is re-expressed by `FinalNeedsOwnUnlock` and `StageLifecycle` (story 022 maps it).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-008-completion-gate-and-stage-effects.md`: the gate (a)-(e), log entries, view push and release scheduling around a completion.
- `story-009-final-completion-and-win-declaration.md`: the final's win declaration.
- `story-012-rejoin-rebind-and-reset.md`: `OnRosterReset` clearing.
- `story-015-rival-signal-release-scheduling.md`: the no-timer-while-idle clause of AC-OC-16.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesCore.h` (site state, `StageIndex`, `ApplyCountedCompletion`, `HandleActiveEntry`)
- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesCore.cpp`
- `Source/StealthGameplay/Private/Tests/Objectives/ProgressOnlyByCompletionTest.cpp` (class `FObjectivesProgressOnlyByCompletionTest`, name `StealthGame.Objectives.ProgressOnlyByCompletion`)
- `Source/StealthGameplay/Private/Tests/Objectives/FinalNeedsOwnUnlockTest.cpp` (`StealthGame.Objectives.FinalNeedsOwnUnlock`)
- `Source/StealthGameplay/Private/Tests/Objectives/StageLifecycleTest.cpp` (`StealthGame.Objectives.StageLifecycle`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`ProgressOnlyByCompletion`): completion drives progress, waiting does not
  - Given: `FObjectivesCore`, fake clock, team 0 and team 1 both Stage1 (Prelim1 Live)
  - When: team 0 Prelim1 completes; separately the fake clock advances 60 min with no completion
  - Then: `Prelim2(0)` is Live in the same slot; team 1 states unchanged; after 60 min no state changes and no event is emitted
  - Edge cases: completion at clock time 0; advancing in many small steps equals one big step.
- **AC-2** (`FinalNeedsOwnUnlock`): final needs the team's own unlock
  - Given: team 1 before unlock, team 0 final Live, stub completion input
  - When: a team 1 pawn completes a tend at the true final to D
  - Then: no effect, no state change, no log entry from the core
  - Edge cases: team 0 unlocked first does not unlock team 1; team 1 with only Prelim1 done is still locked.
- **AC-3** (`StageLifecycle`): lifecycle invariants
  - Given: Active entry; stub roster events (death, disconnect, remove)
  - When: Active entry; every Prelim2 completes; the completer later dies, disconnects, is removed; then round Ended
  - Then: at entry both teams Stage1, Prelim1 Live, all else Locked; completing both Prelim2 makes Final Live for that team only; progress never regresses on roster events; after Ended all states are frozen
  - Edge cases: a completion arriving after Ended is ignored; both teams Final Live at once.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-003-core-chain-validation-and-court-binding.md`
- Unlocks: `production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md`, `production/epics/objectives-chain/story-010-team-view-struct-and-builder.md`, `production/epics/objectives-chain/story-015-rival-signal-release-scheduling.md`
