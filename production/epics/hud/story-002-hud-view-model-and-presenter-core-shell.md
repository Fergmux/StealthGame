# Story 002: FHudViewModel clusters, deterministic Dump() and the headless FHudPresenterCore shell

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-002`, `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input §4 (with ADR-0008 for the headless-core and leak-suite design)
**ADR Decision Summary**: One plain C++ presenter core, `FHudPresenterCore` (no `UWorld`, no `UObject` access), turns an `FHudInputs` snapshot plus `ServerNowEst` and `LocalNow` into an `FHudViewModel`. `FHudViewModel` is a tree of plain `USTRUCT`s, one per cluster, holding only values to draw, with a deterministic `Dump()` (stable field order, fixed float precision) used by the leak suite and host parity. Widgets are dumb and apply a value only when it differs from the last applied value.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: reflection only (plain `USTRUCT`s from CoreUObject); the core itself touches no `UWorld`, `UObject` instance or engine world type, and the module still builds against 5.8.4 / C++20. `FText` values come from string tables (story 005); `UE_LOGF` is preferred over `UE_LOG` in 5.8 for any diagnostics. ADR-0021 widgets-are-dumb rule (no UMG property bindings, no widget animation assets) shapes the view-model: nothing here may require a binding. ADR-0008 headless-core pattern: the Logic test drives the core with a fake clock and fake carriers, no PIE.

**Control Manifest Rules (this layer)**:
- Required: Widgets are dumb (no UMG property bindings, no Blueprint logic beyond layout and style); `FHudViewModel::Dump()` is deterministic — source: ADR-0021
- Forbidden: Never port the prototype `AStealthHUD` drawing code — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story creates the data shape and the empty core that every cluster story (014-044) fills. It builds the test seam first: the golden `Dump()` that the leak suite (AC-HUD-01..07) and host parity (AC-HUD-20) later diff.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC preamble: the dump lists every visible element per frame) and ADR-0021 §4, scoped to this story:*

- [ ] **AC1** (derived, ADR-0021 §4, `StealthGame.HUD.ViewModelDump`): **GIVEN** an `FHudViewModel` with all 11 clusters populated by a factory (Brace, Tally, Objectives, Feed, ResultSlot, NoticeSlot, Card, Banner, Brackets, Latency, WorldAnchor) holding only values to draw (`FText` from string tables, enum ids, alphas, counts) **WHEN** `Dump()` is called twice, the second time with a different `LocalNow` **THEN** the two dumps are byte-identical, the field order matches a checked-in golden, there is no wall-clock field, and floats use one fixed precision.
- [ ] **AC2** (derived, GDD AC preamble, `StealthGame.HUD.ViewModelDump`): **GIVEN** a frame with a visible plate, a collapsed tally and one cue fired **WHEN** the frame is dumped **THEN** the dump lists, per frame, every visible element id, text, glyph, fill, slot and alpha and the cue events fired that frame, and a hidden (collapsed) element appears as absent, not as alpha 0.
- [ ] **AC3** (derived, ADR-0021 §4 and §1 "apply on change only", `StealthGame.HUD.ViewModelDump`): **GIVEN** `FHudPresenterCore` built in a module with no `UWorld`/`UObject` access, a fake clock and fake carriers **WHEN** `Step(In, ServerNowEst, LocalNow, Out)` runs for N frames and the widget-apply helper is called twice with the same value **THEN** the test links and runs without engine world types, and the widget setter is called once per distinct value.

---

## Implementation Notes

*Derived from ADR-0021 §4 and ADR-0008 (headless cores):*

Create, in `Source/StealthPresentation/Private/Hud/`:

