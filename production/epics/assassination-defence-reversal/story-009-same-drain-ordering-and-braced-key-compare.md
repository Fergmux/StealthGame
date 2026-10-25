# Story 009: Braced at the Attack by key compare, and same-drain ordering

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R2, ASN-R5, ASN-R13; Formulas F2, F4; Edge Cases "Same-drain ordering"; AC-ASN-04, AC-ASN-19)
**Requirement**: `TR-asn-002`, `TR-asn-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging (§3 total order, §5 inline, §8 latch)
**ADR Decision Summary**: One total-order queue keyed `(ServerTickIndex, Seq)` is drained in `TG_PostPhysics`; ties never use wall-clock; after the latch every later item is `Rejected_Latched`, and a command whose attacker is no longer `Active` is voided `NotNow`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002; pure ordering logic over item keys)
**Engine Notes**: Tests use the stub Round queue and injected clock of ADR-0008 (Round test seams, stories 001 to 004 in the Round epic), so ordering is asserted by key, never by real time. No engine API in the Assassination logic.

**Control Manifest Rules (this layer)**:
- Required: Braced is computed from key order only: `Braced(V, k_A) = [Key(Begin_V) < k_A] ∧ ¬[Key(End_V) < k_A] ∧ (Slot(V) = Bracing)`, with `Key(End_V) = min(Key(DebounceExpiry), Key(CapEnd))` — source: ADR-0018, GDD F2
- Required: Equal-tick order is Round's stamping, never an Assassination tie rule — source: ADR-0002
- Forbidden: Never compare wall-clock times inside a drain; never reschedule a `Rejected_Latched` item — source: ADR-0002, ADR-0016
- Guardrail: O(1) per Attack

A defender is *braced* for an Attack iff, at the Attack's slot, its IIF slot state is `Bracing`: its `Begin` key is before the Attack's and its window-ending item (the debounce expiry or `CapEnd`, whichever is first) is not. A pending `End` inside its debounce still protects. Nothing during the strike animation can change the outcome, so there is no parry window (anti-pillar "not a reflex game"). This story also pins the same-drain interleavings: mutual strikes, two attackers on one victim, a strike racing a final completion, and a braced attacker.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-04, AC-ASN-19), scoped to this story:*

- [ ] **AC1** (AC-ASN-04, [Logic/BLOCKING] `StealthGame.Assassination.BracedAtAttackKeyOrder`): **GIVEN** a defender's `Prepare(Begin)` at key (1000, 4) and its `DebounceExpiry` at (1124, 2) **WHEN** Attacks resolve at (1124, 1) and, in a separate run, at (1124, 3) **THEN** the first is a reversal and the second is a kill; **AND** an Attack keyed before the Begin in the same tick is a kill and the Begin is then voided `NotNow` with no cooldown stamp.
- [ ] **AC2** (AC-ASN-19, [Logic/BLOCKING] `StealthGame.Assassination.SameDrainOrdering`): **GIVEN** stub items in one drain **THEN** (a) A→B (key 3) and B→A (key 7): A kills B and B's Attack is `Voided(NotNow)`; (b) A1→B (key 3) and A2→B (key 7): A1 kills B and A2 is `NoContact`; (c) a final completion (key 3) and a strike (key 7): the strike is `Rejected_Latched` with no lockout and no `FStrikeResult`; (d) a braced A strikes braced B: A is reversed.

---

## Implementation Notes

*Derived from GDD F2, F4 and ADR-0002 §3, §5, §8:*

The key compare uses Round's `Compare` on `(ServerTickIndex, Seq)`; the Begin and End items are the IIF's `Prepare` items (IIF story 050, `HoldTimerKeyOrder`, covers the hold-timer side). If the Attack precedes the `Begin` in the same tick the victim is not `Bracing` at the slot, the Attack kills (ASN-R5), and the later `Begin` is voided `NotNow` because the victim is no longer `Active`, writing no `LastPrepareActivation` (story 003 stamp stays `none`).

Interleavings (F4): `Outcome(Attack_k) = Whiff if V = ∅; Reversal if Braced(V, k); Kill otherwise`, with V resolved on the state after every item with key < k. (a) The lower key resolves; the other attacker is no longer `Active`, so its Attack is `Voided(NotNow)` at its slot and no lockout, pose or `FStrikeResult` results. (b) A2's `PawnRef` names a `Dead` pawn (the role is `Dead` from A1's slot, story 005), so it is a terminal whiff. (c) The latch step publishes nothing; the later strike returns `Rejected_Latched`, whose owner reply is `NotNow` through the held-reply buffer, with no outcome and no pose. (d) `Attack` preempts the attacker's own brace (`Superseded`) at execution, so a braced pawn striking a braced defender is reversed; bracing never protects a striker.

The per-Attack lifecycle states are `Submitted → Resolving → Executed(Kill | Reversal | Whiff)`, `Voided(NotNow)` or `Rejected_Latched`; log them in the Event Log rows `KillResolved` and `Reversal` only (story 007), never per void.

**Prototype replaced:** the prototype resolves in arrival order on the game thread (`HandleAssassinationRequest`) and tests `bPreparingDefence` live, so two same-tick strikes were decided by network arrival; key order replaces it. Networked prototype tests A, C and F remain the acceptance reference.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-attack-outcomes-resolved-in-slot.md`: the effects of each outcome.
- `story-010-reversal-counter-and-chained-strikes.md`: the counter, the second attacker on a countering defender, a striker struck mid-lunge.
- IIF stories 050 and 053: hold-timer key order and "brace sampled at execution" at the IIF level.
- Round epic: the queue, `Compare`, the latch and `Rejected_Latched` itself.

