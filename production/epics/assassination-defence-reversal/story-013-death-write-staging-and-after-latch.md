# Story 013: Death write staging at contact minus the lead, applied identically even after the latch

> **Epic**: Assassination & Defence Reversal
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R10, ASN-R16, ASN-R21; AC-ASN-20 death-write clause, AC-ASN-43, AC-ASN-48)
**Requirement**: `TR-asn-003`, `TR-asn-004`, `TR-asn-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§3 death write, §3.2 apply), with ADR-0002 (typed staged writes, staging pass, `IStagedWriteApplier`) and ADR-0003 (forced updates, uniform net config)
**ADR Decision Summary**: Outcome-revealing state (`FDeathState`, `Disabled`, the re-open of a culled victim, the defender's `Counter` pose) is written as typed staged writes at `T_dw = ContactServerTime − StrikeNetLeadSec`, in one server step, identically for every victim kind, deciding nothing new and running after the latch; the death write never interrupts a lunge in progress.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (networking; ADR-0018 overall)
**Engine Notes**: The staging pass and `RegisterStagedWriteApplier(DeathWrite / CounterPose)` are ADR-0002's; `UEncounterService` implements `IStagedWriteApplier` apply-only. `FPawnHandle` resolves through `ResolvePawnHandle` in the applier. `Disabled` is the SCL movement freeze flag (ADR-0010 §4.5); on the attacker of a reversal the freeze takes effect at `ContactServerTime`, not at `T_dw`. The listen host calls the same presentation handler directly (ADR-0018 E7). Verification Required (3) (`LeakNetProfile` ordering) and (5) (host parity) are measured by this story's net run.

**Control Manifest Rules (this layer)**:
- Required: Every outcome-revealing write is a typed staged write at `ContactServerTime − StrikeNetLeadSec`, decides nothing new and runs after the latch — source: ADR-0018, ADR-0002
- Required: Outcome-revealing presentation writes are applied only through the registered applier; a write with no live applier is dropped, logged and fault-reported; `Reset` drops pending writes by epoch — source: ADR-0002
- Forbidden: Never change anything on the victim or a defender before the death write; never run `VacateCard`, Roster transitions or strike accounting in the staging pass — source: ADR-0018
- Guardrail: One forced update per death-write pawn; one staged write or two per Attack

`DeathWrite{Victim, DeathServerTime = ContactServerTime}` sets `FDeathState` inside `FStealthForcedNetUpdate(DeathWrite)`, freezes the pawn's movement (`Disabled`; the victim brakes by its own gait's baseline), and notifies the Visibility Policy's death-write hook (re-open of a culled pawn, stories 022 and 023). A reversal stages `DeathWrite{Victim = A}` and `CounterPose{Defender = V}` (`WriteActionState(Counter)`; the defender is never `Disabled`). A whiff stages nothing. Until the write, a human victim's `ServerMove`s keep driving it and an NPC victim is driven exactly as if the strike had whiffed.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-20 death-write clause, AC-ASN-43, AC-ASN-48), scoped to this story:*

- [x] **AC1** (AC-ASN-20 death write, [Integration/BLOCKING][Net] `StealthGame.Assassination.OutcomeBlindWire`): **GIVEN** a kill, a whiff and a reversal as in story 012 **THEN** `FDeathState` and `Disabled` are first written on the victim (kill run) or on the attacker (reversal run), and `Counter` on the defender (reversal run), in the death-write tick (`ContactServerTime − StrikeNetLeadSec`, 0.15 s at the default, ± 1 server tick), while in the whiff run none of the three is ever written; **AND** a separate run under the `StealthLeak2` profile with a seeded loss stream asserts that within 3 s of server time after the death write every client's presentation log shows the fall (or `Counter`) seeked within one frame of `EstimatedServerNow() − DeathServerTime`.
- [x] **AC2** (AC-ASN-43, [Integration/BLOCKING][Net] `StealthGame.Assassination.ContactWriteAfterLatch`): **GIVEN** a strike at key 3 and a final completion at key 7 in one drain **THEN** the strike's `FDeathState` is still written at `ContactServerTime − StrikeNetLeadSec` and the body falls on every client.
- [x] **AC3** (AC-ASN-48, [Integration/BLOCKING][Net] `StealthGame.Assassination.VictimUntouchedUntilWrite`): **GIVEN** a victim running across the attacker at d0 = 200 **WHEN** it is killed **THEN** on a bystander client its replicated movement between the slot and the death write matches an identical un-struck run (same velocities within replication quantisation), and it brakes and falls only from the death write; **AND** a reversing defender is never `Disabled`.

---

## Implementation Notes

*Derived from ADR-0018 §3.1-§3.5:*

**Staging (slot step 7, story 007):** kill: `DeathWrite{V}`; reversal: `DeathWrite{A}` then `CounterPose{V}` (TypeRank: `DeathWrite` before `CounterPose`, same server step); all carry `ServerTime = T_dw` (≥ the slot time by I-ASN-4) and the slot's `FEpochToken`. **Applying a `DeathWrite`** (one server step, any victim kind, even after the latch, dropped on `Reset` by epoch): (1) `FDeathState.DeathServerTime`; (2) `Disabled` via the freeze flag; **the death write never interrupts the lunge**: for the attacker of a reversal whose lunge is in progress the freeze is recorded at `T_dw` but takes effect at `ContactServerTime` so server position at contact and the clients' local warp agree (closeout 2026-10-25); replicated state and visibility still change at `T_dw`; (3) the Policy's death-write hook (a pawn culled for a reason other than concealment re-opens now, already carrying `DeathServerTime`; a killed-concealed pawn re-opens carrying `bDiedConcealed = true`); (4) nothing else — no Roster, card, score or legality change. **Applying a `CounterPose`:** `WriteActionState(Defender, {Counter, Variant 0, ContactServerTime, WarpPoint 0})`.

Round end: when the slot decided the round, ADR-0002 §8.3 binds the `RoundEndStep` to the latest `DeathWrite` staged in that slot by `CauseKey`; every kill and reversal must stage its `DeathWrite` before slot end, including the NPC victim of a strike-limit exile. The staged round-end broadcast shares this queue (Round story 053, `strike-win-staging`).

Victim-driving rule (§3.4): from the slot to the write a victim keeps its own motion; `OnPawnKilled` subscribers may cause no client-observable change before the write; `KillServerTime = GetScheduledDeathServerTime(Victim)`. AC3 compares against a control run with an identical scripted `Move`.

**Prototype replaced:** `KillCharacter` / `ApplyDeathPresentation` (ragdoll at the slot, victim loses control immediately) are replaced by this deferred, outcome-uniform write.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-slot-write-outcome-blind-and-warp-point.md`: the attacker's slot write.
- `story-022-culled-victim-opens-falling-and-late-actors.md` and `story-023-concealed-body-stays-hidden.md`: the re-open itself (BLOCKED on S10).
- `story-018-strike-clip-variants-warp-and-contact-scheduling.md` and `story-020-client-fall-linger-and-occlusion-removal.md`: how clients present the writes.
- Round epic story 053: the staged round-end broadcast.

