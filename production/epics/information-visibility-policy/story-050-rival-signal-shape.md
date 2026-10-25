# Story 050: The rival progress signal has the R3 shape and a shared release grid

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: [Dep:Objectives] needs objectives-chain FObjectiveTeamView / RivalPrelimCount and its release quantum
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

A delayed sub-stage count of an opponent's objective progress could confirm that a watched hold was real (leak row L34). The R3 matrix therefore allows one revised rival signal, the preliminary-stage count `RivalPrelimCount`, with a fresh per-completion delay on one global release grid shared by both teams, and keeps `RivalStageStep` denied. This story proves the shape of that signal on the C-TeamView schema and its timing statistics over 1,000 pinned sub-seeds. It guards the one place where opponent state is deliberately released.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-001` (`CanViewerKnow` server query, default deny, seven viewer classes)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 5, the carrier model); ADR-0008: Test and automation strategy for leak and parity tests (sections 1 T1, 9)
**ADR Decision Summary**: Each audience has exactly one kind of carrier; team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController, and adding a part or a field is a change request to the Visibility Policy (new fact row and allowlist row), never a local decision. Statistical tests use fixed pre-registered seed sets.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The C-TeamView parts are replicated `UActorComponent`s on the controller's registered sub-object list (ADR-0001 E11) [V]; the PlayerController is relevant only to its owner (E6) [V], so a part reaches exactly one connection. The schema check is reflection-based and the timing check runs on a plain-C++ core; no [U] engine claim is owned by this story.

**Control Manifest Rules (this layer)**:
- Required: **Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController** — source: ADR-0001
- Required: **Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row**; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Required: **Every draw comes from `FSecretStream` (hidden decisions, from `GetServerSubSeed`) or `FIntStream` (public geometry and decoration only)** — source: ADR-0006
- Forbidden: **Never use `FMath::Rand`, `FRandomStream`, `<random>` engines or time-seeded randomness in gameplay or server code** — source: ADR-0006

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-40, scoped to this story:*

- [ ] **AC-IVP-40** [Unit][Dep:Objectives] `StealthGame.Visibility.RivalSignalShape`: GIVEN the C-TeamView schema, WHEN it is checked against R3, THEN no opponent-progress field exists unless the R3 row for a revised signal exists (`RivalStageStep` as specified in Objectives OC-12 is absent); for a revised signal, with 1,000 pinned sub-seeds: the value never exceeds the preliminary-stage count and never changes on the final, every publication time lies on the global release quantum grid shared by both teams, and per-event delays are independent of completer, court and team (KS p > 0.01 against the declared distribution for each team). (R3, L34, OQ4)

---

## Implementation Notes

*Derived from ADR-0001 section 5, ADR-0008 sections 1 and 9 and the GDD R12 row L34:*

- T1 test `StealthGame.Visibility.RivalSignalShape` (class `FVisibilityRivalSignalShapeTest`), in `Source/StealthFoundation/Private/Tests/Visibility/RivalSignalShapeTest.cpp`. Two parts. **Schema**: reflect over the C-TeamView parts (including `UObjectiveTeamViewComponent` / `FObjectiveTeamView` from ADR-0020) and fail if any field carries opponent progress that has no R3 row; `RivalStageStep` must be absent. The only opponent-progress field the R3 matrix allows is the `RivalPrelimCount` row (preliminary-stage count only, never the final step). **Timing**: with 1,000 pinned sub-seeds (derived through `GetServerSubSeed`, entropy injected through `IEntropySource`, ADR-0006), assert that the published value never exceeds the preliminary-stage count and never changes on the final, that every publication time lies on the global release quantum grid shared by both teams, and that per-event delays are independent of completer, court and team (KS p > 0.01 against the declared distribution, for each team).
- The declared distribution and the release quantum come from the Objectives GDD and epic; the GDD R12 row L34 names a global 30 s release grid and the F4 bound `LR_sig <= 2.4` (Objectives OC-12). Read them from the objectives-chain epic's tuning keys, never hardcode them in the test.
- Negative control: the GDD criterion states none (and none is added by the specs for this story). The "schema" part of the criterion is itself a failing case when a field without an R3 row appears; ADR-0008 section 3 still applies, so if the test is implemented with a seeded violation fixture (a schema with a stage-step field), keep it harness-only.
- Statistical test code must use the pinned seeds from a checked-in `constexpr` set (ADR-0008 section 9); no wall-clock time, no unseeded randomness.
- Prototype: not applicable (no objectives code exists). Replaces: none (new).
- Cross-epic needs: objectives-chain epic (`FObjectiveTeamView`, `RivalPrelimCount`, its release quantum and declared delay distribution), participant-state-roster epic (C-TeamView base and schema), tuning-data-config Exit A.
- Unblocks when: the objectives-chain story that delivers `RivalPrelimCount` with its release grid exists.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-011: the R3 matrix fixture itself
- story-014: carrier and audience consistency
- story-053: scope additions 2026-10-25 (`UTeamPingViewComponent`, `bReconnecting`, `TeamSummary`)
- story-051: court ingress parity
- objectives-chain epic: producing the signal and its release grid

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `Source/StealthFoundation/Private/Tests/Visibility/RivalSignalShapeTest.cpp`
- seed set as a `constexpr` array beside the test

---

## QA Test Cases

- **AC-1**: AC-IVP-40 `StealthGame.Visibility.RivalSignalShape` (T1, statistical)
  - Given: the C-TeamView schema and R3, with 1,000 pinned sub-seeds for a revised signal.
  - When: the schema is checked.
  - Then: no opponent-progress field exists unless an R3 row for a revised signal exists (`RivalStageStep` is absent); for a revised signal the value never exceeds the preliminary-stage count and never changes on the final; every publication time lies on the release quantum grid shared by both teams; per-event delays are independent of completer, court and team (KS p > 0.01 against the declared distribution, for each team).
  - Edge cases: the declared distribution and quantum come from the Objectives GDD/epic.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/RivalSignalShapeTest.cpp` (the studio docs' "tests/unit/visibility/") must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-011-matrix-per-fact.md`; objectives-chain epic (`FObjectiveTeamView`, `RivalPrelimCount`, release quantum and declared delay distribution); participant-state-roster epic (C-TeamView base)
- Unlocks: None
