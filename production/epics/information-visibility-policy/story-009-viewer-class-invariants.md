# Story 009: Viewer classes follow the R2 order and class invariants

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Everything the Policy decides starts from one question: which of seven classes is this viewer, relative to this subject, right now? The class is computed server-side from the Roster (team and state of the viewer's participant, participant of the subject) and Round's phase, in a fixed order, and it must follow the server connection (never client input) so rejoin and `RebindView` keep it. This story creates the viewer-class computation as a plain-C++ core with a consumer-owned roster-query interface, tested with fixture rosters, so the matrix, the default-deny query and every later audience decision have one tested foundation. Informant status must never change what an opponent or teammate may know about another subject (art bible 5.2).

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-001` (`CanViewerKnow` server query, default deny, seven viewer classes)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 5 carriers, R2 classes); also ADR-0007: Code module layout, composition root and include boundaries (plain-C++ cores)
**ADR Decision Summary**: Each audience has exactly one carrier, and a carrier holds only its audience; the Policy computes the viewer class from Roster and Round state by relation, not by alive/informant status, as a server-side query that is compiled into the carriers. Queue/Roster-style logic lives in plain C++ cores (`F...Core`) with no `UWorld` dependency behind a subsystem facade.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure C++ logic; no post-cutoff API. The engine-facing parts (viewer cache keyed on the server connection) arrive with story-023 and the cull stories. Keep the core free of `UObject`/`UWorld` so it runs in `-nullrhi` T1 tests (ADR-0008).

**Control Manifest Rules (this layer)**:
- Required: **Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F...Core`) with no `UWorld` dependency, behind a subsystem facade** — source: ADR-0007
- Required: **Every `Public/Server/` header includes `StealthServerOnlyGuard.h` first** — source: ADR-0007
- Forbidden: **Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state** in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001
- Guardrail: **Tests**: per-commit pipeline ≤ 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-04):*

- [ ] **AC-IVP-04** [Unit] `StealthGame.Visibility.ViewerClassInvariants`: GIVEN fixture rosters covering every state, relation and phase, WHEN classes are computed, THEN the R2 order holds (`Ended` → `PostRound` first, unbound → NPC row, then Self, Teammate, Opponent); `TeammateInformant ≡ Teammate` and `OpponentInformant ≡ Opponent` for every fact about another subject; the class follows the server connection across rejoin and `RebindView`; client-supplied team, id or class is ignored.

---

## Implementation Notes

*Derived from the GDD rules R1-R2 and ADR-0001 section 5, ADR-0007 sections 2, 4, 5:*

- R2 order (first match wins): round phase `Ended` gives `PostRound` for every viewer; a subject that is an unbound pawn (an NPC) gives the NPC row, with no class computed (so no code path in which an NPC answer depends on the viewer); viewer participant equals subject participant gives `SelfActive` or `SelfInformant`; same team gives `Teammate` or `TeammateInformant`; otherwise `Opponent` or `OpponentInformant`.
- Class invariants: for every fact about another subject, `TeammateInformant == Teammate` and `OpponentInformant == Opponent`. `SelfInformant` differs from `SelfActive` only in owner-HUD presentation, not in knowledge.
- Viewer is a connection's participant keyed on the server connection (`FViewerId`), never client-supplied team, id or class. The Roster supplies `GetTeam`, `GetState`, `GetParticipant` through a small consumer-owned interface declared by the Policy (ADR-0007 section 4); this story's tests use a fixture implementation, so it does not wait for the production Roster store.
- Files go in `StealthFoundation`: `Public/Server/` for the server-only types (with the `StealthServerOnlyGuard.h` include first, `NotBlueprintType, NotBlueprintable` on any UCLASS), `Private/Visibility/` for the core. Header names carry the `Stealth` prefix and are unique across modules.
- Prototype: replaces the client-side `IsNpc()` kind detection (`Source/StealthGame/Private/StealthCharacter.cpp:138`); nothing is promoted.
- Cross-epic: needs the tuning-data-config Exit A module skeleton, and the participant-state-roster query contract (`GetTeam`/`GetState`/`GetParticipant`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-010: `CanViewerKnow` default deny and the fault-sink report
- story-011: the R3 matrix (fact x class)
- story-012: the NPC-row invariance
- story-023: the PlayerController-to-viewer cache and the cull predicate

---

## Files

- `Source/StealthFoundation/Public/Server/StealthVisibilityPolicy.h` (class enum, `FViewerId`)
- `Source/StealthFoundation/Private/Visibility/StealthViewerClassCore.h` / `.cpp`
- `Source/StealthFoundation/Private/Tests/Visibility/ViewerClassInvariantsTest.cpp` (test `StealthGame.Visibility.ViewerClassInvariants`, class `FVisibilityViewerClassInvariantsTest`)

*(The module folders do not exist yet; tuning-data-config Exit A creates them.)*

---

## QA Test Cases

- **AC-1**: ViewerClassInvariants (AC-IVP-04)
  - Given: fixture rosters covering every state, relation and phase, through a fixture roster-query interface.
  - When: viewer classes are computed.
  - Then: R2 order holds (Ended gives PostRound first, then unbound gives the NPC row, then Self, Teammate, Opponent); TeammateInformant == Teammate and OpponentInformant == Opponent for every fact about another subject; the class follows the connection across rejoin and RebindView; client-supplied team, id or class is ignored.
  - Edge cases: Ended combined with an unbound subject; a fixture whose client-claimed team contradicts the server's. (Tier: T1.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/ViewerClassInvariantsTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None in this epic; tuning-data-config Exit A (module skeleton, harness); participant-state-roster epic (roster query contract)
- Unlocks: `production/epics/information-visibility-policy/story-010-query-default-deny.md`, `production/epics/information-visibility-policy/story-011-matrix-per-fact.md`, `production/epics/information-visibility-policy/story-023-relevancy-truth-table.md`, `production/epics/information-visibility-policy/story-036-post-round-latch-release.md`, `production/epics/information-visibility-policy/story-038-kill-confirmation-parity.md`
