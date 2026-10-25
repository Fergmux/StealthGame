# Story 007: One strike, one outcome: kill, reversal or whiff resolved with every effect inside the slot

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R10, ASN-R11, ASN-R13; Formulas F4; Edge Cases "Same-drain ordering"; AC-ASN-10, AC-ASN-13, AC-ASN-29)
**Requirement**: `TR-asn-001`, `TR-asn-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging (§5 inline follow-ons, §8 latch), with ADR-0018 §2.1 steps 3-9
**ADR Decision Summary**: Every interacting state change resolves inside the round's single queue slot; the Attack's follow-on runs the Roster transition, then `OnPawnKilled` (inline), `VacateCard`, Penalty's accounting and Round's elimination check, all before slot end, and decides the outcome at the slot — only presentation-timed writes are deferred.
**ADR Version**: 2026-10-24 (ADR-0002); ADR-0018 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002) / HIGH (ADR-0018 overall; this story is server logic on the queue contract, no post-cutoff API)
**Engine Notes**: Tick-group placement (`TG_PostPhysics` drain) and the double server clock are ADR-0002's; this story adds a resolver registered through `IQueueResolver` and runs entirely inside the slot. `OnPawnKilled` is a server-only native multicast, never Blueprint-assignable.

**Control Manifest Rules (this layer)**:
- Required: Every interacting state change resolves as an item (or an inline follow-on in an item's slot); nothing outside the drain mutates gameplay state — source: ADR-0002
- Required: `RequestTransition` before `OnPawnKilled`; `VacateCard`, Roster transitions, strike accounting and holds' server cancels happen in the slot, never in the staging pass — source: ADR-0018
- Required: Only Round's `TryTerminate` calls `Latch`; Assassination never decides a round — source: ADR-0002
- Forbidden: Assassination holds no strike counter and creates no deferred queue item for an outcome — source: ADR-0018, GDD ASN-R11
- Guardrail: µs per Attack inside the 0.5 ms queue line

The outcome table (ASN-R10): no victim is a **whiff**; a braced victim (F2, story 009) is a **reversal** (the attacker is eliminated, the defender's brace is consumed and it enters `Countering`, story 010); anything else is a **kill**. For a kill of an `Active` rival the Roster transition `Informant / Assassinated` is requested first; `OnPawnKilled(Victim, Killer, VictimKind, Cause)` then fires for every kind (`Player`, `Npc`, `Informant`) with `Cause = Strike`, and for a reversal with `Victim = A`, `Killer = V`, `Cause = Reversal`. Nothing is emitted for a whiff.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-10, AC-ASN-13, AC-ASN-29), scoped to this story:*

- [ ] **AC1** (AC-ASN-10, [Logic/BLOCKING] `StealthGame.Assassination.OutcomeInSlot`): **GIVEN** a kill of a team's last Active player **WHEN** the slot executes **THEN** `RequestTransition` is called before `OnPawnKilled`, Round's elimination check returns `EliminationWin` for the attacker's team inside the same slot, no deferred queue item is created by Assassination, and a later item in the drain returns `Rejected_Latched`.
- [ ] **AC2** (AC-ASN-13, [Logic/BLOCKING] `StealthGame.Assassination.KillEventMatrix`): **GIVEN** kills of an NPC, an Active rival and an informant, and a reversal **THEN** `OnPawnKilled` carries `(Npc, Strike)`, `(Player, Strike)`, `(Informant, Strike)` and `(Player, Reversal)` respectively; only the rival kill and the reversal request a Roster transition; and the Assassination module holds no strike counter (reflected field check).
- [ ] **AC3** (AC-ASN-29, [Logic/BLOCKING] `StealthGame.Assassination.ReversalCanEndTheRound`): **GIVEN** team 0 with one Active player A and team 1 with ≥ 1 Active player **WHEN** A is reversed by (a) a team-1 player and (b) an NPC **THEN** in both runs Round latches `EliminationWin(1)` in the reversal's slot.

---

## Implementation Notes

*Derived from ADR-0018 §2.1 (order inside the slot) and ADR-0002 §5:*

Order inside the Attack's slot (steps owned by this story in bold): (1) **victim resolution and outcome** (stories 002, 005, 006, 009); (2) the attacker's outcome-blind `WriteActionState` (story 012); (3) **Roster transition** if any, `RequestTransition(V, Informant, Assassinated)` or `(A, Informant, ReversedAgainst)`, before `OnPawnKilled`, with the binding retirement carrying `RetireAtServerTime` = this strike's `ContactServerTime` (Roster story 021); (4) **`VacateCard(Dead pawn)`** for the killed pawn (kill: V; reversal: A), no kind parameter, in the slot because the card ledger is server-only state; (5) Concealment's occupancy exit for a victim killed while concealed (the un-cull is staged with the death write, stories 013 and 023); (6) **`OnPawnKilled`** for kills and reversals, subscribers inline (Penalty strike, tally and exile; Crowd vacancy; Round log); (7) staging of the death-write items (story 013); (8) the owner results (story 015); (9) **Event Log rows** `KillResolved{VictimKind, Cause}` / `Reversal{Attacker, Defender}` with the slot key (Round story 011).

The strike role becomes `Dead` for the pawn that falls in this slot, and `GetScheduledDeathServerTime(V)` is recorded at step 1 so it is valid inside the `OnPawnKilled` fan-out and the Roster flush (Penalty reads it, ADR-0019). Mutual strikes, same-drain chains and the latch interplay are tested in story 009; `Rejected_Latched` and the elimination check are Round's. No `Assassination` code calls `Latch`.

A reversal eliminates the attacker even when the reversing defender is an NPC (`ResolveVictimKind(A)` supplies the kind); the trigger reads team counts, not who reversed. Strike accounting is Penalty's: Assassination never counts strikes (ASN-R11).

**Prototype replaced / promoted as reference:** `AStealthGameMode::HandleAssassinationRequest` and `KillCharacter` (immediate ragdoll, one-second attacker hold) are replaced. `FStealthRules::ResolveAssassination` (test `StealthGame.Rules.Assassination`: unprepared enemy eliminated, prepared enemy reverses, NPC and informant kills distinguished) is the proven spec for the outcome table; networked prototype tests A and C (reversal eliminates the attacker) are the acceptance reference for AC3.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-009-same-drain-ordering-and-braced-key-compare.md`: braced-at-Attack sampling and same-drain interleavings.
- `story-010-reversal-counter-and-chained-strikes.md`: the counter, brace consumption and chained strikes.
- `story-013-death-write-staging-and-after-latch.md`: the staged `DeathWrite` / `CounterPose` items.
- `story-015-owner-results-reply-entries-and-tally.md`: `FStrikeResult` / `FDefenceResult`.
- Penalty epic (stories not yet created): strike counting, tally, exile, informant respawn.

