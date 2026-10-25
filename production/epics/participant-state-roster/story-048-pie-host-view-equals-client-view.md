# Story 048: The listen host's presentation model equals a client's and presentation code never includes the roster query header

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**Also governed by**: ADR-0001: Crowd identity rules
**ADR Decision Summary**: ADR-0007 fixes four runtime modules plus Editor and a Shipping-excluded `StealthTestHarness`, and splits every public header into `Public/View/` (client-safe) or `Public/Server/` (server-only, first include `StealthServerOnlyGuard.h`, which `#error`s inside `StealthPresentation` through the private `STEALTH_PRESENTATION_MODULE=1` definition). The include boundary has three enforcement layers: the compile guard, a CI/pre-commit include lint, and an allowlist-first Blueprint audit, because on the listen host server calls return authoritative objects (the host-parity hole the C++ guard cannot see). ADR-0001 adds that presentation reads only its own controller's C-TeamView, GameState public structs and client-local queries, host included.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Post-cutoff: `UE_LOGF` (5.8) and the C++20 default standard (5.8). Verification required: the `STEALTH_PRESENTATION_MODULE` private define reaches every translation unit of `StealthPresentation` under unity build and shared or private PCH, and does not reach dependents (keep the CI self-test); `TargetConfigurationDenyList: ["Shipping", "Test"]` on the `StealthTestHarness` entry is honoured; the DataValidation Blueprint audit can walk `K2Node_CallFunction::GetTargetFunction`, cast and variable pins and widget bindings; `[CoreRedirects]` resolve for moved classes.

**Control Manifest Rules (this layer)**:
- Required: every header in a `Public/` folder sits under `View/` (client-safe) or `Server/` (server-only); every `Public/Server/` header includes `StealthServerOnlyGuard.h` first; `StealthPresentation` declares `STEALTH_PRESENTATION_MODULE=1` as a `PrivateDefinitions` entry, never a `PublicDefinitions` entry — source: ADR-0007
- Required: a service with both faces is split into a `View/` facade and a `Server/` implementation; project header file names are unique across all modules (`Stealth` prefix plus system name) — source: ADR-0007
- Required: the include lint (rules a-j) runs in CI and pre-commit — source: ADR-0007
- Forbidden: Never let a `Public/View/` header include a `Server/` header — source: ADR-0007
- Forbidden: Never put `BlueprintCallable`, `BlueprintPure`, `BlueprintReadOnly`, `BlueprintReadWrite` or `BlueprintAssignable` on a member of a server type — source: ADR-0007
- Guardrail: C-TeamView pushes change-only, at most one per frame, within the 50 KB/s per-client budget — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-42** [PIE-2][UI-ev] `StealthGame.Roster.PIE.HostViewEqualsClientView`: GIVEN the listen-server host and one client on the same team, with a fixed camera transform and TAA off, WHEN each serializes its presentation model (marker set with positions, prompt list, HUD widget tree) while looking at the same group of opponent and NPC pawns, THEN:
  - the two serialized models are equal apart from own-body entries (screenshots kept as evidence);
  - an instrumentation hook records **zero** calls from presentation code to Roster server queries or pawn `PlayerState`/`Controller`/`Owner` reads on the host;
  - a build-time include check (`StealthGame.Roster.PresentationIncludeCheck`) fails if any UI/HUD/marker/camera/audio-cue module includes the server Roster query header (R6 presentation rule).

---

## Implementation Notes

*Derived from ADR-0007 sections 2, 3, 4 and Implementation Guidelines, ADR-0001 section 4.6 and 5 ("Host parity"), and GDD R6 "Presentation reads only C-TeamView, also on the host (binding rule)":*

