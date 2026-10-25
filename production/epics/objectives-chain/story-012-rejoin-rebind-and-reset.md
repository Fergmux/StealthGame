# Story 012: Rejoin, view rebind and round reset

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-17 lifecycle; Edge Cases on rejoin, Unresponsive and grace; AC-OC-15)
**Requirement**: `TR-oc-003` (the view is rebuilt on rejoin and cleared on reset)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 3 and 6)
**Other ADRs**: ADR-0005: Session, seamless travel and state stores (`RebindView`, "subscribe to Roster events" rule, controller replacement at travel)
**ADR Decision Summary**: Objectives subscribes to the Roster's rebind event `URosterStore::OnParticipantViewRebound(FParticipantId)` (GDD `OnViewRebound`), raised after `RebindView`, and writes that participant's team view inline in the handler (server, game thread). At seamless travel no write is needed: Objectives state lives for one round, so a new controller's default empty view is already correct until `Active` entry. On a rejoin inside a round the handler writes the team's current view. Everything clears on `OnRosterReset`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0020 MEDIUM for the carrier; the handler is plain C++ over a stub event)
**Engine Notes**: The rebind event is a Roster native delegate (ADR-0005 section 2); the carrier behaviour across rejoin and travel is verified by OBJ-V1 in story 011 and the Session harness, not here. This story's tests drive the handler with a stub event and a stub view sink.

**Control Manifest Rules (this layer)**:
- Required: Team-audience objective data is written only inside a queue slot, the `OnRoundActive` step or the Roster rebind handler — source: ADR-0020
- Required: Any consumer that needs world wiring subscribes to Roster events instead of depending on providers registered at world wiring — source: ADR-0005, ADR-0020
- Forbidden: Never carry objective state across rounds; never log site ids or draws outside the Event Log — source: ADR-0020
- Guardrail: handler O(team size) view writes — source: ADR-0020

The view must be right at every moment a player can look at it, including after the controller that carried it has been replaced. ADR-0005 replaces PlayerControllers at seamless travel and re-creates one on rejoin, and the Roster's `RebindView` re-pushes the Roster part but cannot depend on providers registered at world wiring, so Objectives subscribes to the rebind event and writes its own view in the handler. The edge cases of the GDD (a tender who rejoins does not resume the hold; an Unresponsive tender is handed to AI and the hold is cancelled; team progress is unaffected by any of it) are the behaviours this story pins down.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-15, Edge Cases) and ADR-0020 section 6, scoped to this story. QL-STORY-READY revised AC2 (write count) and AC3 (hold progress versus team progress, owner of the cancel):*

- [ ] **AC1** (AC-OC-15 [U] `ResetAndRejoin`): **GIVEN** a round reset **THEN** all site states, team views and pending signals are cleared on `OnRosterReset`; **GIVEN** a rejoin inside the grace **THEN** the rejoiner's team view equals its teammates'; a rejoin after the grace expires restores nothing.
- [ ] **AC2** (ADR section 6 [U] `ViewReboundWritesTeamView`): the service is subscribed to `URosterStore::OnParticipantViewRebound` and writes that participant's team view inline in the handler (only the participant's own team; an event before `Active` writes the empty default); at seamless travel the **view write count is 0** and the new controller's view equals the empty default until `Active` entry.
- [ ] **AC3** (GDD Edge Cases [U] `RejoinHoldDoesNotResume`): **GIVEN** a tender who rejoins inside the grace **THEN** the hold does not resume (a new `Begin` is required) and team stage state is unchanged; **GIVEN** a tender who becomes Unresponsive mid-hold **THEN** the IIF cancels the hold (Round hands the pawn to AI by the normal blend; Objectives only receives `OnInteractEnded(..., Cancelled, ...)`, so the test stubs the IIF side), its elapsed hold time reads 0, a new `Begin` is required to resume and team stage state is unchanged.

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 3 and 6 and the GDD Edge Cases:*

Subscription: the composition root binds `URosterStore::OnParticipantViewRebound(FParticipantId)` to a handler on `UObjectivesService` (story 005 does the binding; this story provides the handler body). The handler resolves the participant's team (`GetTeam`), builds that team's view (story 010) and writes it inline to the participant's new controller component (story 011's writer), on the server game thread. An informant of the same team gets the same view. An event before `Active` entry writes the empty default (no client may know a site before the round starts, AC-OC-29).