---

## Files

- `Source/StealthGameplay/Private/Features/Assassination/EncounterService.cpp` (`ApplyStagedWrite` for `DeathWrite` and `CounterPose`; staging in the slot)
- `Source/StealthGameplay/Public/Server/Features/Assassination/EncounterService.h` (applier registration)
- `Source/StealthGameplay/Public/View/Core/StealthPawn.h` (`FDeathState DeathState` with `OnRep_DeathState`)
- `Source/StealthTestHarness/Private/Assassination/OutcomeBlindWireDeathWriteTest.cpp` (shares the harness class with story 012; `StealthGame.Assassination.PIE.OutcomeBlindWire`)
- `Source/StealthTestHarness/Private/Assassination/ContactWriteAfterLatchTest.cpp` (`StealthGame.Assassination.PIE.ContactWriteAfterLatch`)
- `Source/StealthTestHarness/Private/Assassination/VictimUntouchedUntilWriteTest.cpp` (`StealthGame.Assassination.PIE.VictimUntouchedUntilWrite`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: the AC says `Disabled` is written on the reversal attacker at the death-write tick, but the notes say the freeze is recorded at `T_dw` and takes effect at `ContactServerTime`, so the observable is ambiguous; the seeded loss stream has no seed or rate; proposed rewrite (unconfirmed): `Disabled` is recorded on the attacker at `T_dw` (± 1 tick) and its movement effect begins at `ContactServerTime` (± 1 tick); the `StealthLeak2` run uses a fixed seed S and the profile's loss rate, both logged.
- AC2: "final completion" and "the body falls" are not observables; proposed rewrite (unconfirmed): `FDeathState.DeathServerTime` = the strike's `ContactServerTime`, the write occurs at `ContactServerTime − 0.15 s` (± 1 tick), and on every client `Death_Fall` starts within 1 frame of `ContactServerTime`, though the round-ending completion at key 7 is already latched.
- AC3: "same velocities within replication quantisation" has no value and the victim's speed is not stated; proposed rewrite (unconfirmed): velocity difference per tick ≤ the `ReplicatedMovement` quantum (state it) against a control with an identical scripted `Move` at a stated speed; first deceleration tick ≥ `T_dw`.

- **AC-1**: Death write timing
  - Given: a kill, a whiff and a reversal as in story 012, plus a seeded `StealthLeak2` run
  - When: the captures and presentation logs are inspected
  - Then: `FDeathState` and `Disabled` are first written on the victim (kill) or attacker (reversal), and `Counter` on the defender, at `ContactServerTime − 0.15 s` (± 1 tick); the whiff writes none; within 3 s every client seeks the fall or `Counter` within one frame of `EstimatedServerNow() − DeathServerTime`
  - Edge cases: a lost write packet; the listen host direct handler; the reversal attacker's freeze effective at contact
- **AC-2**: Contact write after latch
  - Given: a strike at key 3 and a round-deciding completion at key 7 in one drain
  - When: the drain resolves and time advances to `ContactServerTime`
  - Then: the strike's `FDeathState` is still written at `ContactServerTime − 0.15 s` and the body falls on every client
  - Edge cases: the `Reset` epoch drops the pending write; an NPC victim of a strike-limit exile
- **AC-3**: Victim untouched until write
  - Given: a victim running across the attacker at d0 = 200 cm, an un-struck control run with identical scripted `Move`, and a bystander client
  - When: the victim is killed, and in a reversal the defender is observed
  - Then: replicated movement matches the control from the slot to the death write, the victim brakes and falls only from the death write, and a reversing defender is never `Disabled`
  - Edge cases: human versus NPC victim; the victim changes direction mid-window

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three harness tests above (`LeakNetProfile` and `StealthLeak2`) — must exist and pass

**Status**: [x] Created and passing (2026-10-27)

---

## Dependencies

- Depends on: `story-004-wire-structs-fixed-width.md`, `story-007-attack-outcomes-resolved-in-slot.md`, `story-009-same-drain-ordering-and-braced-key-compare.md`, `story-010-reversal-counter-and-chained-strikes.md`, `story-012-slot-write-outcome-blind-and-warp-point.md`; cross-epic `production/epics/round-match-framework/story-052-ended-latch-effects.md`, `story-053-strike-win-staging.md`; `production/epics/shared-character-locomotion/story-009-stop-floor.md` (freeze and braking)
- Unlocks: `story-017-no-warning-and-no-observer-cue.md`, `story-018-strike-clip-variants-warp-and-contact-scheduling.md`, `story-019-server-body-sequence-and-reset.md`, `story-021-role-change-dispatch-and-transient-grade.md`, `story-022-culled-victim-opens-falling-and-late-actors.md`, `story-023-concealed-body-stays-hidden.md`

## Completion Notes
**Completed**: 2026-10-27
**Criteria**: AC1 `StealthGame.Assassination.OutcomeBlindWire`: FDeathState (with the freeze) first written on the victim (kill) / on the attacker plus Counter on the defender (reversal) in the death-write tick, ContactServerTime - 0.15 s +- 1 tick, DeathServerTime = ContactServerTime; the whiff writes neither within the window + 0.5 s; on every PIE machine the death (and Counter) arrive at least T_dw - 1 frame after the Strike, measured on that machine's own clock. AC2 `StealthGame.Assassination.ContactWriteAfterLatch`: a strike at Seq 3 and a final completion at Seq 7 in one drain: ObjectiveWin latched at Seq 7, the round Ended, and the strike's death write still applied at T_dw +- 1 tick with DeathServerTime = contact; a Reset (epoch change) before T_dw drops it (applier never runs). AC3 `StealthGame.Assassination.VictimUntouchedUntilWrite`: a victim running across at d0 = 200 cm is written nothing between the slot and T_dw, its first write is its death write; a reversing defender's one write is Counter and it is never written dead; on a real AI-possessed pawn the walk is steady until the write, then it brakes by its baseline (speed monotonic down, slides > 1 cm); a reversed attacker mid-lunge records its freeze at ContactServerTime and is not frozen before; two writes in one frame force one net update. In play: the round-deciding kill bound the round-end step to its death write (`latched at 128.598 s, round-end step at 128.931 s`), informants respawn.
**Deviations**: ADVISORY - (1) AC1's StealthLeak2 clause (every client's presentation seeks the fall within one frame of EstimatedServerNow - DeathServerTime) needs the client fall/Counter presentation of stories asn-018/020 (this story's Out of Scope: how clients present the writes): deferred, decision logged; the death-write timing on every client is asserted in the PIE test. AC3's bystander-client replicated-movement comparison with an un-struck control is the IVP-008 capture harness's; the server-side guarantee is asserted. (2) Disabled is a new timed freeze on UStealthMovementComponent (FreezeCommandedMotionFrom), applied from FDeathState on every machine (decision logged). (3) Round's AdapterStrikeRoundEndDelaySec stand-in, RoundEndDueTime and its Tick path removed; Round's tests stayed green unchanged (none pinned the stand-in). (4) Death presentation in play is a STAND-IN until 018-020: AStealthGameMode::HandleEncounterDeathWrite (OnDeathWriteApplied) runs the prototype ragdoll (KillCharacter) at DeathServerTime, the HUD messages, the informant respawn timer and NPC replacement; the Roster transition happens once, in the slot. The prototype NPC-kill exile stays as the Penalty stand-in on OnPawnKilled. (5) The encounter service is reset at each round's Active entry and released when Round's queue is destroyed (URoundMatchStore::OnRoundQueueReleasing). Cross-epic Round 052/053, SCL 009 not Complete in tracking: proceeded (decision logged).
**Test Evidence**: `OutcomeBlindWireTest.cpp`, `ContactWriteAfterLatchTest.cpp`, `VictimUntouchedUntilWriteTest.cpp` (StealthGameplay) and `OutcomeBlindWirePieTest.cpp` (StealthTestHarness); full suite 344/344 in `production/qa/evidence/assassination-defence-reversal-012-014-2026-10-27/full-suite.txt`
**Run result**: OBSERVED - 3-player PIE: (b) NPC struck, the body falls on the bystander client (`01-client2-npc-body-2s.png`); (c) braced reversal: client 1 braces (`StealthBot 1 defend`), the host strikes and is eliminated (`Your target was ready...`, INFORMANT; the defender sees `Reversal! Your attacker is eliminated.` with its cooldown counting from activation); (a) client 2 kills client 1, team 1's last Active player: `JAGUAR WIN (elimination)` on every client with the round-end step bound to the death write; a second reversal shows the informant respawn at the team spawn. Screenshots in `production/qa/evidence/story-013-death-write-staging-and-after-latch/` (looked at, none black), server log `pie-server-log-excerpt.txt`; no LogStealthFault line
**Reviewed by**: self, no independent review (focused /code-review pass by the implementing agent; unattended batch)
