# Story 055: Same-tick assassinations and strike-outs resolve in queue order

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0002 resolves every interacting state change in one total order: one `FResolutionQueueCore` per round keyed `(ServerTickIndex, Seq)`, with per-source mailboxes permuted by a per-tick secret shuffle so arrival order carries no meaning, and resolvers that re-validate against live state at the slot and run follow-ons (Roster transitions, `OnPawnKilled`, strike-limit transitions, the elimination check) inline and depth-first inside the slot. The Roster applies calls in the order received and never orders requests. ADR-0005 fixes that `RequestTransition(P, Informant, StrikeLimit)` releases, not retires, the participant's binding in the same call.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff APIs (tick groups and `FTickFunction` are pre-cutoff and re-verified; `UE_LOGF` preferred for diagnostics). Verification required: spawning a Shared Pawn inside `TG_PostPhysics` from a resolver (spike with ADR-0010), estimator accuracy under net emulation, and the once-per-frame drain on the listen host. This story drives a stub queue, so none of these gate it.

**Control Manifest Rules (this layer)**:
- Required: every interacting state change resolves as an item (or an inline follow-on in an item's slot) of the round's single queue; resolvers re-validate against live state at the slot and run follow-ons inline — source: ADR-0002
- Required: mailbox and tie permutations come from `GetServerSubSeed(SeqOrder|TieRank, ServerTickIndex)` — source: ADR-0002
- Forbidden: Never order events with a feature-local clock, timestamp, tick-function order or `FTimerManager` — source: ADR-0002
- Forbidden: Never use the public seed, join order or a rotating index for permutations; never order commands by client-supplied or estimated send time — source: ADR-0002
- Guardrail: queue drain + staging + teardown + Roster flush at most 0.5 ms on the listen-host game thread — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-35** [Int][Dep:Assassination][Dep:NPC-Kill Penalty] `StealthGame.Roster.SimultaneousResolution` (with Assassination and NPC-Kill Penalty), three cases:
  - (a) two assassins hit one pawn in the same tick: 1 transition, 0 strikes;
  - (b) the 3rd NPC kill is queued before the player's own assassination: `StrikeLimit`, then `AlreadyResolved`;
  - (c) the assassination is queued before the 3rd NPC kill: `Assassinated`, the NPC attack is voided, and the NPC lives (Edge Cases; Player Fantasy anchor 4).

---

## Implementation Notes

*Derived from ADR-0002 section 5, ADR-0005 section 2 ("Strike-limit release inside the transition"), and GDD R8, R9, R10, Edge Cases ("Simultaneous events"):*

- The Roster applies calls in the order received. The order is decided by Round's resolution queue (key: server tick, then arrival sequence id); the tick-ordering rules belong to the Round, Penalty and Assassination GDDs. The test only supplies a controllable queue to the stubs and asserts what the Roster does with each ordering.
- Case (a), two assassins hit one pawn in the same tick: the first is `Applied` (one transition, binding `Retired`). For the second, `ResolveVictimKind` returns `NotTargetable(AlreadyResolved)` (tombstone): no transition, no strike. Strike counts are Penalty's quantity, asserted through the Penalty stub.
- Case (b), the 3rd NPC kill is queued before the player's own assassination: the strike is counted, `RequestTransition(A, Informant, StrikeLimit)` is `Applied` and A's live binding is released (the pawn becomes `Unbound`, R8, ADR-0005 section 2). The assassin's attack then resolves against an `Unbound` pawn.
- Case (c), the assassination is queued before the 3rd NPC kill: A becomes `Informant` (`Assassinated`), and A's queued NPC attack is voided at resolution because the attacker is no longer `Active` (Assassination checks attacker state at resolution, not submission); the NPC lives.
- Open point (ruling needed, game-designer): the criterion text (b) says "`StrikeLimit`, then `AlreadyResolved`". That contradicts R8, R9, AC-PSR-14 and the GDD Edge Cases section, which all say a `StrikeLimit` releases the pawn, so the second attack resolves `Npc` (a `WrongTarget` plate and tally +1, but no strike inside `StrikeOutReleaseGraceSec`, Penalty PEN-R5), not `NotTargetable(AlreadyResolved)`. The criterion above is quoted verbatim from the GDD and must not be weakened by this story; the contradiction must be resolved in the GDD (the QA report proposes changing (b) to "then `Npc` with `WrongTarget` and no strike") before this story is sprint-ready. Implement the assertion for (b) per the ruling; until then assert the R8/R9 behaviour (Unbound, `Npc`) and record the divergence from the criterion text.
- [Dep:Assassination][Dep:NPC-Kill Penalty]: stubs of Assassination and Penalty with a controllable queue until `production/epics/assassination-defence-reversal/EPIC.md` and `production/epics/npc-kill-penalty-informants/EPIC.md` land. Reuse the shared stub headers from Stories 052 and 053.
- Prototype disposition: re-tests the lifecycle bookkeeping that the prototype did inline in `Source/StealthGame/Private/StealthGameMode.cpp` (`EliminatePlayer`-style handling, per-attack resolution order by arrival); the new ordering is queue-based and the prototype logic is replaced.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): the transition and `StrikeLimit` release.
- Story 028 (Re-entrant listener calls see live state and events dispatch FIFO with strict EventSeq): FIFO dispatch.
- Story 053 (Two last-active attacks in one tick latch exactly one elimination win): same-tick no-draw latch.
- Story 013 (ResolveVictimKind answers the full pawn-state matrix): the query matrix.
- Story 050 (Killing an informant produces the same attacker-side RPCs, HUD and cues as killing an NPC): attacker-side parity.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterSimultaneousResolutionTest.cpp` (class `FRosterSimultaneousResolutionTest`, test name `StealthGame.Roster.SimultaneousResolution`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterAssassinationStub.h` (stub Assassination with a controllable queue; created by Story 053, extended here)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterPenaltyStub.h` (stub NPC-Kill Penalty exposing the strike count)
- Header names follow ADR-0007: `Stealth` prefix, unique project-wide.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS)**: AC-35(b) says "StrikeLimit, then AlreadyResolved". That contradicts R8, R9, AC-14 and the Edge Cases section: a StrikeLimit releases the pawn, so the second attack resolves Npc, not NotTargetable(AlreadyResolved). Needs a ruling (game-designer): change (b) to "then Npc with WrongTarget and no strike". The tick-ordering rules belong to the Round/Penalty/Assassination GDDs.

**Provisional spec (qa-lead, pending gap resolution)**:

- **AC-1**: same-tick assassinations and strike-outs resolve in queue order.
  - Given: stubs for Assassination and Penalty with a controllable queue.
  - When: (a) two assassins hit one pawn. (b) the third NPC kill is queued before the assassination. (c) the assassination is queued before the third NPC kill.
  - Then: (a) one transition, no strike. (b) StrikeLimit applies; the second attack's result follows the author's decision. (c) Assassinated applies, the NPC attack is voided, the NPC lives.
  - Edge cases: strike counts are Penalty's quantity, asserted via the stub.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthRosterSimultaneousResolutionTest.cpp` (`StealthGame.Roster.SimultaneousResolution`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`, `production/epics/participant-state-roster/story-028-reentrant-listener-ordering.md`
- Cross-epic: `production/epics/assassination-defence-reversal/EPIC.md` ([Dep:Assassination]) and `production/epics/npc-kill-penalty-informants/EPIC.md` ([Dep:NPC-Kill Penalty]); stubbed until they land
- Unlocks: None
