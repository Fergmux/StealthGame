# Story 010: Reversal eliminates the attacker, the counter is not protected, and chained strikes

> **Epic**: Assassination & Defence Reversal
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R10, ASN-R13, ASN-R14; States and Transitions; AC-ASN-03, AC-ASN-28, AC-ASN-37)
**Requirement**: `TR-asn-002`, `TR-asn-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§2.1, §3.3 `CounterPose`)
**ADR Decision Summary**: A reversal decides in the slot (attacker eliminated, defender's brace consumed, `Countering` entered, results sent) and stages a `DeathWrite{A}` and a `CounterPose{V}` at `ContactServerTime − StrikeNetLeadSec`; the defender is never `Disabled`, and the counter writes through `WriteActionState` as an apply-only consumer.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0018 overall; the integration runs on real server state and the IIF hold cancel path)
**Engine Notes**: Runs as a PIE integration with a listen host and two clients, or the stub-queue harness where the AC allows; no post-cutoff API in the logic. The brace cancel reason `Reversed` and the `FHoldStatus` end with `ServerEndTime = ContactServerTime` are the IIF's contract (IIF story 049).

**Control Manifest Rules (this layer)**:
- Required: Every outcome-revealing write (`FDeathState`, `Disabled`, re-open, `Counter`) is a typed staged write at `ContactServerTime − StrikeNetLeadSec`; nothing on the defender changes at the slot but its brace state — source: ADR-0018
- Required: `UEncounterService` is the registered applier for `CounterPose`; `WriteActionState` stays the only `FActionState` writer — source: ADR-0018, ADR-0011
- Forbidden: Never make the counter protected, never charge the defender anything on a reversal, never `Disable` a reversing defender — source: ADR-0018, GDD ASN-R11, ASN-R14
- Guardrail: One `FActionState` write for the defender per reversal

On a reversal the attacker A is eliminated (`RequestTransition(A, Informant, ReversedAgainst)` if bound), `OnPawnKilled(Victim = A, Killer = V, VictimKind = ResolveVictimKind(A), Cause = Reversal)` fires once, V's brace is cancelled with reason `Reversed`, and V's slot becomes `Countering` for `CounterDurationSec` (1.2 s): `ActionPose = Counter`, pose-locked (SCL `PoseLocked`), not braced, every discrete command `Busy`, cooldown still counting from the original activation, then `Cooling` or `Ready`. A striker is not protected either: its own outcome stands and it can be killed mid-lunge.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-03, AC-ASN-28, AC-ASN-37), scoped to this story:*

- [x] **AC1** (AC-ASN-03, [Integration/BLOCKING] `StealthGame.Assassination.ReversalEliminatesAttacker`): **GIVEN** a defender (player or NPC) whose `Prepare` executed earlier in the round and is still in its window **WHEN** a rival player's Attack resolves on it **THEN** the attacker's Roster state becomes `Informant` with reason `ReversedAgainst`, `OnPawnKilled(Victim = attacker, Killer = defender, Player, Reversal)` fires once, the defender's brace is cancelled with reason `Reversed`, its slot is `Countering` for 1.2 s, the attacker receives `FStrikeResult.Reversed`, a bound defender receives `FDefenceResult.Reversed`, and the defender is not eliminated and is charged nothing.
- [x] **AC2** (AC-ASN-28, [Integration/BLOCKING] `StealthGame.Assassination.CounterIsNotProtected`): **GIVEN** a defender that has just reversed A1 **WHEN** a second rival A2's Attack resolves during `Countering` **THEN** the defender is killed; during `Countering` every discrete command from the defender replies `Busy`, and after 1.2 s its cooldown still counts from the original activation.
- [x] **AC3** (AC-ASN-37, [Logic/BLOCKING] `StealthGame.Assassination.StrikerStruckMidLunge`): **GIVEN** A kills B at key k **WHEN** C's Attack on A resolves in a later drain while A is `Striking` **THEN** A is killed, B stays killed, and two `OnPawnKilled` events are recorded.

---

## Implementation Notes

*Derived from ADR-0018 §2.1, §3.1, §3.3 and GDD ASN-R13, ASN-R14:*

Reversal slot effects (in order, story 007): transition for A, `VacateCard(A)`, `OnPawnKilled` with `Cause = Reversal`, the defender's brace consumed through the IIF cancel with reason `Reversed` (the owner `FHoldStatus` end carries `ServerEndTime = ContactServerTime`, so the HUD ends the brace square at contact, story 024), the defender's strike role `Bracing → Countering`, and staged `DeathWrite{Victim = A}` plus `CounterPose{Defender = V, ContactServerTime}` (TypeRank: `DeathWrite` before `CounterPose`, same server step; staging is story 013). The `Counter` pose clip starts at the local contact frame on every machine (story 018), never on receipt; the defender holds `Brace_Hold` until then. After `CounterDurationSec` the role goes `Countering → Cooling` (or `Ready` if the cooldown has expired).

The counter is not protection: the brace is already consumed, so `Braced` (story 009) is false for a `Countering` pawn and a second attacker kills it. A pair can trade one life for a braced defender; one attacker cannot (accepted, ASN-R13, OQ-ASN-4 telemetry in story 026). A striker struck mid-lunge: its own outcome was fixed at its execution and stands; both bodies fall; `Striking → Dead`.

An NPC defender reverses identically: same transition, same `OnPawnKilled` fields with `Killer` = the NPC pawn, same presentation; only the null sink means no `FDefenceResult`. Nothing is charged to anyone (ASN-R11, Penalty counts only `Cause = Strike ∧ VictimKind = Npc`).

**Prototype replaced / acceptance reference:** the prototype's reversal path (`EStealthAssassinationOutcome::Reversed` in `HandleAssassinationRequest`: message plus immediate `KillCharacter` on the attacker) is replaced; networked prototype tests A and C (reversal eliminates the attacker; evidence `production/qa/evidence/2026-10-24_client1-reversal-elimination-win.png`) are the acceptance reference for AC1.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-npc-brace-parity-never-strike-planted.md`: the NPC brace and strike path itself.
- `story-013-death-write-staging-and-after-latch.md`: the staged writes' application and timing.
- `story-015-owner-results-reply-entries-and-tally.md`: building the `FStrikeResult` / `FDefenceResult` payloads.
- IIF story 049: the IIF-side `ReversalConsumesBrace` event and cancel reason.

