# Story 012: Answers about an unbound subject never depend on who asks

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Pillar 1 in one invariant: for every fact, what an opponent may learn about a bound (player) pawn must be exactly what anyone may learn about an NPC. The NPC row is computed without a viewer class at all (R2 step 2), so there is no code path where an NPC answer varies by viewer, and the `Opponent` answer about a bound pawn must equal that row. If a fact cannot meet the parity invariant it must be `ServerOnly` for opponents. This story proves both directions on top of the matrix of story-011.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-001` (`CanViewerKnow` server query, default deny, seven viewer classes)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (one pawn class, identity blindness; section 1 and 5)
**ADR Decision Summary**: Every body is one `AStealthPawn` class driven through one command path; a pawn carries only `All` facts, identical in type, path, condition and timing for every pawn kind; nothing another client can observe may depend on player-ness.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure data and C++ logic; no post-cutoff API.

**Control Manifest Rules (this layer)**:
- Forbidden: **Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state** in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Forbidden: **Never add an NPC subclass, an NPC-only component, a lightweight NPC pawn or a Mass crowd representation** — every difference is a Pillar-1 detector — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-08):*

- [ ] **AC-IVP-08** [Unit] `StealthGame.Visibility.NpcRowViewerIndependent`: GIVEN an unbound subject, WHEN every fact is queried from every class, THEN all answers are identical and equal exactly the `All` facts; for every fact the `Opponent` answer about a bound pawn equals the NPC-row answer. (R2 parity invariant)

---

## Implementation Notes

*Derived from the GDD rule R2 and ADR-0001 sections 1 and 5:*

- The set of `All` facts comes from story-011's fixture; the NPC-row answer is exactly that set for every viewer class. Query every fact from every class for an unbound subject and assert identical answers; then, for a bound pawn, assert the `Opponent` (and `OpponentInformant`) answer equals the NPC-row answer for every fact.
- Negative control (added by the story, ADR-0008 section 3): a test-only variant of the table where one fact differs by viewer class (or `Opponent` differs from the NPC row for one fact) must be detected by the same assertions.
- A fact that cannot meet parity is `ServerOnly` for opponents (R2 parity invariant); the fix is a table change plus change request, not a test exemption.
- Files in `StealthFoundation`; the test reads the story-011 table.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-011: the matrix itself; story-009: class computation
- story-054 / story-018: pawn-level net-config parity

---

## Files

- `Source/StealthFoundation/Private/Tests/Visibility/NpcRowViewerIndependentTest.cpp` (test `StealthGame.Visibility.NpcRowViewerIndependent`, class `FVisibilityNpcRowViewerIndependentTest`)
- Any small accessor added to `Source/StealthFoundation/Private/Visibility/StealthVisibilityMatrix.cpp`

*(The module folders do not exist yet; tuning-data-config Exit A creates them.)*

---

## QA Test Cases

- **AC-1**: NpcRowViewerIndependent (AC-IVP-08)
  - Given: an unbound subject and a bound pawn.
  - When: every fact is queried from every viewer class.
  - Then: all answers about the unbound subject are identical and equal exactly the All facts; for every fact the Opponent answer about a bound pawn equals the NPC-row answer.
  - Edge cases: the All-fact set comes from the story-011 fixture; negative control (added): a variant where one fact differs by viewer class must be detected. (Tier: T1.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/NpcRowViewerIndependentTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-011-matrix-per-fact.md`
- Unlocks: None
