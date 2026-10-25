# Story 002: Marker model, C-TeamView-only source and validation

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

This story creates `FTeammateMarkerModel`, the plain C++ marker set that every other marker story extends, and its single input rule: the set is built from exactly the viewer's own C-TeamView teammate entries (plus the fallback feed, added in story 004) and nothing else. It never queries the world. Entries are validated on every update: the viewer's own handle and a `PawnRef` that resolves to the viewer's own pawn are ignored, duplicate handles keep the first, two entries resolving to one pawn collapse to one marker (lower handle wins), and at most four markers exist (ascending handle). Each rejection is reported once per update to the client fault log and never to the server. Marker objects are created lazily. This is the data half of the locked rule that markers exist only on teammates' clients.

**GDD**: `design/gdd/teammate-markers.md` (TM-R1, TM-R3)
**Requirement**: `TR-tmk-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 5)
**Also governed by**: ADR-0001: Crowd identity rules (owner-only C-TeamView; section 5)
**ADR Decision Summary**: `FTeammateMarkerModel` is plain C++ holding the marker set keyed by `PublicHandle`, with validation, updated every frame from the local controller's C-TeamView only, never by world iteration. Team data reaches a client only through the owner-only carrier, so an opponent never holds an entry.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The model is engine-free C++ so it runs headless with `-nullrhi`; the HIGH rating lives at the paint and replication edges (stories 016, 019). A client-side fault is logged under `LogStealthMarkers`; tests use `ensure` through the client fault sink seam (tuning-data-config story 008).

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never iterate actors, read a pawn's `PlayerState`/`Controller`/`Owner`, call `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, or write anything replicated from Presentation — source: ADR-0021, ADR-0001
- Guardrail: At most 4 markers; module cost at most 0.05 ms per frame and zero per-frame heap allocation (AC-TMK-27)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-08, AC-TMK-09, TM-R3), scoped to this story:*

- [ ] **AC1** (AC-TMK-08, [U] `SourceIsTeamViewOnly`): **GIVEN** a mocked local C-TeamView with three teammate entries and a mocked world containing ten other pawns **WHEN** the marker set is built **THEN** it has exactly three markers, one per entry, attached to the entries' pawns, and none to any other pawn.
- [ ] **AC2** (AC-TMK-09, [U] `ValidationCapacityDedupe`): **GIVEN** entries that include the own handle, a duplicated handle, two handles resolving to one pawn and six valid entries **WHEN** validated **THEN** the own handle is ignored, the first duplicate is kept, the shared pawn gets one marker (the lower handle wins), at most four markers exist (ascending handle), and one fault is logged per rejection (`LogStealthMarkers` in development builds, an `ensure` in tests, never sent to the server).
- [ ] **AC3** (TM-R3, derived): **GIVEN** a client with no teammate entries (`TeamSize` 1) **THEN** no marker presentation object is ever instantiated; objects are created lazily only when a valid entry first needs drawing and destroyed when the entry leaves.

---

## Implementation Notes

*Derived from ADR-0021 §5 and TM-R1, TM-R3:*

Inputs are `FMarkerInputs` (defined here, extended by later stories): the local handle, the teammate entries `{PublicHandle, State, PawnRef, IsConcealed, RetireAtServerTime, bReconnecting}`, and later the feed entries. `Step(const FMarkerInputs&, double LocalNow, FWorldAnchorMarkers& Out)` is the whole API surface (ADR-0021 Key Interfaces). Validation order is fixed by TM-R3: (1) own handle or a ref that resolves to the local player's own pawn, (2) duplicate handles keep the first in received order, (3) two entries resolving to one pawn keep the lower `PublicHandle`, (4) capacity `MarkerCapacityCount = 4` drops the rest in ascending `PublicHandle` order. Report every rejection once per update to the client fault sink.

Marker presentation objects are created lazily; a client with no teammate entries never creates one. The mocked world in AC1 must contain real distinct pawn stand-ins so a bug that walks the world would fail the test (the model takes pawn references only through `PawnRef`).

Prototype replaced: `AStealthHUD::DrawTeammateMarkers` (`Source/StealthGame/Private/StealthHUD.cpp:164`) iterates every character and reads a team field replicated on every pawn; that pattern is forbidden by TM-R2 and nothing is promoted. Retirement is story 034.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-003-fill-precedence-and-roundended-freeze.md`: the fill of each marker.
- `story-004-anchor-fed-position-and-null-ref.md`: anchors and the feed input.
- `story-016-presenter-local-controller-only.md`: the presenter that fills `FMarkerInputs` from the controller.
- `story-017-include-grep-ban-no-replicated-fields.md`: the build-time include and grep ban.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/TeammateMarkerModel.h` and `TeammateMarkerModel.cpp` (`FTeammateMarkerModel`, `FMarkerInputs`, validation)
- `Source/StealthPresentation/Private/Markers/MarkerTypes.h` (entry and marker value types)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerSourceAndValidationTest.cpp` (names `StealthGame.Markers.SourceIsTeamViewOnly`, `StealthGame.Markers.ValidationCapacityDedupe`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: a mock C-TeamView with 3 entries and a mock world with 10 other pawns. When: the marker set is built. Then: exactly 3 markers, each on its entry's pawn, none on the other 10. Edge cases: zero entries; an entry pawn that is also in the world list.
- **AC2**: Given: entries containing the own handle, a duplicated handle, two handles resolving to one pawn, and six valid entries. When: validated. Then: the own handle is ignored, the first duplicate is kept, the shared pawn gets one marker (lower handle), at most 4 markers (ascending handle), one fault per rejection (`ensure` caught in the test, nothing sent to the server). Edge cases: exactly 4 and exactly 5 valid entries; a fault counter per rejection kind.
- **AC3**: Given: `TeamSize` 1 and a counting marker factory. When: frames run, then an entry is added, then removed. Then: 0 objects created while alone; one is created only when the entry first needs drawing; it is destroyed when the entry leaves. Edge cases: a valid entry present while the view gate is Neutral.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerSourceAndValidationTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`, `story-008-fault-sink-and-test-seams-define.md`; `production/epics/participant-state-roster/story-042-team-view-pawn-ref-ordering.md` (the entry shape)
- Unlocks: `production/epics/teammate-markers/story-003-fill-precedence-and-roundended-freeze.md`, `story-004-anchor-fed-position-and-null-ref.md`, `story-006-lifecycle-retire-hold-takeover-direct-switch.md`, `story-014-is-marked-ally-query.md`, `story-015-view-gate-model.md`, `story-016-presenter-local-controller-only.md`
