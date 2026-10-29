# Story 018: A pawn struck while Entering dies as a visible pawn before the commit and as a spot candidate after it

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R5 no invulnerable window, Edge Cases, AC-CON-25)
**Requirement**: `TR-con-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§3 Commit, generation drop), with ADR-0018 (strike resolution against live state at its slot)
**ADR Decision Summary**: Before the commit an Entering pawn is an ordinary visible pawn (a `TargetPawn` or open-spot candidate); a strike kills it visibly, its Commit item is dropped by generation id and the client's pending swap is cancelled. From the commit it is a bush-spot candidate. Being Busy until +0.9 s grants no immunity, and a hider already in the same bush can strike the entrant either way.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; plain C++ over a stub strike resolver)
**Engine Notes**: The test uses a stub strike resolver seam from the IIF/Round test seams; the PIE form (two clients, cancelled client swap) is covered by story 033.

**Control Manifest Rules (this layer)**:
- Required: A stale generation drops its transition item with no effect — source: ADR-0017
- Forbidden: Never grant an invulnerable window while Entering — source: ADR-0017, GDD CON-R5
- Guardrail: Same-tick ordering is the queue's (due, seq)

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-25), scoped to this story:*

- [ ] **AC1** (AC-CON-25, [U] `StealthGame.Concealment.NoImmunityWhileEntering`): **GIVEN** a pawn struck by `TargetPawn` at +0.3 s of `BushEnter` (before commit) **THEN** it dies as a visible pawn, no record or cull is created, its Commit item is dropped by generation id and the pending swap is cancelled (the fall is visible); **GIVEN** a strike at +0.6 s (after commit) **THEN** it resolves as a spot candidate in the record.
- [ ] **AC2** (AC-CON-25 tail): **GIVEN** a hider already in the same bush **THEN** it can strike the entrant at +0.3 s (as a visible `TargetPawn`) and at +0.6 s (as a spot candidate), even though the entrant's Hide is Busy until +0.9 s.

---

## Implementation Notes

*Derived from ADR-0017 Decision §3 and the GDD edge cases:*

Kill before commit: state Entering → Outside with no record, the Commit item drops by generation id, and the cancelled swap is signalled so clients cancel their pending hide (CON-R6b rule 4: death before the commit cancels the pending swap). A strike on the same tick as Commit (+0.5 s) follows queue order by (due, seq): kill first gives an ordinary kill of a visible pawn, commit first gives a concealed pawn and a `TargetPawn` naming it falls back to its `WorldPoint`, which lies in the zone (F4 decides). The implementer confirms and covers that ordering as an edge test.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-033-blind-strike-parity-and-kill-timing.md`: the PIE two-client visible-fall check.
- assassination-defence-reversal epic: the real strike resolver.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentCore.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/NoImmunityWhileEnteringTest.cpp` (class `FConcealmentNoImmunityWhileEnteringTest`, name `StealthGame.Concealment.NoImmunityWhileEntering`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: NoImmunityWhileEntering (verbatim AC-CON-25)
  - Given: a pawn Entering.
  - When: it is struck at +0.3 s and in a separate run at +0.6 s (stub resolver, IIF story 095).
  - Then: at +0.3 s it dies as a visible pawn with no record, no cull call, the Commit item dropped by generation and the swap cancelled; at +0.6 s it resolves as a spot candidate in the record.
  - Edge cases: a strike on the same tick as Commit (+0.5 s) is ordered by (due, seq); confirm the ordering.
- **AC2**: HiderStrikesEntrant
  - Given: a hider already in the bush.
  - When: it strikes the entrant at +0.3 s and at +0.6 s.
  - Then: both strikes land (first as a visible TargetPawn, second as a spot candidate), even though the entrant's Hide is Busy until +0.9 s.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `NoImmunityWhileEnteringTest.cpp` under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [x] Created and passing (2026-10-29): StealthGame.Concealment.NoImmunityWhileEntering; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: N/A - headless core rule (no player-observable surface of its own); the integrated behaviour is observed in the batch's 3-player PIE run (con-*.png)

---

## Dependencies

- Depends on: `story-004-hide-enter-execution-and-commit.md`, `story-015-spot-zone-and-occupants.md`, `story-017-kill-while-concealed-server-rules.md`; IIF `production/epics/input-interaction-framework/story-095-command-test-seams.md`
- Unlocks: `story-033-blind-strike-parity-and-kill-timing.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1-AC2 passing.
**Deviations**: None.
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
