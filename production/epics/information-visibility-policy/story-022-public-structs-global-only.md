# Story 022: Public structs are global only: no field is keyed by team or participant and none varies with opponent counts or objective progress

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs FRoundPublicState (round-match-framework) and the tuning-data-config public snapshot (tuning-data-config Exit B) to exist for the reflection check; not a spike
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Structs that replicate to every client (`FRoundPublicState` on the GameState and the tuning client snapshot) are the All-audience carrier, so a single per-team array or an opponent count inside one would hand every client, including opponents, a signal about the other team. This story makes that checkable: a reflection check that no field is keyed by team or participant, and a mutation check that changing opponent counts or objective progress leaves the serialised structs byte-identical. It enforces the ADR-0001 carrier rule that a public struct carries no per-participant and no per-team value, and it complements the runtime check AC-PSR-41 owned by participant-state-roster.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-002` (One carrier kind per audience)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 5, carrier model); also ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 3); ADR-0003: Replication, relevancy and the concealment cull hybrid (section 6)
**ADR Decision Summary**: `All` audience data travels on the pawn, in C-Public on `AStealthPlayerState`, and in GameState public structs (`FRoundPublicState`, tuning client snapshot, `FLayoutManifest`); a public struct carries no per-participant and no per-team value. `FRoundPublicState` is the only All-audience round struct (ADR-0003 section 6).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Reflection over `USTRUCT` fields (`FProperty` iteration, array and map property types) is pre-cutoff and stable; ADR-0008 section 1 places reflection audits in T1 on a test-created world. No unreported spike owns a claim here. Serialisation byte-identity is measured on the struct's own serialiser, not on the wire (wire parity is AC-IVP-02, story-044).

**Control Manifest Rules (this layer)**:
- Required: **Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row**; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Forbidden: **Never write a non-zero `TeamSummary` (or any per-team count) into `FRoundPublicState` before `Ended`** — source: ADR-0002 (user decision 2026-10-25)
- Required: **Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController** — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-21** [Unit] `StealthGame.Visibility.PublicStructsGlobalOnly`: GIVEN `FRoundPublicState` and the TDC public snapshot, WHEN fields are enumerated, THEN none is keyed by team or participant and none varies with opponent counts or objective progress. (R4, L24, L26; runtime check AC-PSR-41) Clarification (story-level, QL-STORY-READY): two checks: (a) reflection: no field is a per-team or per-participant array/map or a team-id/participant-id typed field; (b) mutation: changing opponent counts or objective progress leaves the serialised `FRoundPublicState` and TDC public snapshot byte-identical.

---

## Implementation Notes

*Derived from ADR-0001 section 5, ADR-0003 section 6 and ADR-0008 sections 1 and 3:*

- **Check (a), reflection.** Enumerate the fields of `FRoundPublicState` and the TDC public snapshot by reflection and fail on any per-team or per-participant array or map, and on any team-id or participant-id typed field. Detect arrays and maps by key or element type, not by meaning; an array of any other type goes to the allowlist review rather than failing silently.
- **Check (b), mutation.** Build both structs from a fixture, mutate opponent counts and objective progress (each input separately and together), re-serialise, and require byte-identical output. The post-round `FRoundResult.TeamSummary` is published only in the round-end step and is all zero before (ADR-0002 amendment); the fixture covers the pre-`Ended` state.
- **Negative control (added by the story, ADR-0008 section 3).** A harness-only fixture struct with one team-keyed field must be detected by check (a), and a fixture whose serialised bytes change with an opponent count must be detected by check (b); the suite passes only when both controls fail their inner checks. The criterion text stays the GDD's. Control types live only in `StealthTestHarness` or a `#if WITH_DEV_AUTOMATION_TESTS` block.
- **Placement.** Tier T1: class `FVisibilityPublicStructsGlobalOnlyTest`, name `StealthGame.Visibility.PublicStructsGlobalOnly`, in `Source/StealthFoundation/Private/Tests/Visibility/`. Because the tuning snapshot belongs to tuning-data-config and `FRoundPublicState` to round-match-framework, the test references them through their `Public/View/` declarations only.
- **Prototype disposition.** None promoted; the prototype has no public round or tuning struct of this shape.
- **Spike gates.** None.
- **Unblocks when.** round-match-framework delivers `FRoundPublicState`; tuning-data-config Exit B delivers the public snapshot.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-021: C-TeamView never reaching opponent connections
- story-053: scope additions (`TeamSummary`, ping and reconnect fields in the audit)
- story-015: the replicated-property audit against the allowlist
- participant-state-roster epic: runtime check AC-PSR-41 (opponent counts never replicated)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Visibility/PublicStructsGlobalOnlyTest.cpp` (class `FVisibilityPublicStructsGlobalOnlyTest`, test `StealthGame.Visibility.PublicStructsGlobalOnly`)
- Harness-only negative-control fixture structs (team-keyed field; opponent-count-dependent bytes), inside the test file's `WITH_DEV_AUTOMATION_TESTS` block or `StealthTestHarness`

---

## QA Test Cases

- **AC-1**: Public structs are global only (AC-IVP-21)
  - Given: `FRoundPublicState` and the TDC public snapshot.
  - When: (a) fields are enumerated by reflection, and (b) opponent counts and objective progress are mutated.
  - Then: (a) no field is a per-team or per-participant array or map, or a team-id or participant-id typed field; (b) the serialised `FRoundPublicState` and TDC public snapshot are byte-identical after the mutation.
  - Edge cases: for (a) detect arrays and maps by key or element type, not by meaning; an array of any other type goes to the allowlist review; mutate each input separately and together; negative control (added): a fixture struct with one team-keyed field must be detected. (Tier: T1.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/PublicStructsGlobalOnlyTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: round-match-framework epic story delivering `FRoundPublicState`; tuning-data-config Exit B (the public tuning snapshot); tuning-data-config Exit A (module skeleton, test harness)
- Unlocks: None
