# Story 030: Strike hint predicate (HUD-R8) and lockout suppression

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (with ADR-0001 for identity blindness)
**ADR Decision Summary**: One plain C++ presenter core, `FHudPresenterCore`, turns an `FHudInputs` snapshot plus `ServerNowEst` and `LocalNow` into an `FHudViewModel` and owns every presentation state machine, including the HUD-R13 slot priorities. `UStealthHudPresenter` samples its carriers every frame by value, so the hint is level-triggered and identical on the listen host and a remote client. ADR-0001 requires that nothing about a non-ally pawn reveals its kind, which is why the hint predicate may read only the aim ray, rendering, reach and `PresentedOwnState`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ core, no UWorld/UObject); the module still builds against 5.8.4 / C++20. The camera aim ray is behind an injected backend (a fake in tests; the production adapter reads Camera's aim ray, `production/epics/camera-system/story-016-aim-ray.md`, from the owner's own camera only). The include-boundary lint (story 004) proves the predicate reads no team, controller, kind, concealment or `PlayerState` field.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never iterate actors, read a pawn's `PlayerState`/`Controller`/`Owner`, call `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, include a `Public/Server/` header, or write anything replicated from Presentation — source: ADR-0021, ADR-0001
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

The strike hint is the one HUD element that reacts to a non-ally pawn, so its trigger is the most exposed leak surface. This story builds the predicate and its lockout suppression as pure logic with a spy proving what it reads.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` HUD-R8 and UX spec UXA-HUD-09, scoped to this story:*

- [ ] **AC1** (derived, HUD-R8; `StealthGame.HUD.StrikeHint` predicate cases): **GIVEN** a fake aim-ray backend returning each of a rendered non-ally pawn in reach, out of reach, not rendered, an empty spot, a bush and a marked ally **WHEN** the hint is evaluated **THEN** it is shown only for a rendered non-ally pawn in reach, never for a spot, a bush or a marked ally; the hint is computed from the camera aim ray, rendering, reach and `PresentedOwnState` only, and a spy shows the predicate reads no team, controller, kind, concealment or `PlayerState` field.
- [ ] **AC2** (UXA-HUD-09; `StealthGame.HUD.StrikeHint` lockout cases): **GIVEN** the owner's action slot `Striking` for 1.75 s, a card up and an own fall **WHEN** frames step across t = 1.74 and 1.75 **THEN** the hint is hidden while `Striking`, while any card is up and from the owner's own fall (HUD-R6a, ruling D1); it returns when the lockout ends (at 1.75) if the aim ray still hits a rendered non-ally pawn in reach and the Notice slot is free; it is level-triggered (re-evaluated every frame, shown whenever the predicate holds and the Notice slot is free).

---

## Implementation Notes

*Derived from ADR-0021 §4, GDD HUD-R8, HUD-R6a, HUD-R13 and UX element E14:*

**What to build.** A predicate inside `FHudPresenterCore` (`Source/StealthPresentation/Private/Hud/`), fed by a small injected interface, for example `IHudAimRayBackend`, returning a value struct per frame: `{ bHitsPawn, bRendered, bInReach, bIsSpot, bIsBush, bIsMarkedAlly }`. The production adapter (in `UStealthHudPresenter`, not in the core) fills it from Camera's aim ray and the markers model's `IsMarkedAlly` (ADR-0021 §5; "marked ally" means a pawn whose public handle is in the own roster, and test fixtures must say so). Tests use a fake backend. The core never sees an actor, a team or a kind.

**The predicate (HUD-R8, E14).** Show the hint when all hold: the aim ray hits a rendered non-ally pawn in reach; `PresentedOwnState == Active` (never the raw C-TeamView state, HUD-R6a); the owner's action slot is not `Striking` (the 1.75 s lockout, `AttackAttemptLockoutSec` owned by Assassination; read through `FHudInputs`, not hardcoded); no card is up; the owner has not begun its own fall (`OnRoleChangePresented` already fired, ruling D1: hidden at once). It is never shown for a spot, a bush or a marked ally, and a concealed pawn produces no actor on this client, so no hint. The predicate reads no team, controller, kind, concealment or `PlayerState` field.

**Level-triggered.** The predicate is re-evaluated every frame. "Dropped, never queued" applies to the one-shot notices (story 031); the hint returns on the first frame the predicate holds and the Notice slot is free, including the frame a higher-priority notice leaves. This story produces the hint's candidate flag; story 031 resolves the slot occupant by priority. Keep the two separable: expose `bStrikeHintWanted` from the predicate and let the slot logic (story 031) decide visibility.

**Why parity matters.** Because the trigger is identical for every rendered non-ally pawn and the slot occupant is chosen by priority alone (never by what the viewer looks at), the choice is the same for an NPC, an enemy player and an enemy informant (HUD-R3). The PIE parity proof is story 028 (`LookParity`).

**Tests.** `Source/StealthPresentation/Private/Tests/HUD/StrikeHintTest.cpp`, class `FHudStrikeHintTest`, name `StealthGame.HUD.StrikeHint`. Include a read-spy: a fake `FHudInputs` or backend that records every field accessed, asserting only aim ray, rendering, reach and `PresentedOwnState` (plus the lockout, card and fall flags the AC lists) are touched. Step frames across t = 1.74 and 1.75 s with a fake clock and a fake action-slot state.

**Edge cases.** Notice slot occupied by a higher notice and then freed: the hint appears on the freeing frame if the predicate still holds (verified together with story 031). A reversed attacker's `Striking` state ends with the plate; the hint stays hidden under any card.

**Prototype.** The prototype `Source/StealthGame/Private/StealthHUD.cpp` has no hint logic worth promoting; nothing is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`: Notice-slot priority, one-shot drops and limiters.
- `production/epics/hud/story-032-hud-rejection-informant-and-hint-widgets.md`: the strike hint widget.
- `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md`: PIE parity for NPC, enemy and informant subjects.
- `production/epics/hud/story-004-hud-include-boundary-and-forbidden-api-lint.md`: the lint that proves the reads.
- `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`: `PresentedOwnState` itself.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudStrikeHint.h` and `.cpp` (predicate and `IHudAimRayBackend`, used by `FHudPresenterCore`)
- `Source/StealthPresentation/Private/Hud/HudPresenterCore.cpp` (wire the predicate into `Step`)
- `Source/StealthPresentation/Private/Hud/HudInputs.h` (aim-ray fields in `FHudInputs`, if not present)
- `Source/StealthPresentation/Private/Tests/HUD/StrikeHintTest.cpp` (class `FHudStrikeHintTest`, name `StealthGame.HUD.StrikeHint`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `StrikeHint` predicate
  - Given: a fake aim-ray backend returning each of: rendered non-ally pawn in reach, out of reach, not rendered, empty spot, bush, marked ally.
  - When: the hint is evaluated.
  - Then: it is shown only for rendered non-ally pawns in reach. A spy shows the predicate reads only aim ray, rendering, reach and `PresentedOwnState`.
  - Edge cases: "marked ally" means a pawn whose public handle is in the own roster. Fixtures must say so.
- **AC-2**: lockout and level-triggering
  - Given: the action slot `Striking` for 1.75 s, a card up, an own fall.
  - When: frames step across t=1.74 and 1.75.
  - Then: the hint is hidden while `Striking`, while any card is up and from own fall. It returns at 1.75 if the ray still hits and the Notice slot is free. It is re-evaluated every frame.
  - Edge cases: the Notice slot occupied by a higher notice, then freed.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/StrikeHintTest.cpp` (`StealthGame.HUD.StrikeHint`) — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`; gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`; `production/epics/camera-system/story-016-aim-ray.md`
- Unlocks: `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md`, `production/epics/hud/story-032-hud-rejection-informant-and-hint-widgets.md`
