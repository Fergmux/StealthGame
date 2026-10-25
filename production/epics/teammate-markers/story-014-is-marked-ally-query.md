# Story 014: IsMarkedAlly and the IAllyMarkerQuery interface

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The honest client must never issue an `Attack` at a teammate and must not show a Strike hint on one, so the Input & Interaction Framework needs a client-local answer to "is this pawn a marked ally?". `IsMarkedAlly(Pawn)` is true iff the pawn is the resolved `PawnRef` of a valid teammate entry or the cached pawn of a marker in its retire hold; it does not depend on whether the marker is drawn (view gate, off-screen, settings). Because IIF is Core and cannot include Presentation, ADR-0021 puts a consumer-owned interface in Core (`IAllyMarkerQuery`) that the markers presenter implements and the bootstrap injects weakly into `UPawnCommandClient`; with nothing bound (headless runs) it answers false. The server's own teammate rule (Assassination ASN-R9) is unaffected; `IsMarkedAlly` is never sent to the server.

**GDD**: `design/gdd/teammate-markers.md` (TM-R10)
**Requirement**: `TR-tmk-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 5 and Key Interfaces)
**ADR Decision Summary**: `IAllyMarkerQuery { bool IsMarkedAlly(const APawn*) const; }` is declared in `StealthGameplay/Public/View/Core/`, implemented by the markers presenter and injected (weak) into `UPawnCommandClient` by the bootstrap; with no presentation bound it answers false.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: A `UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))` so `TWeakInterfacePtr` works (ADR-0007 weak registration); the test uses a stub implementing the interface. Pointer comparison against the `PawnRef`-resolved actor only.

**Control Manifest Rules (this layer)**:
- Required: `IsMarkedAlly` is served only from the local marker model built from the own controller's C-TeamView — source: ADR-0021
- Forbidden: Never send `IsMarkedAlly` to the server — source: ADR-0021
- Guardrail: The query is a pointer lookup in a set of at most four entries

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-24, TM-R10) and ADR-0021 §5, scoped to this story:*

- [ ] **AC1** (AC-TMK-24, [U] `IsMarkedAllyConsistency`): **GIVEN** tracked, fed, stale, held and released entries and the view gate closed **WHEN** `IsMarkedAlly` is queried for each pawn the client holds **THEN** it is true for resolved and held ally pawns (drawn or not), false for every other pawn, and false for the old pawn after a retarget.
- [ ] **AC2** (AC-TMK-24, IIF side): **GIVEN** `IsMarkedAlly` true for a pawn **THEN** the IIF client issues no `Attack` and shows no Strike hint for it (IIF R10, UI 4), checked through the injected `IAllyMarkerQuery`; with no presentation bound the query answers false.
- [ ] **AC3** (ADR-0021 §5, derived): **GIVEN** the interface **THEN** `IAllyMarkerQuery` is declared in `StealthGameplay/Public/View/Core/`, implemented by the markers presenter and injected weakly into `UPawnCommandClient` by the bootstrap, and `IsMarkedAlly` is in no RPC and in no server-side call (grep).

---

## Implementation Notes

*Derived from TM-R10 and ADR-0021 Key Interfaces:*

`FTeammateMarkerModel::IsMarkedAlly(const APawn*)` returns true for the resolved `PawnRef` of a valid entry (any of `Tracking`, `Fed` entries whose pawn happens to resolve, `Stale`) or the cached pawn of a marker in `Held`; a `Fed` entry with no pawn on this client has no pawn to match. It is independent of the view gate and of off-screen state. After a direct pawn switch (story 006) the old pawn is false and the new pawn true on the same frame. Declare `IAllyMarkerQuery` and `UAllyMarkerQuery` per the ADR's Key Interfaces block; the presenter (story 016) implements it and the bootstrap calls `IOwnerCommandClientView::SetAllyMarkerQuery(TWeakInterfacePtr<IAllyMarkerQuery>)`. The IIF code that consumes it is IIF stories 064 and 073; here the test uses the real `UPawnCommandClient` entry point or its seam with a stub query.

Accepted harmless case (CD-GDD-ALIGN item 5): for 0.25 s after a `ReleaseToCrowd` the held marker's pawn is a fresh NPC and `IsMarkedAlly` still returns true for it; the only effect is that the honest client cannot strike that pawn for that long.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `input-interaction-framework/story-064-no-ally-attack-no-aim-assist.md` and `story-073-strike-hint-parity.md`: the consumer side in IIF.
- `story-016-presenter-local-controller-only.md`: the presenter that owns the model and registers the interface.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/AllyMarkerQuery.h` (`UAllyMarkerQuery`, `IAllyMarkerQuery`)
- `Source/StealthPresentation/Private/Markers/TeammateMarkerModel.cpp` (`IsMarkedAlly`)
- `Source/StealthPresentation/Private/Tests/Markers/IsMarkedAllyTest.cpp` (name `StealthGame.Markers.IsMarkedAllyConsistency`, class `FMarkersIsMarkedAllyConsistencyTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: tracked, fed, stale, held and released entries, view gate closed. When: `IsMarkedAlly` is queried for every pawn the client holds. Then: true for resolved and held ally pawns (drawn or not), false for all others, false for the old pawn after a retarget. Edge cases: the same query with the gate open.
- **AC2**: Given: an injected stub `IAllyMarkerQuery` returning true for a pawn, and a second case with none bound. When: `UPawnCommandClient` evaluates that pawn. Then: no `Attack` is issued and no Strike hint is shown; with no presentation bound the query answers false. Edge cases: the query flips to false mid-frame.
- **AC3**: Given: the module's headers and class reflection. When: structural checks run. Then: `IAllyMarkerQuery` is declared under `StealthGameplay/Public/View/Core/`, the presenter implements it, the bootstrap injects it as a weak interface into `UPawnCommandClient`, and `IsMarkedAlly` is in no RPC and in no server-side call (grep). Edge cases: a seeded server call fails the check.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/IsMarkedAllyTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-002-marker-model-teamview-source-and-validation.md`, `story-006-lifecycle-retire-hold-takeover-direct-switch.md`; cross-epic `production/epics/input-interaction-framework/story-094-controller-transport-skeleton.md` (the `UPawnCommandClient` entry point)
- Unlocks: `production/epics/teammate-markers/story-016-presenter-local-controller-only.md`; `input-interaction-framework/story-064-no-ally-attack-no-aim-assist.md`, `story-073-strike-hint-parity.md`