Reset: `OnRosterReset` (Roster R-reset, the same hook Round uses) clears the store, the claim table, the pending rival-release list (story 015) and every team view write cache. The service is world-scoped and new each round, so nothing is carried across rounds; clearing on reset is still asserted because the reset can fire within a world's lifetime (a restart).

Rejoin semantics from the GDD Edge Cases: if the tender rejoins inside the grace, control returns by blend; the hold does not resume and must be begun again; team progress is unchanged; the team view is re-sent with the C-TeamView rebuild. If the tender becomes Unresponsive (3 s silence) mid-hold, Round hands the pawn to AI by the normal blend and the **IIF** cancels the hold (Objectives learns it through `OnInteractEnded(Cancelled)`, which releases the claim only, story 006); progress of the hold resets to 0 and team stage state is unchanged. If `Complete` falls in the slot where the participant is Unresponsive or Disconnected, gate (c) gives no effect (story 008).

Prototype replaced: nothing in the prototype handled rejoin for objective state (it lived in replicated actors that survived on the server).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-team-view-component-and-replication.md`: the carrier and its owner-only reach, including after rejoin and travel (OBJ-V1).
- `story-008-completion-gate-and-stage-effects.md`: gate (c) at the `Complete` slot.
- `story-019-pie2-team-view-ingress-and-informant-parity.md`: the multi-client informant-rejoin check (AC-OC-33).
- Session and Roster epics: the rebind event itself and the reset hook.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesService.cpp` (rebind handler, reset)
- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesCore.cpp` (`ResetAll`)
- `Source/StealthGameplay/Private/Tests/Objectives/ResetAndRejoinTest.cpp` (class `FObjectivesResetAndRejoinTest`, name `StealthGame.Objectives.ResetAndRejoin`)
- `Source/StealthGameplay/Private/Tests/Objectives/ViewReboundWritesTeamViewTest.cpp` (`StealthGame.Objectives.ViewReboundWritesTeamView`)
- `Source/StealthGameplay/Private/Tests/Objectives/RejoinHoldDoesNotResumeTest.cpp` (`StealthGame.Objectives.RejoinHoldDoesNotResume`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS; the AC2 and AC3 rewrites were applied above and the specs cover the revised text). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`ResetAndRejoin`): reset and rejoin
  - Given: round with progress, a pending rival signal, and two teammates; stub roster events
  - When: `OnRosterReset` fires; separately a teammate rejoins inside the grace
  - Then: all site states, team views and pending signals cleared on reset; the rejoiner's view equals the teammates'
  - Edge cases: reset with no pending signal; rejoin after the grace expires gives no restore.
- **AC-2** (`ViewReboundWritesTeamView`): rebind handling
  - Given: service subscribed to `URosterStore::OnParticipantViewRebound`; stub sink
  - When: the event fires for participant P; separately seamless travel occurs
  - Then: P's view is written inline inside the handler; at travel the view write count is 0 and the new controller view is the empty default
  - Edge cases: event for a participant on the other team writes only that team's view; event before Active writes empty.
- **AC-3** (`RejoinHoldDoesNotResume`): hold and progress across rejoin
  - Given: a tender holding; stub IIF; Unresponsive tender case
  - When: the tender rejoins inside the grace; separately the tender goes Unresponsive
  - Then: hold does not resume (a new Begin is needed); team progress unchanged; Unresponsive cancels the hold with elapsed time 0
  - Edge cases: rejoin in the same tick as D; Unresponsive cancel after the debounce.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md`, `production/epics/objectives-chain/story-010-team-view-struct-and-builder.md`, `production/epics/objectives-chain/story-011-team-view-component-and-replication.md`; cross-epic: `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md`, `production/epics/session-travel/story-056-roster-rebind-both-paths.md`, `production/epics/participant-state-roster/story-059-mp-rejoin-within-grace.md`
- Unlocks: `production/epics/objectives-chain/story-019-pie2-team-view-ingress-and-informant-parity.md`