- The structural rule (R6): all presentation logic on every machine reads its viewer's C-TeamView and nothing else, including the listen host's local player view (HUD, teammate markers, audio, UI prompts, camera). Presentation never calls the Roster server queries (`IsPawnBound`, `ResolveVictimKind`, `GetRelation`, `GetState` of an opponent) and never reads pawn pointers. On the host the R2 authority guard protects nothing, so the rule is enforced structurally: the server Roster query API lives in `Public/Server/StealthRosterService.h` (first include `StealthServerOnlyGuard.h`), which no presentation module may include.
- Part 1, the build-time include check `StealthGame.Roster.PresentationIncludeCheck`: scan the source of the presentation-side modules for an include of `StealthRosterService.h` (and any other `Public/Server/` Roster header) and fail the build on a hit. Pin the scanned module list in the check (the QA report notes the AC gives none, so the check would be vacuous): `StealthPresentation` (HUD view-models and widgets, Teammate Markers, owner presentation helpers) for certain. Camera and audio-cue code: ADR-0007 places Camera (client) under `StealthGameplay/Core`, which is outside the `#error` guard, so name explicitly which of those paths the check also scans (Open point). Include a positive control: a synthetic offending file or fixture that the check must flag. Complements, does not replace, ADR-0007 lint rules (c), (h) and the Blueprint allowlist audit.
- Part 2, host-versus-client presentation model equality (PIE-2, one process, host plus one same-team client): both serialize their presentation model (marker set with positions, prompt list, HUD widget tree) through a stub serializer while looking at the same group of opponent and NPC pawns with a fixed camera transform and TAA off. The models must be equal apart from own-body entries; define the own-body exclusion (entries keyed to the machine's own pawn) in the test so it is not hand-waved. The real serializer belongs to HUD, Teammate Markers and Camera (see Open point), so use a stub serializer until those epics land.
- Part 3, an instrumentation hook on the host records calls from presentation code to Roster server queries and to pawn `PlayerState`/`Controller`/`Owner` reads; the assertion is zero. The hook is test-only (`WITH_STEALTH_TEST_SEAMS`), never compiled into Shipping.
- Screenshots of both views are retained in `production/qa/evidence/` as extra [UI-ev] evidence; they are never the pass condition.
- Open point: the QA report flags that the presentation-model serializer belongs to HUD, markers and camera and is not tagged [Dep:]. Stub it and name the owners: `production/epics/hud/EPIC.md`, `production/epics/teammate-markers/EPIC.md`, `production/epics/camera-system/EPIC.md`.
- Open point: AC-PSR-42 says any "UI/HUD/marker/camera/audio-cue module" must not include the server query header, but the ADR-0007 compile guard only covers `StealthPresentation`; Camera client code lives in `StealthGameplay/Core` per the ADR-0007 module table. Resolve which module list the include check scans.
- Prototype disposition: no prototype counterpart (`Source/StealthGame/Private/StealthHUD.cpp` reads pawn `IsNpc()` and PlayerState directly; it is replaced, not ported).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 037 (Maximal C-TeamView net-serializes in at most 96 bytes): defines `FTeamView` and the view components presentation reads.
- Story 041 (Replaced controller or reclaim re-pushes C-TeamView within one server frame): controller-swap re-push.
- Story 046 (Non-owned pawns carry null PlayerState, Controller and Owner): wire-level pawn pointer absence.
- Story 047 (No opponent count reaches a client before the round ends): HUD opponent-count absence.
- Story 044 (Public player list is identical before and after eliminations and disconnects): player-list widget.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/StealthRosterService.h` (server query face; guarded by `StealthServerOnlyGuard.h`; created or extended by the facade story, checked here)
- `Source/StealthTestHarness/Private/Roster/StealthRosterHostViewEqualsClientViewTest.cpp` (CQTest `NETWORK_TEST_CLASS(HostViewEqualsClientView, "StealthGame.Roster.PIE")`, exempt from the `F<System><Scenario>Test` class-name rule)
- `Source/StealthTestHarness/Private/Roster/StealthRosterPresentationIncludeCheckTest.cpp` (`StealthGame.Roster.PresentationIncludeCheck`, with a positive control)
- `Source/StealthTestHarness/Private/Roster/StealthPresentationModelStub.h` (stub serializer and call-recording hook)
- `production/qa/evidence/` host and client screenshots (extra [UI-ev] evidence)
- Header names follow ADR-0007: `Stealth` prefix, unique project-wide.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS)**: the presentation-model serializer belongs to HUD, markers and camera (not tagged [Dep:]). The include check lacks a module list so it is vacuous until those modules exist. "Equal apart from own-body entries" needs a defined exclusion. Needs: list the scanned modules, add a positive control, stub the serializer.

**Provisional spec (qa-lead, pending gap resolution)**:

- **AC-1**: the host's presentation model equals a client's.
  - Given: PIE with a host and a same-team client, fixed camera, TAA off, a stub serializer.
  - When: each serializes its presentation model while looking at the same opponent and NPC pawns.
  - Then: models equal apart from own-body entries. The hook records zero presentation-code calls to Roster queries or pawn pointer reads on the host. StealthGame.Roster.PresentationIncludeCheck fails the build if a presentation module includes the server query header.
  - Edge cases: screenshots retained as extra evidence.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated tests `Source/StealthTestHarness/Private/Roster/StealthRosterHostViewEqualsClientViewTest.cpp` (`StealthGame.Roster.PIE.HostViewEqualsClientView`) and `Source/StealthTestHarness/Private/Roster/StealthRosterPresentationIncludeCheckTest.cpp` (`StealthGame.Roster.PresentationIncludeCheck`) OR a documented playtest; `qa.level` is full, so the automated tests are required.
- Extra: retained host and client screenshots in `production/qa/evidence/` (human-reviewed, never the pass condition).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-037-team-view-serialized-size.md`
- Cross-epic: `production/epics/hud/EPIC.md`, `production/epics/teammate-markers/EPIC.md`, `production/epics/camera-system/EPIC.md` (presentation-model serializer); stubbed until they land
- Unlocks: None
