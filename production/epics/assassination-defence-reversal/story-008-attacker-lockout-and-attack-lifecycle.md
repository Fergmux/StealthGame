# Story 008: Attack is never rejected by its target; every executed Attack locks the attacker for 1.75 s

> **Epic**: Assassination & Defence Reversal
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R1, ASN-R12, ASN-R15; Formulas F3; Edge Cases "Attacker legality"; AC-ASN-06, AC-ASN-07, AC-ASN-11)
**Requirement**: `TR-asn-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline (slot, predicates, replies), with ADR-0018 §2.1 and §4.1
**ADR Decision Summary**: Every discrete command enters through one of seven entry points, passes two-stage validation and occupies one action slot per pawn; feature predicates are registered per action type, and no reply code may depend on a target pawn's identity, team, concealment or brace state.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 overall; this story is predicate and slot logic over plain C++ state)
**Engine Notes**: No post-cutoff API in the Assassination part: `Attack`'s `CanBegin` is registered through the IIF's weak predicate registry and returns `Allow` unconditionally; the lockout is the action slot's `Striking` state plus the `LockoutEnd` stamp on the defence component, both on the queue clock.

**Control Manifest Rules (this layer)**:
- Required: Feature predicates are registered weakly from the composition root per `EActionType` — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011
- Required: Every executed Attack, whatever its outcome, puts the attacker in `Striking` for `AttackAttemptLockoutSec`; there is no extra cooldown on misses — source: ADR-0018, GDD ASN-R15
- Guardrail: The IIF Attack bucket (burst 2, 1 per s) never binds before the lockout because the lockout is above 1 s

`Attack` is always `Allow`: no Attack is rejected for a reason about its target, so no reply can confirm a target's kind, team, brace or concealment. The lockout is enforced by the slot (`Striking` is busy for `AttackAttemptLockoutSec`, default 1.75 s, IIF R9 → `Busy`) for every outcome, including a whiff, and is the probe limiter. The same story handles running and traversing victims (speed and parkour are tells, never protection) and an attacker that is itself running or traversing, which the IIF rejects `NotNow`.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-06, AC-ASN-07, AC-ASN-11), scoped to this story:*

- [x] **AC1** (AC-ASN-06, [Logic/BLOCKING] `StealthGame.Assassination.NoTargetDependentRejection`): **GIVEN** Attacks at an NPC, an Active rival, an informant, an ally, a concealed rival, an empty open spot, an empty bush and a dead body **WHEN** each resolves **THEN** every one executes (none is rejected), each produces the same IIF reply path and enqueue step, each starts the 1.75 s lockout, and only `FStrikeResult.Result` differs.
- [x] **AC2** (AC-ASN-07, [Logic/BLOCKING] `StealthGame.Assassination.LockoutForEveryOutcome`): **GIVEN** an executed kill and, separately, an executed whiff **WHEN** Attack, Prepare (cooldown clear), Interact (at a live site) and Hide (beside a bush) are issued at +1.74 s **THEN** each replies `Busy`; at +1.75 s (± 1 server tick) each passes the slot check and executes given those preconditions, and no other cooldown follows the whiff; **AND** 10 Attack presses at 10 Hz during the lockout all reply `Busy` and an Attack at +1.75 s executes; **AND** for a reversed attacker `LockoutEnd` is stamped at t_exec + 1.75 (its later commands reply `NotNow` as an informant).
- [x] **AC3** (AC-ASN-11, [Logic/BLOCKING] `StealthGame.Assassination.MovingVictims`): **GIVEN** a runner moving away at d0 = 150 and a vaulter at d0 = 150 with Δz ≤ 75 **THEN** both are killed at their server positions; **GIVEN** a climber with Δz = 80 **THEN** `NoContact`; **AND** an attacker that is running or traversing is rejected `NotNow` (IIF R7).

---

## Implementation Notes

*Derived from ADR-0011 (predicates, slot), ADR-0018 §4.1 and GDD ASN-R1, ASN-R12, ASN-R15, F3:*

`CanAttack(t) = Slot ≠ Striking`, with `Slot = Striking` for `t − t_exec < Λ` (`AttackAttemptLockoutSec`). Write `LockoutEnd = t_exec + Λ` on the defence component (story 003) at execution for every outcome; for a reversed attacker the stamp is still `t_exec + 1.75` while its Roster state is `Informant` (replies `NotNow`). `Striking` ends by elapsed time (queue `TimerFired`, never `FTimerManager`), by the pawn being killed (`Dead`) or reversed (`Dead`). The strike role table's `Striking` entry and the IIF slot state must agree; `Striking` is not an interrupt of an in-progress death write (story 013).

Moving victims: the F1 reach test (story 002) applies to the server position including the vertical limit; a runner fleeing straight away is reachable from d0 ≤ about 152 cm. A traversing victim is killed from its server position and the traversal clip stops at the death write (SCL `Disabled`, story 013). The attacker itself cannot strike while running or traversing: the legality is the IIF's (R7, stories 033 and 034 there); this story's test asserts the reply, not the rule.

The pose and the lockout mean no strike cancel exists; the bucket cannot bind before the lockout. A strike spam of 10 presses at 10 Hz yields only `Busy` replies and no queue growth beyond the IIF's ingress clamp.

**Prototype replaced:** the prototype's one-second attacker hold (`AttackerRecoveryTime` via `HoldInPlace` after `KillCharacter` in `StealthGameMode.cpp`) becomes the 1.75 s lockout for every outcome, including whiffs, which the prototype never locked.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-attack-outcomes-resolved-in-slot.md`: the outcome itself.
- `story-018-strike-clip-variants-warp-and-contact-scheduling.md`: the strike pose and clips.
- IIF stories 030, 035, 036: the slot priority and lockout-occupies-slot rules in the IIF, and `Attack` never rejected by target at the IIF level (this story adds the Assassination resolver's side and the full outcome matrix).

---

## Files

- `Source/StealthGameplay/Private/Features/Assassination/AttackPredicate.cpp` (`CanBegin` for `Attack`: always `Allow`)
- `Source/StealthGameplay/Private/Features/Assassination/AttackResolverCore.cpp` (lockout stamping and `Striking` role, shared with story 007)
- `Source/StealthGameplay/Private/Tests/Assassination/NoTargetDependentRejectionTest.cpp` (class `FAssassinationNoTargetDependentRejectionTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/LockoutForEveryOutcomeTest.cpp` (class `FAssassinationLockoutForEveryOutcomeTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/MovingVictimsTest.cpp` (class `FAssassinationMovingVictimsTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "the same IIF reply path and enqueue step" is not measurable and the expected `Result` per target type is not listed; proposed rewrite (unconfirmed): for the 8 target types the reply code, reply tick, enqueue count and `LockoutEnd` (= t_exec + 1.75) are identical, and `FStrikeResult.Result` equals the expected-value table (fixture) per type.
- AC2: ± 1 tick around a 0.01 s margin (1.74 vs 1.75) is not separable at 60 Hz, the 10 Hz x 10 presses probe may hit the IIF Attack bucket (burst 2, 1 per s) before the slot check, and the Interact and Hide fixtures are undefined; proposed rewrite (unconfirmed): times are queue keys at tick-aligned offsets (+1.74 becomes the last tick before t_exec + 1.75); the spec states validation order (slot `Busy` before bucket) and expects all 10 presses `Busy`; Interact at a live site and Hide beside a bush are set up in the fixture.
- AC3: "vaulter ... Δz ≤ 75" is a range, not a case; proposed rewrite (unconfirmed): runner 450 cm/s away at d0 = 150 and a vaulter at Δz = 75 and Δz = 0 with its velocity stated, killed; climber at Δz = 80 `NoContact`; a running or traversing attacker replies `NotNow`.

- **AC-1**: No target-dependent rejection
  - Given: Attacks at an NPC, Active rival, informant, ally, concealed rival, empty open spot, empty bush, dead body
  - When: each resolves
  - Then: all execute; reply code, reply tick and enqueue count are identical; each stamps `LockoutEnd` = t_exec + 1.75; only `Result` differs, matching the fixture table
  - Edge cases: whiff and ally cases locked too; no per-target reply code
- **AC-2**: Lockout for every outcome
  - Given: an executed kill and a whiff; Prepare (cooldown clear), Interact (live site), Hide (beside bush) fixtures
  - When: each command is issued one tick before and at t_exec + 1.75; 10 Attack presses at 10 Hz
  - Then: before the boundary all reply `Busy`; at it each executes; all 10 presses reply `Busy` and the next Attack executes; no extra cooldown after a whiff; a reversed attacker's `LockoutEnd` = t_exec + 1.75 and later commands reply `NotNow`
  - Edge cases: IIF bucket order versus slot `Busy`; no queue growth from the spam
- **AC-3**: Moving victims
  - Given: runner 450 cm/s away at d0 = 150; vaulter at d0 = 150 with stated velocity and Δz in {0, 75}; climber Δz = 80; a running or traversing attacker
  - When: each Attack resolves
  - Then: runner and vaulters are killed at server positions; climber is `NoContact`; the attacker case replies `NotNow`
  - Edge cases: Δz 75 inclusive, 80 out; traversal stops at the death write (not asserted here)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files above — must exist and pass

**Status**: [x] Created and passing (2026-10-27)

---

## Dependencies

- Depends on: `story-002-reach-f1-pure-function.md`, `story-003-defence-state-component-and-cooldown.md`, `story-007-attack-outcomes-resolved-in-slot.md`; cross-epic `production/epics/input-interaction-framework/story-030-attack-never-rejected-by-target.md`, `story-036-lockout-occupies-slot.md`, `story-033-legality-matrix.md`
- Unlocks: `story-010-reversal-counter-and-chained-strikes.md`, `story-011-npc-brace-parity-never-strike-planted.md`, `story-026-advisory-economics-telemetry-resolver-cost.md`

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: 3/3 passing (AC1 NPC, Active rival, informant, ally, concealed rival, empty spot, empty bush, dead body: every Attack executes, same reply (Executed, tick), nothing scheduled, LockoutEnd = t_exec + 1.75 exactly, Result per the table (WrongTarget, RivalEliminated, WrongTarget, NoContact, RivalEliminated, NoContact, NoContact, NoContact); Attack's CanBegin is Allow. AC2 after a kill and after a whiff, Attack, Prepare, Interact and Hide on the last tick before t_exec + 1.75 reply Busy and at t_exec + 1.75 execute (one round per command); 10 presses at 10 Hz all Busy with no queue growth and the +1.75 press executes (no miss cooldown); a reversed attacker's LockoutEnd = t_exec + 1.75 and its later Attack and Prepare reply NotNow. AC3 a runner at 450 cm/s from d0 150 killed (X_eff 238.25), vaulters at dz 0 and 75 killed, a climber at dz 80 NoContact; a running or traversing attacker NotNow) - `StealthGame.Assassination.NoTargetDependentRejection`, `.LockoutForEveryOutcome`, `.MovingVictims`
**Deviations**: ADVISORY - Striking / Countering expire lazily at slot time from the stamped ends on the queue clock (no timer item, never FTimerManager; decision logged). The IIF slot (IIF 030/035/036) is not built: the resolver gates every discrete command it sees (Busy while Striking/Countering, and while Bracing for non-Prepare kinds) and forwards Interact/Hide/Traverse to an injected fallback (the stand-in dispatch until IIF stage 2). `UAssassinationAttackPredicate` (always Allow) is registered for EActionType::Attack in UStealthCompositionRoot.
**Test Evidence**: `NoTargetDependentRejectionTest.cpp`, `LockoutForEveryOutcomeTest.cpp`, `MovingVictimsTest.cpp` under `Source/StealthGameplay/Private/Tests/Assassination/` (shared rigs `EncounterTestRig.h`, `EncounterRoundRig.h`); run in `production/qa/evidence/assassination-defence-reversal-005-011-2026-10-27/full-suite.txt`
**Run result**: N/A - server resolver logic, not yet registered in play (the composition-root wiring and the prototype-bridge removal are stories asn-012/013), so nothing player-visible changes in this story
**Reviewed by**: self, no independent review (focused /code-review pass by the implementing agent; unattended batch)