---

## Files

- `Source/StealthGameplay/Private/Features/Assassination/AttackResolverCore.h` and `.cpp` (outcome decision and in-slot effect order, plain C++)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterService.cpp` (`IQueueResolver` for `Attack`, `OnPawnKilled` multicast, `GetScheduledDeathServerTime`)
- `Source/StealthGameplay/Public/Server/Features/Assassination/EncounterService.h` (`FOnPawnKilled`, `EVictimKind`/`EKillCause` use)
- `Source/StealthGameplay/Private/Tests/Assassination/OutcomeInSlotTest.cpp` (class `FAssassinationOutcomeInSlotTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/KillEventMatrixTest.cpp` (class `FAssassinationKillEventMatrixTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/ReversalCanEndTheRoundTest.cpp` (class `FAssassinationReversalCanEndTheRoundTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "no deferred queue item is created by Assassination" collides with step 7 (death-write staging, story 013) which enqueues items; proposed rewrite (unconfirmed): Assassination enqueues no item that decides or carries the outcome (an enqueue spy shows only the `DeathWrite`/`CounterPose` staging items of story 013, or none when that story is stubbed), the call log shows `RequestTransition` before `OnPawnKilled`, and a later drain item returns `Rejected_Latched`.
- AC2: "holds no strike counter (reflected field check)" has no rule for what counts as a counter, and plain C++ cores are not reflected; proposed rewrite (unconfirmed): the Penalty stub's strike counter is mutated only via its `OnPawnKilled` subscriber (spy shows zero direct writes from `UEncounterService` or the `F...Core` types), and `UEncounterService` exposes no integer or float `UPROPERTY`.
- AC3: the braced-defender fixture and team-1 Active count are unstated; proposed rewrite (unconfirmed): defender braced by key order, team 1 with exactly 1 Active player; the `EliminationWin(1)` latch key equals the Attack's slot key in both (a) and (b).

- **AC-1**: Outcome is decided and effected inside the slot
  - Given: a kill of a team's last Active player, stub Round queue, injected clock
  - When: the Attack's slot executes
  - Then: call order is `RequestTransition` then `OnPawnKilled`; Round returns `EliminationWin` for the attacker's team in the same slot; no outcome-carrying item is enqueued; a later item returns `Rejected_Latched`
  - Edge cases: strike role is `Dead` at the slot; `GetScheduledDeathServerTime` valid inside the fan-out
- **AC-2**: Kill event matrix
  - Given: kills of an NPC, an Active rival, an informant, and a reversal
  - When: each resolves
  - Then: `OnPawnKilled` carries (Npc, Strike), (Player, Strike), (Informant, Strike), (Player, Reversal); only the rival kill and the reversal request a Roster transition; no strike counter is written by the module
  - Edge cases: a whiff emits nothing; an informant kill is not a transition; a reversal has Victim = A, Killer = V
- **AC-3**: Reversal can end the round
  - Given: team 0 with one Active player A, team 1 with at least 1 Active player, a braced defender
  - When: A is reversed by (a) a team-1 player and (b) an NPC
  - Then: Round latches `EliminationWin(1)` at the reversal's slot key in both runs
  - Edge cases: the trigger reads team counts, not who reversed; `ResolveVictimKind(A)` is `Player`

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files above (stub Round queue and injected clock, ADR-0008) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-001-assassination-tuning-keys-and-invariants.md`, `story-003-defence-state-component-and-cooldown.md`, `story-005-victim-resolution-order.md`; cross-epic `production/epics/round-match-framework/story-008-resolver-contract.md`, `story-009-follow-ons-inline.md`, `story-044-elimination-truth-table.md`, `story-045-same-tick-final-vs-elimination.md`; `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`, `story-020-informant-kill-is-not-a-transition.md`, `story-021-retire-at-server-time-source.md`
- Unlocks: `story-008-attacker-lockout-and-attack-lifecycle.md`, `story-009-same-drain-ordering-and-braced-key-compare.md`, `story-010-reversal-counter-and-chained-strikes.md`, `story-013-death-write-staging-and-after-latch.md`, `story-015-owner-results-reply-entries-and-tally.md`, `story-019-server-body-sequence-and-reset.md`
