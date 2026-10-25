# Story 034: Presentation modules include no server query headers and call no identity-deriving APIs

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The cheapest way for a client to learn who is a player is to ask the pawn: `IsPlayerControlled`, `IsBotControlled`, `GetPlayerState`, `GetController`, `GetOwner`, or to call the server query `CanViewerKnow`. On a listen host those calls return authoritative objects, so the compile guard alone cannot see the hole. This story extends the ADR-0007 include lint (built by tuning-data-config Exit A) with a build-time include and banned-symbol grep over every UI, HUD, marker, camera and audio-cue module, with a seeded violation per rule as the negative control. It is a Pillar 1 build gate: a presentation file that derives identity fails the build.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-011` (Presentation modules cannot include server query headers)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0007: Code modules and include boundaries (section 3, include lint rule h); ADR-0001: Crowd identity rules (section 4.6, presentation never derives identity)
**ADR Decision Summary**: Three enforcement layers guard the include boundary: the compile guard, the include lint (rule h: `StealthPresentation` source may not use `TActorIterator`, `TActorRange`, `GetAllActorsOfClass`, `GetAllActorsWithInterface`, `GetPlayerControllerIterator`, `GetGameMode`/`GetAuthGameMode`, or `APawn::GetPlayerState`/`GetController` on a pawn other than the local one) and the Blueprint reach audit. Client presentation may not call `IsPlayerControlled()`/`IsBotControlled()`, read a pawn's `PlayerState`, `Controller` or `Owner`, iterate world pawns, or include any `Public/Server/` header.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: C++20 floor and `UE_LOGF` (5.8); `STEALTH_PRESENTATION_MODULE=1` is a `PrivateDefinitions` entry of `StealthPresentation` and reaches every translation unit under unity build and shared/private PCH [V in source; keep the CI self-test, ADR-0007 Verification 1]. No networking API is involved; the lint is a static text and include analysis.

**Control Manifest Rules (this layer)**:
- Required: **The include lint (rules a-j) runs in CI and pre-commit** — source: ADR-0007
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Forbidden: **Never let client or presentation code include a `Public/Server/` tuning header or reference `UStealthTuningData`; never let server code read the raw asset** — source: ADR-0004
- Forbidden: **Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state** in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-17), scoped to this story:*

- [ ] **AC-IVP-17** [Unit] `StealthGame.Visibility.PresentationIncludeCheck`: GIVEN every UI, HUD, marker, camera and audio-cue module, WHEN the build-time include and grep check runs, THEN none includes server query headers (Policy, Roster, Round, LSC, Penalty, Objectives) and none calls `IsPlayerControlled`, `IsBotControlled`, pawn `GetPlayerState`/`GetController`/`GetOwner` or `CanViewerKnow`. (R5, R10; extends AC-PSR-42)

---

## Implementation Notes

*Derived from ADR-0007 sections 2-3 and Implementation Guidelines, ADR-0001 section 4.6, ADR-0008 sections 1, 3 and 8:*

- This is a T0 lint plus a T1 wrapper. The T0 script lives in `tools/ci/lint/` and emits observations only (file:line findings); the gate script decides pass or fail (ADR-0008 section 1, the `.claude/scripts` observation rule). Extend the lint that tuning-data-config Exit A builds (rule h and the module-classification input); do not create a second linter.
- A checked-in module classification file lists every UI, HUD, marker, camera and audio-cue module, so scope is data, not a heuristic. Scope the run to production presentation modules (`StealthPresentation` and the folders the classification names) so the prototype `Source/StealthGame/Private/StealthHUD.cpp` does not fail the gate; the prototype HUD is flagged only once it is in scope, and the HUD and Markers epics replace it.
- Checks: (1) none of the classified modules includes a server query header (Policy, Roster, Round, Local Shared Characters/LSC, Penalty, Objectives; every `Public/Server/` header carries the guard first, ADR-0007 rule b); (2) banned-symbol grep for `IsPlayerControlled`, `IsBotControlled`, pawn `GetPlayerState`/`GetController`/`GetOwner` (the local pawn exception of rule h is spelled out in the lint, not guessed) and `CanViewerKnow`. Server query header names are matched by the classification plus the `Public/Server/` path, so the rule keeps working when new server headers appear.
- Negative control (ADR-0008 section 3: one seeded violation per rule): the suite carries a fixture file per rule (a forbidden include, and each banned symbol) that the lint must flag; if any seeded violation is not flagged the suite fails. Fixtures live under test data, never in a production module, and are not compiled into any runtime module.
- T1 test: `StealthGame.Visibility.PresentationIncludeCheck` (class `FVisibilityPresentationIncludeCheckTest`) runs the lint over fixtures and over the real classified modules and reports through the fault-sink assertion pattern (ADR-0008 section 4). The T1 test does no unmanaged file I/O beyond reading the checked-in fixtures the lint needs; if that conflicts with the T1 no-file-I/O rule, run the lint as a T0 step and keep the T1 test for the classification parse and the seeded fixtures through the script's exit contract.
- Prototype: Replaces: `Source/StealthGame/Private/StealthHUD.cpp:171-174` (world `TActorIterator<AStealthCharacter>` filtered by `IsNpc()`/`GetTeam()`), which the lint flags once in scope; the HUD and Markers epics replace it. Promotes: nothing.
- Cross-epic: needs tuning-data-config Exit A (module skeleton and the include lint). The presentation modules it scans are delivered by the HUD and Teammate Markers epics.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-016: the spectator-state grep check
- story-015: the replicated-property audit (reflection, not presentation includes)
- tuning-data-config Exit A: the base include lint rules a-g and i-j, the compile guard and the Blueprint reach audit
- HUD and teammate-markers epics: replacing the prototype `StealthHUD.cpp` marker world iteration

---

## Files

*The code and test files this story creates or changes (planned):*

- `tools/ci/lint/` rule h extension and the identity-API banned-symbol grep (observations only)
- the checked-in module classification file for presentation modules (`tools/ci/lint/` data file, location fixed by this story and recorded here)
- `tools/ci/lint/` seeded-violation fixtures, one per rule
- `Source/StealthFoundation/Private/Tests/Visibility/PresentationIncludeCheckTest.cpp` (T1, `FVisibilityPresentationIncludeCheckTest`, `StealthGame.Visibility.PresentationIncludeCheck`)

*Note: the `StealthFoundation` module folder and the base lint are created by tuning-data-config Exit A (they do not exist yet).*

---

## QA Test Cases

- **AC-1**: Presentation include check (AC-IVP-17)
  - Given: every UI, HUD, marker, camera and audio-cue module, listed in a checked-in module classification.
  - When: the include and grep check runs.
  - Then: none includes server query headers (Policy, Roster, Round, LSC, Penalty, Objectives); none calls `IsPlayerControlled`, `IsBotControlled`, pawn `GetPlayerState`, `GetController` or `GetOwner`, or `CanViewerKnow`; a seeded violation per rule is flagged or the suite fails.
  - Edge cases: scope the run to production presentation modules so the prototype `StealthHUD.cpp` does not fail the gate; the script reports findings and the gate script decides.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/PresentationIncludeCheckTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: tuning-data-config Exit A (module skeleton, compile guard and include lint)
- Unlocks: None
