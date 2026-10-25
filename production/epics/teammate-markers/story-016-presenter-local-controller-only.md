# Story 016: Presenter samples only the local controller

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

`UTeammateMarkerPresenter` is the glue between the carriers and the plain-C++ model. Each frame it builds the model's inputs by value from the local controller's `URosterTeamViewComponent` (the C-TeamView) and `UTeamPositionFeedComponent` (the fallback feed), found with `FindComponentByClass` on its own controller and nowhere else. It never iterates the world and never reads another controller's team view, so a listen host, whose process holds every team's team view, reads only its local controller exactly like a remote client. It depends on no RepNotify having fired (the host gets none for its own writes). When a new local controller arrives (travel replaces controllers), the bootstrap re-binds the presenter and the model is rebuilt from the re-pushed view. The presenter also implements `IAllyMarkerQuery` (story 014) and publishes `OnAllyMarkerRetired` (story 008).

**GDD**: `design/gdd/teammate-markers.md` (TM-R1, TM-R2, TM-R6 step 5; AC-TMK-07 instrumented half)
**Requirement**: `TR-tmk-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (sections 4 and 5)
**Also governed by**: ADR-0001 (owner-only C-TeamView carrier; host parity)
**ADR Decision Summary**: The model is updated every frame by `UTeammateMarkerPresenter` from the local controller's `URosterTeamViewComponent` and `UTeamPositionFeedComponent` only, found with `FindComponentByClass` on its own controller (ADR-0001), never by world iteration; Presentation samples by value so the listen host runs the identical path.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: On the listen host the server process holds every PlayerController; the presenter takes its controller from the local player (`ULocalPlayer::GetPlayerController`), never `GetFirstPlayerController` and never an iterator. Re-binding on controller swap follows the Roster's re-push (roster story 041).

**Control Manifest Rules (this layer)**:
- Required: Presentation builds view-models by sampling its carriers every frame by value (the listen host runs the same path) — source: ADR-0021
- Forbidden: Never depend on a RepNotify having fired; never iterate actors or call `GetFirstPlayerController` — source: ADR-0021, ADR-0007
- Guardrail: Zero per-frame heap allocation in the sampling path

---

## Acceptance Criteria

*From ADR-0021 §4-§5, GDD TM-R2 and AC-TMK-07 (instrumented half), scoped to this story:*

- [ ] **AC1** (ADR-0021 §5, derived): **GIVEN** `UTeammateMarkerPresenter` **THEN** it builds its inputs every frame, by value, from the local controller's `URosterTeamViewComponent` and `UTeamPositionFeedComponent` found with `FindComponentByClass` on its own controller, with no dependence on a RepNotify having fired, and a listen host and a remote client run the same path.
- [ ] **AC2** (AC-TMK-07, instrumented half, [PIE-2]): **GIVEN** a listen host with a same-team client **WHEN** an instrumented host run renders markers **THEN** the module read only the local PlayerController's C-TeamView, never another controller's (extends AC-PSR-42).
- [ ] **AC3** (TM-R6 step 5, derived): **GIVEN** a new local controller after travel **THEN** the presenter is re-bound by the bootstrap and the model rebuilt from the re-pushed view, with no world iteration and no `GetFirstPlayerController`.

---

## Implementation Notes

*Derived from ADR-0021 §4, §5 and TM-R1, TM-R2:*

The presenter is owned by the HUD bootstrap in `StealthPresentation` (`AStealthHUD` only owns the bootstrap, ADR-0007). `Step` reads the components into `FMarkerInputs`, the current Round phase and the view state (story 015), and the clock, and calls `FTeammateMarkerModel::Step`. The leak-test seam: an accessor spy on every controller's C-TeamView component counts reads per controller; the host-run assertion is that only the local controller's counter moves. Ignore `GameState->PlayerArray` except the public-name helper (TM-R11); the name lookup goes by `PublicHandle` through the C-Public copy.

Registration: the presenter implements `IAllyMarkerQuery` and the bootstrap injects it into `UPawnCommandClient` through `IOwnerCommandClientView::SetAllyMarkerQuery` (story 014); it also exposes `OnAllyMarkerRetired` for the HUD presenter (story 008). A missing component (a client with no team view yet) gives an empty set and no crash.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-017-include-grep-ban-no-replicated-fields.md`: the build-time ban over the module.
- `story-026-host-parity-of-marker-set.md`: the serialised-set equality (needs the painted path).
- `story-025-travel-and-viewer-rejoin-rebuild.md`: the real seamless-travel PIE.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/TeammateMarkerPresenter.h` and `TeammateMarkerPresenter.cpp`
- `Source/StealthPresentation/Private/Tests/Markers/MarkerPresenterTest.cpp` (name `StealthGame.Markers.PresenterSampling`, class `FMarkersPresenterSamplingTest`)
- `Source/StealthTestHarness/Private/Markers/PresenterHostReadTest.cpp` (CQTest, `StealthGame.Markers.PIE.HostReadsLocalOnly.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: a presenter bound to a controller with mock `URosterTeamViewComponent` and `UTeamPositionFeedComponent`. When: component data changes with no RepNotify fired, then a frame ticks. Then: the inputs reflect the new data that frame, are built by value via `FindComponentByClass` on its own controller, and the listen-host and remote-client paths are the same (parametrised). Edge cases: a missing component gives an empty set, no crash.
- **AC2**: Given: a listen host plus a same-team client, with an accessor spy on every controller's C-TeamView. When: an instrumented host run renders N frames. Then: reads occur only on the local PlayerController's component, 0 on any other (extends AC-PSR-42). Edge cases: a host with two controllers. (Needs PIE-2.)
- **AC3**: Given: a controller swap after travel, simulated. When: the bootstrap rebinds. Then: the model is rebuilt from the re-pushed view, with 0 world iterations and 0 `GetFirstPlayerController` calls. Edge cases: the old controller is destroyed before the rebind.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `MarkerPresenterTest.cpp` and the CQTest `PresenterHostReadTest.cpp` under the test roots above OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-002-marker-model-teamview-source-and-validation.md`, `story-008-ally-marker-retired-event-ended-reset.md`, `story-014-is-marked-ally-query.md`, `story-015-view-gate-model.md`; cross-epic `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md`, `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`
- Unlocks: `production/epics/teammate-markers/story-019-paint-markers-in-world-anchor-leaf.md`, `story-024-ended-and-reset.md`, `story-026-host-parity-of-marker-set.md`, `story-027-leak-audit-marker-objects-and-canaries.md`