- `HudViewModel.h/.cpp`: `FHudViewModel` as a tree of plain `USTRUCT`s, one per cluster: `FHudBraceVm`, `FHudTallyVm`, `FHudObjectivesVm`, `FHudFeedVm`, `FHudResultSlotVm`, `FHudNoticeSlotVm`, `FHudCardVm`, `FHudBannerVm`, `FHudBracketsVm`, `FHudLatencyVm`, `FHudWorldAnchorVm`. Fields hold only values to draw: `FText` (resolved from the string table by the producing story), enum ids, `bool` visibility, `float` alpha and fill, `int32` counts and slot ids. No `UObject*`, no `AActor*`, no `FString` copied from the wire, no handle types (those would turn the view-model into a leak carrier). Each cluster struct starts with the stable element ids from the UX spec (E01-E22, `design/ux/hud.md` lines 238-500) so a later story adds fields, never renames. The cluster fields are empty shells here; each cluster story adds its fields and extends the golden.
- `FString FHudViewModel::Dump() const`: deterministic. Stable field order (declaration order, never reflection hash order), one fixed float precision (developer's choice, for example 4 decimals, pinned in the golden file), no wall-clock field of any kind (`LocalNow`, `FPlatformTime` readings are excluded; tick index is the key). Per frame it lists every visible element with id, text, glyph, fill, slot and alpha, then the cue events fired that frame. Define "hidden" as collapsed: a collapsed element has no entry; a fading element at alpha 0 that is not collapsed stays listed.
- `HudInputs.h`: `FHudInputs`, the per-frame snapshot struct (carrier snapshots, replies, phase, tuning, view state, own pawn writes, settings). Story 003 fills it from live carriers; here it holds the minimum the tests need and a documented list of the fields later stories add.
- `HudPresenterCore.h/.cpp`: `FHudPresenterCore` with `void Step(const FHudInputs& In, double ServerNowEst, double LocalNow, FHudViewModel& Out)`. In this story `Step` only resets `Out` from `In` for the empty clusters and bumps a frame counter; every state machine (PresentedOwnState, plate timelines, F2-F10, HUD-R13 slots, HUD-R17 rebuild) arrives in the cluster stories. The file must include no engine world types (`Engine/World.h`, `GameFramework/*`, `UObject` instance APIs); the headless claim is proven by the test target linking and running without them.
- `HudWidgetApply.h`: the apply-if-changed helper (for example `THudApplied<T>` with `bool Apply(const T& New)`), the C++ half of ADR-0021's "apply on change only". The first apply always fires; comparison is by value with NaN equal to itself (compare float bit patterns or treat two NaNs as equal) so a NaN never re-applies every frame.

Prototype disposition: this is a clean-room replacement of the drawing in `Source/StealthGame/Private/StealthHUD.cpp` / `Public/StealthHUD.h` (the `AHUD` canvas). Nothing is promoted; the prototype leaks (it reads a team field replicated on every pawn) are never ported. Removal of the prototype is story 059.

Traps: (1) a reflection scan asserts every `UPROPERTY` in the view-model tree is `FText`, enum, `bool`, integer or float (or a nested cluster struct or `TArray` of those); "only values to draw" has no other oracle, so confirm the allowlist with the author in review and keep it as the test's constant. (2) The golden must pin the float precision, and a developer-chosen precision change is a golden change by design. (3) Do not let `Dump()` read the clock or any global: two calls on the same state with different `LocalNow` must match. (4) Decisions D11, D16, D19, D20, D29, D23, D26 add content to later clusters; none changes the shape defined here.

Out of this story: any state machine, any carrier read, any widget.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`: `UStealthHudPresenter` and the live fill of `FHudInputs`.
- `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`, `story-015-hud-result-show-time-f2.md`, `story-016-hud-plate-timeline-f3-and-reduce-motion.md`, `story-018-hud-brace-indicator-state-logic.md`, `story-020-hud-wrong-target-tally-logic.md`, `story-024-hud-removal-card-timing-f4-logic.md`, `story-035-hud-team-feed-queue-f8-and-removal-lines.md`, `story-039-hud-objectives-cluster-f9-logic.md`, `story-040-hud-end-banner-f10-and-round-summary-logic.md`: the state machines that fill the clusters.
- `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`: the widgets that apply the view-model.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudViewModel.h`
- `Source/StealthPresentation/Private/Hud/HudViewModel.cpp`
- `Source/StealthPresentation/Private/Hud/HudInputs.h`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.h`
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp`
- `Source/StealthPresentation/Private/Hud/HudWidgetApply.h`
- `Source/StealthPresentation/Private/Tests/HUD/ViewModelDumpTest.cpp` (class `FHudViewModelDumpTest`, name `StealthGame.HUD.ViewModelDump`)
- `Source/StealthPresentation/Private/Tests/HUD/Golden/ViewModelDump.golden.txt` (the checked-in golden pinning field order and float precision)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: view-model tree and `Dump()` determinism
  - Given: an `FHudViewModel` with all 11 clusters populated by a factory.
  - When: `Dump()` is called twice, once with a different `LocalNow`.
  - Then: the dumps are byte-identical, field order matches a checked-in golden, there is no wall-clock field, and floats use one fixed precision.
  - Edge cases: a reflection scan asserts every UPROPERTY is FText, enum, bool, int or float ("only values to draw" has no other oracle; confirm the allowlist with the author). The precision is the developer's choice but must be pinned in the golden.
- **AC-2**: dump content
  - Given: a frame with a visible plate, a collapsed tally and one cue fired.
  - When: the frame is dumped.
  - Then: the visible element lists id, text, glyph, fill, slot and alpha, plus that frame's cue events. The collapsed element has no entry.
  - Edge cases: a fading element at alpha 0 that is not collapsed stays listed. Define "hidden" as collapsed.
- **AC-3**: headless core and apply-if-changed helper
  - Given: `FHudPresenterCore` built in a module with no `UWorld`/`UObject` includes, a fake clock and fake carriers.
  - When: `Step` runs for N frames, and the apply helper is called twice with the same value.
  - Then: the test links and runs, and the widget setter is called once per distinct value.
  - Edge cases: the first apply always fires. NaN compares equal to itself.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/ViewModelDumpTest.cpp` (`StealthGame.HUD.ViewModelDump`) — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)
- Gate: BLOCKING (Logic default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`
- Unlocks: `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`, `production/epics/hud/story-004-hud-include-boundary-and-forbidden-api-lint.md`, `production/epics/hud/story-005-hud-string-table-and-language-lint.md`, `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`, `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`, `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`, `production/epics/hud/story-015-hud-result-show-time-f2.md`, `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`, `production/epics/hud/story-018-hud-brace-indicator-state-logic.md`, `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`, `production/epics/hud/story-024-hud-removal-card-timing-f4-logic.md`, `production/epics/hud/story-030-hud-strike-hint-predicate-and-lockout.md`, `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`, `production/epics/hud/story-033-hud-returning-countdown-f7.md`, `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`, `production/epics/hud/story-039-hud-objectives-cluster-f9-logic.md`, `production/epics/hud/story-040-hud-end-banner-f10-and-round-summary-logic.md`, `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`
