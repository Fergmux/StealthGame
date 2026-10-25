# Story 011: Every fact answers exactly as the R3 matrix says for every viewer class

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The R3 visibility matrix is the heart of the Policy: about 40 fact rows (position, possession pointers, `IsPlayer`, `IsConcealed`, `StrikeCount`, `WrongTargetTally`, `T_due`, kill confirmation, objective progress, `TeamPing`, the clock-exchange and reconnecting rows and more) against the classes SA, SI, TM(I), OP(I), NPC row, PR, each Y, N, E (PostRoundEligible) or not applicable. This story encodes the matrix as the Policy's fact table (data, not scattered `if`s) and proves per fact that code and the checked-in fixture agree, including that an `E` cell is false before `Ended` and gated by release class in `Ended`. Nothing downstream may re-derive a rule; a system that needs a new fact raises a change request and a new row.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-001` (`CanViewerKnow` server query, default deny, seven viewer classes)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 5 carrier model); also ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Each audience (`All`, `Team`, `Owner`, `ServerOnly`) has exactly one carrier kind and a carrier holds only its audience; a new fact, audience or carrier needs a Visibility Policy row and a signed-off allowlist row before it ships.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure data and C++ logic; no post-cutoff API. The fixture is checked-in data read by the T1 test (no file I/O in other T1 tests, ADR-0008 section 9; the allowlist file is the one data-file exception, so prefer a `constexpr` fixture for the matrix).

**Control Manifest Rules (this layer)**:
- Required: **Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row**; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Forbidden: **Never put team, state, connection, appearance or pawn data on a PlayerState; never give NPCs a PlayerState** — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-07):*

- [ ] **AC-IVP-07** [Unit] `StealthGame.Visibility.MatrixPerFact`: GIVEN the R3 matrix fixture, WHEN every fact × {SA, SI, TM, TMI, OP, OPI, PR} is evaluated for a bound subject, THEN Y cells return `true`, N cells `false`, E cells `false` before `Ended` and gated by release class in `Ended`; a fixture/code disagreement fails.

---

## Implementation Notes

*Derived from the GDD rule R3 and ADR-0001 section 5:*

- Encode the matrix as a table in code (fact id, per-class cell, audience, carrier) and a separate checked-in fixture that the test reads; a disagreement between the two fails. The fact rows are the R3 table in the GDD (position visible and concealed, possession pointers, `IsPlayer`, `TeamMembership`, `IsConcealed`, `IsInformant`, `StrikeCount`, `WrongTargetTally`, `T_due`, `RetireAtServerTime`, `DefenceState`, kill confirmation, `FActionState.StartServerTime`, `FDeathState.bDiedConcealed`, objective rows, `ConnectionState`, `TeammateReconnecting`, `TeammateReconnectEndCause`, `TeamPing`, opponent counts, `RoundTeamSummary`, public structs and the rest).
- E cells (`PostRoundEligible`) are false before `Ended` and, in `Ended`, gated by release class (MVP releases only `FRoundResult`).
- Cells marked "—" return false with no fault report (story-010). The NPC row is the `All` facts only (story-012).
- Negative control (added by the story, ADR-0008 section 3): a test-only copy of the fixture with one cell flipped must make the test fail.
- Files in `StealthFoundation` (`Public/Server/` with the guard include first; `Private/Visibility/`). A new fact needs both the table row and an allowlist row (story-013).
- Cross-epic: some fact rows name carriers delivered by other epics (C-TeamView, `FRoundPublicState`); the table lists them by name, the carriers are built there.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-009 / story-010: classes and default deny
- story-012: NPC row viewer-independence
- story-013 / story-014: the allowlist and the carrier consistency check
- the carriers themselves (participant-state-roster, round-match-framework epics)

---

## Files

- `Source/StealthFoundation/Public/Server/StealthVisibilityFacts.h` (fact ids, cell enum)
- `Source/StealthFoundation/Private/Visibility/StealthVisibilityMatrix.cpp`
- `Source/StealthFoundation/Private/Tests/Visibility/MatrixPerFactTest.cpp` (test `StealthGame.Visibility.MatrixPerFact`, class `FVisibilityMatrixPerFactTest`) and a matrix fixture header under `Private/Tests/Data/`

*(The module folders do not exist yet; tuning-data-config Exit A creates them.)*

---

## QA Test Cases

- **AC-1**: MatrixPerFact (AC-IVP-07)
  - Given: the checked-in R3 matrix fixture and the Policy fact table.
  - When: every fact crossed with {SA, SI, TM, TMI, OP, OPI, PR} is evaluated for a bound subject.
  - Then: Y cells return true, N cells false; E cells false before Ended and gated by release class in Ended; a fixture/code disagreement fails.
  - Edge cases: negative control (added by the story): a test-only copy of the fixture with one cell flipped must fail; cover E cells both before and during Ended. (Tier: T1.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/MatrixPerFactTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-009-viewer-class-invariants.md`, `production/epics/information-visibility-policy/story-010-query-default-deny.md`
- Unlocks: `production/epics/information-visibility-policy/story-012-npc-row-viewer-independent.md`, `production/epics/information-visibility-policy/story-014-carrier-audience-consistency.md`, `production/epics/information-visibility-policy/story-036-post-round-latch-release.md`, `production/epics/information-visibility-policy/story-038-kill-confirmation-parity.md`, `production/epics/information-visibility-policy/story-050-rival-signal-shape.md`, `production/epics/information-visibility-policy/story-051-court-ingress-parity.md`, `production/epics/information-visibility-policy/story-053-scope-additions-2026-10-25.md`