---

## Files

- `Source/StealthGameplay/Private/Features/Assassination/AttackResolverCore.cpp` (reversal and counter branches; shared with stories 007 and 009)
- `Source/StealthGameplay/Private/Features/Assassination/EncounterService.cpp` (`Countering` role, `CounterPose` staging call)
- `Source/StealthGameplay/Private/Tests/Assassination/ReversalEliminatesAttackerTest.cpp` (class `FAssassinationReversalEliminatesAttackerTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/CounterIsNotProtectedTest.cpp` (class `FAssassinationCounterIsNotProtectedTest`)
- `Source/StealthGameplay/Private/Tests/Assassination/StrikerStruckMidLungeTest.cpp` (class `FAssassinationStrikerStruckMidLungeTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: the 1.2 s `Countering` duration has no tolerance and "charged nothing" has no observable; proposed rewrite (unconfirmed): its slot is `Countering` for 1.2 s from the slot (± 1 server tick), and the defender's strike count, `LastPrepareActivation`, Roster state and score are identical to the pre-slot snapshot.
- AC2: "every discrete command" is not enumerated, and the A2 attack time and the cooldown check are not numeric; proposed rewrite (unconfirmed): A2's Attack resolves at slot + 0.6 s (inside the 1.2 s window); each of the seven discrete commands issued at 0.2 s, 0.6 s and 1.0 s replies `Busy`; at 1.2 s + 1 tick the cooldown end equals the original `LastPrepareActivation` + 60 s exactly (not re-stamped).
- AC3: adequate; name the stub fixture: A, B, C stub pawns with keys k < k' in separate drains.

- **AC-1**: Reversal eliminates the attacker
  - Given: a defender (player run, NPC run) whose `Prepare` executed earlier and is in window, plus a rival attacker A
  - When: A's Attack resolves on the defender
  - Then: A is `Informant`/`ReversedAgainst`, one `OnPawnKilled(A, defender, Player, Reversal)`, brace cancelled with `Reversed`, `Countering` for 1.2 s (± 1 tick), `FStrikeResult.Reversed` to A, `FDefenceResult.Reversed` only if bound, defender not killed and otherwise unchanged
  - Edge cases: NPC defender (no `FDefenceResult`); `OnPawnKilled` fires exactly once; exactly one defender `FActionState` write
- **AC-2**: Counter is not protected
  - Given: a defender that has just reversed A1, in `Countering`
  - When: A2's Attack resolves during `Countering`, and discrete commands are issued during it
  - Then: the defender is killed, all commands reply `Busy`, and the cooldown end still equals original activation + 60 s
  - Edge cases: A2 resolves at the first and last tick of the 1.2 s window; A2 in the same drain as A1 (story 009 ordering)
- **AC-3**: Striker struck mid-lunge
  - Given: stub A, B, C; A kills B at key k
  - When: C's Attack on A resolves in a later drain while A is `Striking`
  - Then: A is killed, B stays killed, and exactly two `OnPawnKilled` events are recorded
  - Edge cases: C's key equals k + 1; C attacks after A has left `Striking` (normal kill path, not this assertion)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three test files above (AC1 and AC2 in PIE or the integration harness; AC3 headless on stub items) — must exist and pass

**Status**: [x] Created and passing (2026-10-27)

---

## Dependencies

- Depends on: `story-003-defence-state-component-and-cooldown.md`, `story-007-attack-outcomes-resolved-in-slot.md`, `story-008-attacker-lockout-and-attack-lifecycle.md`, `story-009-same-drain-ordering-and-braced-key-compare.md`; cross-epic `production/epics/input-interaction-framework/story-049-reversal-consumes-brace.md`, `story-035-slot-priority.md`
- Unlocks: `story-011-npc-brace-parity-never-strike-planted.md`, `story-013-death-write-staging-and-after-latch.md`, `story-025-reversal-readability-and-brace-tell-playtest.md`, `story-026-advisory-economics-telemetry-resolver-cost.md`

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: 3/3 passing (AC1 a player defender (sink 5) and an NPC defender (null sink): attacker Informant / ReversedAgainst, one OnPawnKilled(attacker, defender, Player, Reversal), brace cancelled Reversed, Countering to slot + 1.2 s (still Countering one tick before, Cooling at 1.2 s), FStrikeResult.Reversed, FDefenceResult only for the bound defender, the defender not eliminated and its stamp unchanged, exactly one CounterPose write and one attacker DeathWrite. AC2 A2 kills the countering defender at the first tick, at 0.6 s and at the last tick of the window; every discrete kind replies Busy at 0.2, 0.6 and 1.0 s; after 1.2 s + 1 tick the cooldown end is the original activation + 60 s exactly. AC3 C kills A mid-lunge one tick and 0.5 s after A killed B: A killed, B stays killed, two OnPawnKilled, A's own outcome stands) - `StealthGame.Assassination.ReversalEliminatesAttacker`, `.CounterIsNotProtected`, `.StrikerStruckMidLunge`
**Deviations**: ADVISORY - the confirmed user rule is applied (a reversal eliminates the attacker). Countering is measured from the slot (QA rewrite). AC1/AC2 run on the integration harness (the resolver over its seams with real pawn identities), not PIE. The defender's planted stance is kept until its window item, then the CounterPose applier (asn-013) owns the pose; the IIF 049 FHoldStatus end with ServerEndTime = ContactServerTime belongs to the reply step (asn-015).
**Test Evidence**: `ReversalEliminatesAttackerTest.cpp`, `CounterIsNotProtectedTest.cpp`, `StrikerStruckMidLungeTest.cpp` under `Source/StealthGameplay/Private/Tests/Assassination/` (shared rigs `EncounterTestRig.h`, `EncounterRoundRig.h`); run in `production/qa/evidence/assassination-defence-reversal-005-011-2026-10-27/full-suite.txt`
**Run result**: N/A - server resolver logic, not yet registered in play (the composition-root wiring and the prototype-bridge removal are stories asn-012/013), so nothing player-visible changes in this story
**Reviewed by**: self, no independent review (focused /code-review pass by the implementing agent; unattended batch)
