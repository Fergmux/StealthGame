# Story 024: IsConcealedFrom matches a checked-in truth table and is server-only

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R10, AC-CON-05)
**Requirement**: `TR-con-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§7 per-viewer result)
**ADR Decision Summary**: `IsConcealedFrom(FViewerId, Pawn)` = `State ∈ {Concealed} ∪ {Exiting, Emerging before Cross}` (or killed while concealed, until the death write) ∧ `¬CanViewerKnow(Viewer, Pawn, Position)`; server-only. It is true for every non-ally of a concealed player (active or informant viewers alike: informants have no x-ray) and for every viewer of a concealed NPC; co-occupants who are not allies do not see each other.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; plain C++ over the Policy's `CanViewerKnow`)
**Engine Notes**: No engine API. Any server-side sight or perception check on a pawn reads this function (CON-R16). The function lives under `Public/Server/` and must not be compiled into a client module (include lint).

**Control Manifest Rules (this layer)**:
- Required: `IsConcealedFrom` is server-only; any server-side sight or perception check reads it — source: ADR-0017
- Forbidden: No "same-bush" clause: the relevancy rule stays `owner ∨ ally ∨ ¬concealed` — source: ADR-0017
- Guardrail: O(1) per query

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-05, CON-R10), scoped to this story:*

- [ ] **AC1** (AC-CON-05, [U] `StealthGame.Concealment.IsConcealedFromTruthTable`): **GIVEN** {player, NPC} × {Outside, Entering, Concealed, Exiting, Emerging} × {owner, ally, opponent, informant-opponent}, plus a separate killed-while-concealed row group, **THEN** `IsConcealedFrom` equals a checked-in hand-written expected table (not computed from `CanViewerKnow`): true for every non-ally of a player and every viewer of an NPC exactly in Concealed and in Exiting/Emerging before the crossing; false elsewhere, including every owner-viewer row; killed-while-concealed rows are true for non-allies until the death write and false at it.
- [ ] **AC2** (CON-R10): **GIVEN** two rival co-occupants in Concealed **THEN** `IsConcealedFrom(A, B)` and `IsConcealedFrom(B, A)` are both true; for two allies both are false; an informant viewer gets the same result as an active opponent (no x-ray, IVP R9a); the function is not compiled into any client module (include lint).

---

## Implementation Notes

*Derived from ADR-0017 Decision §7 and CON-R10:*

The expected table is checked in as a fixture and written by hand from the GDD, deliberately not generated from `CanViewerKnow`, so the test cannot pass by mirroring the implementation. The table marks the not-applicable cells (NPC × owner and NPC × ally) explicitly. A rival co-occupant mid-Exiting still reads true until its crossing. `CanViewerKnow` and the viewer-class model come from the Visibility Policy (IVP `story-010-query-default-deny.md`); the Policy's own checks are not duplicated here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-026-per-viewer-visibility-pie.md`: the observed two-client co-occupant and informant checks.
- Information Visibility Policy epic: `CanViewerKnow` itself.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentQueries.cpp`
- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentService.h` (`IsConcealedFrom`)
- `Source/StealthGameplay/Private/Tests/Concealment/IsConcealedFromTruthTableTest.cpp` (class `FConcealmentIsConcealedFromTruthTableTest`, name `StealthGame.Concealment.IsConcealedFromTruthTable`) with the checked-in table fixture

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS on AC2 of the first draft; revised as above, re-check ADEQUATE).*

- **AC1**: IsConcealedFromTruthTable
  - Given: a hand-written checked-in table of {player, NPC} × {Outside, Entering, Concealed, Exiting, Emerging} × {owner, ally, opponent, informant-opponent}.
  - When: `IsConcealedFrom` is evaluated per row, plus a separate killed-while-concealed row group.
  - Then: true for non-allies of a player, and for every viewer of an NPC, exactly in Concealed and in Exiting/Emerging before the crossing; false in all other rows, including every owner-viewer row; killed-while-concealed rows are true for non-allies until the death write.
  - Edge cases: the table marks the not-applicable NPC × owner and NPC × ally cells explicitly; the row at the death write is false (un-culled).
- **AC2**: RivalsAndInformants
  - Given: two rival co-occupants in Concealed, then two allies, then an informant viewer.
  - When: `IsConcealedFrom` is called in both directions.
  - Then: rivals give true in both directions, allies give false in both, the informant gets the same result as an active opponent; the function is not compiled into any client module (include lint).
  - Edge cases: a rival co-occupant mid-Exiting still reads true until its crossing.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `IsConcealedFromTruthTableTest.cpp` under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-003-conceal-state-machine-and-occupancy-record.md`; IVP `production/epics/information-visibility-policy/story-010-query-default-deny.md`
- Unlocks: `story-026-per-viewer-visibility-pie.md`, `story-033-blind-strike-parity-and-kill-timing.md`