---

## Files

- `Source/StealthGameplay/Private/Features/Assassination/BracedAtAttackCore.h` and `.cpp` (`FBracedAtAttackCore`, key compare, plain C++)
- `Source/StealthGameplay/Private/Features/Assassination/AttackResolverCore.cpp` (void and latch handling; shared with story 007)
- `Source/StealthGameplay/Private/Tests/Assassination/BracedAtAttackKeyOrderTest.cpp` (class `FAssassinationBracedAtAttackKeyOrderTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/SameDrainOrderingTest.cpp` (class `FAssassinationSameDrainOrderingTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: the Attack key before `Begin` and the `CapEnd` key are not given; proposed rewrite (unconfirmed): `CapEnd` keyed after (1124,2); the Attack keyed (1000,3) kills, and `Begin` (1000,4) is then voided `NotNow` with `LastPrepareActivation` still `none`.
- AC2: "key 3" and "key 7" are not `(ServerTickIndex, Seq)` pairs, (b) does not say how A2 targets, and (c) omits the held owner reply `NotNow`; proposed rewrite (unconfirmed): keys are (T,3) and (T,7) in the same tick; (b) A2 names B by `PawnRef`; (c) the strike returns `Rejected_Latched`, no lockout, no pose, no `FStrikeResult`, and the held owner reply is `NotNow`.

- **AC-1**: Braced is decided by key order
  - Given: `Begin` at (1000,4), `DebounceExpiry` at (1124,2), `CapEnd` later
  - When: Attacks resolve at (1124,1) and, in a separate run, (1124,3); a third Attack at (1000,3)
  - Then: (1124,1) is a reversal, (1124,3) is a kill; the (1000,3) Attack kills and the `Begin` is voided `NotNow` with no stamp
  - Edge cases: a pending `End` inside the debounce still protects; `Key(End)` is the minimum of `DebounceExpiry` and `CapEnd`; no wall-clock comparisons
- **AC-2**: Same-drain interleavings
  - Given: stub queue items at (T,3) and (T,7) for cases (a) to (d)
  - When: the drain runs
  - Then: (a) A kills B and B's Attack is `Voided(NotNow)`; (b) A1 kills B and A2 is `NoContact`; (c) the strike is `Rejected_Latched` with no lockout, no `FStrikeResult` and a held `NotNow` reply; (d) braced A striking braced B reverses A
  - Edge cases: a voided attacker gets no pose or lockout; Attack preempts own brace (`Superseded`); equal-tick order is Round's stamping

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `BracedAtAttackKeyOrderTest.cpp` and `SameDrainOrderingTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-003-defence-state-component-and-cooldown.md`, `story-007-attack-outcomes-resolved-in-slot.md`; cross-epic `production/epics/round-match-framework/story-004-test-seam-stub-resolvers.md`, `story-007-intake-ordering-rules.md`, `story-052-ended-latch-effects.md`; `production/epics/input-interaction-framework/story-050-hold-timer-key-order.md`, `story-053-brace-sampled-at-execution.md`
- Unlocks: `story-010-reversal-counter-and-chained-strikes.md`, `story-013-death-write-staging-and-after-latch.md`
